// ============================================================================
//  BuildGridCS.hlsl & SPHSimulateCS.hlsl (統合ソース)
//  Author :
// ----------------------------------------------------------------------------
//  本ファイルは 2 つの Compute Shader を同一ファイルに収めています。
//  1) BuildGridCS   : パーティクルをユニフォームグリッドへ分類し、
//                     SortedIndices と GridStartIndex を構築します。
//  2) SPHSimulateCS : SPH の『密度→圧力→力計算→位置更新→地形衝突』
//                     さらに Alive / Free List の管理を 1 Dispatch で行います
// ============================================================================
//  * u4  : RWStructuredBuffer<uint>      SortedIndices            (BuildGrid)
//  * u5  : RWStructuredBuffer<uint>      GridStartIndex           (BuildGrid)
//  * u0  : RWStructuredBuffer<Particle>  Particles                (Simulate)
//  * u1  : AppendStructuredBuffer<uint>  AliveList                (Simulate)
//  * u2  : AppendStructuredBuffer<uint>  FreeList                 (Simulate)
//  * t6  : StructuredBuffer<Particle>    Particles (Read Only)    (Both)
//  * t12 : StructuredBuffer<uint>        SortedIndices            (Simulate)
//  * t13 : StructuredBuffer<uint>        GridStartIndex           (Simulate)
// ============================================================================
#include "VoxelCollisionUtil.hlsl"
#include "SharedParticleUtil.hlsl"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SLOT_CB_EFFECT_SPH                  b8
#define SLOT_UAV_SORTED_INDICES             u4
#define SLOT_UAV_GRID_START_INDEX           u5
#define SLOT_UAV_GRID_COUNTER               u6
#define SLOT_SRV_SORTED_INDICES             t12
#define SLOT_SRV_GRID_START_INDEX           t13
#define SLOT_SRV_GRID_COUNTER               t14
#define SLOT_SRV_SPH_GRID_PAGE_HEADER       t15
#define SLOT_SRV_SPH_INDEX_TRIPLETS         t16

#define EPSILON                             1e-7f
#define PI                                  3.141592

//*****************************************************************************
// 構造体定義
//*****************************************************************************

// 粒子構造体
struct WaterFluidParticle
{
    uint particleID; // パーティクルID（ユニークな識別子）
    float3 position; // 現在位置
    float3 prevPosition; // 前フレーム位置
    float density; // 密度（周囲の粒子から算出）
    
    float3 velocity; // 速度
    float lambda; // 拘束条件解（位置補正のための係数）
    float pressure; // 圧力（密度から算出される）
    
    float3 predictedPosition; // 予測位置（次の位置、拘束前）
    float lifeRemaining; // 寿命（将来拡張用、今は1で固定でも可）

    float4 color; // デバッグ用カラー（密度に応じた可視化など）
    
    float size; // サイズ
};

// SPH 計算用のパラメータ構造体
struct SPHSimParams
{
    float smoothingRadius; // h (SPH 平滑半径)
    float particleMass; // 質量 m
    float restDensity; // 目標密度 ρ0
    float pressureMultiplier; // 圧力係数 k (圧力 = k * (密度 - ρ0))
    
    float viscosity; // 粘性係数
    float3 acceleration; // 外力 (重力など)
    
    uint3 gridDim; // グリッド分割数 (x,y,z)
    uint gridCellStart; // グリッドセルの開始インデックス（SortedIndices のオフセット）
    
    float3 gridMin; // ワールド空間でのグリッド最小座標
    uint maxParticleCount; // 総粒子数
    
    float cellSize; // 1 セルの大きさ (= h 推奨)
    uint maxParticlesPerCell; // 1 セルあたりの最大パーティクル数
    
    float deltaTime; // タイムステップ (Δt)
    uint totalPageHeaderCount; // SPH グリッドページヘッダーの総数
    
    float pressureKernelScale; // 圧力カーネルスケール係数（圧力計算に使用されるスケーリング係数）
    float viscosityKernelScale; // 粘性カーネルスケール係数
    float taitExponent; // Tait 式の指数（圧力計算用）
    float maxPressure; // 最大圧力制限（過度な圧力を防ぐため）
    float xsphFactor; // XSPH 粘性項の係数（0.0f で無効化、1.0f で有効化）
    
