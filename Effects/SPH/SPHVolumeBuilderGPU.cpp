//=============================================================================
//
// [SPHVolumeBuilderGPU.h]
// Author : 
//
//=============================================================================
#include "Effects/SPH/SPHVolumeBuilderGPU.h"

void SPHVolumeBuilderGPU::Initialize(UINT maxParticles)
{
	m_shaderSet = ShaderManager::get_instance().GetComputeShader(ParticleComputeGroup::WaterFluid, ComputePassType::GenerateScalarField);
	m_maxParticles = maxParticles; // 最大粒子数を設定

    // 定数バッファの作成
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(CBVolumeParams);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_cbVolumeParams);
    if (FAILED(hr))
    {
        OutputDebugStringA("[FluidThicknessRenderer] Constant Buffer 作成失敗にゃ…！\n");
    }

	m_scalarFieldCapacity = m_maxParticles * 32; // 初期容量を最大粒子数の32倍に設定（適宜調整）
	// スカラー場バッファの作成
	ParticleEffectRendererBase::CreateStructuredBuffer(
		m_device,
		sizeof(UINT),							// 構造体サイズ（InterlockedAdd で使用する UINT）
		m_scalarFieldCapacity,                  // 要素数（グリッドセル数）
		nullptr,                                // 初期データはなし（全て 0）
		&m_scalarFieldBufferSet.buffer,         // バッファ
		&m_scalarFieldBufferSet.srv,            // SRV
		&m_scalarFieldBufferSet.uav,            // UAV
		false,                                  // Append UAV ではない
		false);                                 // CPU 書き込み不可（CS 専用）

	m_isInitialized = true;
}

void SPHVolumeBuilderGPU::Release(void)
{
	ReleaseScalarFieldBuffer();
	SafeRelease(&m_cbVolumeParams);
	m_isInitialized = false;
}

void SPHVolumeBuilderGPU::ReleaseScalarFieldBuffer(void)
{
	SafeRelease(&m_scalarFieldBufferSet.buffer);
	SafeRelease(&m_scalarFieldBufferSet.srv);
	SafeRelease(&m_scalarFieldBufferSet.uav);
}

void SPHVolumeBuilderGPU::BuildScalarField(ID3D11ShaderResourceView* particleBufferSRV, const VolumeReconstructionParams& params)
{
	if (!m_isInitialized || !m_shaderSet.cs)
		return;

	EnsureScalarFieldBufferCapacity(params);
	UpdateVolumeParamsCB();

	// カラー場バッファのクリア（重要！）
	UINT clearValue[4] = { 0, 0, 0, 0 }; // UINTベースのUAVに適用
	m_context->ClearUnorderedAccessViewUint(m_scalarFieldBufferSet.uav, clearValue);

	// コンピュートシェーダーのバインド
	m_context->CSSetShader(m_shaderSet.cs, nullptr, 0);
	// 定数バッファのバインド
	m_shaderResourceBinder.BindConstantBuffer(ShaderStage::CS, SLOT_CB_SPH_VOLUM_PARAMS, m_cbVolumeParams);
	// パーティクルバッファSRVのバインド
	m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_PARTICLE, particleBufferSRV);
	// スカラー場UAVのバインド
	m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_VOLUME_SCALAR_FIELD, m_scalarFieldBufferSet.uav);

	if (params.useSparseGrid)
	{
		// 稀疏構造の場合：1次元 Dispatch
		UINT threadGroupCount = GET_THREAD_GROUP_COUNT(m_maxParticles, THREAD_COUNT);
		m_context->Dispatch(threadGroupCount, 1, 1);
	}
	else
	{
		// 密なグリッドの場合：3次元 Dispatch
		// スレッドグループ数を計算
		UINT threadGroupCountX = (params.gridX + THREAD_COUNT - 1) / THREAD_COUNT;
		UINT threadGroupCountY = (params.gridY + THREAD_COUNT - 1) / THREAD_COUNT;
		UINT threadGroupCountZ = (params.gridZ + THREAD_COUNT - 1) / THREAD_COUNT;
		// コンピュートシェーダーのディスパッチ
		m_context->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
	}

	// UAV のリセット
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_VOLUME_SCALAR_FIELD, nullUAV);
}

void SPHVolumeBuilderGPU::EnsureScalarFieldBufferCapacity(const VolumeReconstructionParams& params)
{
	m_lastParams = params;

	// 稀疏構造の場合は grid サイズでなく voxelCount を使う
	bool useSparse = params.useSparseGrid;

	UINT required = useSparse
		? params.activeVoxelCount						// 稀疏グリッドの場合はアクティブなボクセル数を使用
		: (params.gridX * params.gridY * params.gridZ); // 密なグリッドの場合は全セル数を使用

	if (required <= m_scalarFieldCapacity)
		return;

	m_scalarFieldCapacity = static_cast<UINT>(required * 1.5f); // バッファの冗長確保

	// リソースを解放
	ReleaseScalarFieldBuffer();

	// 新しいスカラー場バッファを作成
    ParticleEffectRendererBase::CreateStructuredBuffer(
        m_device, 
		sizeof(UINT),							// 構造体サイズ（InterlockedAdd で使用する UINT）
		m_scalarFieldCapacity,                  // 要素数（グリッドセル数）
        nullptr,                                // 初期データはなし（全て 0）
        &m_scalarFieldBufferSet.buffer,         // バッファ
        &m_scalarFieldBufferSet.srv,            // SRV
        &m_scalarFieldBufferSet.uav,            // UAV
        false,                                  // Append UAV ではない
		false);                                 // CPU 書き込み不可（CS 専用）
}

void SPHVolumeBuilderGPU::UpdateVolumeParamsCB(void)
{
	D3D11_MAPPED_SUBRESOURCE mapped;
	m_context->Map(m_cbVolumeParams, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	CBVolumeParams* params = reinterpret_cast<CBVolumeParams*>(mapped.pData);
	params->volumeOrigin = m_lastParams.origin;
	params->cellSize = m_lastParams.cellSize;
	params->gridDim = XMUINT3(m_lastParams.gridX, m_lastParams.gridY, m_lastParams.gridZ);
	params->densityScale = m_lastParams.densityScale;
	params->isoRadius = m_lastParams.radius;
	params->invIsoRadius = 1.0f / m_lastParams.radius;
	params->padding = XMFLOAT2(0.0f, 0.0f); // パディングを追加して構造体のサイズを16バイト境界に揃える

	m_context->Unmap(m_cbVolumeParams, 0);
}


