//=============================================================================
//
//  [SPHSystem.cpp]
// Author : 
//
//=============================================================================
#include "Effects/SPH/SPHSystem.h"
#include "Core/Shader/ShaderManager.h"

bool SPHSystem::Initialize(ID3D11Device* device, 
    ID3D11DeviceContext* context,
    const SPHConfig& config,
    ParticleBufferSet particleBufferSet,
    ParticleBufferSet aliveListBufferSet)
{
	m_device = device; 
	m_context = context;

    m_particleBufferSet = particleBufferSet;
	m_aliveListBufferSet = aliveListBufferSet;

	SetConfig(config); // SPHシミュレーションの設定を適用

    // 疎らなグリッドを使用する場合はアップローダーを初期化
    if (m_config.useSparseGrid)
    {
        if (m_config.recordActiveVoxelCoords)
            m_gridUploader.SetRecordActiveVoxelCoords(true); // アクティブなボクセル座標を記録する場合は設定
        m_gridUploader.Initialize(m_config.cellSize, m_config.maxParticles, &m_config.gridMin);
    }


    // シェーダー読み込み
    if (!LoadComputeShaders())
        return false;

	// 定数バッファの作成
	if (!CreateConstantBuffer())
		return false;

	// 生存パーティクルリストの読み取り用バッファを作成
	if (!CreateReadbackBuffers())
		return false;

    // パーティクルのソート用インデックスバッファを作成
    ParticleEffectRendererBase::CreateStructuredBuffer(m_device, sizeof(UINT), m_config.maxParticles, nullptr,
        &m_sortedIndicesBufferSet.buffer, &m_sortedIndicesBufferSet.srv, &m_sortedIndicesBufferSet.uav);
    if (!m_sortedIndicesBufferSet.buffer)
        return false;

    // グリッドカウンターバッファを作成
    ParticleEffectRendererBase::CreateStructuredBuffer(m_device, sizeof(UINT), m_totalGridCells, nullptr,
		&m_gridCounterBufferSet.buffer, &m_gridCounterBufferSet.srv, &m_gridCounterBufferSet.uav);
    if (!m_gridCounterBufferSet.buffer)
        return false;

    // グリッド開始インデックスのPing-Pong バッファを2つ作成
    for (int i = 0; i < 2; ++i)
    {
        ParticleEffectRendererBase::CreateStructuredBuffer(m_device, sizeof(UINT), m_totalGridCells, nullptr,
            &m_gridStartIndexBufferSet[i].buffer, &m_gridStartIndexBufferSet[i].srv, &m_gridStartIndexBufferSet[i].uav);
        if (!m_gridStartIndexBufferSet[i].buffer)
            return false;
    }

    // 流体の厚みレンダラー初期化
    m_thicknessRenderer.Initialize(SCREEN_WIDTH, SCREEN_HEIGHT);

    return true;
}

void SPHSystem::Update(void)
{
#ifdef _DEBUG
	LoadComputeShaders(); //ホットリロード対応(デバッグビルドのみ)
#endif // DEBUG

    UINT threadGroupCount = GET_THREAD_GROUP_COUNT(m_config.maxParticles, THREAD_COUNT); // スレッドグループ数計算

    // Grid モード切替
	if (m_config.useSparseGrid) // 疎らなグリッドを使用する場合
	{
        // CPU 側で Morton Grid を構築
		m_gridUploader.BuildGrid(m_prevFrameAliveListCPU); // 前フレームの生存パーティクルリストを使用してグリッドを構築
		m_gridUploader.UploadSortedIndicesTo(m_sortedIndicesBufferSet.buffer); // ソートされたインデックスをアップロード
		m_gridUploader.UploadAndBindGridBuffers(); // グリッドページバッファをアップロード

        // SPHParams に含まれる gridMin は「BuildGrid() の実行によって決まった座標」を使う必要がある
        UpdateSPHSimParamsCB(); // SPHシミュレーションパラメータの更新
        m_shaderResourceBinder.BindConstantBuffer(ShaderStage::CS, SLOT_CB_EFFECT_SPH, m_cbSPHParams); // SPHシミュレーションパラメータの定数バッファをバインド

	}
	else                        
    {
        UpdateSPHSimParamsCB(); // SPHシミュレーションパラメータの更新
        m_shaderResourceBinder.BindConstantBuffer(ShaderStage::CS, SLOT_CB_EFFECT_SPH, m_cbSPHParams); // SPHシミュレーションパラメータの定数バッファをバインド

        // 密なグリッドを使用する場合
        SwapGridBuffers(); // グリッドバッファをスワップ
        BindResources_BuildGrid(); // グリッド構築用リソースをバインド
        m_context->CSSetShader(m_buildGridCS.cs, nullptr, 0);
        m_context->Dispatch(threadGroupCount, 1, 1); // グリッド構築のディスパッチ
    }

	BindResources_Simulate(); // シミュレーション用リソースをバインド
    m_context->CSSetShader(m_simulateCS.cs, nullptr, 0);
	m_context->Dispatch(threadGroupCount, 1, 1); // SPHシミュレーションのディスパッチ

    UpdateAliveList(); // 生存パーティクルリストの更新
}

