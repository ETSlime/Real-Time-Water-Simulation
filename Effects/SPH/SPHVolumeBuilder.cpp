//=============================================================================
//
// [SPHVolumeBuilder.cpp]
// Author : 
//
//=============================================================================
#include "Effects/SPH/SPHVolumeBuilder.h"

void SPHVolumeBuilder::Initialize(UINT maxParticles)
{
	m_maxParticles = maxParticles;

    m_scalarFieldMap = HashMap<VoxelCoord, float, HashVoxelKey64, EqualVoxelKey64>(
        maxParticles * ACTIVE_VOXEL_FACTOR, // 初期バケット数
        HashVoxelKey64(), // ハッシュ関数
        EqualVoxelKey64() // 等価比較関数
    );

	m_writtenVoxelKeys.reserve(static_cast<UINT>(maxParticles * ACTIVE_VOXEL_FACTOR)); // 書き込まれたボクセルキーの予約
}

void SPHVolumeBuilder::InitializeVolume(const VolumeReconstructionParams& params)
{
    m_params = params;
    m_volume.resize(m_params.gridX);
    for (int x = 0; x < m_params.gridX; ++x)
    {
        m_volume[x].resize(m_params.gridY);
        for (int y = 0; y < m_params.gridY; ++y)
        {
            m_volume[x][y].resize(m_params.gridZ);
            for (int z = 0; z < m_params.gridZ; ++z)
            {
                m_volume[x][y][z] = 0.0f; // 初期化
            }
        }
    }
}

void SPHVolumeBuilder::Clear(void)
{
	UINT size = m_writtenVoxelKeys.getSize();
	for (UINT i = 0; i < size; ++i)
	{
		m_scalarFieldMap.remove(m_writtenVoxelKeys[i]);
	}
	m_writtenVoxelKeys.clear();
}

void SPHVolumeBuilder::ClearVolume(void)
{
    for (int x = 0; x < m_params.gridX; ++x)
    {
        for (int y = 0; y < m_params.gridY; ++y)
        {
            for (int z = 0; z < m_params.gridZ; ++z)
            {
                m_volume[x][y][z] = 0.0f; // 各ボクセルをクリア
            }
        }
    }
}

VolumeReconstructionParams SPHVolumeBuilder::ComputeVolumeParamsFromParticles(const BOUNDING_BOX& aabb, 
    float particleRadius, float densityScale, float isoLevel, float cellSize)
{
    VolumeReconstructionParams params;

	// AABBを拡張（iso surfaceのためのマージン）
    BOUNDING_BOX bounds = aabb;
	bounds.Expand(cellSize * 2.5f); 

	// AABBの最小点をセルサイズの倍数に調整
    bounds.minPoint.x = floor(bounds.minPoint.x / cellSize) * cellSize;
    bounds.minPoint.y = floor(bounds.minPoint.y / cellSize) * cellSize;
    bounds.minPoint.z = floor(bounds.minPoint.z / cellSize) * cellSize;

	// グリッドサイズを計算
    XMFLOAT3 size = bounds.GetSize();
    params.gridX = static_cast<int>(ceil(size.x / cellSize));
    params.gridY = static_cast<int>(ceil(size.y / cellSize));
    params.gridZ = static_cast<int>(ceil(size.z / cellSize));

	// パラメータを設定
    params.origin = bounds.minPoint;
    params.cellSize = cellSize;
    params.radius = particleRadius;
    params.densityScale = densityScale;
    params.isoLevel = isoLevel;

    return params;
}

void SPHVolumeBuilder::AccumulateFromParticles(const SimpleArray<WaterFluidParticle>& particles)
{
    const float radius2 = m_params.radius * m_params.radius;
    const float invRadius = 1.0f / m_params.radius;

    for (const auto& p : particles)
    {
        const XMFLOAT3& pos = p.position;

        int minX = max(0, int((pos.x - m_params.radius - m_params.origin.x) / m_params.cellSize));
        int maxX = min(m_params.gridX - 1, int((pos.x + m_params.radius - m_params.origin.x) / m_params.cellSize));
        int minY = max(0, int((pos.y - m_params.radius - m_params.origin.y) / m_params.cellSize));
        int maxY = min(m_params.gridY - 1, int((pos.y + m_params.radius - m_params.origin.y) / m_params.cellSize));
        int minZ = max(0, int((pos.z - m_params.radius - m_params.origin.z) / m_params.cellSize));
        int maxZ = min(m_params.gridZ - 1, int((pos.z + m_params.radius - m_params.origin.z) / m_params.cellSize));

        for (int x = minX; x <= maxX; ++x)
            for (int y = minY; y <= maxY; ++y)
                for (int z = minZ; z <= maxZ; ++z)
                {
                    float cx = m_params.origin.x + x * m_params.cellSize;
                    float cy = m_params.origin.y + y * m_params.cellSize;
                    float cz = m_params.origin.z + z * m_params.cellSize;

                    float dx = pos.x - cx;
                    float dy = pos.y - cy;
                    float dz = pos.z - cz;

                    float dist2 = dx * dx + dy * dy + dz * dz;
                    if (dist2 > radius2) continue;

                    float q = sqrtf(dist2) * invRadius;
                    float weight = powf(1.0f - q, 4.0f) * (1.0f + 4.0f * q); // Wendland C2 カーネル

                    m_volume[x][y][z] += weight * m_params.densityScale;
                }
    }
}

