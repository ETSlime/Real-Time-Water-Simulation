## 水体粒子渲染

水体模拟系统采用粒子驱动方式实现，所有水面表现均由流体粒子叠加生成，支持真实流动、碰撞与聚集等行为。该系统不同于一次性销毁的火焰或烟雾等特效，其粒子具有持续生命周期，支持大规模模拟并与场景发生交互。

### 水粒子发射逻辑（Emitter）
- 水粒子通过 `Emit()` 接口由发射器控制，每帧生成一定数量的粒子
- 粒子为**持续存在的物理单元**，不会在短时间内自动消亡
- 发射区域支持 Cone、Box 等形状，粒子会在发射时获取初始速度与位置
- 利用 `AliveList` 管理存活状态，通过 `DrawIndirect` 实现 GPU 端高效绘制

### SPH 流体系统
水体模拟基于 SPH（Smoothed Particle Hydrodynamics）算法。每个粒子作为流体单元，在重力、粘性、压力等作用下流动、积聚与碰撞。

系统支持两种粒子空间索引结构：

#### Dense Grid
- 均匀分布的体素栅格
- 用于初期测试或粒子数量较少的场景
- 使用 `FlattenIndex(x, y, z)` 访问体素数据

#### Sparse Grid（稀疏体素结构）
- 用于优化大规模水体模拟，粒子稀疏分布且空间大
- 采用 Morton 编码对粒子进行空间离散与排序
- 全程在 GPU 上进行粒子查询与交互计算

#### SPH 粒子稀疏 Grid 构建流程
在每帧模拟中，系统会基于上帧的活跃粒子构建稀疏体素结构，用于加速粒子之间的邻居查询与交互计算。整体流程如下：

1. CPU 侧通过 ReadBack 操作获取前一帧存活的粒子列表，作为构建 Grid 的基础数据。
2. 遍历所有粒子的位置，计算其所属的体素坐标，并更新 `gridMin` 以确保所有坐标非负。
3. 对每个体素单元计算其 Morton 编码，将对应粒子的 ID 累积到 `mortonMap` 哈希表中。
4. 将 `mortonMap` 中的元素转换为 `TempTriplet` 结构（记录 Morton 编码与粒子 ID），并按 Morton 编码进行排序。
5. 根据排序结果构建 `SPHIndexTriplet`，将相同 Morton 编码的粒子聚合。
6. 从 `SPHIndexTriplet` 生成 `PagedIndexTripletBuffer`，用于分页存储 Morton 编码及粒子范围，并上传至 GPU。

> 从下一帧开始，GPU 将基于该分页结构高效地进行邻居检索与粒子间作用力计算：

7. 每个粒子使用其 `prevPosition` 反推上帧所属体素，并获取周围邻接体素的 Morton 编码。
8. 通过 `PagedIndexTripletBuffer` 查询邻近 Morton 编码对应的粒子范围（即页信息）。
9. 使用 `g_SortedIndicesSRV` 获取实际粒子索引，并通过 `g_ParticlesUAV` 获取邻居粒子数据，执行密度、压力、粘性等 SPH 力的计算。

#### SPH 力计算公式（Pressure / Viscosity）

在每帧模拟中，GPU 会对每个粒子执行 3×3×3 邻域粒子搜索，并根据 SPH 核函数进行密度与力的估算。

- 使用 **Wendland C2 核函数** 计算每对粒子之间的影响权重
- 通过核函数结果加权累积邻域粒子质量，估算自身密度
- 压力项由密度差与核函数梯度（`∇W`）共同决定，用于模拟粒子间的排斥
- 粘性项利用粒子间速度差与 `∇W` 进行平滑，防止高速粒子穿透
- 最终力为：
  - `pressureForce += -mass * (p_i + p_j) / 2ρ_j * ∇W`
  - `viscosityForce += viscosity * velDiff * mass / ρ_j * ∇W`