    float restitution; // 反発係数（衝突時の挙動制御用）
    float friction; // 摩擦係数（衝突時の摩擦挙動制御用）
    float pressureForceScale; // 圧力力スケール係数（圧力計算に使用されるスケーリング係数）
    float viscosityForceScale; // 粘性力スケール係数（粘性力の計算に使用されるスケーリング係数）
    
    float maxAccel; // 最大加速度制限（過度な加速度を防ぐため）
    float maxVelocity; // 最大速度制限（過度な速度を防ぐため）
    float padding; // パディング（アライメント用）
};

// IndexTripletPageHeader：IndexTripletのページヘッダー
struct IndexTripletPageHeaderVec2
{
    uint2 pageStartMorton;  // ページ最小モートン番号
    uint2 pageEndMorton;    // ページ最大モートン番号
    uint pageOffset;        // g_IndexTripletsでのオフセット
    uint pageCount;         // このページのトリプレット数
};


struct SPHIndexTriplet
{
    uint3 gridCoord;    // 明示的ボクセル座標（decode 不要）
    uint2 mortonIndex;  // 64-bit Morton（下位: x, 上位: y）
    uint startOffset;   // g_GridSortedIndices[] の開始位置
    uint count;         // そのセルに含まれる粒子数
};

//*****************************************************************************
// 定数バッファ
//*****************************************************************************

cbuffer CBSPHParams : register(SLOT_CB_EFFECT_SPH)
{
    SPHSimParams SPHParams;
};

// バッファ
RWStructuredBuffer<WaterFluidParticle> g_ParticlesUAV               : register(SLOT_UAV_PARTICLE); // 書き込み用パーティクルデータバッファ
StructuredBuffer<WaterFluidParticle> g_ParticlesSRV                 : register(SLOT_SRV_PARTICLE); // 読み取り用パーティクルデータバッファ
RWStructuredBuffer<uint> g_SortedIndicesUAV                         : register(SLOT_UAV_SORTED_INDICES); // 書き込み用ソート済みインデックス
StructuredBuffer<uint> g_SortedIndicesSRV                           : register(SLOT_SRV_SORTED_INDICES); // 読み取り用ソート済みインデックス
RWStructuredBuffer<uint> g_GridStartIndexUAV                        : register(SLOT_UAV_GRID_START_INDEX); // 書き込み用グリッド開始位置
StructuredBuffer<uint> g_GridStartIndexSRV                          : register(SLOT_SRV_GRID_START_INDEX); // 読み取り用グリッド開始位置
RWStructuredBuffer<uint> g_GridCounterUAV                           : register(SLOT_UAV_GRID_COUNTER); // 各グリッドセル内のパーティクル数カウンタ（原子加算用）
StructuredBuffer<uint> g_GridCounterSRV                             : register(SLOT_SRV_GRID_COUNTER); // 読み取り用グリッドカウンタ
StructuredBuffer<IndexTripletPageHeaderVec2> g_GridPageHeadersSRV   : register(SLOT_SRV_SPH_GRID_PAGE_HEADER); // SPH グリッドページヘッダー（各グリッドセルのパーティクルインデックスを管理）
StructuredBuffer<SPHIndexTriplet> g_IndexTripletsSRV                : register(SLOT_SRV_SPH_INDEX_TRIPLETS); // SPH インデックス三つ組（モートンコードとパーティクルインデックスの対応）

//================== 補助関数 ==================
// Poly6 / Spiky / Viscosity カーネル
float W_poly6(float r, float h)
{
    float x = h * h - r * r;
    return (315.0 / (64.0 * PI * pow(abs(h), 9))) * pow(x, 3);
}

float W_spiky_grad(float r, float h)
{
    if (r >= h || r == 0.0f)
        return 0.0f;

    float coef = -45.0 / (PI * pow(h, 6));
    return coef * pow(h - r, 2);
}

float W_wendland_grad(float r, float h)
{
    if (r >= h || r == 0.0f)
        return 0.0f;

    float q = r / h;
    float x = 1.0 - q;
    float coeff = 315.0 / (64.0 * PI * pow(h, 3));
    return coeff * -20.0 * x * x * x * q / h;
}

