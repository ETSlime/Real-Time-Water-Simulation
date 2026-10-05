//=============================================================================
//
//  [MCVoxelClusterUploader.cpp]
// Author : 
//
//=============================================================================
#include "Effects/MarchingCubes/MCVoxelClusterUploader.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
constexpr XMFLOAT3 FLOAT_MAX_VEC3 = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
constexpr XMFLOAT3 FLOAT_MIN_VEC3 = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
constexpr UINT UINT32_MAX_ARR[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };

void MCVoxelClusterUploader::Initialize(UINT maxParticles)
{
    m_bufferCapacity = static_cast<UINT>(maxParticles * ACTIVE_VOXEL_FACTOR);

    m_labelPropagationCS = ShaderManager::get_instance().GetComputeShader(ParticleComputeGroup::WaterFluid, ComputePassType::MCVoxelLabelPropagation);
    m_clusterPartitionCS = ShaderManager::get_instance().GetComputeShader(ParticleComputeGroup::WaterFluid, ComputePassType::MCVoxelClusterPartition);
    m_prefixSumLocalCS = ShaderManager::get_instance().GetComputeShader(ParticleComputeGroup::WaterFluid, ComputePassType::MCVoxelPrefixSumLocal);
	m_prefixSumGlobalCS = ShaderManager::get_instance().GetComputeShader(ParticleComputeGroup::WaterFluid, ComputePassType::MCVoxelPrefixSumGlobal);
	m_prefixSumAddOffsetCS = ShaderManager::get_instance().GetComputeShader(ParticleComputeGroup::WaterFluid, ComputePassType::MCVoxelPrefixSumAddOffset);


	// ラベル伝播用のバッファセットを作成
    for (int i = 0; i < 2; ++i)
    {
        RecreateBuffer(m_labelBufferSet[i], sizeof(UINT), m_bufferCapacity);        // ラベル配列
    }        

	// クラスタ分割用のバッファセットを作成
	RecreateBuffer(m_clusterCountBufferSet, sizeof(UINT), m_bufferCapacity);        // 各クラスタのボクセル数格納用
	RecreateBuffer(m_clusterMinBufferSet, sizeof(XMFLOAT3), m_bufferCapacity);      // クラスタ AABB 最小
    RecreateBuffer(m_clusterMaxBufferSet, sizeof(XMFLOAT3), m_bufferCapacity);      // クラスタ AABB 最大
    RecreateBuffer(m_voxelClusterIndexBufferSet, sizeof(UINT), m_bufferCapacity);   // 各 voxel のクラスタ ID
    RecreateBuffer(m_clusterRemapBufferSet, sizeof(UINT), m_bufferCapacity);        // クラスタリマップバッファ（label → compact clusterID）
    RecreateBuffer(m_validClusterCounterBufferSet, sizeof(UINT), 1);                // 有効クラスタカウンタ（構成：1要素 → InterlockedAddに使用）
	RecreateBuffer(m_isValidClusterBufferSet, sizeof(UINT), m_bufferCapacity);      // 有効クラスタフラグ（構成：1要素 → クラスタが有効かどうかのフラグ）

    UINT groupCount = GET_THREAD_GROUP_COUNT(m_bufferCapacity, THREAD_COUNT);
	RecreateBuffer(m_prefixSumBufferSet, sizeof(UINT), m_bufferCapacity);           // プレフィックスサム計算用のバッファ
    RecreateBuffer(m_localSumsBufferSet, sizeof(UINT), groupCount);                 // 各 Group の prefix sum 合計
	RecreateBuffer(m_globalOffsetsBufferSet, sizeof(UINT), groupCount);             // 各 Group の prefix sum offset   
	CreatePrefixSumCB(); // プレフィックスサム計算用のCBを作成

    m_initialLabels.reserve(m_bufferCapacity);

	InitClusterAABB(); // クラスタのAABBを初期化
}

void MCVoxelClusterUploader::BuildClusterLabels(const SimpleArray<SPHIndexTriplet>& triplets)
{
    m_numActiveVoxels = triplets.getSize();

    if (m_numActiveVoxels == 0)
        return;

	// 必要なラベルバッファの容量を確保
    EnsureLabelBufferCapacity(m_numActiveVoxels);

    // ラベルバッファに初期ラベルを書き込む
    assert(m_initialLabels.getSize() >= m_numActiveVoxels);
    m_context->UpdateSubresource(
        m_labelBufferSet[m_labelCurrentBufferIndex].buffer, 0, nullptr, 
        m_initialLabels.data(), 0, 0
    );
}

