//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SLOT_CB_VIEW_MATRIX                 b1
#define SLOT_CB_PROJECTION_MATRIX           b2
#define SLOT_CB_PROJECTILE_PREVIEW          b5

#define BREATH_GEOM_AMP   0.20   // 幾何スケールの振幅（±20%）
#define BREATH_GEOM_FREQ  3.5    // 幾何スケールの周波数（Hz相当）
#define BREATH_ALPHA_AMP  0.40   // アルファ呼吸の振幅（±40%）
#define BREATH_ALPHA_FREQ 3.5    // アルファ呼吸の周波数
#define FADE_RADIUS_SCALE 1.5    // フェード半径 = 実効半径 * これ

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
};

struct VS_OUTPUT
{
    float4 posH : SV_POSITION; // クリップ座標
    float3 normalW : NORMAL; // ワールド法線（半球用）
    float3 worldPos : TEXCOORD0; // ワールド座標（PSで使う場合用）
};

//*****************************************************************************
// 定数バッファ
//*****************************************************************************
cbuffer ViewBuffer : register(SLOT_CB_VIEW_MATRIX)
{
    matrix View;
}

cbuffer ProjectionBuffer : register(SLOT_CB_PROJECTION_MATRIX)
{
    matrix Projection;
}

cbuffer CBPreview : register(SLOT_CB_PROJECTILE_PREVIEW)
{
    float3 g_HitPos; // 半球のワールド位置
    float g_Scale; // 半球スケール係数
    float4 g_Color; // 基本カラー（軌跡/半球共通）
    float g_Time; // 時間（呼吸アニメ用）
    int g_RenderType; // 0 = Trajectory（軌跡）, 1 = Sphere（半球）
    float2 g_Padding; // 16byte alignment
};

//*****************************************************************************
// グローバル変数
//*****************************************************************************


//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;

    // 今回は normal で軌跡/半球を判定しない
    // g_RenderType によって分岐する
    bool isTrajectory = (g_RenderType == 0);

    float3 worldPos = input.pos;

    // 半球：ヒット位置原点でスケール＋呼吸アニメ
    if (!isTrajectory)
    {
        float breathScale = 1.0 + (float) BREATH_GEOM_AMP * sin(g_Time * (float) BREATH_GEOM_FREQ);
        worldPos = g_HitPos + (input.pos * g_Scale * breathScale);
    }

    output.worldPos = worldPos;
    output.normalW = input.normal;

    // View * Projection
    float4 viewPos = mul(float4(worldPos, 1.0f), View);
    output.posH = mul(viewPos, Projection);
    return output;
}

//=============================================================================
// ピクセルシェーダ
//=============================================================================
float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 color = g_Color;

    // 0 = 軌跡（Trajectory）, 1 = 半球（Sphere）
    if (g_RenderType == 0)
    {
        // 軌跡：法線の厚み方向によるアルファフェード
        // CPU側でnormalWのy成分を厚み方向（上下）に設定しておくことで、
        // 中央を濃く、上下端を薄くするグラデーションを実現
        float alphaFade = saturate(0.3 + abs(input.normalW.y) * 1.5);
        color.a = 0.35f * alphaFade;

        // 流動感を追加したい場合：UVやworldPosにg_Timeを加えてスクロール
        // 例: color.a *= 0.8 + 0.2 * sin(input.uv.x * 30 + g_Time * 6);
    }
    else
    {
        // 半球：VSと同じ幾何呼吸を再現して実効半径を算出
        float breathScale = 1.0 + (float) BREATH_GEOM_AMP * sin(g_Time * (float) BREATH_GEOM_FREQ);
        float effectiveR = g_Scale * breathScale;
        
        // 中心からの距離でフェード（半径に追従させる）
        float dist = length(input.worldPos - g_HitPos);
        float fade = saturate(1.0 - dist / (effectiveR * (float) FADE_RADIUS_SCALE));
        
        // アルファ呼吸（サイズは変えず、明滅だけ強調したいときに効く）
        float pulseAlpha = 1.0 + (float) BREATH_ALPHA_AMP * sin(g_Time * (float) BREATH_ALPHA_FREQ);

        float baseAlpha = 0.5f;
        color.a = saturate(baseAlpha * fade * pulseAlpha);
        return color;
    }

    return color;
}