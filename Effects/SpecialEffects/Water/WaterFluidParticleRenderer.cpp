//=============================================================================
//
//  [WaterFluidParticleRenderer.h]
// Author : 
//
//=============================================================================
#include "Effects/SpecialEffects/Water/WaterFluidParticleRenderer.h"
#include "Effects/EffectSystem.h"
#include "Effects/ParticleStructs.h"
#include "Utility/Debug/Debugproc.h"

bool WaterFluidParticleRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
#ifdef _DEBUG
	//DebugProc::get_instance().Register(this);
#endif // _DEBUG

    // ブレンドモードをアルファブレンドに設定
    m_blendMode = BLEND_MODE_ALPHABLEND;

    // 基底クラスの初期化
    if (!ParticleEffectRendererBase::Initialize(device, context))
        return false;

    // シェーダー読み込み
    if (!LoadShaders())
        return false;

    // WaterFluid 専用のCBを作成
	if (!CreateWaterFluidCB())
        return false;

	// EmitCBの作成
	if (!CreateEmitCB())
		return false;

	// SPHシステムの初期化
	SPHConfig sphConfig;
	sphConfig.useSparseGrid = true; // 疎なグリッドを使用
	sphConfig.recordActiveVoxelCoords = false; // アクティブなボクセル座標を記録
	sphConfig.maxParticles = m_maxParticles;
	sphConfig.gridMin = XMFLOAT3(0.0f, 0.0f, 0.0f);
	sphConfig.cellSize = m_supportRadius * 0.5f; // SPHの平滑半径をセルサイズに設定
	sphConfig.gridDim = XMUINT3(256, 256, 256); // グリッド分割数
	sphConfig.maxParticlesPerCell = MAX_PARTICLES_PER_CELL; // セルあたりの最大パーティクル数
	sphConfig.smoothingRadius = sphConfig.cellSize * 1.2f; // 平滑化半径をセルサイズの1.2倍に設定
	sphConfig.particleMass = m_particleMass; // 粒子質量
	sphConfig.restDensity = m_restDensity; // 静止時の密度
	sphConfig.pressureMultiplier = m_stiffness; // 圧力係数
	sphConfig.viscosity = m_viscosity; // 粘性係数
	sphConfig.acceleration = m_acceleration; // 外力（重力など）
	sphConfig.friction = m_friction; // 摩擦係数
	sphConfig.restitution = m_restitution; // 反発係数（デフォルト値）
	m_sphSystem.Initialize(device, context, sphConfig, m_particleBufferSet, m_aliveListBufferSet);

	//m_volumeMeshGPU.Initialize(m_maxParticles); // ボリュームメッシュの初期化
	//m_volumeBuilderGPU.Initialize(m_maxParticles); // SPHボリュームビルダーの初期化

    m_volumeMesh.Initialize(m_maxParticles); // ボリュームメッシュの初期化
    m_volumeBuilder.Initialize(m_maxParticles); // ボリュームビルダーを初期化
    return true;
}

void WaterFluidParticleRenderer::Shutdown(void)
{
#ifdef _DEBUG
    DebugProc::get_instance().Unregister(this);
#endif // _DEBUG

    ParticleEffectRendererBase::Shutdown();
}

void WaterFluidParticleRenderer::ConfigureEffect(const ParticleEffectParams& params)
{
    // 基底クラスの共通設定（色、寿命、スケールなど）
    ParticleEffectRendererBase::ConfigureEffect(params);

    // WaterFluidParams型への安全なダウンキャスト
    const WaterFluidParams* wfParams = dynamic_cast<const WaterFluidParams*>(&params);
    if (!wfParams) return;

    // パラメータのコピー
    m_supportRadius = wfParams->supportRadius;
    m_restDensity = wfParams->restDensity;
    m_particleMass = wfParams->particleMass;
    m_stiffness = wfParams->stiffness;
    m_friction = wfParams->friction;
	m_viscosity = wfParams->viscosity;
	m_restitution = wfParams->restitution;
}

void WaterFluidParticleRenderer::EmitWater(const XMFLOAT3& dir, float speed, float radius, int emitCount, const XMFLOAT3& center)
{
    m_particlesToEmitThisFrame = emitCount;

    // EmitCB 設定
    m_emitCBData.emitCount = emitCount;
    m_emitCBData.emitCenter = center;
    m_emitCBData.emitRadius = radius;
	m_emitCBData.emitDirection = dir;
	m_emitCBData.emitSpeed = speed;

    // Emit専用定数バッファのアップロード
    UploadEmitCB();
}