float W_wendland(float r, float h)
{
    if (r >= h)
        return 0.0f;
    float q = r / h;
    float x = 1.0f - q;
    float coeff = 315.0 / (64.0 * PI * pow(h, 3)); // 3D normalization
    return coeff * x * x * x * x * (1.0 + 4.0 * q);
}

float W_visc_laplacian(float r, float h)
{
    return (45.0 / (PI * pow(h, 6))) * (h - r);
}

// 座標がグリッド内かどうかを判定
bool IsCoordInBounds(uint3 coord, uint3 gridDim)
{
    return (coord.x < gridDim.x) &&
           (coord.y < gridDim.y) &&
           (coord.z < gridDim.z);
}

// グリッド座標を 1次元インデックスに変換
uint Flatten3DTo1D(uint3 coord, uint3 gridDim)
{
    return coord.x + coord.y * gridDim.x + coord.z * gridDim.x * gridDim.y;
}

bool EqualMorton64(uint2 a, uint2 b)
{
    return (a.x == b.x) && (a.y == b.y);
}

bool LessMorton64(uint2 a, uint2 b)
{
    return (a.y < b.y) || (a.y == b.y && a.x < b.x);
}

// 1ビットおきにゼロを挿入して 64bit 展開する関数 (uint2 を返す)
// 21bit 入力に対応 (x, y, z の max 値 = 2^21 = 2097152 未満)
uint2 Part1By2_64_HLSL(uint n)
{
    n &= 0x1FFFFF; // 上位ビットマスク（21bit制限）

    // 32bit内で展開（下位ビット）
    uint x = n;
    x = (x | (x << 16)) & 0x030000FF;
    x = (x | (x << 8)) & 0x0300F00F;
    x = (x | (x << 4)) & 0x030C30C3;
    x = (x | (x << 2)) & 0x09249249;

    // 64bitに相当する uint2 を返す（上位は 0）
    return uint2(x, 0);
}

// GPU版のMortonエンコード
uint2 EncodeMorton3_64_GPU(uint x, uint y, uint z)
{
    uint2 xx = Part1By2_64_HLSL(x);
    uint2 yy = Part1By2_64_HLSL(y);
    uint2 zz = Part1By2_64_HLSL(z);

    // x | (y << 1) | (z << 2)
    uint2 result;
    result.x = (xx.x) | (yy.x << 1) | (zz.x << 2);
    result.y = (yy.x >> 31) | (zz.x >> 30); // これで上位bit補完（3方向の合成）
    return result;
}

// 64ビットMortonコードをコンパクト化する関数
uint Compact1By2_64_HLSL(uint n)
{
    n &= 0x09249249;
    n = (n ^ (n >> 2)) & 0x030C30C3;
    n = (n ^ (n >> 4)) & 0x0300F00F;
    n = (n ^ (n >> 8)) & 0x030000FF;
    n = (n ^ (n >> 16)) & 0x000003FF;
    return n;
}

// 64ビットMortonコードをデコードする関数
uint3 DecodeMorton3_64_GPU(uint2 morton)
{
    uint x = Compact1By2_64_HLSL(morton.x);
    uint y = Compact1By2_64_HLSL(morton.x >> 1);
    uint z = Compact1By2_64_HLSL(morton.x >> 2);
    return uint3(x, y, z);
}



// ------------------------------
// BuildGridCS
// ------------------------------
// グリッドセル内カウンタを確保するため、GridStartIndex は毎フレーム Ping-Pong
// GridStartIndex 初期値は 0xFFFFFFFF (未設定)。
// -----------------------------------------------------------------------------
[numthreads(THREAD_COUNT, 1, 1)]
void BuildGirdCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint id = dispatchThreadID.x;
    if (id >= g_MaxParticleCount)
        return;

    // 読み込み
    WaterFluidParticle p = g_ParticlesUAV[id];

    // ワールド座標をグリッド座標へ変換
    float3 rel = p.position - SPHParams.gridMin;
    uint3 gridCoord = (uint3) floor(rel / SPHParams.cellSize);
    uint flat = Flatten3DTo1D(gridCoord, SPHParams.gridDim);

    // 各セルにおける書き込みインデックスを取得（原子加算）
    uint localOffset;
    InterlockedAdd(g_GridCounterUAV[flat], 1, localOffset);
    
    // Global list の中でこの粒子が書き込む場所を計算（スタート + オフセット）
    uint globalIndex = SPHParams.gridCellStart + flat * SPHParams.maxParticlesPerCell + localOffset;
    g_SortedIndicesUAV[globalIndex] = id;

    // GridStartIndex を一度だけ設定（CompareExchange）
    uint expected = 0xFFFFFFFF;
    InterlockedCompareExchange(g_GridStartIndexUAV[flat], globalIndex, expected, expected); // start = 最初の粒子位置

    // インデックスを書き込み
    g_SortedIndicesUAV[globalIndex] = id;
}

