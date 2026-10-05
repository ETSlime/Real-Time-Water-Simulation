## Water Particle Rendering

The water simulation system is implemented in a particle-driven manner, where all water surface visuals are constructed by aggregating fluid particles. It supports realistic behaviors such as flow, collision, and accumulation. Unlike transient effects like fire or smoke, water particles have a continuous lifespan and support large-scale simulation with full scene interaction.

### Water Particle Emission Logic (Emitter)
- Water particles are emitted per frame via the `Emit()` interface, controlled by the emitter.
- Particles are **persistent physical entities** that do not disappear quickly.
- Emission areas support shapes such as Cone and Box, assigning initial position and velocity on creation.
- `AliveList` is used to manage active particles, rendered efficiently via `DrawIndirect` on the GPU.

### SPH Fluid System
The simulation uses SPH (Smoothed Particle Hydrodynamics). Each particle acts as a fluid unit and moves, accumulates, and collides under forces like gravity, viscosity, and pressure.

Two types of spatial indexing structures are supported:

#### Dense Grid
- Uniform voxel grid
- Used in early testing or for small particle counts
- Accessed via `FlattenIndex(x, y, z)`

#### Sparse Grid
- Optimized for large-scale simulation with sparsely distributed particles
- Particles are spatially hashed and sorted using Morton encoding
- Entirely GPU-based particle lookup and interaction

#### SPH Sparse Grid Construction Flow

In each frame, the system constructs a sparse voxel grid based on the alive particles from the previous frame to accelerate neighbor queries:

1. The CPU retrieves alive particles from the previous frame via ReadBack.
2. For each particle, its voxel coordinate is computed and `gridMin` is updated to avoid negative coordinates.
3. Morton code is computed for each voxel, and particle IDs are accumulated into the `mortonMap`.
4. `mortonMap` entries are converted into `TempTriplet` structures (holding Morton code and particle ID), and sorted.
5. From the sorted list, `SPHIndexTriplet` is constructed to group particles sharing the same Morton code.
6. `PagedIndexTripletBuffer` is then generated from `SPHIndexTriplet` and uploaded to the GPU.

> From the next frame onward, the GPU performs efficient neighbor queries and SPH force calculations using this structure:

7. Each particle uses its `prevPosition` to determine its previous voxel and nearby Morton codes.
8. Neighboring pages are queried using `PagedIndexTripletBuffer`.
9. Using `g_SortedIndicesSRV`, neighbor indices are obtained and used with `g_ParticlesUAV` to compute SPH interactions like density, pressure, and viscosity.

#### SPH Force Formulas (Pressure / Viscosity)

Every frame, the GPU performs a 3×3×3 neighbor search and evaluates SPH forces:

- **Wendland C2 kernel** is used to calculate interaction weights between particles
- Density is estimated by accumulating mass from neighboring particles using the kernel
- Pressure force is derived from density differences and the kernel gradient (`∇W`)
- Viscosity force is smoothed using velocity differences and `∇W`
- Final forces:
  - `pressureForce += -mass * (p_i + p_j) / 2ρ_j * ∇W`
  - `viscosityForce += viscosity * velDiff * mass / ρ_j * ∇W`
- **XSPH correction** is added to unify particle velocities:
  - `velocity += ε * Σ (neighbor.velocity - self.velocity) * W`

All computations are handled by Compute Shader, iterating nearby Morton-coded voxel pages.

#### Voxel Grid + Triangle Collision Detection

Fluid particles interact with static terrain using Voxel Grid accelerated triangle collision detection:

- Each frame builds or reuses a `VoxelGrid` (sparse Morton-encoded triangle map)
- Each particle emits a ray from its `predictedPosition`
- `RayIntersectsTriangle()` is used to test against triangles in nearby voxels
- On collision:
  - **Penetration prevention**: reflect or slide the particle along the normal
  - **Surface sliding**: retain tangential velocity, remove vertical component
- Enables natural downhill flow, edge accumulation, container flowback, etc.

Executed in parallel with SPH force evaluation for coherent simulation.

### Surface Reconstruction with Marching Cubes

Since water particles are discrete, Marching Cubes is used to reconstruct a continuous surface mesh for rendering.

#### Scalar Field Generation

Two methods supported for building scalar fields:

##### Dense Grid
- Fixed-size 3D grid allocation
- Cleared and rebuilt each frame
- Indexed using `FlattenIndex(x, y, z)`
- Pros: Simple structure, easy to debug
- Cons: High memory usage, not suitable for large areas

##### Sparse Grid
- Builds only active voxels near particles
- Each particle visits affected voxels within influence radius
- Uses **64-bit Morton encoding** to support large spatial range
- Uses **Voxel-Centric (pull-based)** construction:
  - Each voxel queries nearby particles to accumulate weight
  - Weight is distance-attenuated using **Wendland C2**
- Result is uploaded as `StructuredBuffer<float>`, supporting concurrent GPU writes via `InterlockedAdd`

Follows the same paging system and Morton infrastructure as `SPHSystem`.

#### Marching Cubes Surface Mesh Generation

Once scalar fields are built, Marching Cubes is run on the GPU:

1. For each voxel, check if the scalar values of its 8 corners cross the isosurface threshold
2. Use `g_TriTableSRV` to get triangle topology
3. Calculate vertex positions and normals along edges, write to `VertexBuffer` and `IndexBuffer` via `InterlockedAdd`
4. Render using `DrawIndexedInstancedIndirect` with optional cluster parallelism

An **Edge Vertex Cache** is used to avoid duplicate vertex creation.

#### Thickness Map-based Surface Shading

To enhance volume and realism, surface shading uses a screen-space **thickness map**:

- Fluid particles write depth to a thickness texture
- Surface mesh samples the map during shading to enhance darkening and absorption
- Enables:
  - Soft translucent edges
  - Darker color in deeper areas
  - Support for SoftParticles, edge blur, foam transition

Managed by the `FluidThicknessRenderer` module.