void MCVoxelClusterUploader::DispatchLabelPropagation(void)
{
    if (m_numActiveVoxels == 0)
        return;

    const int PROPAGATION_ITERATION_COUNT = 8;

    // シェーダー設定
    m_context->CSSetShader(m_labelPropagationCS.cs, nullptr, 0);

    // スレッドグループ数を計算
    UINT threadGroupCount = GET_THREAD_GROUP_COUNT(m_numActiveVoxels, THREAD_COUNT);

    for (int i = 0; i < PROPAGATION_ITERATION_COUNT; ++i)
    {
		// Ping-Pong バッファを切り替え
        int readIndex = m_labelCurrentBufferIndex;
        int writeIndex = 1 - readIndex;

        ID3D11ShaderResourceView* inputSRV = m_labelBufferSet[readIndex].srv;
        ID3D11UnorderedAccessView* outputUAV = m_labelBufferSet[writeIndex].uav;

        // SRV / UAV バインド
        m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_VOXEL_CLUSTER_LABELS, inputSRV);
        m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_VOXEL_CLUSTER_LABELS, outputUAV);

        // シェーダー実行
        m_context->Dispatch(threadGroupCount, 1, 1);

        // 次のループのためにバッファインデックスを更新
		m_labelCurrentBufferIndex = writeIndex;

        // バインド解除
        ID3D11ShaderResourceView* nullSRV = nullptr;
        ID3D11UnorderedAccessView* nullUAV = nullptr;
        m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_VOXEL_CLUSTER_LABELS, nullSRV);
        m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_VOXEL_CLUSTER_LABELS, nullUAV);
    }

    m_context->CSSetShader(nullptr, nullptr, 0);
}

void MCVoxelClusterUploader::DispatchClusterPartition(void)
{
	if (m_numActiveVoxels == 0)
		return;

    // UAVの初期化（必要ならClear）
    UINT initCounts[4] = { 0, 0, 0, 0 };
    m_context->ClearUnorderedAccessViewUint(m_clusterCountBufferSet.uav, initCounts);
    m_context->UpdateSubresource(m_clusterMinBufferSet.buffer, 0, nullptr, m_clusterMinAABBs.data(), 0, 0);
    m_context->UpdateSubresource(m_clusterMaxBufferSet.buffer, 0, nullptr, m_clusterMaxAABBs.data(), 0, 0);

	// クラスタ分割シェーダーをバインド
    ID3D11UnorderedAccessView* uavs[] = {
    m_voxelClusterIndexBufferSet.uav,
    m_clusterMinBufferSet.uav,
	m_clusterMaxBufferSet.uav,
    m_clusterCountBufferSet.uav
    };
    m_shaderResourceBinder.BindUnorderedAccessViews(SLOT_UAV_VOXEL_CLUSTER_INDEX, 4, uavs);

	// ラベルバッファのSRVをバインド
    m_shaderResourceBinder.BindShaderResource(
        ShaderStage::CS,
        SLOT_SRV_VOXEL_CLUSTER_LABELS,
        m_labelBufferSet[m_labelCurrentBufferIndex].srv
    );


	m_context->CSSetShader(m_clusterPartitionCS.cs, nullptr, 0);
	UINT threadGroupCount = GET_THREAD_GROUP_COUNT(m_numActiveVoxels, THREAD_COUNT);
	m_context->Dispatch(threadGroupCount, 1, 1);

	// UAV と SRV を解除
    ID3D11UnorderedAccessView* nullUAV[4] = {};
    m_shaderResourceBinder.BindUnorderedAccessViews(SLOT_UAV_VOXEL_CLUSTER_INDEX, 4, nullUAV);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_VOXEL_CLUSTER_LABELS, nullSRV);
	m_context->CSSetShader(nullptr, nullptr, 0);
}

void MCVoxelClusterUploader::DispatchClusterRemap(void)
{
    m_context->ClearUnorderedAccessViewUint(m_clusterRemapBufferSet.uav, UINT32_MAX_ARR);

    UINT zeros[4] = { 0 };
    m_context->UpdateSubresource(m_validClusterCounterBufferSet.buffer, 0, nullptr, zeros, 0, 0);
    m_context->ClearUnorderedAccessViewUint(m_isValidClusterBufferSet.uav, zeros);

    m_context->CSSetShader(m_remapCS.cs, nullptr, 0);
    ID3D11UnorderedAccessView* uavs[3] = {
        m_clusterRemapBufferSet.uav,
        m_validClusterCounterBufferSet.uav,
		m_isValidClusterBufferSet.uav
    };
    m_shaderResourceBinder.BindUnorderedAccessViews(SLOT_UAV_CLUSTER_REMAP, 3, uavs);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_CLUSTER_COUNT, m_clusterCountBufferSet.srv);

    UINT threadGroupCount = GET_THREAD_GROUP_COUNT(m_numActiveVoxels, THREAD_COUNT);
    m_context->Dispatch(threadGroupCount, 1, 1);

	// UAV と SRV を解除
	ID3D11UnorderedAccessView* nullUAV[3] = {};
	m_shaderResourceBinder.BindUnorderedAccessViews(SLOT_UAV_CLUSTER_REMAP, 3, nullUAV);
	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_CLUSTER_COUNT, nullSRV);
	m_context->CSSetShader(nullptr, nullptr, 0);
}