void SPHSystem::Draw(void)
{
    // 流体の厚みを描画
    m_thicknessRenderer.Draw(m_prevFrameAliveListCPU.getSize()); // 生存パーティクル数を渡してレンダリング
}

void SPHSystem::Shutdown(void)
{
	SafeRelease(&m_cbSPHParams);
	SafeRelease(&m_sortedIndicesBufferSet.buffer);
	SafeRelease(&m_sortedIndicesBufferSet.srv);
	SafeRelease(&m_sortedIndicesBufferSet.uav);
	SafeRelease(&m_gridCounterBufferSet.buffer);
	SafeRelease(&m_gridCounterBufferSet.srv);
	SafeRelease(&m_gridCounterBufferSet.uav);
	for (int i = 0; i < 2; ++i)
	{
		SafeRelease(&m_gridStartIndexBufferSet[i].buffer);
		SafeRelease(&m_gridStartIndexBufferSet[i].srv);
		SafeRelease(&m_gridStartIndexBufferSet[i].uav);
	}

	SafeRelease(&m_aliveCountBuffer);
	SafeRelease(&m_aliveListReadbackBuffer);
	SafeRelease(&m_particleReadbackBuffer);

	m_gridUploader.Shutdown(); // グリッドアップローダーのシャットダウン
}

void SPHSystem::SetConfig(const SPHConfig& config)
{
    m_config = config;

	m_gridCellStart = 0; // グリッド開始インデックスを初期化
    m_totalGridCells = m_config.gridDim.x * m_config.gridDim.y * m_config.gridDim.z; // グリッドセルの総数を計算
}

const BOUNDING_BOX& SPHSystem::ComputeBoundsFromAliveParticles(void)
{
    if (m_prevFrameAliveListCPU.empty()) return m_bounds;

    m_bounds.minPoint = m_prevFrameAliveListCPU[0].position;
    m_bounds.maxPoint = m_prevFrameAliveListCPU[0].position;

    for (UINT i = 1; i < m_prevFrameAliveListCPU.getSize(); ++i)
    {
        const XMFLOAT3& p = m_prevFrameAliveListCPU[i].position;
        m_bounds.minPoint.x = min(m_bounds.minPoint.x, p.x);
        m_bounds.minPoint.y = min(m_bounds.minPoint.y, p.y);
        m_bounds.minPoint.z = min(m_bounds.minPoint.z, p.z);
        m_bounds.maxPoint.x = max(m_bounds.maxPoint.x, p.x);
        m_bounds.maxPoint.y = max(m_bounds.maxPoint.y, p.y);
        m_bounds.maxPoint.z = max(m_bounds.maxPoint.z, p.z);
    }

    return m_bounds;
}

bool SPHSystem::CreateConstantBuffer(void)
{
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(CBSPHSimParams);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    m_device->CreateBuffer(&cbDesc, nullptr, &m_cbSPHParams);

	if (!m_cbSPHParams)
		return false;

    return true;
}

