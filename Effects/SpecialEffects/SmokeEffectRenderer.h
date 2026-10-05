#pragma once
//=============================================================================
//
// スモークエフェクトちゃん [SmokeEffectRenderer.h]
// Author : 
// - もくもくエモエモな煙を描くよ
// - Compute Shaderでパーティクル更新！GPUの力でふわっと演出
// - DrawIndirectで超大量スモークもサクサク描画
//
//=============================================================================
#include "Effects/ParticleEffectRendererBase.h"
#include "Utility/SimpleArray.h"

class SmokeEffectRenderer : public ParticleEffectRendererBase
{
public:
    // 初期化処理
    virtual bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
    // シャットダウン処理
    virtual void Shutdown(void) override;
    // エフェクトの種類を返す
    EffectType GetEffectType(void) const override { return EffectType::Smoke; }
    // 描画はIndirectで行う
    bool UseDrawIndirect(void) const override { return true; }

private:
    virtual void DispatchComputeShader(void) override; // パーティクル更新（Dispatch）
    virtual void DrawParticles(void) override; // パーティクル描画（Draw）
	bool CreateDiffuseTex(void); // ディフューズテクスチャの生成

    ID3D11ShaderResourceView* m_diffuseTextureSRV = nullptr; 
};