void MCVoxelClusterUploader::DispatchPrefixSumRemap(void)
{
    if (m_numActiveVoxels == 0)
        return;

    // スレッドグループ数を計算
    UINT threadGroupCount = GET_THREAD_GROUP_COUNT(m_numActiveVoxels, THREAD_COUNT);

    // 定数バッファ内容を作成
    CBPrefixSumInfo cb = {};
    cb.numGroups = threadGroupCount;
	cb.padding = XMFLOAT3(0.0f, 0.0f, 0.0f);

    // マップして書き込み
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(m_context->Map(m_cbPrefixSumInfo, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &cb, sizeof(CBPrefixSumInfo));
        m_context->Unmap(m_cbPrefixSumInfo, 0);
    }

    // シェーダーにバインド
    m_shaderResourceBinder.BindConstantBuffer(ShaderStage::CS, SLOT_CB_CLUSTER_REMAP, m_cbPrefixSumInfo);

    UINT zeros[4] = { 0 };
    m_context->UpdateSubresource(m_validClusterCounterBufferSet.buffer, 0, nullptr, zeros, 0, 0);
    m_context->ClearUnorderedAccessViewUint(m_isValidClusterBufferSet.uav, zeros);

	ID3D11UnorderedAccessView* nullUAV = nullptr;
    ID3D11ShaderResourceView* nullSRV = nullptr;

    m_context->CSSetShader(m_prefixSumLocalCS.cs, nullptr, 0);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_CLUSTER_COUNT, m_clusterCountBufferSet.srv);
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_REMAP_PREFIX_SUM, m_prefixSumBufferSet.uav);
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_REMAP_LOCAL_SUMS, m_localSumsBufferSet.uav);
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_CLUSTER_ISVALID_FLAG, m_isValidClusterBufferSet.uav);

    m_context->Dispatch(threadGroupCount, 1, 1);

    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_REMAP_PREFIX_SUM, nullUAV);
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_REMAP_LOCAL_SUMS, nullUAV);
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_CLUSTER_ISVALID_FLAG, nullUAV);


    m_context->CSSetShader(m_prefixSumGlobalCS.cs, nullptr, 0);
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_CLUSTER_VALID_COUNTER, m_validClusterCounterBufferSet.uav);
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_REMAP_GLOBAL_OFFSETS, m_globalOffsetsBufferSet.uav);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_REMAP_LOCAL_SUMS, m_localSumsBufferSet.srv);

    m_context->Dispatch(1, 1, 1);

    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_REMAP_GLOBAL_OFFSETS, nullUAV);
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_CLUSTER_VALID_COUNTER, nullUAV);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_REMAP_LOCAL_SUMS, nullSRV);



    m_context->CSSetShader(m_prefixSumAddOffsetCS.cs, nullptr, 0);
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_REMAP_PREFIX_SUM, m_prefixSumBufferSet.uav);
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_CLUSTER_ISVALID_FLAG, m_isValidClusterBufferSet.uav);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_REMAP_GLOBAL_OFFSETS, m_globalOffsetsBufferSet.srv);

    m_context->Dispatch(threadGroupCount, 1, 1);

    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_REMAP_PREFIX_SUM, nullUAV);
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_CLUSTER_ISVALID_FLAG, nullUAV);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_REMAP_GLOBAL_OFFSETS, nullSRV);


    m_context->CSSetShader(nullptr, nullptr, 0);
}

void MCVoxelClusterUploader::BindClusterBuffers(void)   
{
	// クラスタ分割用のバッファをバインド
	m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_VOXEL_CLUSTER_INDEX, m_voxelClusterIndexBufferSet.srv);
	m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_CLUSTER_AABB_MIN, m_clusterMinBufferSet.srv);
	m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_CLUSTER_AABB_MAX, m_clusterMaxBufferSet.srv);
	//m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_CLUSTER_REMAP, m_clusterRemapBufferSet.srv);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_CLUSTER_REMAP, m_prefixSumBufferSet.srv);
}

void MCVoxelClusterUploader::UnbindClusterBuffers(void)
{
	// クラスタ分割用のバッファをアンバインド
	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_VOXEL_CLUSTER_INDEX, nullSRV);
	m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_CLUSTER_AABB_MIN, nullSRV);
	m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_CLUSTER_AABB_MAX, nullSRV);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_CLUSTER_REMAP, nullSRV);
}