bool SPHSystem::CreateReadbackBuffers(void)
{
    HRESULT hr = S_OK;

    // 生存パーティクルのカウント用バッファを作成
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(UINT);
	desc.Usage = D3D11_USAGE_STAGING; // 読み取り専用のステージングバッファ
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
	hr = m_device->CreateBuffer(&desc, nullptr, &m_aliveCountBuffer); 
	assert(SUCCEEDED(hr));

	// 生存パーティクルリストの読み取り用バッファを作成
    desc.ByteWidth = sizeof(UINT) * m_config.maxParticles;
	desc.Usage = D3D11_USAGE_STAGING; // 読み取り専用のステージングバッファ
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    hr = m_device->CreateBuffer(&desc, nullptr, &m_aliveListReadbackBuffer);
    assert(SUCCEEDED(hr));

	// パーティクルデータの読み取り用バッファを作成
    desc.ByteWidth = sizeof(WaterFluidParticle) * m_config.maxParticles;
    hr = m_device->CreateBuffer(&desc, nullptr, &m_particleReadbackBuffer);
    assert(SUCCEEDED(hr));

    return true;
}

void SPHSystem::UpdateSPHSimParamsCB(void)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_cbSPHParams, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CBSPHSimParams* params = reinterpret_cast<CBSPHSimParams*>(mapped.pData);

    params->gridMin = m_config.gridMin;
    params->cellSize = m_config.cellSize;
    params->gridDim = m_config.gridDim;
    params->maxParticlesPerCell = m_config.maxParticlesPerCell;
    params->gridCellStart = m_gridCellStart;

    params->maxParticleCount = m_config.maxParticles;
    params->smoothingRadius = m_config.smoothingRadius;
    params->particleMass = m_config.particleMass;
    params->restDensity = m_config.restDensity;
    params->pressureMultiplier = m_config.pressureMultiplier;
    params->viscosity = m_config.viscosity;
    params->acceleration = m_config.acceleration;
    params->deltaTime = m_timer.GetDeltaTime();
	params->totalPageHeaderCount = m_gridUploader.GetTotalGridPageHeaderSize();

	params->pressureKernelScale = m_pressureKernelScale; // 圧力カーネルスケールを設定
	params->viscosityKernelScale = m_viscosityKernelScale; // 粘性カーネルスケールを設定
	params->taitExponent = m_taitExponent; // Tait 式の指数を設定
	params->maxPressure = m_maxPressure; // 最大圧力制限を設定
	params->xsphFactor = m_xsphFactor; // XSPH 粘性補正係数を設定

	params->friction = m_config.friction; // 摩擦係数を設定
	params->restitution = m_config.restitution; // 反発係数を設定
	params->pressureForceScale = PRESSURE_FORCE_SCALE; // 圧力力のスケールを設定
	params->viscosityForceScale = VISCOSITY_FORCE_SCALE; // 粘性力のスケールを設定

	params->maxAccel = m_maxAccel; // 最大加速度を設定
	params->maxVelocity = m_maxVelocity; // 最大速度を設定
    params->padding = 0; // 16バイトアライメント用のパディング

    m_context->Unmap(m_cbSPHParams, 0);
}

bool SPHSystem::LoadComputeShaders(void)
{
    bool loadShaders = true;

    // BuildGridチェック＆取得
    loadShaders &= ShaderManager::get_instance().HasComputerShader(ParticleComputeGroup::WaterFluid, ComputePassType::SPH_BuildGrid);
    if (loadShaders)
        m_buildGridCS = ShaderManager::get_instance().GetComputeShader(ParticleComputeGroup::WaterFluid, ComputePassType::SPH_BuildGrid);

    // SimulateCSチェック＆取得
    if (m_config.useSparseGrid)
    {
        loadShaders &= ShaderManager::get_instance().HasComputerShader(ParticleComputeGroup::WaterFluid, ComputePassType::SPH_Simulate_Sparse_Grid);
        if (loadShaders)
            m_simulateCS = ShaderManager::get_instance().GetComputeShader(ParticleComputeGroup::WaterFluid, ComputePassType::SPH_Simulate_Sparse_Grid);
    }
    else
    {
        loadShaders &= ShaderManager::get_instance().HasComputerShader(ParticleComputeGroup::WaterFluid, ComputePassType::SPH_Simulate_Dense_Grid);
        if (loadShaders)
            m_simulateCS = ShaderManager::get_instance().GetComputeShader(ParticleComputeGroup::WaterFluid, ComputePassType::SPH_Simulate_Dense_Grid);
    }


	return loadShaders;
}

