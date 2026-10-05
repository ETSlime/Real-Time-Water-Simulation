//=============================================================================
//
//  [SPHGridUploader.cpp]
// Author : 
//
//=============================================================================
#include "Effects/SPH/SPHGridUploader.h"

void SPHGridUploader::Initialize(float cellSize, UINT maxParticles, XMFLOAT3* gridMin)
{
	Clear();

	m_cellSize = cellSize;
    m_gridMin = gridMin;
	m_indexTripletCapacity = static_cast<UINT>(maxParticles * ACTIVE_VOXEL_FACTOR); // 粒子数の27倍程度を見込む
    m_indexTripletPageHeaders.reserve(MAX_SPH_GRID_PAGE_SIZE);
	m_sphIndexTriplets.reserve(m_indexTripletCapacity);
	m_gridSortedIndices.reserve(maxParticles);
	m_labelMortonKeysToRemove.reserve(m_indexTripletCapacity); // ラベル削除用のモートンキー

    // グリッドページヘッダ用バッファセット作成
    ParticleEffectRendererBase::CreateStructuredBuffer(m_device, 
        sizeof(IndexTripletPageHeaderVec2),
        MAX_SPH_GRID_PAGE_SIZE, 
        nullptr,
		&m_gridPageBufferSet.buffer, 
        &m_gridPageBufferSet.srv, 
        nullptr, false, true);

    // IndexTriplet用バッファセット作成
    ParticleEffectRendererBase::CreateStructuredBuffer(m_device, 
        sizeof(SPHIndexTriplet),
        m_indexTripletCapacity, 
        nullptr,
        &m_indexTripletBufferSet.buffer, 
        &m_indexTripletBufferSet.srv, 
        nullptr, false, true);
}

void SPHGridUploader::Shutdown(void)
{
	SafeRelease(&m_gridPageBufferSet.buffer);
	SafeRelease(&m_gridPageBufferSet.srv);
	SafeRelease(&m_gridPageBufferSet.uav);
	SafeRelease(&m_indexTripletBufferSet.buffer);
	SafeRelease(&m_indexTripletBufferSet.srv);
	SafeRelease(&m_indexTripletBufferSet.uav);

	// すべてのリソースを解放
	Clear();
}

void SPHGridUploader::Clear(void)
{
    m_indexTripletPageHeaders.clear();
    m_sphIndexTriplets.clear();
    m_gridSortedIndices.clear();
    m_mortonMap.clear();

    // 既に削除予定のモートンキーがあれば、ラベルを削除
    UINT size = m_labelMortonKeysToRemove.getSize();
    for (UINT j = 0; j < size; ++j)
    {
        uint64_t key = m_labelMortonKeysToRemove[j];
        auto it = m_voxelLabelMap.find(key);
        if (it != m_voxelLabelMap.end())
        {
            m_voxelLabelMap.erase(it); // ラベルを削除
        }
    }
    m_labelMortonKeysToRemove.clear(); // 削除予定のモートンキーをクリア
}

