//=============================================================================
//
// FireBallEffectRenderer 処理 [FireBallEffectRenderer.cpp]
// Author : 
//
//=============================================================================
#include "Effects/SpecialEffects/FireBallEffectRenderer.h"
#include "Effects/EffectSystem.h"
#include "Core/TextureMgr.h"

//*****************************************************************************
// マクロ定義
//***************************************************************************** 
#define FIREBALL_DIFFUSE_PATH      "data/TEXTURE/Effects/Fireball.png"

bool FireBallEffectRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    // ブレンドモードをアルファブレンドに設定
    m_blendMode = BLEND_MODE_ADD;

    // 基底クラスの初期化
    if (!ParticleEffectRendererBase::Initialize(device, context))
        return false;

    // FireBallEffect用シェーダーの読み込み
    if (!LoadShaders())
        return false;

    // ディフューズテクスチャの読み込み
    if (!CreateDiffuseTex())
        return false;

	// FireBall専用のCBを作成
    if (!CreateFireBallCB())
        return false;

    return true;
}

void FireBallEffectRenderer::Shutdown(void)
{
    ParticleEffectRendererBase::Shutdown();

    SafeRelease(&m_cbFireBall);
}

void FireBallEffectRenderer::ConfigureEffect(const ParticleEffectParams& params)
{
    // 基底クラスの共通設定（色、寿命、スケールなど）
    ParticleEffectRendererBase::ConfigureEffect(params);

	// FireBallEffectParamsの型チェック
    const FireBallEffectParams* fbParams = dynamic_cast<const FireBallEffectParams*>(&params);
    if (!fbParams) return;

	// FireBallEffectParamsからパラメータを取得
    m_tilesX = fbParams->tilesX;
    m_tilesY = fbParams->tilesY;
    m_spawnConeAngleDegree = fbParams->coneAngleDegree;
    m_spawnConeRadius = fbParams->coneRadius;
    m_spawnConeLength = fbParams->coneLength;
    m_rotationSpeed = fbParams->rotationSpeed;
    m_frameLerpCurve = fbParams->frameLerpCurve;
	m_startSpeedMin = fbParams->startSpeedMin;
    m_startSpeedMax = fbParams->startSpeedMax;
}

void FireBallEffectRenderer::DispatchComputeShader(void)
{
    // FireBall専用CBのアップロード
    UploadFireBallCB();

    // Update用のComputeShaderをバインド
    m_context->CSSetShader(m_updateParticlesCS.cs, nullptr, 0);

    // スレッドグループ数を計算（例: 64スレッド/グループ）
    UINT threadGroupCount = GET_THREAD_GROUP_COUNT(m_maxParticles, THREAD_COUNT);
    m_context->Dispatch(threadGroupCount, 1, 1);

    // Emit処理は基底クラスに任せる
    DispatchEmitParticlesCS();
}

void FireBallEffectRenderer::DrawParticles(void)
{
    // FireBallテクスチャをピクセルシェーダーにバインド
    m_shaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_DIFFUSE, m_diffuseTextureSRV);

    m_shaderResourceBinder.BindConstantBuffer(ShaderStage::GS, SLOT_CB_EFFECT_PARTICLE, m_cbFireBall);

    // 間接描画（DrawIndirect）で描画
    DrawIndirect();
}

bool FireBallEffectRenderer::CreateFireBallCB(void)
{
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(FireBallCB);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags = 0;

    HRESULT hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_cbFireBall);
    if (FAILED(hr))
        return false;

    return true;
}

void FireBallEffectRenderer::UploadFireBallCB(void)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_context->Map(m_cbFireBall, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        FireBallCB* cb = reinterpret_cast<FireBallCB*>(mapped.pData);
        cb->tilesX = m_tilesX;
        cb->tilesY = m_tilesY;
        cb->initialFrameOffset = 0.0f;
        cb->frameLerpCurve = m_frameLerpCurve;
        cb->rotationSpeed = m_rotationSpeed;
        cb->coneAngleDegree = m_spawnConeAngleDegree;
        cb->coneLength = m_spawnConeLength;
        cb->coneRadius = m_spawnConeRadius;
		cb->startSpeedMin = m_startSpeedMin;
		cb->startSpeedMax = m_startSpeedMax;
        m_context->Unmap(m_cbFireBall, 0);

        // CS に定数バッファをセット
        m_shaderResourceBinder.BindConstantBuffer(ShaderStage::CS, SLOT_CB_EFFECT_PARTICLE, m_cbFireBall);
    }
}

bool FireBallEffectRenderer::CreateDiffuseTex(void)
{
    m_diffuseTextureSRV = TextureMgr::get_instance().CreateTexture(FIREBALL_DIFFUSE_PATH);
    if (!m_diffuseTextureSRV)
        return false;

    return true;
}