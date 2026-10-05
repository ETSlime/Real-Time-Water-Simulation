#pragma once
//=============================================================================
//
// 全エフェクト共通の描画インターフェースクラス [IEffectRenderer.h]
// Author : 
// 各種パーティクル・ソフトボディ等のエフェクトに共通する、
// 初期化／更新／描画／位置制御などのインターフェースを提供する
// 
//=============================================================================
#include "Core/Graphics/Renderer.h"
#include "Scene/Scene.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
// 総スレッド数 total を groupSize 単位で割り、足りない分を繰り上げて補う
#define GET_THREAD_GROUP_COUNT(total, groupSize) (((total) + (groupSize) - 1) / (groupSize))
#define THREAD_COUNT 256 // スレッド数

enum class EffectType
{
    None,

    Particle_Start,
    Fire,
    FireBall,
    Smoke,
    WaterFluid,
    Falling,
    Particle_End,

    LakeSurface,
    SoftBody,
};

class IEffectRenderer : public ISceneEntity
{
public:
    virtual ~IEffectRenderer() = default;

    virtual bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) = 0;
    virtual void Shutdown(void) = 0;
    virtual bool IsShutdown(void) const = 0;
    virtual void Update(void) = 0;
    virtual void Draw(void) = 0;
    virtual void SetupPipeline(void) = 0;

    virtual const XMFLOAT3& GetPosition(void) const = 0;
    virtual void SetPosition(const XMFLOAT3& pos) = 0;
    virtual EffectType GetEffectType() const = 0;
    virtual bool IsEffectRenderer(void) const override { return true; }
};