void MCVoxelClusterUploader::ReleaseBuffers(void)
{
    for (int i = 0; i < 2; ++i)
    {
		SafeRelease(&m_labelBufferSet[i].buffer);
		SafeRelease(&m_labelBufferSet[i].srv);
		SafeRelease(&m_labelBufferSet[i].uav);
    }

	SafeRelease(&m_clusterCountBufferSet.buffer);
	SafeRelease(&m_clusterCountBufferSet.srv);
	SafeRelease(&m_clusterCountBufferSet.uav);

	SafeRelease(&m_clusterMinBufferSet.buffer);
	SafeRelease(&m_clusterMinBufferSet.srv);
	SafeRelease(&m_clusterMinBufferSet.uav);

	SafeRelease(&m_clusterMaxBufferSet.buffer);
	SafeRelease(&m_clusterMaxBufferSet.srv);
	SafeRelease(&m_clusterMaxBufferSet.uav);

	SafeRelease(&m_voxelClusterIndexBufferSet.buffer);
	SafeRelease(&m_voxelClusterIndexBufferSet.srv);
	SafeRelease(&m_voxelClusterIndexBufferSet.uav);

	m_numActiveVoxels = 0;
    m_bufferCapacity = 0; // 再確保のために初期化
}

void MCVoxelClusterUploader::RecreateBuffer(ParticleBufferSet& bufferSet, UINT elementSize, UINT elementCount)
{
    // 古いバッファを解放
    SafeRelease(&bufferSet.buffer);
    SafeRelease(&bufferSet.srv);
    SafeRelease(&bufferSet.uav);

    // 新しいバッファを作成
    ParticleEffectRendererBase::CreateStructuredBuffer(m_device,
        elementSize,             // 要素のサイズ（構造体サイズ）
        elementCount,            // 要素数（バッファ容量）
        nullptr,                 // 初期データはなし
        &bufferSet.buffer,       // 実バッファ本体
        &bufferSet.srv,          // ShaderResourceView
        &bufferSet.uav           // RWStructuredBuffer
    );
}

void MCVoxelClusterUploader::EnsureLabelBufferCapacity(UINT requiredCount)
{
    const float RESERVE_SCALE = 1.5f; // 冗長確保スケーリング

    if (requiredCount <= m_bufferCapacity)
        return;

    // 古いバッファを解放
    ReleaseBuffers();

    // 新しいサイズでバッファを確保
    m_bufferCapacity = static_cast<UINT>(requiredCount * RESERVE_SCALE);

    // ラベルバッファ再構築
    for (int i = 0; i < 2; ++i)
    {
		RecreateBuffer(m_labelBufferSet[i], sizeof(UINT), m_bufferCapacity);
    }

	// クラスタ情報バッファも再構築
    RecreateBuffer(m_clusterCountBufferSet, sizeof(UINT), m_bufferCapacity);
    RecreateBuffer(m_clusterMinBufferSet, sizeof(XMFLOAT3), m_bufferCapacity);
    RecreateBuffer(m_clusterMaxBufferSet, sizeof(XMFLOAT3), m_bufferCapacity);
    RecreateBuffer(m_voxelClusterIndexBufferSet, sizeof(UINT), m_bufferCapacity);

 //   // 初期ラベルリストを拡張
	//m_initialLabels.reserve(m_bufferCapacity);
 //   UINT oldSize = m_initialLabels.getSize();
	//m_initialLabels.setSize(m_bufferCapacity);
	//for (int i = oldSize; i < m_bufferCapacity; ++i)
	//{
	//	m_initialLabels[i] = i; // 新しいラベルを初期化
	//}
}

void MCVoxelClusterUploader::InitClusterAABB(void)
{
    m_clusterMinAABBs.reserve(m_bufferCapacity);
	m_clusterMinAABBs.setSize(m_bufferCapacity);
	m_clusterMaxAABBs.reserve(m_bufferCapacity);
	m_clusterMaxAABBs.setSize(m_bufferCapacity);

    for (UINT i = 0; i < m_bufferCapacity; ++i)
    {
		m_clusterMinAABBs[i] = FLOAT_MAX_VEC3; // 最小値を初期化
		m_clusterMaxAABBs[i] = FLOAT_MIN_VEC3; // 最大値を初期化
    }
}

void MCVoxelClusterUploader::CreatePrefixSumCB(void)
{
    if (m_cbPrefixSumInfo) return; // 一度だけ作成する

    D3D11_BUFFER_DESC desc = {};
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.ByteWidth = sizeof(CBPrefixSumInfo);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = m_device->CreateBuffer(&desc, nullptr, &m_cbPrefixSumInfo);
    if (FAILED(hr))
    {
        OutputDebugStringA("CBPrefixSumInfo 作成失敗...\n");
    }
}

