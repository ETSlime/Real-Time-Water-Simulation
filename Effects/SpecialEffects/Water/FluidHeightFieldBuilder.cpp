//=============================================================================
//
// [FluidHeightFieldBuilder.cpp]
// Author : 
//
//=============================================================================
#include "Effects/SpecialEffects/Water/FluidHeightFieldBuilder.h"

FluidHeightFieldBuilder::FluidHeightFieldBuilder() 
{
    // 初期グリッドサイズ（X方向）を仮に32とする
    const int initialGridX = 32;
    const int initialGridZ = 32;

	m_grid.reserve(initialGridX); // 外側配列の容量を確保
	m_grid.setSize(initialGridX); // 外側配列のサイズを設定

    // 内側配列（Z方向）も各列に対して事前に確保
    for (int gx = 0; gx < initialGridX; ++gx)
    {
        m_grid[gx].reserve(initialGridZ);  // 各列に対してZ方向の容量を確保
        m_grid[gx].setSize(initialGridZ);  // 各列のサイズを設定
    }
}

FluidHeightFieldBuilder::~FluidHeightFieldBuilder()
{
    // 全グリッド列を解放（Clear()はm_activeGridXしか処理しないので使用不可）
    for (UINT gx = 0; gx < m_grid.getSize(); ++gx)
    {
        m_grid[gx].clear();      // 各列のセルを削除
        m_grid[gx].setSize(0);   // サイズを0に設定（メモリは保持される）
    }

    m_grid.clear();              // 外側配列を削除
    m_grid.setSize(0);           // サイズをリセット
}

void FluidHeightFieldBuilder::Initialize(float minX, float maxX, float minZ, float maxZ, int gridX, int gridZ)
{
	m_gridX = gridX;
	m_gridZ = gridZ;
	m_minX = minX;
	m_maxX = maxX;
	m_minZ = minZ;
	m_maxZ = maxZ;

    m_cellSizeX = (maxX - minX) / static_cast<float>(gridX);
    m_cellSizeZ = (maxZ - minZ) / static_cast<float>(gridZ);

    // 現在のフレームで使用するグリッドサイズを記録
    m_activeGridX = gridX;
    m_activeGridZ = gridZ;

    // 外側配列（X方向）を拡張または初期化
    if (static_cast<UINT>(gridX) > m_grid.getSize())
    {
        // 容量不足ならリザーブ（2倍に拡張）
        if (static_cast<UINT>(gridX) > m_grid.getCapacity())
            m_grid.reserve(gridX * 2);

        // サイズを拡張（小さくはしない）
        m_grid.setSize(gridX);

        // 新しく追加された列を初期化
        for (int gx = m_activeGridX; gx < gridX; ++gx)
        {
            m_grid[gx].reserve(gridZ * 2); // 内側配列を事前にリザーブ
            m_grid[gx].setSize(gridZ);     // 必要な行数に設定
        }
    }

    // 既存の列に対して、内側配列（Z方向）を拡張
    for (int gx = 0; gx < m_activeGridX; ++gx)
    {
        if (static_cast<UINT>(gridZ) > m_grid[gx].getSize())
        {
            if (static_cast<UINT>(gridZ) > m_grid[gx].getCapacity())
                m_grid[gx].reserve(gridZ * 2); // 容量不足なら拡張

            m_grid[gx].setSize(gridZ); // サイズ更新（縮小しない）
        }
    }

	// セルの初期化
    m_smoothBuffer.reserve(m_gridX);
    for (int x = 0; x < m_gridX; ++x)
    {
        m_smoothBuffer[x].reserve(m_gridZ);
		for (int z = 0; z < m_gridZ; ++z)
		{
			m_smoothBuffer[x][z] = NAN; // 初期化
		}
    }

}

