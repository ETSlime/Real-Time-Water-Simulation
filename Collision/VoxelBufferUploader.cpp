//=============================================================================
//
//  [VoxelBufferUploader.cpp]
// Author : 
//
//=============================================================================
#include "VoxelBufferUploader.h"
#include "Core/Async/DeferredTaskAPI.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_INDEX_TRIPLET_PAGE_SIZE           4096  // 各ページに含まれる最大トリプレット数
#define MAX_TRI_PER_VOXEL                     64    // ボクセルあたりの最大三角形数

VoxelBufferUploader::VoxelBufferUploader(void)
{
    m_triangleStructs.reserve(MAX_VOXEL_TRIANGLE_NUM);
	m_triangleIndices.reserve(MAX_VOXEL_TRIANGLE_NUM);
    m_indexTriplets.reserve(MAX_VOXEL_NUM);
    m_indexTripletPageHeaders.reserve(MAX_INDEX_TRIPLET_PAGE_SIZE);

}

void VoxelBufferUploader::GenerateVoxelTriangleMapping(void* param)
{
    if (!m_cachedGrid) return;

    m_triangleIndices.clear(); // 三角形インデックス配列をクリア
    m_triangleToIndex.clear(); // 三角形からインデックスへのマッピングをクリア
    m_triangleStructs.clear(); // 三角形構造体配列をクリア
	m_indexTriplets.clear(); // 三角形インデックスのオフセット情報をクリア
    m_currentTriangleIndex = 0;

    m_triangleStructs.reserve(MAX_VOXEL_TRIANGLE_NUM);
    for (const auto& it : m_cachedGrid->m_voxelMap)
    {
        const VoxelKey& key = it.key;
        const auto& triangles = it.value.triangles;
        assert(triangles.getSize() > 0); // 三角形がないボクセルにトリプレットを作らない

        // 現在のVoxelにおける、三角形インデックスの開始位置を記録
        UINT startIdx = m_triangleIndices.getSize();

        for (const Triangle* tri : triangles)
        {
            // すでに登録されていない三角形であれば、初めての追加なので処理する
            if (!m_triangleToIndex.contains(tri))
            {
                // 三角形の頂点をStructuredBuffer用に変換
                TriangleStructuredBuffer buffer;
                buffer.v0 = tri->v0;
                buffer.pad0 = 0.0f;
                buffer.v1 = tri->v1;
                buffer.pad1 = 0.0f;
                buffer.v2 = tri->v2;
                buffer.pad2 = 0.0f;
                buffer.normal = tri->normal;
                buffer.pad3 = 0.0f;

                // 重複を避けるため、初回のみ追加
                m_triangleStructs.push_back(buffer);

                // GPU側インデックスとして使用する番号を割り当てて記録
                m_triangleToIndex[tri] = m_currentTriangleIndex++;
            }

            // Voxelに所属する三角形のインデックスをpush（重複OK）
            m_triangleIndices.push_back(m_triangleToIndex[tri]);
        }

        // MortonIndex計算し三元組として保存
        UINT mortonIndex = static_cast<UINT>(EncodeMorton3(key.x, key.y, key.z));
        UINT totalCount = static_cast<uint32_t>(triangles.getSize());

        // MAX_TRI_PER_VOXEL を超える場合は裁剪（警告を出しておく）
        // ※ MortonIndex が一意であることが絶対条件！
        if (totalCount > MAX_TRI_PER_VOXEL)
        {
            // --- オーバーフローチェック（デバッグ用警告）
            OutputDebugStringA("[VoxelUploader] WARNING: Voxel triangle count exceeds MAX_TRI_PER_VOXEL. Triangles will be clipped.\n");
            totalCount = MAX_TRI_PER_VOXEL;
        }

        // --- トリプレットを一つだけ登録（裁剪後の数）
        m_indexTriplets.push_back({ mortonIndex, startIdx, totalCount, 0});
    }

    assert(m_indexTriplets.getSize() <= m_triangleIndices.getSize());
    assert(m_currentTriangleIndex == m_triangleStructs.getSize());



    if (m_indexTriplets.getSize() > 1)
        MergeSortTriplets(0, static_cast<int>(m_indexTriplets.getSize()) - 1);

    // ページングされたIndexTripletバッファを構築
	BuildPagedIndexTripletBuffer();
}