void SPHSystem::BindResources_BuildGrid(void)
{
    // UAVをクリア
    UINT clear[4] = { 0 };
	m_context->ClearUnorderedAccessViewUint(m_gridCounterBufferSet.uav, clear);
    UINT clearValue[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
    m_context->ClearUnorderedAccessViewUint(m_gridStartIndexBufferSet[m_currentGridIndex].uav, clearValue);

    // UAVスロット
    ID3D11UnorderedAccessView* uavs[3] = {
		m_sortedIndicesBufferSet.uav,                       // ソートされたパーティクルインデックスバッファ
        m_gridStartIndexBufferSet[m_currentGridIndex].uav,  // 現在のPing-Pongバッファ
		m_gridCounterBufferSet.uav                          // グリッドカウンターバッファ
    };
	m_shaderResourceBinder.BindUnorderedAccessViews(SLOT_UAV_SORTED_INDICES, 3, uavs);
}

void SPHSystem::BindResources_Simulate(void)
{
    // UAVスロットをクリア
    ID3D11UnorderedAccessView* nullUAVs[3] = {};
	m_shaderResourceBinder.BindUnorderedAccessViews(SLOT_UAV_SORTED_INDICES, 3, nullUAVs);

    // SRVスロット（読み取り用）
	if (m_config.useSparseGrid) // 疎らなグリッドを使用する場合
	{
		m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_SORTED_INDICES, m_sortedIndicesBufferSet.srv);
	}
    else // 密なグリッドを使用する場合
    {
        ID3D11ShaderResourceView* srvs[3] = {
            m_sortedIndicesBufferSet.srv,                       // 粒子インデックス（ソート済み）
            m_gridStartIndexBufferSet[m_currentGridIndex].srv,  // 現在のPing-Pongバッファ
            m_gridCounterBufferSet.srv                          // グリッドカウンターバッファ
        };

        m_shaderResourceBinder.BindShaderResources(ShaderStage::CS, SLOT_SRV_SORTED_INDICES, 3, srvs);
    }
}

void SPHSystem::UpdateAliveList(void)
{
    // Append構造体バッファの構造体数をカウントバッファへコピー
    m_context->CopyStructureCount(m_aliveCountBuffer, 0, m_aliveListBufferSet.uav);

    // カウントバッファをマップして粒子数を取得
    D3D11_MAPPED_SUBRESOURCE mappedCount;
    m_context->Map(m_aliveCountBuffer, 0, D3D11_MAP_READ, 0, &mappedCount);
    m_aliveListCount = *reinterpret_cast<UINT*>(mappedCount.pData);
    m_context->Unmap(m_aliveCountBuffer, 0);

    // 空の場合はリストをクリアして終了
    if (m_aliveListCount == 0) 
    {
        m_prevFrameAliveListCPU.clear();
        return;
    }

    // GPUバッファ → CPUへの読み出し

    // AliveList（インデックス）を読み出しバッファへコピー
    m_context->CopyResource(m_aliveListReadbackBuffer, m_aliveListBufferSet.buffer);

    // 粒子バッファも読み出しバッファへコピー
    m_context->CopyResource(m_particleReadbackBuffer, m_particleBufferSet.buffer);

    // CPUメモリにマッピングして取得

    // インデックス配列をマップ
    D3D11_MAPPED_SUBRESOURCE mappedAlive;
    m_context->Map(m_aliveListReadbackBuffer, 0, D3D11_MAP_READ, 0, &mappedAlive);
    UINT* aliveIndices = reinterpret_cast<UINT*>(mappedAlive.pData);

    // 粒子データをマップ
    D3D11_MAPPED_SUBRESOURCE mappedParticles;
    m_context->Map(m_particleReadbackBuffer, 0, D3D11_MAP_READ, 0, &mappedParticles);
    WaterFluidParticle* allParticles = reinterpret_cast<WaterFluidParticle*>(mappedParticles.pData);

    // 有効な粒子を収集してCPU配列に保存
    m_prevFrameAliveListCPU.clear();
    for (UINT i = 0; i < m_aliveListCount; ++i)
    {
        UINT id = aliveIndices[i];
        m_prevFrameAliveListCPU.push_back(allParticles[id]);
    }

    m_context->Unmap(m_aliveListReadbackBuffer, 0);
    m_context->Unmap(m_particleReadbackBuffer, 0);

}