void FluidHeightFieldBuilder::Initialize(const BOUNDING_BOX& bounds, float cellSize)
{
    const int minGridX = 8;
    const int minGridZ = 8;

    float width = bounds.GetWidth();
    float depth = bounds.GetDepth();

    int gridX = static_cast<int>(ceil(width / cellSize));
    int gridZ = static_cast<int>(ceil(depth / cellSize));

    // 最小グリッドサイズ保証（小さすぎると崩れる）
    gridX = max(gridX, minGridX);
    gridZ = max(gridZ, minGridZ);

    float adjustedWidth = gridX * cellSize;
    float adjustedDepth = gridZ * cellSize;

    float minX = bounds.minPoint.x;
    float maxX = minX + adjustedWidth;
    float minZ = bounds.minPoint.z;
    float maxZ = minZ + adjustedDepth;

    // 通常の初期化呼び出し
    Initialize(minX, maxX, minZ, maxZ, gridX, gridZ);
}

void FluidHeightFieldBuilder::InitializeBySlidingWindow(const BOUNDING_BOX& particleBounds, float cellSize, int fixedGridX, int fixedGridZ)
{
    // 粒子包囲ボックスの中心座標
    float centerX = (particleBounds.minPoint.x + particleBounds.maxPoint.x) * 0.5f;
    float centerZ = (particleBounds.minPoint.z + particleBounds.maxPoint.z) * 0.5f;

    // ウィンドウ開始点をセルサイズの倍数にスナップ（下方向に丸め）
    float minX = floor(centerX / cellSize - fixedGridX * 0.5f) * cellSize;
    float minZ = floor(centerZ / cellSize - fixedGridZ * 0.5f) * cellSize;

    float maxX = minX + fixedGridX * cellSize;
    float maxZ = minZ + fixedGridZ * cellSize;

    // 通常の初期化処理（既存の Initialize を再利用）
    Initialize(minX, maxX, minZ, maxZ, fixedGridX, fixedGridZ);
}

void FluidHeightFieldBuilder::AccumulateParticles(const SimpleArray<WaterFluidParticle>& particles)
{
	int count = particles.getSize();
    for (int i = 0; i < count; ++i)
    {
        const WaterFluidParticle& p = particles[i];

        XMFLOAT3 pos = p.position;
        float radius = p.size;

		// 粒子の位置と半径からグリッドセルを計算
        int radiusX = static_cast<int>(radius / m_cellSizeX) + 1;
        int radiusZ = static_cast<int>(radius / m_cellSizeZ) + 1;

		// 粒子の位置をグリッド座標に変換
        int centerX = static_cast<int>((pos.x - m_minX) / m_cellSizeX);
        int centerZ = static_cast<int>((pos.z - m_minZ) / m_cellSizeZ);

        for (int dz = -radiusZ; dz <= radiusZ; ++dz)
        {
            for (int dx = -radiusX; dx <= radiusX; ++dx)
            {
				// 中心セルからのオフセットを計算
                int gx = centerX + dx;
                int gz = centerZ + dz;

				if (gx < 0 || gx >= m_gridX || gz < 0 || gz >= m_gridZ)
					continue; // グリッド外はスキップ

				// 粒子の位置をグリッドセルの中心に変換
                float cellX = m_minX + gx * m_cellSizeX;
                float cellZ = m_minZ + gz * m_cellSizeZ;
				
				// 粒子の位置とセル中心の距離を計算
                float dist2 = (cellX - pos.x) * (cellX - pos.x) + (cellZ - pos.z) * (cellZ - pos.z);
                float radius2 = radius * radius;

                if (dist2 > radius2)
                    continue;

				// kernel関数を適用して重みを計算
                float weight = (1.0f - dist2 / radius2);

                m_grid[gx][gz].heightSum += pos.y * weight;
                m_grid[gx][gz].weightSum += weight;

            }
        }
    }
}

