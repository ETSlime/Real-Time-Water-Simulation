//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SLOT_CB_EFFECT_UPDATE       b5
#define SLOT_CB_EFFECT_DRAW         b6
#define SLOT_CB_EFFECT_PARTICLE     b7
#define SLOT_UAV_PARTICLE           u0
#define SLOT_UAV_ALIVE_LIST         u1
#define SLOT_UAV_FREE_LIST          u2
#define SLOT_UAV_FREE_LIST_CONSUME  u3
#define SLOT_SRV_PARTICLE           t6
#define SLOT_SRV_ALIVE_LIST         t7
#define SLOT_TEX_DIFFUSE            t0
#define SLOT_SAMPLER_DEFAULT        s0
#define SLOT_CB_VIEW_MATRIX         b1

#define THREAD_COUNT                256

//*****************************************************************************
// 定数バッファ
//*****************************************************************************
cbuffer CBParticleUpdate : register(SLOT_CB_EFFECT_UPDATE)
{
    float3 g_Acceleration; // 加速度（例：float3(0, 1.5f, 0)）
    float g_Scale; // サイズスケール
    
    float g_DeltaTime; // 経過時間
    float g_TotalTime; // 全体経過時間
    float g_LifeMin; // 最小ライフ
    float g_LifeMax; // 最大ライフ
    float g_SpawnRateMin; // 毎秒発射数
    float g_SpawnRateMax; // 毎秒発射数
    
    uint g_MaxParticleCount; // 最大パーティクル数
    uint g_ParticlesToEmitThisFrame; // このフレームに発射するパーティクル数
    
    float4 g_startColor;
    float4 g_endColor;
};

cbuffer CBParticleDraw : register(SLOT_CB_EFFECT_DRAW)
{
    matrix g_World;
    matrix g_WorldInv;
    matrix g_ViewProj;
    
    float3 g_CameraPos;
    float  particleDrawPadding;
};

cbuffer ViewBuffer : register(SLOT_CB_VIEW_MATRIX)
{
    matrix g_View;
}

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct VS_INPUT
{
    uint instanceID : SV_InstanceID; // DrawInstancedIndirect で渡される
};

// VS → 単にParticleIDを渡す
struct VS_OUTPUT
{
    uint ParticleID : PARTICLE_ID;
};

struct GS_INPUT
{
    uint ParticleID : PARTICLE_ID;
};

// GS → Billboard展開
struct GS_OUTPUT
{
    float4 PosH : SV_POSITION;
    float2 TexCoord : TEXCOORD;
    float4 Color : COLOR0;
};

struct PS_INPUT
{
    float4 PosH : SV_POSITION;
    float2 TexCoord : TEXCOORD;
    float4 Color : COLOR0;
};

//================== 補助関数 ==================
float Rand(float seed)
{
    return frac(sin(seed * 12.9898) * 43758.5453);
}

float Hash11(float x)
{
    return frac(sin(x * 12.9898f) * 43758.5453f);
}

//*****************************************************************************
// グローバル変数
//*****************************************************************************
Texture2D g_DiffuseTex : register(SLOT_TEX_DIFFUSE); // ノイズテクスチャ
SamplerState g_Sampler : register(SLOT_SAMPLER_DEFAULT); // サンプラーステート

// バッファ
AppendStructuredBuffer<uint> g_AliveListUAV : register(SLOT_UAV_ALIVE_LIST); // 書き込み用生存パーティクルリスト
AppendStructuredBuffer<uint> g_FreeListUAV : register(SLOT_UAV_FREE_LIST); // 書き込み用空きパーティクルリスト
StructuredBuffer<uint> g_AliveListSRV : register(SLOT_SRV_ALIVE_LIST); // 読み取り用生存パーティクルリスト
ConsumeStructuredBuffer<uint> g_FreeListConsumeUAV : register(SLOT_UAV_FREE_LIST_CONSUME); // 読み取り用空きパーティクルリスト