// ------------------------------
// SPHSimulateCS
// ------------------------------
// SPH計算と位置更新をまとめて行う
// -------------------------------
[numthreads(THREAD_COUNT, 1, 1)]
void SimulateCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint id = dispatchThreadID.x;
    if (id >= g_MaxParticleCount)
        return;

    WaterFluidParticle p = g_ParticlesUAV[id];
    p.particleID = id;
    
    // ==== 生存している場合：更新処理 ====
    if (p.lifeRemaining > 0.0f)
    {
        // ------------------
        // Step 1: 密度 & 圧力計算
        // ------------------
        float density = 0.0f;

        // 所属グリッド座標
        float3 rel = p.position - SPHParams.gridMin;
        uint3 gridCoord = (uint3) floor(rel / SPHParams.cellSize);

        // 3x3x3 の近傍セルを走査
        [allow_uav_condition]
        for (int z = -1; z <= 1; ++z)
        {
            [allow_uav_condition]
            for (int y = -1; y <= 1; ++y)
            {
                [allow_uav_condition]
                for (int x = -1; x <= 1; ++x)
                {
                    int3 neighborCoordInt = int3(gridCoord) + int3(x, y, z);
                    if (any(neighborCoordInt < 0)) // 負の座標は無視
                        continue;
                    uint3 neighborCoord = uint3(neighborCoordInt);
                    
                    if (!IsCoordInBounds(neighborCoord, SPHParams.gridDim))
                        continue;
                    
                    uint flat = Flatten3DTo1D(neighborCoord, SPHParams.gridDim);
                    uint startIndex = g_GridStartIndexSRV[flat];
                    if (startIndex == 0xFFFFFFFF)
                        continue;
                    
                    uint count = g_GridCounterSRV[flat];
                    [allow_uav_condition]
                    for (uint i = 0; i < count; ++i)
                    {
                        if (startIndex + i >= SPHParams.maxParticleCount)
                            break;
                        
                        uint neighborID = g_SortedIndicesSRV[startIndex + i];
                        WaterFluidParticle q = g_ParticlesUAV[neighborID];
                        
                        float3 r = p.position - q.position;
                        float dist = length(r);
                        if (dist < SPHParams.smoothingRadius && dist > EPSILON)
                        {
                            float W = SPHParams.pressureKernelScale * W_poly6(dist, SPHParams.smoothingRadius);
                            density += SPHParams.particleMass * W;
                        }
                    }
                }
            }
        }
    
        p.density = density;
        p.pressure = SPHParams.pressureMultiplier * max(density - SPHParams.restDensity, 0.0);

        // ------------------
        // Step 2: 力計算 (圧力 + 粘性)
        // ------------------
        float3 pressureForce = float3(0, 0, 0);
        float3 viscosityForce = float3(0, 0, 0);
        float3 xsphVel = float3(0, 0, 0);
        
        // 近傍セルを再度走査して圧力と粘性の力を計算
        [allow_uav_condition]
        for (int zz = -1; zz <= 1; ++zz)
        {
            [allow_uav_condition]
            for (int yy = -1; yy <= 1; ++yy)
            {
                [allow_uav_condition]
                for (int xx = -1; xx <= 1; ++xx)
                {
                    int3 neighborCoordInt = int3(gridCoord) + int3(xx, yy, zz);
                    if (any(neighborCoordInt < 0)) // 負の座標は無視
                        continue;
                    uint3 neighborCoord = uint3(neighborCoordInt);
                    
                    if (!IsCoordInBounds(neighborCoord, SPHParams.gridDim))
                        continue;
                    uint flat = Flatten3DTo1D(neighborCoord, SPHParams.gridDim);
                    uint startIndex = g_GridStartIndexSRV[flat];
                    if (startIndex == 0xFFFFFFFF)
                        continue;
                    uint count = g_GridCounterSRV[flat];
                    
                    [allow_uav_condition]
                    for (uint i = 0; i < count; ++i)
                    {
                        if (startIndex + i >= SPHParams.maxParticleCount)
                            break;
                        uint neighborID = g_SortedIndicesSRV[startIndex + i];
                        WaterFluidParticle q = g_ParticlesUAV[neighborID];
                        
                        float3 r = p.position - q.position;
                        float dist = length(r);
                        if (dist < SPHParams.smoothingRadius && dist > EPSILON)
                        {
                            if (q.density <= 0.0f || isnan(q.density))
                                continue;
                            float invDensityQ = 1.0f / max(q.density, EPSILON);
                            
                            if (p.density <= 0.0f || isnan(p.density))
                                continue;
                            float invDensityP = 1.0f / max(p.density, EPSILON);
                            
                            // 圧力項
                            float3 dir = (dist > EPSILON) ? r / dist : float3(0, 0, 0);
                            float W_spiky = SPHParams.pressureKernelScale * W_spiky_grad(dist, SPHParams.smoothingRadius);
                            float avgPressure = (p.pressure + q.pressure) / 2.0f;
                            pressureForce -= dir * avgPressure * W_spiky * SPHParams.particleMass * SPHParams.particleMass * invDensityQ * invDensityP;

                            // 粘性項
                            float3 velDiff = q.velocity - p.velocity;
                            float viscosity = SPHParams.viscosity;
                            float W_visc = SPHParams.viscosityKernelScale * W_visc_laplacian(dist, SPHParams.smoothingRadius);
                            viscosityForce += viscosity * velDiff * W_visc * SPHParams.particleMass * SPHParams.particleMass * invDensityQ * invDensityP;
                            float Wxshp = SPHParams.viscosityKernelScale * W_poly6(dist, SPHParams.smoothingRadius);
                            xsphVel += SPHParams.xsphFactor * velDiff * Wxshp * SPHParams.particleMass * SPHParams.particleMass * invDensityQ * invDensityP;
                        }
                    }
                }
            }
        }

        // ------------------
        // Step 3: 位置 & 速度更新
        // ------------------
        float3 gravity = g_Acceleration;
        //float3 accel = (pressureForce + viscosityForce) / p.density + gravity;
        float3 accel = gravity;
        p.velocity += accel * g_DeltaTime;
        float3 deltaPos = p.velocity * g_DeltaTime;
        p.predictedPosition = p.position + deltaPos;

        // ------------------
        // Step 4: 地形衝突（Voxel Grid）
        // ------------------
        float4 worldPredicted = mul(float4(p.predictedPosition, 1.0), g_World);
        uint3 voxelCoord = GetVoxelCoord(worldPredicted.xyz);
        uint mortonIndex = EncodeMorton3(voxelCoord.x, voxelCoord.y, voxelCoord.z);
        
        int found = FindIndexTriplet(mortonIndex);
        if (found >= 0)
        {
            IndexTriplet trip = g_IndexTriplets[found];
            float3 bestHit = float3(0, 0, 0);
            float3 bestNormal = float3(0, 0, 0);
            float closestRayDistance = 1e20;
            float4 rayStartWS = mul(float4(p.position, 1.0), g_World);
            float4 rayDirWS = mul(float4(deltaPos, 0.0), g_World);
            float3 rayDir = normalize(rayDirWS.xyz);
            float rayRange = length(rayDirWS.xyz);
            float3 rayStart = rayStartWS.xyz + rayDir * 0.001f;

            [allow_uav_condition]
            for (uint i = 0; i < trip.count; ++i)
            {
                TriangleStruct tri = g_Triangles[g_VoxelTriangleIndices[trip.startOffset + i]];
                float3 hitPoint = float3(0, 0, 0);
                float rayDist = 0;
                if (RayIntersectsTriangle(rayStart, rayDir, tri, hitPoint, rayDist))
                {
                    if (rayDist <= rayRange && rayDist < closestRayDistance)
                    {
                        closestRayDistance = rayDist;
                        bestHit = hitPoint;
                        bestNormal = tri.normal; // 命中した三角形の法線を保存
                    }
                }
            }
            if (closestRayDistance < 1e19)
            {
                // 位置修正 + 速度投影 (滑り)
                float3 terrainNormal = normalize(bestNormal);
                //p.velocity = 0;
                p.color = float4(1, 0, 0, 1); // 衝突時は赤くする
                float4 localHit = mul(float4(bestHit, 1.0), g_WorldInv);
                p.predictedPosition = localHit.xyz + terrainNormal * 0.01f; // 地面より少しだけ押し戻す
                p.velocity = ApplyCollisionSlideVelocity(p.velocity, terrainNormal, 0.99f, 0.01f);
                
            }
        }
        
        p.position = p.predictedPosition;

        // ------------------
        // Step 5: 書き戻し & Alive/Free List 更新
        // ------------------
        g_ParticlesUAV[id] = p;
        g_AliveListUAV.Append(id);
    
    }
    else
    {
        // 死亡した場合は FreeList に登録
        g_FreeListUAV.Append(id);
    }
}