- 所有力项最终加入 **XSPH 补正项**，用于统一粒子速度以防离散：
  - `velocity += ε * Σ (neighbor.velocity - self.velocity) * W`

这些计算全部通过 Compute Shader 实现，遍历所有周围 Morton 编码体素并处理其中粒子。


#### Voxel Grid 联动三角形碰撞检测

水粒子与场景地形之间的交互通过 Voxel Grid 实现粒子与三角形的高效碰撞检测，主要过程如下：

- 每帧构建或复用 `VoxelGrid`（基于 Morton 编码的稀疏结构）以存储静态地形三角形
- 在 GPU 中，每个粒子根据预测位置（`predictedPosition`）射出一条 Ray
- 使用 `RayIntersectsTriangle()` 函数测试与周围体素中的所有三角形是否相交
- 若发生碰撞，根据碰撞点法线计算反射力并调整速度：
  - **阻止穿透**：粒子沿法线方向反射/滑动
  - **地表滑动**：保留法线切向速度，消除垂直速度分量
- 最终实现自然下坡流动、边缘积聚、容器内回旋等真实水体行为

该模块与 SPH 力计算并行执行，实现流体对流、地形响应与粒子粘聚等效果的融合模拟。

### 液面重建与 Marching Cubes

水粒子在模拟过程中本身为离散粒子，无法直接构成连续液面，因此需通过 Marching Cubes 算法对其进行**体素场重建与网格提取**，形成真实可渲染的水面。

#### Scalar Field 构建方式

系统支持两种方式创建标量场（Scalar Field）以供 Marching Cubes 使用：

##### Dense Grid（均匀体素场）
- 分配固定大小的 3D Grid
- 每帧清零后重新累积粒子权重
- 使用 `FlattenIndex(x, y, z)` 映射为线性数组
- 优点：结构简单，适合测试或局部重建
- 缺点：内存占用大，不适用于大范围动态水体

##### Sparse Grid（稀疏体素场）
- 使用粒子位置构建稀疏体素结构，仅保留活跃体素
- 每帧由粒子发射中心出发，遍历影响半径内的体素
- 使用 **64 位 Morton 编码**（支持超大范围）作为哈希索引  
- ScalarField 采用拉式构建（Voxel-Centric）：  
  - 每个体素主动查找周围粒子并累积权重  
  - 权重根据距离衰减，核函数统一使用 **Wendland C2**  
- 所有体素数据上传为 `StructuredBuffer<float>`，支持 GPU 并发写入（`InterlockedAdd`）

稀疏 Scalar Field 构建逻辑与 `SPHSystem` 架构一致，配合共享的 Morton 编码、页表、索引排序机制实现粒子/体素统一空间表示。

#### Marching Cubes 网格重建流程

一旦 Scalar Field 构建完成，系统会在 GPU 上执行 Marching Cubes 过程以生成流体表面网格：

1. 遍历所有体素格点，判断该格点周围 8 个顶点的密度值是否跨越阈值
2. 根据格点配置查表 (`g_TriTableSRV`) 获取当前 cube 对应的三角形拓扑
3. 计算每条边上的交点位置与法线，使用 `InterlockedAdd` 写入 `VertexBuffer` 与 `IndexBuffer`
4. 所有数据通过 `DrawIndexedInstancedIndirect` 进行渲染，支持多 Cluster 分区并行重建

为了避免重复创建顶点，引擎内部使用 **Edge Vertex Cache** 机制缓存边索引，提升构建效率。

#### 基于 Thickness Map 的液面着色

为提高水体的体积感与真实感，系统结合了屏幕空间厚度图（Thickness Map）进行辅助着色：

- 在渲染水粒子时将其深度信息写入厚度图
- Marching Cubes 网格的像素在渲染阶段采样该厚度图进行遮蔽和加深处理
- 实现效果包括：
  - 液体边缘半透明
  - 深水区域更暗

该模块与 `FluidThicknessRenderer` 组件协作，统一管理所有厚度图资源与渲染流程。