void SPHVolumeBuilder::AccumulateFromParticlesSparse(const VolumeReconstructionParams& params, const SimpleArray<WaterFluidParticle>& particles)
{
	m_params = params; // パラメータを更新
    const float radius2 = m_params.radius * m_params.radius;
    const float invRadius = 1.0f / m_params.radius;

    for (const auto& p : particles)
    {
        const XMFLOAT3& pos = p.position;

        int minX = int((pos.x - m_params.radius - m_params.origin.x) / m_params.cellSize);
        int maxX = int((pos.x + m_params.radius - m_params.origin.x) / m_params.cellSize);
        int minY = int((pos.y - m_params.radius - m_params.origin.y) / m_params.cellSize);
        int maxY = int((pos.y + m_params.radius - m_params.origin.y) / m_params.cellSize);
        int minZ = int((pos.z - m_params.radius - m_params.origin.z) / m_params.cellSize);
        int maxZ = int((pos.z + m_params.radius - m_params.origin.z) / m_params.cellSize);


        //for (int x = minX - 1; x <= maxX; ++x)
        //    for (int y = minY - 1; y <= maxY; ++y)
        //        for (int z = minZ - 1; z <= maxZ; ++z)
        //        {
        //            if (x < 0 || y < 0 || z < 0) continue;
        //            VoxelCoord base = { x, y, z };
        //            RegisterVoxel(base);
        //        }

        for (int x = minX; x <= maxX; ++x)
            for (int y = minY; y <= maxY; ++y)
                for (int z = minZ; z <= maxZ; ++z)
                {
                    if (x < 0 || y < 0 || z < 0)
                    {
                        assert(false && "Negative voxel index not supported with 64-bit Morton encoding!");
                        continue;
                    }

                    float cx = m_params.origin.x + x * m_params.cellSize;
                    float cy = m_params.origin.y + y * m_params.cellSize;
                    float cz = m_params.origin.z + z * m_params.cellSize;

                    float dx = pos.x - cx;
                    float dy = pos.y - cy;
                    float dz = pos.z - cz;

                    float dist2 = dx * dx + dy * dy + dz * dz;
                    if (dist2 > radius2) continue;

                    float q = sqrtf(dist2) * invRadius;
                    float weight = powf(1.0f - q, 4.0f) * (1.0f + 4.0f * q); // Wendland C2

                    VoxelCoord base = { x, y, z };
                    m_scalarFieldMap[base] += weight * m_params.densityScale;
                    m_writtenVoxelKeys.push_back(base);

					// ボクセルを登録
                    RegisterVoxel(VoxelCoord{ base.x + 1, base.y, base.z });
                    RegisterVoxel(VoxelCoord{ base.x, base.y + 1, base.z });
                    RegisterVoxel(VoxelCoord{ base.x, base.y, base.z + 1 });
                    RegisterVoxel(VoxelCoord{ base.x + 1, base.y + 1, base.z });
                    RegisterVoxel(VoxelCoord{ base.x + 1, base.y, base.z + 1 });
                    RegisterVoxel(VoxelCoord{ base.x, base.y + 1, base.z + 1 });
                    RegisterVoxel(VoxelCoord{ base.x + 1, base.y + 1, base.z + 1 });
                }
    }
}

void SPHVolumeBuilder::RegisterVoxel(const VoxelCoord& coord)
{
    if (!m_scalarFieldMap.contains(coord))
    {
        m_scalarFieldMap[coord] = 0.0f;
        m_writtenVoxelKeys.push_back(coord);
    }
}