bool VoxelBufferUploader::IsVoxelGridReady(void* param)
{
    if (!m_cachedGrid) return false;

    return m_cachedGrid->IsReady();
}

void VoxelBufferUploader::BuildPagedIndexTripletBuffer(void)
{
    m_indexTripletPageHeaders.clear();

    const UINT totalTriplets = m_indexTriplets.getSize();
    UINT currentOffset = 0;

    while (currentOffset < totalTriplets)
    {
        UINT pageStart = currentOffset;
        UINT pageEnd = min(currentOffset + MAX_INDEX_TRIPLET_PAGE_SIZE, totalTriplets);

        IndexTripletPageHeader header;
        header.pageStartMorton = m_indexTriplets[pageStart].voxelIndex;
        header.pageEndMorton = m_indexTriplets[pageEnd - 1].voxelIndex;
        header.pageOffset = pageStart;
        header.pageCount = pageEnd - pageStart;

        m_indexTripletPageHeaders.push_back(header);

        currentOffset = pageEnd;  // 次のページへ進む
    }

    m_totalIndexTripletSize = totalTriplets;
    m_totalIndexTripletPageSize = m_indexTripletPageHeaders.getSize();
}

void VoxelBufferUploader::UploadBuffers(void)
{
	// Voxel Grid がキャッシュされていない場合は何もしない
	if (!m_cachedGrid) return;

	// 既存のバッファを解放
    SafeRelease(&m_voxelTriangleIndexBuffer);
    SafeRelease(&m_triangleStructBuffer);
    SafeRelease(&m_indexTripletBuffer);
	SafeRelease(&m_indexTripletPageBuffer);

    // --- StructuredBuffer アップロード ---
	// Voxelに属する三角形のインデックス配列
    m_voxelTriangleIndexBuffer = CreateStructuredBuffer(
        sizeof(UINT),
        m_triangleIndices.getSize(), 
        m_triangleIndices.data(), 
        &m_voxelTriangleIndexSRV);

	// Voxelに属する三角形の構造体配列
    m_triangleStructBuffer = CreateStructuredBuffer(
        sizeof(TriangleStructuredBuffer), 
        m_triangleStructs.getSize(), 
        m_triangleStructs.data(), 
        &m_triangleStructSRV);

	// Voxelインデックスの三元組（voxelIndex, startOffset, triangleCount）
    m_indexTripletBuffer = CreateStructuredBuffer(
        sizeof(IndexTriplet),
        m_totalIndexTripletSize,
        m_indexTriplets.data(),
        &m_indexTripletSRV);

	// IndexTripletのページヘッダ
    m_indexTripletPageBuffer = CreateStructuredBuffer(
        sizeof(IndexTripletPageHeader),
        m_totalIndexTripletPageSize,
        m_indexTripletPageHeaders.data(),
        &m_indexTripletPageSRV);
   
    // Voxelメタデータをアップロード
	UploadVoxelMeta(m_cachedGrid->m_origin, m_cachedGrid->m_voxelSize);


    if (m_voxelTriangleIndexBuffer && m_indexTripletBuffer && m_triangleStructBuffer && m_indexTripletPageBuffer)
	{
		m_isBuffersUploaded = true; // バッファがアップロードされたフラグを立てる
	}
}


void VoxelBufferUploader::UploadVoxelMeta(const XMFLOAT3& origin, float voxelSize)
{
    VoxelMetaCbuffer cbData;
    cbData.origin = origin;
    cbData.voxelSize = voxelSize;
    cbData.totalIndexTripletSize = m_totalIndexTripletSize;
    cbData.totalPageHeaderSize = m_totalIndexTripletPageSize;
    cbData.padding = XMFLOAT2(); // パディングを追加

    if (!m_cbVoxelMeta)
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(VoxelMetaCbuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA subData = {};
        subData.pSysMem = &cbData;

        HRESULT hr = m_device->CreateBuffer(&desc, &subData, &m_cbVoxelMeta);
        assert(SUCCEEDED(hr));
    }
    else
    {
        m_renderer.GetDeviceContext()->UpdateSubresource(m_cbVoxelMeta, 0, nullptr, &cbData, 0, 0);
    }
}