void WaterFluidParticleRenderer::DispatchComputeShader(void)
{
    // PBF専用のCBをアップロード（例：deltaTime、supportRadiusなど）
    UploadWaterFluidCB();

	// UpdateParticlesCSのリソースをバインド
    m_voxelBufferUploader.BindVoxelResources();

	// 定数バッファをバインド
    m_shaderResourceBinder.BindConstantBuffer(ShaderStage::CS, SLOT_CB_EFFECT_DRAW, m_cbParticleDraw);

    //// PBF粒子更新用のComputeShaderをバインド
    //m_context->CSSetShader(m_updateParticlesCS.cs, nullptr, 0);

    //// スレッドグループ数を計算（1グループ = 64粒子 と仮定）
    //UINT threadGroupCount = GET_THREAD_GROUP_COUNT(m_maxParticles, THREAD_COUNT);
    //m_context->Dispatch(threadGroupCount, 1, 1);

	// SPHシステムの更新
	m_sphSystem.Update();

    // Emit処理（新しい水の注入）を基底クラスに任せる
    DispatchEmitParticlesCS();

	// SPHシステムから流体表面メッシュを更新
	//UpdateSurfaceMeshFromSPH();

	// SPHシステムからボリュームメッシュを更新
	//UpdateVolumMeshFromSPH();

    ID3D11UnorderedAccessView* nullUAV = nullptr;
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_ALIVE_LIST, nullUAV);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_ALIVE_LIST, m_aliveListBufferSet.srv);
    m_renderer.BindViewBuffer(ShaderStage::CS);
    // SPHシステムの描画
    m_sphSystem.Draw();
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_ALIVE_LIST, nullSRV);
}

void WaterFluidParticleRenderer::DrawParticles(void)
{
    // SPHシステムからボリュームメッシュを更新
    UpdateVolumMeshFromSPH();

    // デバッグ用の水粒子テクスチャをピクセルシェーダーにバインド
    // 例：ぼやけた半透明な青い円形テクスチャ
    m_shaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_DIFFUSE, m_waterDotTextureSRV);

    // 必要なら、定数バッファ（例：可視化設定）をバインド
    m_shaderResourceBinder.BindConstantBuffer(ShaderStage::GS, SLOT_CB_EFFECT_PARTICLE, m_cbWaterFluid);

    // DrawIndirect によって粒子を一括描画（PointSprite or Billboard）
    //DrawIndirect();

	// 水粒子のメッシュを描画
    //m_volumeMeshGPU.SetThicknessSRV(m_sphSystem.GetThicknessSRV());
    //m_volumeMeshGPU.Draw();

    m_volumeMesh.SetThicknessSRV(m_sphSystem.GetThicknessSRV());
    m_volumeMesh.Draw();
}

void WaterFluidParticleRenderer::DispatchEmitParticlesCS(void)
{
    // 発射数がゼロならスキップ（EmitWater呼び出されていない）
    if (m_emitCBData.emitCount == 0 || !m_emitParticlesCS.cs)
        return;

    // Emit用CSの準備
    PrepareForEmitCS();

    // Emit用CSのバインド
    m_context->CSSetShader(m_emitParticlesCS.cs, nullptr, 0);

    // スレッドグループ数を計算（Emitは1スレッド = 1粒子）
    UINT threadGroupCount = GET_THREAD_GROUP_COUNT(m_emitCBData.emitCount, THREAD_COUNT); // スレッドグループ数計算
    m_context->Dispatch(threadGroupCount, 1, 1);

    // 使用済みなのでゼロリセット（次フレームまた呼び出される）
    m_emitCBData.emitCount = 0;
}

bool WaterFluidParticleRenderer::CreateWaterFluidCB(void)
{
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(WaterFluidCB);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags = 0;

    HRESULT hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_cbWaterFluid);
    if (FAILED(hr))
        return false;

    return true;
}

