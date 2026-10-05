//=============================================================================
//
// SmokeEffectRenderer処理 [SmokeEffectRenderer.cpp]
// Author : 
//
//=============================================================================
#include "Effects/SpecialEffects/SmokeEffectRenderer.h"
#include "Core/TextureMgr.h"

//*****************************************************************************
// マクロ定義
//***************************************************************************** 
#define SMOKE_DIFFUSE_PATH      "data/TEXTURE/Effects/Smoke.png"

bool SmokeEffectRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    // ブレンドモードをアルファブレンドに設定
    m_blendMode = BLEND_MODE_ALPHABLEND;

    // 基底クラスの初期化
    if (!ParticleEffectRendererBase::Initialize(device, context))
        return false;

    // シェーダー読み込み
    if (!LoadShaders())
        return false;

    // テクスチャ読み込み
    if (!CreateDiffuseTex())
        return false;

    return true;
}

void SmokeEffectRenderer::Shutdown(void)
{
    ParticleEffectRendererBase::Shutdown();
}

bool SmokeEffectRenderer::CreateDiffuseTex(void)
{
    m_diffuseTextureSRV = TextureMgr::get_instance().CreateTexture(SMOKE_DIFFUSE_PATH);
    if (!m_diffuseTextureSRV)
        return false;

    return true;
}

void SmokeEffectRenderer::DispatchComputeShader(void)
{
    // CSのバインド
    m_context->CSSetShader(m_updateParticlesCS.cs, nullptr, 0);

    // CSのディスパッチ
    UINT threadGroupCount = GET_THREAD_GROUP_COUNT(m_maxParticles, THREAD_COUNT); // スレッドグループ数計算
    m_context->Dispatch(threadGroupCount, 1, 1);

    // Emit用CS は基底クラスに任せる（Prepare + Bind + Dispatch）
    DispatchEmitParticlesCS();
}

void SmokeEffectRenderer::DrawParticles(void)
{
    // スモーク専用リソースバインド（ノイズテクスチャ）
    m_shaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_DIFFUSE, m_diffuseTextureSRV);

    DrawIndirect();
}