// 64-bit Morton に対応した FindGridPageHeader 関数
int FindGridPageHeader(uint2 morton)
{
    if (SPHParams.totalPageHeaderCount == 0)
        return -1; // ページヘッダーがない場合はエラー
    
    uint left = 0;
    uint right = SPHParams.totalPageHeaderCount - 1;

    // IndexTripletPageHeader の二分探索（ページ範囲）
    [allow_uav_condition]
    while (left <= right)
    {
        uint mid = (left + right) / 2;
        IndexTripletPageHeaderVec2 pageHeader = g_GridPageHeadersSRV[mid];

        if (LessMorton64(morton, pageHeader.pageStartMorton))
        {
            if (mid == 0)
                return -1; // ページ範囲外
            right = mid - 1;
        }
        else if (LessMorton64(pageHeader.pageEndMorton, morton))
        {
            if (mid == SPHParams.totalPageHeaderCount - 1)
                return -1; // ページ範囲外
            left = mid + 1;
        }
        else
        {
            // ページの範囲に入っている！
            uint offset = pageHeader.pageOffset;

            [allow_uav_condition]
            for (uint i = 0; i < pageHeader.pageCount; ++i)
            {
                SPHIndexTriplet triplet = g_IndexTripletsSRV[offset + i];
                if (EqualMorton64(triplet.mortonIndex, morton)) // ここで「一致」を確認
                {
                    return offset + i;
                }
            }

            // ページ内にない場合は構造エラー
            return -2;
        }
    }

    // 完全に見つからない場合
    return -1;
}