void VoxelBufferUploader::MergeSortTriplets(int left, int right)
{
    if (left >= right) return;

    int mid = (left + right) / 2;
    MergeSortTriplets(left, mid);
    MergeSortTriplets(mid + 1, right);

    SimpleArray<IndexTriplet> temp;
    temp.reserve(right - left + 1);

    int i = left, j = mid + 1;
    while (i <= mid && j <= right)
    {
        if (m_indexTriplets[i].voxelIndex < m_indexTriplets[j].voxelIndex)
            temp.push_back(m_indexTriplets[i++]);
        else
            temp.push_back(m_indexTriplets[j++]);
    }

    while (i <= mid) temp.push_back(m_indexTriplets[i++]);
    while (j <= right) temp.push_back(m_indexTriplets[j++]);

    for (UINT k = 0; k < temp.getSize(); ++k)
        m_indexTriplets[left + k] = temp[k];
}

ID3D11Buffer* VoxelBufferUploader::CreateStructuredBuffer(UINT structSize, UINT elementCount, const void* initData, ID3D11ShaderResourceView** outSRV)
{
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = structSize * elementCount;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = structSize;

    D3D11_SUBRESOURCE_DATA subData = {};
    subData.pSysMem = initData;

    ID3D11Buffer* buffer = nullptr;
    HRESULT hr = m_device->CreateBuffer(&desc, &subData, &buffer);
    assert(SUCCEEDED(hr));
    if (FAILED(hr)) return nullptr;


    // --- SRV作成 ---
    if (outSRV)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.ElementOffset = 0;
        srvDesc.Buffer.NumElements = elementCount;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;

        hr = m_device->CreateShaderResourceView(buffer, &srvDesc, outSRV);
        if (FAILED(hr))
        {
            buffer->Release();
            return nullptr;
        }
    }

    return buffer;
}

void VoxelBufferUploader::BindVoxelResources(void)
{
    if (m_voxelTriangleIndexSRV)
    {
        m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_VOXEL_TRIANGLE_INDEX, m_voxelTriangleIndexSRV);
    }

    if (m_triangleStructSRV)
    {
		m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_VOXEL_TRIANGLE_STRUCT, m_triangleStructSRV);
    }

    if (m_indexTripletSRV)
    {
        m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_VOXEL_IDX_TRIPLET, m_indexTripletSRV);
    }

    if (m_indexTripletPageSRV)
    {
        m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_VOXEL_IDX_TRIPLET_PAGE, m_indexTripletPageSRV);
    }

    if (m_cbVoxelMeta)
    {
        m_shaderResourceBinder.BindConstantBuffer(ShaderStage::CS, SLOT_CB_VOXEL_META, m_cbVoxelMeta);
    }
}

void VoxelBufferUploader::RegisterAsyncMappingBuild(void)
{
    //--- DeferredTask のオプション設定 ---
    DeferredTaskOptions options{};
    options.async = true;                         // 非同期スレッドでマッピング生成！
    options.debugName = "VoxelBufferUploader_GenerateVoxelTriangleMapping"; // デバッグ用の識別名
    options.callbackAsync = false;                // コールバックはメインスレッドで！

    //--- 完了時に呼び出す Upload 関数（GPUリソースの作成） ---
    options.onComplete = [](void* self) {
        VoxelBufferUploader* uploader = static_cast<VoxelBufferUploader*>(self);
        uploader->UploadBuffers();
        };
    options.callbackParam = this;

    //--- Deferred 登録（Bind でメンバ関数直接指定）---
    DeferredTaskBinder<VoxelBufferUploader>::Add(
        this,
        nullptr,
        &VoxelBufferUploader::GenerateVoxelTriangleMapping,  // 実行関数（非同期）
        &VoxelBufferUploader::IsVoxelGridReady,              // 条件関数（外部フラグ）
        options
    );
}

void VoxelBufferUploader::ShutDown(void)
{
    SafeRelease(&m_voxelTriangleIndexSRV);
    SafeRelease(&m_voxelTriangleIndexBuffer);

    SafeRelease(&m_indexTripletSRV);
    SafeRelease(&m_indexTripletBuffer);

    SafeRelease(&m_triangleStructBuffer);
    SafeRelease(&m_cbVoxelMeta);

    m_triangleIndices.clear();
    m_indexTriplets.clear();
    m_triangleStructs.clear();
    m_triangleToIndex.clear();

    m_currentTriangleIndex = 0;
}