void SPHGridUploader::BuildGrid(const SimpleArray<WaterFluidParticle>& particles)
{
    Clear();

    UINT numParticles = particles.getSize();
	if (numParticles == 0) return; // パーティクルがいない場合は何もしない

	// グリッドの最小座標をパーティクルの位置から計算
    XMFLOAT3 newGridMin = *m_gridMin;

    XMFLOAT3 rawMin = { FLT_MAX, FLT_MAX, FLT_MAX };
    //XMFLOAT3 rawMin = { -2222, -2222, -2222};
    for (UINT i = 0; i < numParticles; ++i)
    {
        const auto& p = particles[i];

        rawMin.x = min(rawMin.x, p.position.x);
        rawMin.y = min(rawMin.y, p.position.y);
        rawMin.z = min(rawMin.z, p.position.z);
    }

    // グリッドの最小座標をセルサイズで切り捨て
    const float padding = m_cellSize;
    rawMin.x -= padding;
    rawMin.y -= padding;
    rawMin.z -= padding;

    // セルの境界に切り捨て
    rawMin.x = floorf(rawMin.x / m_cellSize) * m_cellSize;
    rawMin.y = floorf(rawMin.y / m_cellSize) * m_cellSize;
    rawMin.z = floorf(rawMin.z / m_cellSize) * m_cellSize;

    // グリッドの最小座標を更新
    *m_gridMin = rawMin;

    UINT currentLabel = 0; // ラベルの初期化
    // すべての粒子の近傍セルに登録
    for (UINT i = 0; i < numParticles; ++i)
    {
        const auto& p = particles[i];
		UINT particleID = p.particleID; // 粒子のID

        // 自身のグリッド座標を取得
        XMFLOAT3 rel = {
            p.position.x - (*m_gridMin).x,
            p.position.y - (*m_gridMin).y,
            p.position.z - (*m_gridMin).z
        };

        // モートンキーを使って粒子をグリッドに登録
        XMUINT3 gridCoord;
        gridCoord.x = static_cast<UINT>(floorf(rel.x / m_cellSize));
        gridCoord.y = static_cast<UINT>(floorf(rel.y / m_cellSize));
        gridCoord.z = static_cast<UINT>(floorf(rel.z / m_cellSize));
        uint64_t morton = EncodeMorton3_64(gridCoord.x, gridCoord.y, gridCoord.z);
		auto& entry = m_mortonMap[morton];
		entry.gridCoord = gridCoord; // グリッド座標を保存
		entry.particleIDs.push_back(particleID); // モートンキーをキーにして粒子IDを登録

        // アクティブなボクセル座標を記録する場合
		if (m_recordActiveVoxelCoords)
		{
            // 共通ラベルを生成（この粒子のクラスタ）
            UINT clusterLabel = currentLabel++;

            // 本体ボクセルにも初期ラベルを付ける
            if (m_voxelLabelMap.find(morton) == m_voxelLabelMap.end())
            {
                m_voxelLabelMap[morton] = currentLabel;
				m_labelMortonKeysToRemove.push_back(morton); // ラベル削除用にモートンキーを保存
            }

            // 3x3x3近傍探索セルを登録
            for (int dz = -1; dz <= 1; ++dz)
            {
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        int nx = gridCoord.x + dx;
                        int ny = gridCoord.y + dy;
                        int nz = gridCoord.z + dz;

                        if (nx < 0 || ny < 0 || nz < 0)
                            continue; // 安全チェック！

                        // 自分のセルはスキップ（既に登録済み）
                        if (dx == 0 && dy == 0 && dz == 0)
                            continue;

                        uint64_t neighborMorton = EncodeMorton3_64(static_cast<UINT>(nx), static_cast<UINT>(ny), static_cast<UINT>(nz));
                        // 重複チェック（同一 Morton に同一ID 入れるのは1回だけ）
                        auto& neighborEntry = m_mortonMap[neighborMorton]; // 近傍セルのエントリを取得
						neighborEntry.gridCoord = { static_cast<UINT>(nx), static_cast<UINT>(ny), static_cast<UINT>(nz) }; // グリッド座標を保存
                        auto& neighborParticles = neighborEntry.particleIDs;

                        // 重複しなければ粒子を追加
                        if (neighborParticles.find_index(particleID) == -1)
                        {
                            // もしまだ登録されていなければ追加
                            neighborParticles.push_back(particleID);
                        }

                        // ラベルの登録（まだ登録していない場合のみ）
                        if (m_voxelLabelMap.find(neighborMorton) == m_voxelLabelMap.end())
                        {
                            m_voxelLabelMap[neighborMorton] = clusterLabel;
							m_labelMortonKeysToRemove.push_back(neighborMorton); // ラベル削除用にモートンキーを保存
                        }
                    }
                }
            }
		}
    }

	// 一時的な Triplet リストを作成
    SimpleArray<TempTriplet> tempTriplet;
    tempTriplet.reserve(static_cast<UINT>(m_mortonMap.getSize()));

    // ハッシュ表を一度だけ走査
    for (const auto& it : m_mortonMap)
    {
        // uint64_t Morton Index を XMUINT2 に変換する
        uint64_t morton64 = it.key;
        XMUINT2 mortonVec;
        mortonVec.x = static_cast<UINT>(morton64 & 0xFFFFFFFF);         // 下位32bit
        mortonVec.y = static_cast<UINT>((morton64 >> 32) & 0xFFFFFFFF); // 上位32bit

		// 一時的な Triplet を作成
        TempTriplet t;
		t.gridCoord = it.value.gridCoord; // グリッド座標
        t.morton = mortonVec;
        t.count = it.value.particleIDs.getSize();
        t.indices = &it.value.particleIDs;
        tempTriplet.push_back(t);
    }

    //  Morton 昇順にソート
    if (tempTriplet.getSize() > 1)
        MergeSortTriplets(tempTriplet, 0, static_cast<int>(tempTriplet.getSize()) - 1);

    //  ソート済みリストから Triplet & 連続ID を構築
    UINT currentStart = 0;
    for (const TempTriplet& t : tempTriplet)
    {
        // Triplet を生成（startOffset は並べ替え後に確定）
        SPHIndexTriplet triplet;
		triplet.gridCoord = t.gridCoord; // グリッド座標
        triplet.mortonIndex.x = t.morton.x;
        triplet.mortonIndex.y = t.morton.y;
        triplet.startOffset = currentStart; // GPU が読み出す開始位置
        triplet.count = t.count;
        m_sphIndexTriplets.push_back(triplet);

        // ソートされたインデックスを作成
        for (UINT j = 0; j < triplet.count; ++j)
        {
			UINT index = (*t.indices)[j]; // 元リストの粒子ID
            m_gridSortedIndices.push_back(index);
        }

        currentStart += t.count; // オフセットを前進
    }

	BuildPagedIndexTripletBuffer(); // ページヘッダを構築
}

