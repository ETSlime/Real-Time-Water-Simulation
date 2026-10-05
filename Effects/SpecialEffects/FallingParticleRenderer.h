#pragma once
//=============================================================================
//
// [FallingParticleRenderer.h]
// Author : 
//
//=============================================================================
#include "Effects/ParticleEffectRendererBase.h"
#include "Collision/VoxelBufferUploader.h"
#include "Effects/SpecialEffects/Water/WaterFluidParticleRenderer.h"

//*********************************************************
// 構造体
//*********************************************************


class FallingParticleRenderer : public ParticleEffectRendererBase, IDebugUI
{
public:
    // 初期化処理
    virtual bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
    // シャットダウン処理
    virtual void Shutdown(void) override;
    // エフェクトの種類を返す
    EffectType GetEffectType() const override { return EffectType::Falling; }
    // 描画はIndirectで行う
    bool UseDrawIndirect(void) const override { return true; }
    // パーティクルエフェクトの設定
    //virtual void ConfigureEffect(const ParticleEffectParams& params) override;
    // パーティクルを指定位置に放出
    void EmitParticles(float radius, int emitCount, const XMFLOAT3& center = XMFLOAT3{});
    // 放出するパーティクル数を設定
    void SetEmitCount(UINT count) { m_emitCBData.emitCount = count; }


private:
    virtual void DispatchComputeShader(void) override; // パーティクル更新（Dispatch）
    virtual void DrawParticles(void) override; // パーティクル描画（Draw）

    // パーティクル更新用のCompute Shaderをディスパッチ
    virtual void DispatchEmitParticlesCS(void) override;

    bool CreateEmitCB(void); // パーティクル放出用の定数バッファを作成
    void UploadEmitCB(void); // パーティクル放出用の定数バッファをアップロード

    virtual void RenderImGui(void) override {};
    virtual const char* GetPanelName(void) const override { return "Falling Particle Renderer"; };

    EmitCB m_emitCBData;              // パーティクル放出用の定数バッファデータ
    ID3D11Buffer* m_cbEmit = nullptr;

    VoxelBufferUploader& m_voxelBufferUploader = VoxelBufferUploader::get_instance();
};
