#pragma once
//=============================================================================
//
// 火球パーティクル専用の更新・描画処理クラス [FireBallEffectRenderer.h]
// Author : 
// 円錐状の放射／回転／初速／アニメーションなどをパラメータ指定で制御し、
// Compute Shader + DrawIndirect による高効率な火球エフェクトを描画する
//
//=============================================================================
#include "Effects/ParticleEffectRendererBase.h"

//*****************************************************************************
// マクロ定義
//***************************************************************************** 
#define FIREBALL_ANIM_TILE_X        7
#define FIREBALL_ANIM_TILE_Y        7

//*********************************************************
// 構造体
//*********************************************************
struct FireBallCB
{
    UINT tilesX = 7;
    UINT tilesY = 7;
    float initialFrameOffset = 0.0f;
    float frameLerpCurve = 1.0f;

    float rotationSpeed = 0.0f;
    float coneAngleDegree = 25.0f;
    float coneRadius = 0.6193843f;
    float coneLength = 5.0f;

	float startSpeedMin = 0.0f;
	float startSpeedMax = 0.0f;
	XMFLOAT2 padding = XMFLOAT2(0.0f, 0.0f); // float4のサイズに合わせるためのパディング
}; // 火球パーティクル用の定数バッファ

class FireBallEffectRenderer : public ParticleEffectRendererBase
{
public:
    // 初期化処理
    virtual bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
    // シャットダウン処理
    virtual void Shutdown(void) override;
    // エフェクトの種類を返す
    EffectType GetEffectType(void) const override { return EffectType::FireBall; }
    // 描画はIndirectで行う
    bool UseDrawIndirect(void) const override { return true; }
	// パーティクルエフェクトの設定
    virtual void ConfigureEffect(const ParticleEffectParams& params) override;

private:
    virtual void DispatchComputeShader(void) override; // パーティクル更新（Dispatch）
    virtual void DrawParticles(void) override;  // パーティクル描画（Draw）

	bool CreateFireBallCB(void); // 火球用の定数バッファの生成
	void UploadFireBallCB(void); // 火球用の定数バッファにデータをアップロード

	bool CreateDiffuseTex(void); // ディフューズテクスチャの生成

    float m_spawnConeAngleDegree = 25.0f;   // コーン角度（度）
    float m_spawnConeRadius = 0.6193843f;   // コーン底面の半径
    float m_spawnConeLength = 5.0f;         // コーンの高さ
    float m_rotationSpeed = 0.0f;
    float m_frameLerpCurve = 0.0f;

	float m_startSpeedMin = 0.0f;           // 初速最小値
	float m_startSpeedMax = 0.0f;           // 初速最大値

    UINT m_tilesX = 7;                      // テクスチャシート横枚数
    UINT m_tilesY = 7;                      // テクスチャシート縦枚数

    ID3D11Buffer* m_cbFireBall = nullptr;
    ID3D11ShaderResourceView* m_diffuseTextureSRV = nullptr;
};