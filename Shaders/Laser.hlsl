//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SLOT_CB_VIEW_MATRIX                 b1
#define SLOT_CB_PROJECTION_MATRIX           b2
#define SLOT_CB_LASER                       b7
#define SLOT_TEX_DIFFUSE                    t0
#define SLOT_SAMPLER_DEFAULT                s0

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct VS_INPUT
{
    float3 pos : POSITION; // ワールド座標（CPUで作成した帯の頂点）
    float2 sv  : TEXCOORD0; // (s,v) = (長手0..1, 半径-1..+1)
};


struct VS_OUTPUT
{
    float4 posH : SV_POSITION;
    float2 sv : TEXCOORD0;
};

//*****************************************************************************
// 定数バッファ
//*****************************************************************************
cbuffer ViewBuffer : register(SLOT_CB_VIEW_MATRIX)
{
    matrix g_View;
}

cbuffer ProjectionBuffer : register(SLOT_CB_PROJECTION_MATRIX)
{
    matrix g_Projection;
}

cbuffer CBLaser : register(SLOT_CB_LASER)
{
    float4 g_ColorHDR; // レーザー基調色（HDR想定）
    float g_Time; // 時間（秒）: アニメ用
    float g_FlowSpeed; // 流速（手続きフローの速度）
    float g_CorePower; // 中心部の鋭さ（大きいほど芯が細く強い）
    float g_Radius; // 見た目用途の半径（PS内ではスケール調整で使用可）
}

//*****************************************************************************
// グローバル変数
//*****************************************************************************
Texture2D g_DiffuseTexture : register(SLOT_TEX_DIFFUSE);
SamplerState g_SamplerState : register(SLOT_SAMPLER_DEFAULT); // サンプラーステート

//================ チューニング用（必要に応じて調整） ================
static const float CORE_WEIGHT = 0.55; // 中心コア寄与（下げると中心が暗くなる）
static const float BODY_WEIGHT = 0.45; // 体積感（中心～中間の“身”）
static const float RIM_WEIGHT = 0.65; // 外縁ハロー寄与（上げると輪郭が見える）
static const float CORE_SOFTNESS = 1.35; // 中心の緩さ（>1で柔らかく）
static const float BODY_GAMMA = 0.85; // “身”の分布（<1で広がる）
static const float RIM_START = 0.75; // 外縁フェード開始（小さくすると縁が太く）
static const float RIM_END = 1.00; // 外縁フェード終了
static const float MIN_ALPHA = 0.18; // αの下限（エッジが消えないようにベースを確保）
static const float ALPHA_SCALE = 1.00; // α全体のスケール
static const float FLOW_FREQ = 12.0; // 軸方向の模様周波数
static const float FLOW_STRENGTH = 0.25; // 流れの振幅（0..1）
static const float HEAD_FADE = 0.02; // ヘッドのフェード長
static const float TAIL_FADE = 0.02; // テールのフェード長
//===================================================================

//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    
    float4 viewPos = mul(float4(input.pos, 1.0f), g_View);
    output.posH = mul(viewPos, g_Projection);
    
    output.sv = input.sv; // PSへそのまま渡す

    return output;
}

// 補助: ソフトステップ（エッジを滑らかに）
float softstep(float a, float b, float x)
{
    float t = saturate((x - a) / (b - a));
    return t * t * (3.0 - 2.0 * t);
}

//=============================================================================
// ピクセルシェーダ
//=============================================================================
float4 PS(VS_OUTPUT input) : SV_TARGET
{
        // 軸方向 0..1
    float s = saturate(input.sv.x);
    // 半径方向（0..1）
    float r = abs(input.sv.y);

    // ---- 成分分解：Core / Body / Rim ----------------------------
    // Core: 中心ピーク（少し柔らかめに）
    float core = pow(saturate(1.0 - r * r), max(g_CorePower / CORE_SOFTNESS, 0.8));

    // Body: 体積感（中心～中間を持ち上げる）
    //  1-r を広げるためにガンマ補正（BODY_GAMMA<1で裾野が広がる）
    float body = pow(saturate(1.0 - r), BODY_GAMMA);

    // Rim: 外縁のハロー。開始半径を下げ、幅を広くして“縁”を強調
    float rim = 1.0 - softstep(RIM_START, RIM_END, r);

    // ---- 軸方向のフェード ----------------------------------------
    float head = softstep(0.00, HEAD_FADE, s);
    float tail = 1.0 - softstep(1.0 - TAIL_FADE, 1.0, s);

    // ---- 手続き的“流れ” ------------------------------------------
    float phase = (s * FLOW_FREQ + g_Time * g_FlowSpeed) * 6.2831853;
    float flow = 1.0 - FLOW_STRENGTH + FLOW_STRENGTH * sin(phase); // 1±amp

    // ---- 色成分（見た目の輝度）と α 成分（加算の重み）を半分独立に --------
    //  色はCoreを抑え、BodyとRimの寄与を増やす
    float lumColor = CORE_WEIGHT * core + BODY_WEIGHT * body + RIM_WEIGHT * rim;
    lumColor *= head * tail * flow;

    //  αは“最低限の見え”を確保（MIN_ALPHA）＋上記ルミナンスに比例
    float alpha = max(MIN_ALPHA * (head * tail), lumColor) * ALPHA_SCALE;

    float3 rgb = g_ColorHDR.rgb * lumColor;
    return float4(rgb, alpha);

}