void SPHGridUploader::UploadAndBindGridBuffers(void)
{
    // GridPageHeaders をアップロード
    if (!m_indexTripletPageHeaders.empty())
    {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        HRESULT hr = m_context->Map(m_gridPageBufferSet.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr))
        {
            memcpy(mapped.pData, m_indexTripletPageHeaders.data(), m_indexTripletPageHeaders.getSize() * sizeof(IndexTripletPageHeaderVec2));
            m_context->Unmap(m_gridPageBufferSet.buffer, 0);

            m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_SPH_GRID_PAGE_HEADER, m_gridPageBufferSet.srv);
        }
    }

    // IndexTriplet をアップロード
    UINT tripletCount = m_sphIndexTriplets.getSize();
    if (tripletCount == 0)
        return;

    // バッファサイズが不足していれば再生成
    if (tripletCount > m_indexTripletCapacity)
    {
        // 安全マージン付き再確保（+25%）
        m_indexTripletCapacity = static_cast<UINT>(tripletCount * 1.25f);

        ParticleEffectRendererBase::CreateStructuredBuffer(
            m_device,
            sizeof(SPHIndexTriplet),
            m_indexTripletCapacity,
            nullptr,
            &m_indexTripletBufferSet.buffer,
            &m_indexTripletBufferSet.srv,
            nullptr);
    }

    // マップしてデータを書き込み
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = m_context->Map(m_indexTripletBufferSet.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr))
    {
        memcpy(mapped.pData, m_sphIndexTriplets.data(), tripletCount * sizeof(SPHIndexTriplet));
        m_context->Unmap(m_indexTripletBufferSet.buffer, 0);

        // CSステージにSRVバインド（インデックストリプレット）
        m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_SPH_INDEX_TRIPLETS, m_indexTripletBufferSet.srv);
    }
}

void SPHGridUploader::UploadSortedIndicesTo(ID3D11Buffer* dstBuffer) const
{
    if (m_gridSortedIndices.empty()) return;

    m_context->UpdateSubresource(dstBuffer, 0, nullptr, m_gridSortedIndices.data(), 0, 0);
}

void SPHGridUploader::ExportInitialClusterLabels(SimpleArray<UINT>& outLabels) const
{
    outLabels.clear();

    for (const SPHIndexTriplet& triplet : m_sphIndexTriplets)
    {
        uint64_t morton =
            static_cast<uint64_t>(triplet.mortonIndex.y) << 32 |
            static_cast<uint64_t>(triplet.mortonIndex.x);

        auto it = m_voxelLabelMap.find(morton);
        if (it != m_voxelLabelMap.end())
        {
            outLabels.push_back(it->value); // Label を取得
        }
        else
        {
            assert(false && "Active voxel has no initial label!");
        }
    }

    //for (const auto& pair : m_voxelLabelMap)
    //{
    //    outLabels.push_back(pair.value);
    //}
}

void SPHGridUploader::BuildPagedIndexTripletBuffer(void)
{
    const UINT totalTripletCount = m_sphIndexTriplets.getSize();
    if (totalTripletCount == 0)
        return;

	// ソート済みの m_sphIndexTriplets をページ単位で分割
    for (UINT i = 0; i < totalTripletCount; i += MAX_SPH_GRID_PAGE_SIZE)
    {
        IndexTripletPageHeaderVec2 page;
        UINT tripletCount = min(MAX_SPH_GRID_PAGE_SIZE, totalTripletCount - i);

        const SPHIndexTriplet& first = m_sphIndexTriplets[i];
        const SPHIndexTriplet& last = m_sphIndexTriplets[i + tripletCount - 1];

        page.pageStartMorton = first.mortonIndex;
		page.pageEndMorton = last.mortonIndex;

        page.pageOffset = i;
        page.pageCount = tripletCount;

        m_indexTripletPageHeaders.push_back(page);
    }
}

void SPHGridUploader::MergeSortTriplets(SimpleArray<TempTriplet>& triplets, int left, int right)
{
    if (left >= right) return;

    int mid = (left + right) / 2;
    MergeSortTriplets(triplets, left, mid);
    MergeSortTriplets(triplets, mid + 1, right);

    SimpleArray<TempTriplet> temp;
    temp.reserve(right - left + 1);

    int i = left, j = mid + 1;
    while (i <= mid && j <= right)
    {
        if (CompareMorton64(triplets[i].morton, triplets[j].morton)) // 高ビット優先の比較
            temp.push_back(triplets[i++]);
        else
            temp.push_back(triplets[j++]);
    }

    while (i <= mid) temp.push_back(triplets[i++]);
    while (j <= right) temp.push_back(triplets[j++]);

    for (UINT k = 0; k < temp.getSize(); ++k)
        triplets[left + k] = temp[k];
}

uint64_t Part1By2_64(uint32_t n)
{
    uint64_t x = n & 0x1FFFFF; // 制限20ビット（0x1FFFFF）

    x = (x | (x << 32)) & 0x1F00000000FFFF;
    x = (x | (x << 16)) & 0x1F0000FF0000FF;
    x = (x | (x << 8)) & 0x100F00F00F00F00F;
    x = (x | (x << 4)) & 0x10C30C30C30C30C3;
    x = (x | (x << 2)) & 0x1249249249249249;

    return x;
}

uint64_t EncodeMorton3_64(uint32_t x, uint32_t y, uint32_t z)
{
    return (Part1By2_64(z) << 2) | (Part1By2_64(y) << 1) | Part1By2_64(x);
}