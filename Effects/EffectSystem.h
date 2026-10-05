#pragma once
//=============================================================================
//
// パーティクルエフェクトの生成・管理・描画を統括する演出管理クラス [EffectSystem.h]
// Author : 
// 各種エフェクト（火炎・煙・ソフトボディ等）をパラメータ指定で生成し、
// ランタイムでの更新／描画制御・一括管理・リソース解放を行う
//
//=============================================================================
#include "ParticleEffectRendererBase.h"
#include "Utility/SingletonBase.h"
#include "Core/Camera.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_EFFECT_NUM                  256     // 最大エフェークト数
#define MAX_BILLBOARD_SIMPLE_NUM        64      // 最大ビルボードシンプルエフェクト数
#define MAX_BILLBOARD_FLIPBOOK_NUM      64      // 最大ビルボードフリップブック数
#define MAX_WATER_FLUID_NUM             64      // 最大水面フルイドエフェクト数
#define MAX_FALLING_PARTICLE_NUM        64      // 最大落下パーティクルエフェクト数
#define MAX_SOFTBODY_EFFECT_NUM         64      // 最大ソフトボディフェークト数
#define MAX_OTHER_EFFECT_NUM            64      // その他のエフェクト数

//*********************************************************
// 構造体
//*********************************************************
struct ParticleEffectParams
{
    EffectType type = EffectType::None;
	float duration = -1.0f; // エフェクトの持続時間（秒）
    XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    float scale = 1.0f;
    float lifeMin = 1.0f;
    float lifeMax = 2.0f;
    float spawnRateMin = 5.0f;
    float spawnRateMax = 15.0f;
    XMFLOAT3 acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);
    UINT numParticles = 512;
    XMFLOAT4 color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    XMFLOAT4 startColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    XMFLOAT4 endColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

    virtual ~ParticleEffectParams() = default; // RTTI、dynamic_cast のため
};

// 火炎エフェクト専用パラメータ構造体
struct FireBallEffectParams : public ParticleEffectParams
{
    UINT tilesX = 7;
    UINT tilesY = 7;
    float coneAngleDegree = 25.0f;
    float coneRadius = 0.6f;
    float coneLength = 5.0f;
    float frameLerpCurve = 1.0f;
    float rotationSpeed = 0.5f;

	float startSpeedMin = 0.0f;
	float startSpeedMax = 0.0f;
};

// 水体流体エフェクト専用パラメータ構造体
struct WaterFluidParams : public ParticleEffectParams
{
    float supportRadius = 0.3f;     // 近傍半径（PBF）
    float restDensity = 1.0f;       // 静止時の密度
    float particleMass = 1.0f;      // 粒子質量
    float stiffness = 0.1f;         // 圧力（硬さ）
    float friction = 0.05f;         // 地形との摩擦
	float viscosity = 0.01f;        // 粘性係数（流体の粘り気）
	float restitution = 0.5f;       // 反発係数（衝突時の挙動制御用）
};

class EffectSystem : public SingletonBase<EffectSystem>
{
public:
    void Init(void);
    void Shutdown(void);
    void Update(void);
    void Draw(void);

    inline bool IsParticleEffectType(EffectType type)
    {
        return (type > EffectType::Particle_Start && type < EffectType::Particle_End);
    }

    // エフェクト生成
    ParticleEffectRendererBase* SpawnParticleEffect(ParticleEffectParams& params);
	void AddEffect(IEffectRenderer* effect);

    // 汎用特效削除
    void RemoveEffect(IEffectRenderer* effect);
    // リソースの解除
    void ClearAllEffectBindings(void);

private:
    bool m_initialized = false;
    SimpleArray<IEffectRenderer*> m_allEffects;       // 全特效リスト
	SimpleArray<IEffectRenderer*> m_billboardSimpleEffects; // ビルボードシンプルエフェクト
	SimpleArray<IEffectRenderer*> m_billboardFlipbookEffects; // ビルボードフリップブックエフェクト
	SimpleArray<IEffectRenderer*> m_waterFluidEffects; // 水面フルイドエフェクト
	SimpleArray<IEffectRenderer*> m_fallingParticleEffects; // 落下パーティクルエフェクト
    SimpleArray<IEffectRenderer*> m_softBodyEffects;  // ソフトボディ
	SimpleArray<IEffectRenderer*> m_otherEffects;		// その他のエフェクト
    SimpleArray<IEffectRenderer*> m_toRemove;     // 削除が必要なエフェクトを一時的に保管するリスト

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    ShaderResourceBinder& m_ShaderResourceBinder = ShaderResourceBinder::get_instance();
};