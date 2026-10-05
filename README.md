# GPU Real-Time Water Simulation & Rendering

A real-time particle-based water simulation and rendering system implemented with **DirectX 11 / HLSL Compute Shader** as part of the **bytime Game Engine**.

The system integrates the complete pipeline from **particle emission and SPH fluid simulation**, through **sparse spatial indexing and terrain collision**, to **Marching Cubes surface reconstruction and thickness-based water shading**.

The implementation is designed for large simulation spaces and supports real-time interaction between tens of thousands of persistent water particles, terrain geometry, and dynamically reconstructed liquid surfaces.

---

## Demo & Project Materials / 作品展示

📁 **[GPU 流体模拟 作品展示 — Google Drive](https://drive.google.com/drive/folders/1ANUmz-SOVebbyzRI-PRdkLcsnMDsAirt?usp=drive_link)**

- **中文：**演示视频、项目说明 PDF 和可运行游戏。
- **English:** Demo video, project PDF, and playable game build.
- **日本語：**デモ動画、プロジェクト説明 PDF、プレイ可能なゲーム。

---

## Multi-language Documentation

Detailed technical documentation for the complete water simulation and rendering pipeline is available in three languages:

- 🇨🇳 [中文版技术文档](./README_CN.md)  
  详细介绍水粒子发射、SPH 流体模拟、Dense / Sparse Grid、Morton 编码空间索引、Voxel Grid 地形碰撞、Marching Cubes 液面重建以及 Thickness Map 水体着色等实现。

- 🇯🇵 [日本語版技術ドキュメント](./README_JP.md)  
  水パーティクルの生成、SPH 流体シミュレーション、Dense / Sparse Grid、Morton 符号による空間インデックス、Voxel Grid 地形衝突、Marching Cubes による液面再構築、Thickness Map を利用したシェーディングまでの実装を解説します。

- 🇺🇸 [English Technical Documentation](./README_EN.md)  
  A detailed breakdown of the water rendering pipeline, including particle emission, SPH simulation, Dense / Sparse Grid spatial indexing, Morton encoding, Voxel Grid terrain collision, Marching Cubes surface reconstruction, and Thickness Map-based shading.

---

## Core Features

- GPU-driven persistent water particle system
- Per-frame particle emission through configurable emitters
- SPH (Smoothed Particle Hydrodynamics) fluid simulation
- Pressure, viscosity, gravity, and XSPH velocity correction
- Wendland C2 smoothing kernel
- 3×3×3 neighborhood particle interaction
- Dense Grid and Sparse Grid spatial indexing
- 64-bit Morton encoding for large simulation spaces
- Paged sparse-grid lookup for GPU neighbor queries
- GPU `StructuredBuffer` / SRV / UAV based simulation pipeline
- Voxel Grid accelerated particle-terrain collision
- Ray-triangle intersection against terrain geometry
- Surface-normal-based particle sliding and penetration prevention
- GPU Scalar Field generation
- Dense and Sparse Scalar Field implementations
- GPU Marching Cubes liquid surface reconstruction
- Edge Vertex Cache for shared vertex generation
- Indirect mesh rendering with `DrawIndexedInstancedIndirect`
- Screen-space Thickness Map generation
- Thickness-based translucent water surface shading

---

## Simulation Pipeline

The water system is divided into three major stages:

**Particle Emission → SPH Simulation → Surface Reconstruction & Rendering**

### 1. Particle Emission

Water particles are generated through the `Emit()` interface.

Unlike short-lived visual-effect particles such as fire or smoke, water particles remain active as persistent physical simulation units and continuously participate in fluid interactions.

The particle system maintains active particles through an `AliveList` and uses GPU indirect drawing to minimize CPU-side rendering management.

### 2. SPH Fluid Simulation

Each water particle is treated as a discrete fluid element.

For every particle, the system searches its surrounding **3×3×3 spatial neighborhood** and evaluates interactions with nearby particles using the **Wendland C2 kernel**.

The simulation evaluates:

- Density
- Pressure
- Viscosity
- Gravity
- XSPH velocity correction

To avoid exhaustive particle-to-particle comparisons, particles are organized into spatial grids.

Both **Dense Grid** and **Sparse Grid** implementations are available, with the Sparse Grid serving as the primary solution for large simulation spaces.

The Sparse Grid uses **64-bit Morton encoding**, sorted particle indices, and paged index structures to provide efficient GPU-side neighborhood queries.

### 3. Terrain Collision

Static terrain triangles are organized into a separate Morton-encoded **Voxel Grid**.

Particles query nearby terrain cells and perform precise collision tests using `RayIntersectsTriangle()`.

When a collision occurs, the triangle normal is used to remove the penetrating velocity component while preserving tangential motion, allowing particles to naturally slide along terrain surfaces.

### 4. Scalar Field Generation

The simulated particles are converted into a continuous scalar field before surface reconstruction.

Two implementations are supported:

- **Dense Scalar Field** — fixed-volume grid intended for local simulations and debugging
- **Sparse Scalar Field** — Morton-encoded sparse representation designed for large simulation spaces

The Sparse Scalar Field shares the spatial indexing concepts used by the SPH system and evaluates particle influence using the Wendland C2 kernel.

### 5. Marching Cubes Surface Reconstruction

The generated scalar field is processed by a GPU implementation of **Marching Cubes**.

Each active voxel evaluates its eight scalar-field corners against the isosurface threshold, retrieves triangle topology from the Marching Cubes lookup table, and generates the corresponding surface geometry.

An **Edge Vertex Cache** is used to reuse vertices shared by neighboring cells and reduce redundant geometry generation.

Generated vertex and index buffers are rendered through `DrawIndexedInstancedIndirect`.

### 6. Thickness-based Water Shading

A screen-space **Thickness Map** is generated from the fluid particles and sampled during surface rendering.

The reconstructed Marching Cubes surface combines geometric information with the thickness data to approximate the visual depth of the liquid.

This provides:

- Translucent water boundaries
- Stronger absorption in thicker regions
- Darker appearance in deeper parts of the fluid

---

## Architecture Overview

```text
                         Water Emitter
                              │
                              ▼
                       Particle Buffer
                              │
                              ▼
                     ┌─────────────────┐
                     │   SPH System    │
                     └─────────────────┘
                       │             │
                       │             └──────────────┐
                       ▼                            ▼
               SPH Sparse Grid              Terrain Voxel Grid
                       │                            │
                       ▼                            ▼
                Neighbor Search             Triangle Collision
                       │                            │
                       └────────────┬───────────────┘
                                    ▼
                            Particle Update
                                    │
                  ┌─────────────────┴─────────────────┐
                  ▼                                   ▼
          Scalar Field Builder                Thickness Renderer
                  │                                   │
                  ▼                                   │
          Sparse / Dense Grid                         │
                  │                                   │
                  ▼                                   │
            Marching Cubes                            │
                  │                                   │
                  ▼                                   │
           Surface Mesh ──────────────────────────────┘
                  │
                  ▼
            Water Shading
                  │
                  ▼
              Final Image