void FluidHeightFieldBuilder::Finalize(void)
{
    for (int gx = 0; gx < m_gridX; ++gx)
    {
        for (int gz = 0; gz < m_gridZ; ++gz)
        {
            Cell& cell = m_grid[gx][gz];

            if (cell.weightSum > 1e-5f)
            {
                // 通常の加重平均
                cell.finalHeight = cell.heightSum / cell.weightSum;
            }
            else
            {
                // 周囲の平均高さを使って埋める（3x3）
                float sum = 0.0f;
                float weightSum = 0.0f;

                for (int dx = -1; dx <= 1; ++dx)
                {
                    for (int dz = -1; dz <= 1; ++dz)
                    {
                        int nx = gx + dx;
                        int nz = gz + dz;

                        if (nx >= 0 && nx < m_gridX && nz >= 0 && nz < m_gridZ)
                        {
                            const Cell& neighbor = m_grid[nx][nz];
                            if (neighbor.weightSum > 1e-5f)
                            {
                                sum += neighbor.heightSum / neighbor.weightSum;
                                weightSum += 1.0f;
                            }
                        }
                    }
                }

                if (weightSum > 0.0f)
                {
                    cell.finalHeight = sum / weightSum;
                }
                else
                {
                    // 粒子のないセル → NaN を設定
                    cell.finalHeight = NAN;
                }
            }
        }
    }

	// エッジセルに対してガウシアン平滑化を適用
	ApplyGaussianSmoothingToEdges();
}

float FluidHeightFieldBuilder::GetHeight(int gx, int gz) const
{
    if (gx < 0 || gx >= m_gridX || gz < 0 || gz >= m_gridZ)
        return NAN;

    return m_grid[gx][gz].finalHeight;
}

bool FluidHeightFieldBuilder::IsEdgeCell(int gx, int gz) const
{
    const Cell& center = m_grid[gx][gz];
    if (center.weightSum <= 1e-5f)
		return false; // 中心セルが無効な場合はエッジセルではない

    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dz = -1; dz <= 1; ++dz)
        {
            if (dx == 0 && dz == 0) continue;

            int nx = gx + dx;
            int nz = gz + dz;

            if (nx >= 0 && nx < m_gridX && nz >= 0 && nz < m_gridZ)
            {
                if (m_grid[nx][nz].weightSum <= 1e-5f)
                {
					return true; // 周囲に有効な高さがないセルがある場合はエッジセル
                }
            }
        }
    }

	return false; // 周囲に有効な高さがあるセルが存在する場合はエッジセルではない
}

void FluidHeightFieldBuilder::ApplyGaussianSmoothingToEdges(void)
{
    constexpr float SMOOTHING_SIGMA = 1.0f;
    constexpr float INV_2SIGMA2 = 1.0f / (2.0f * SMOOTHING_SIGMA * SMOOTHING_SIGMA);

	// エッジセルに対してガウシアン平滑化を適用
    for (int gx = 0; gx < m_gridX; ++gx)
    {
        for (int gz = 0; gz < m_gridZ; ++gz)
        {
            if (!IsEdgeCell(gx, gz)) continue;

            float weightedSum = 0.0f;
            float weightSum = 0.0f;

            for (int dx = -2; dx <= 2; ++dx)
            {
                for (int dz = -2; dz <= 2; ++dz)
                {
                    int nx = gx + dx;
                    int nz = gz + dz;
                    if (nx < 0 || nx >= m_gridX || nz < 0 || nz >= m_gridZ) continue;

                    const Cell& neighbor = m_grid[nx][nz];
                    if (neighbor.weightSum > 1e-5f)
                    {
                        float dist2 = static_cast<float>(dx * dx + dz * dz);
                        float weight = expf(-dist2 * INV_2SIGMA2);

                        weightedSum += neighbor.finalHeight * weight;
                        weightSum += weight;
                    }
                }
            }

            if (weightSum > 1e-5f)
                m_smoothBuffer[gx][gz] = weightedSum / weightSum;
        }
    }

	// スムージング結果を元のグリッドに適用
    for (int gx = 0; gx < m_gridX; ++gx)
    {
        for (int gz = 0; gz < m_gridZ; ++gz)
        {
            if (!isnan(m_smoothBuffer[gx][gz]))
                m_grid[gx][gz].finalHeight = m_smoothBuffer[gx][gz];
        }
    }
}

void FluidHeightFieldBuilder::Clear(void)
{
    for (int gx = 0; gx < m_activeGridX; ++gx)
    {
        for (int gz = 0; gz < m_activeGridZ; ++gz)
        {
            m_grid[gx][gz] = Cell{}; // 各セルを初期化
        }
    }
}