// ------------------------------
// SPHSimulateSparseCS
// ------------------------------
// SPH計算と位置更新をまとめて行う
// -------------------------------
[numthreads(THREAD_COUNT, 1, 1)]
void SPHSimulateSparseCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint id = dispatchThreadID.x;
    if (id >= g_MaxParticleCount)
        return;

    WaterFluidParticle p = g_ParticlesUAV[id];
    p.particleID = id;

    // ==== 生存している場合：更新処理 ====
    if (p.lifeRemaining > 0.0f)
    {
        //======================
        // Step 1: 密度 & 圧力計算
        //======================
        float density = 0.0f;
        float3 rel = p.prevPosition - SPHParams.gridMin;
        uint3 gridCoord = (uint3) floor(rel / SPHParams.cellSize);
        uint2 centerMorton = EncodeMorton3_64_GPU(gridCoord.x, gridCoord.y, gridCoord.z);
        
        // 近傍の 3x3x3 セルを走査
        [allow_uav_condition]
        for (int z = -1; z <= 1; ++z)
        {
            [allow_uav_condition]
            for (int y = -1; y <= 1; ++y)
            {
                [allow_uav_condition]
                for (int x = -1; x <= 1; ++x)
                {
                    int3 neighborCoordInt = int3(gridCoord) + int3(x, y, z);
                    if (any(neighborCoordInt < 0)) // 負の座標は無視
                        continue; 
                    uint3 neighborCoord = uint3(neighborCoordInt);
                    uint2 morton = EncodeMorton3_64_GPU(neighborCoord.x, neighborCoord.y, neighborCoord.z);
                    
                    int found = FindGridPageHeader(morton);
                    if (found < 0)
                        continue;
                    
                    SPHIndexTriplet cell = g_IndexTripletsSRV[found];
                    [allow_uav_condition]
                    for (uint i = 0; i < cell.count; ++i)
                    {
                        uint neighborID = g_SortedIndicesSRV[cell.startOffset + i];

                        if (neighborID == id || neighborID >= g_MaxParticleCount)
                        {
                            continue;
                        }

                        WaterFluidParticle q = g_ParticlesUAV[neighborID];
                        float3 r = p.position - q.position;
                        float dist = length(r);
                        dist = max(dist, 0.01f * SPHParams.smoothingRadius); // 最小距離を確保
                        if (dist >= SPHParams.smoothingRadius)
                        {
                            continue;
                        }


                        //float W = SPHParams.kernelScaleFactor * W_poly6(dist, SPHParams.smoothingRadius);
                        float W_density = SPHParams.pressureKernelScale * W_wendland(dist, SPHParams.smoothingRadius);
                        density += SPHParams.particleMass * W_density;
                    }
                }
            }
        }

        p.density = density;
        float densityRatio = max(density / SPHParams.restDensity, 0.001f);
        p.pressure = SPHParams.pressureMultiplier * (pow(densityRatio, SPHParams.taitExponent) - 1.0f);
        p.pressure = clamp(p.pressure, 0.0f, SPHParams.maxPressure);
        
        //======================
        // Step 2: 圧力 + 粘性力
        //======================
        float3 pressureForce = float3(0, 0, 0);
        float3 viscosityForce = float3(0, 0, 0);
        float3 xsphVel = float3(0, 0, 0);
        
        [allow_uav_condition]
        for (int zz = -1; zz <= 1; ++zz)
        {
            [allow_uav_condition]
            for (int yy = -1; yy <= 1; ++yy)
            {
                [allow_uav_condition]
                for (int xx = -1; xx <= 1; ++xx)
                {
                    int3 neighborCoordInt = int3(gridCoord) + int3(xx, yy, zz);
                    if (any(neighborCoordInt < 0)) // 負の座標は無視
                        continue;
                    uint3 neighborCoord = uint3(neighborCoordInt);
                    uint2 morton = EncodeMorton3_64_GPU(neighborCoord.x, neighborCoord.y, neighborCoord.z);
                    
                    int found = FindGridPageHeader(morton);
                    if (found < 0)
                        continue;
                    
                    SPHIndexTriplet cell = g_IndexTripletsSRV[found];
                    [allow_uav_condition]
                    for (uint i = 0; i < cell.count; ++i)
                    {
                        uint neighborID = g_SortedIndicesSRV[cell.startOffset + i];

                        if (neighborID == id || neighborID >= g_MaxParticleCount)
                            continue;

                        WaterFluidParticle q = g_ParticlesUAV[neighborID];
                        float3 r = p.position - q.position;
                        float dist = length(r);
                        dist = max(dist, 0.01f * SPHParams.smoothingRadius); // 最小距離を確保
                        if (dist >= SPHParams.smoothingRadius)
                            continue;

                        if (q.density <= 0.0f || isnan(q.density))
                            continue;
                        float invDensityQ = 1.0f / max(q.density, SPHParams.restDensity);
                            
                        if (p.density <= 0.0f || isnan(p.density))
                            continue;
                        float invDensityP = 1.0f / max(p.density, SPHParams.restDensity);
                        
                        // 圧力項
                        float3 dir = (dist > EPSILON) ? r / dist : float3(0, 0, 0);
                        float W_grad = W_wendland_grad(dist, SPHParams.smoothingRadius);
                        float W_pressure = SPHParams.pressureKernelScale * W_grad;

                        float avgPressure = (p.pressure + q.pressure) * 0.5f;
                        float avgDensity = (p.density + q.density) * 0.5f;
                        float invAvgDensity = 1.0f / max(avgDensity, EPSILON);
                        pressureForce -= dir * avgPressure * W_pressure * SPHParams.particleMass * SPHParams.particleMass * invAvgDensity;

                        // 粘性項
                        float3 velDiff = q.velocity - p.velocity;
                        float W_visc = SPHParams.viscosityKernelScale * W_grad;
                        viscosityForce += SPHParams.viscosity * velDiff * W_visc * SPHParams.particleMass * SPHParams.particleMass * invDensityQ * invDensityP;
                        float Wxshp = SPHParams.viscosityKernelScale * W_wendland(dist, SPHParams.smoothingRadius);
                        xsphVel += SPHParams.xsphFactor * velDiff * Wxshp * SPHParams.particleMass * SPHParams.particleMass * invDensityQ * invDensityP;

                    }
                }
            }
        }
        
        pressureForce *= SPHParams.pressureForceScale;
        viscosityForce *= SPHParams.viscosityForceScale;

        //======================
        // Step 3: 加速度と予測位置
        //======================
        float3 accel = (pressureForce + viscosityForce) / max(p.density, EPSILON) + g_Acceleration;
        if (length(accel) > SPHParams.maxAccel)
            accel = normalize(accel) * SPHParams.maxAccel; // 過度な加速度を制限
        
        p.velocity += accel * g_DeltaTime;
        xsphVel /= max(p.density, EPSILON); // XSPH 力の平均化
        p.velocity += xsphVel * g_DeltaTime; // XSPH 力を速度に加える
        if (length(p.velocity) > SPHParams.maxVelocity)
            p.velocity = normalize(p.velocity) * SPHParams.maxVelocity; // 過度な速度を制限
        float3 deltaPos = p.velocity * g_DeltaTime;
        p.predictedPosition = p.position + deltaPos;

        //======================
        // Step 4: 地形衝突
        //======================
        float4 worldPredicted = mul(float4(p.predictedPosition, 1.0), g_World);
        uint3 voxelCoord = GetVoxelCoord(worldPredicted.xyz);
        uint mortonIndex = EncodeMorton3(voxelCoord.x, voxelCoord.y, voxelCoord.z);
        int found = FindIndexTriplet(mortonIndex);
        if (found >= 0)
        {
            IndexTriplet trip = g_IndexTriplets[found];
            float closestDist = 1e20;
            float3 bestHit = float3(0, 0, 0);
            float3 bestNormal = float3(0, 0, 0);

            float4 rayStartWS = mul(float4(p.position, 1.0), g_World);
            float4 rayDirWS = mul(float4(deltaPos, 0.0), g_World);
            float3 rayDir = normalize(rayDirWS.xyz);
            float rayLength = length(rayDirWS.xyz);
            float3 rayStart = rayStartWS.xyz + rayDir * 0.00001f;

            [allow_uav_condition]
            for (uint i = 0; i < trip.count; ++i)
            {
                TriangleStruct tri = g_Triangles[g_VoxelTriangleIndices[trip.startOffset + i]];
                float3 hit; float dist;
                if (RayIntersectsTriangle(rayStart, rayDir, tri, hit, dist) && dist <= rayLength && dist < closestDist)
                {
                    closestDist = dist;
                    bestHit = hit;
                    bestNormal = tri.normal;
                }
            }

            if (closestDist < 1e19)
            {
                float3 normal = normalize(bestNormal);
                float3 corrected = rayStart + rayDir * closestDist + normal * 0.2f;
                p.predictedPosition = mul(float4(corrected, 1.0f), g_WorldInv).xyz;
                // 速度を法線に投影して滑りを適用
                p.velocity = ApplyCollisionResponseVelocity(p.velocity, normal, SPHParams.restitution, SPHParams.friction, 0.1f);
            }
        }

        //======================
        // Step 5: 書き戻し & Alive 登録
        //======================
        p.prevPosition = p.position;
        p.position = p.predictedPosition;
        g_ParticlesUAV[id] = p;
        g_AliveListUAV.Append(id);
    }
    else
    {
        // 死亡した場合は FreeList に登録
        g_FreeListUAV.Append(id);
    }
}

