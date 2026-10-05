#include "SharedParticleUtil.hlsl"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SLOT_CB_FLUID_SURFACE              b12
#define SLOT_SRV_FLUID_THICKNESS           t17
#define SLOT_SAMPLER_DEFAULT               s0
//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct VS_INPUT_FLUID_SURFACE
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD;
};

struct VS_OUTPUT_FLUID_SURFACE
{
    float4 PosH : SV_POSITION;
    float3 NormalW : NORMAL;
    float2 TexCoord : TEXCOORD;
    float3 ViewDir : TEXCOORD1;
    float3 WorldPos : TEXCOORD2;
    float2 ScreenUV : TEXCOORD3;
};


//*****************************************************************************
// 定数バッファ
//*****************************************************************************

cbuffer CBFluidSurface : register(SLOT_CB_FLUID_SURFACE)
{
    float3 g_ShallowColor;
    float g_FresnelPower;
    
    float3 g_DeepColor;
    float g_AlphaScale;
    
    float g_ThicknessScale; // 厚度強度の倍率
    float g_ThicknessBias; // 最小透明度調整用
    float g_ThicknessNormalizeFactor; // 厚度の正規化係数（テクスチャの最大値で割る）
    float g_Padding;
    float2 g_ScreenSize;
    float2 g_InvScreenSize;
}

//*****************************************************************************
// グローバル変数
//*****************************************************************************
Texture2D<uint> g_ThicknessTex : register(SLOT_SRV_FLUID_THICKNESS);
SamplerState g_SamplerLinear : register(SLOT_SAMPLER_DEFAULT);

//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUTPUT_FLUID_SURFACE VS(VS_INPUT_FLUID_SURFACE input)
{
    VS_OUTPUT_FLUID_SURFACE output;

    // ワールド空間に変換
    float4 worldPos4 = mul(float4(input.Position, 1.0f), g_World);
    float3 worldPos = worldPos4.xyz;
    output.WorldPos = worldPos;

    // 法線をワールド空間に変換（必要であれば逆転置行列を使ってもOK）
    float3 worldNormal = normalize(mul(float4(input.Normal, 0.0f), g_World).xyz);
    output.NormalW = worldNormal;
    
    // テクスチャ座標をそのまま渡す
    output.TexCoord = input.TexCoord;
    
    // カメラ→ピクセル方向
    output.ViewDir = normalize(g_CameraPos - worldPos);
    
    // スクリーン空間位置
    float4 clipPos = mul(worldPos4, g_ViewProj);
    output.PosH = clipPos;

    // 画面UVを求める（NDC [-1,1] → UV [0,1]）
    float2 ndc = clipPos.xy / clipPos.w;
    output.ScreenUV = ndc * 0.5f + 0.5f;
    
    return output;
}

float4 PS(VS_OUTPUT_FLUID_SURFACE input) : SV_TARGET
{
    // フレネル効果
    float fresnel = pow(1.0f - abs(dot(normalize(input.NormalW), normalize(input.ViewDir))), g_FresnelPower);

    // 厚度マップをスクリーンUVでサンプリング
    //float thickness = asfloat(g_ThicknessTex.Sample(g_SamplerLinear, input.ScreenUV).r);
    int2 pixelCoord = int2(input.ScreenUV * g_ScreenSize);
    uint thicknessInt = g_ThicknessTex.Load(int3(pixelCoord, 0)).r;
    float thickness = float(thicknessInt) * g_ThicknessNormalizeFactor;
    thickness = 1.0f;
    
    // 厚度による透明度計算
    float t = saturate(thickness * g_ThicknessScale + g_ThicknessBias);
    float thicknessAlpha = smoothstep(0.0f, 1.0f, t); // 厚度を0-1に正規化
    
    // 色合成（例えば厚度で深い青に）
    float3 thicknessColor = lerp(g_ShallowColor.rgb, g_DeepColor.rgb, 1.0f - thicknessAlpha);
    
    // 最終色とフレネル合成
    float3 finalColor = thicknessColor + fresnel * 0.3f;
    
    //// 色の決定
    //float3 baseColor = g_BaseColor.rgb;
    //float3 finalColor = baseColor + fresnel * 0.3f;

    // アルファ値は厚度に応じて減衰
    float alpha = g_AlphaScale * pow(thicknessAlpha, 1.5f);
    alpha = 0.5f;
    return float4(finalColor, alpha);
}