void WaterFluidParticleRenderer::UploadWaterFluidCB(void)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(m_context->Map(m_cbWaterFluid, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        WaterFluidCB* cb = reinterpret_cast<WaterFluidCB*>(mapped.pData);
        cb->supportRadius = m_supportRadius;        // 近傍探索半径（PBF）
        cb->restDensity = m_restDensity;            // 目標密度（初期状態）
        cb->particleMass = m_particleMass;          // 粒子の質量
        cb->stiffness = m_stiffness;                // 弾性係数
        cb->friction = m_friction;                  // 地形接触時の摩擦
        cb->padding1 = 0.0f;
        cb->padding2 = 0.0f;
        cb->padding3 = 0.0f;

        m_context->Unmap(m_cbWaterFluid, 0);

        // CS に定数バッファをセット
        m_shaderResourceBinder.BindConstantBuffer(ShaderStage::CS, SLOT_CB_EFFECT_PARTICLE, m_cbWaterFluid);
    }
}

bool WaterFluidParticleRenderer::CreateEmitCB(void)
{
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(EmitCB);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags = 0;

    HRESULT hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_cbEmit);
    if (FAILED(hr))
        return false;

    return true;
}

void WaterFluidParticleRenderer::UploadEmitCB(void)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_cbEmit, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return;

    // 構造体にデータ書き込み
    EmitCB* cb = reinterpret_cast<EmitCB*>(mapped.pData);
    *cb = m_emitCBData;

    m_context->Unmap(m_cbEmit, 0);

    // CSにバインド
    m_shaderResourceBinder.BindConstantBuffer(ShaderStage::CS, SLOT_CB_EFFECT_EMIT, m_cbEmit);
}

void WaterFluidParticleRenderer::UpdateVolumMeshFromSPH(void)
{
	const auto& particles = m_sphSystem.GetAliveParticles(); // 生存パーティクルのリスト
	if (particles.empty())
		return; // パーティクルが存在しない場合は何もしない


	float particleRadius = m_scale * m_volumeMeshScale; // 粒子の半径をスケールに基づいて計算
    float cellSize = particleRadius * 0.4f;// m_sphSystem.GetCellSize(); // SPHシステムのセルサイズを取得
    float densityScale = m_densityScale; // 密度スケール（必要に応じて調整）
	float isoLevel = m_isoLevel; // 等値面レベル（0.5は一般的な値）

    // 生存パーティクルのAABBを計算
    BOUNDING_BOX bounds = m_sphSystem.ComputeBoundsFromAliveParticles();
    VolumeReconstructionParams volumeParams = SPHVolumeBuilder::ComputeVolumeParamsFromParticles(
        bounds, particleRadius, densityScale, isoLevel, cellSize);
	volumeParams.useSparseGrid = true; // 稀疏グリッドを使用

	// ボリュームパラメータを更新
    volumeParams.activeVoxelCount = m_sphSystem.GetActiveVoxelCount(); // アクティブなボクセル数を設定
    //m_volumeBuilderGPU.BuildScalarField(m_particleBufferSet.srv, volumeParams);

	m_volumeBuilder.Clear(); // ボリュームビルダーをクリア
	m_volumeBuilder.AccumulateFromParticlesSparse(volumeParams, particles); // パーティクルからボリュームを蓄積
    m_volumeMesh.UpdateMesh(m_volumeBuilder.GetScalarFieldMap(), volumeParams); // ボリュームメッシュを構築

	// ボリュームメッシュを構築
    //SimpleArray<UINT>& labelBuffer = m_volumeMeshGPU.GetInitialLabelBuffer();
	//m_sphSystem.ExportInitialClusterLabels(labelBuffer); // 初期クラスタラベルをエクスポート
    //m_volumeMeshGPU.UpdateMesh(m_volumeBuilderGPU.GetScalarFieldSRV(), m_sphSystem.GetGridTriplets(), volumeParams);
}

void WaterFluidParticleRenderer::RenderImGui(void)
{
    if (ImGui::CollapsingHeader("Marching Cubes Settings"))
    {
        ImGui::SliderFloat("Particle Radius: ", &m_volumeMeshScale, 0.5f, 1.5f, "%.01f");
        ImGui::SliderFloat("Density Scale: ", &m_densityScale, 0.0f, 5.0f, "%.1f");
        ImGui::SliderFloat("Iso Level: ", &m_isoLevel, 0.0f, 1.0f, "%.1f");
    }
}
