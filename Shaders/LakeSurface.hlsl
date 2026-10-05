//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SLOT_CB_VIEW_MATRIX                 b1
#define SLOT_CB_PROJECTION_MATRIX           b2
#define SLOT_CB_EFFECT_LAKE_SURFACE         b12

#define SLOT_TEX_NORMAL                     t9
#define SLOT_TEX_NORMAL_2                   t22
#define SLOT_TEX_REFLECT                    t12

#define SLOT_SAMPLER_DEFAULT                s0

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct VS_INPUT
{
    float3 position : POSITION;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float2 uv : TEXCOORD1;
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

cbuffer CBWaterParams : register(SLOT_CB_EFFECT_LAKE_SURFACE)
{
    float4x4 g_World;
    float3 g_CameraPos;
    float g_Time;
    
    float g_WaveStrength;
    float g_FresnelBias;
    float g_FresnelPower;
    float padding1;
    
    float3 g_WaterColor;
    float padding2;
}

//*****************************************************************************
// グローバル変数
//*****************************************************************************
// 入力法線マップ（2枚）
Texture2D g_NormalMap1 : register(SLOT_TEX_NORMAL);
Texture2D g_NormalMap2 : register(SLOT_TEX_NORMAL_2);

//TextureCube g_ReflectionMap : register(SLOT_TEX_REFLECT); // 環境反射（キューブマップ or 反射テクスチャ）
SamplerState g_SamplerLinear : register(SLOT_SAMPLER_DEFAULT);

//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // 基本UV座標を計算（位置のXZ成分を[-1,1]→[0,1]に変換）
    float2 baseUV = input.position.xz * 0.5f + 0.5f;
    
    // 波のスピードを計算（WaveStrengthが小さいほど速くなる）
    float waveSpeed = (1.0f - g_WaveStrength) * 0.015f + 0.005f;

    // 第1と第2のUVを時間とともにずらして計算
    float2 uv1 = baseUV + float2(g_Time * waveSpeed, g_Time * waveSpeed * 1.2f);
    float2 uv2 = baseUV + float2(-g_Time * waveSpeed * 0.8f, g_Time * waveSpeed * 1.1f);

    // 法線マップから法線ベクトルをサンプル（SampleLevel使ってるにゃん）
    float3 n1 = g_NormalMap1.SampleLevel(g_SamplerLinear, uv1, 0).xyz * 2.0f - 1.0f;
    float3 n2 = g_NormalMap2.SampleLevel(g_SamplerLinear, uv2, 0).xyz * 2.0f - 1.0f;

    // 法線を合成して正規化
    float3 normal = normalize(n1 + n2);

    // 波の強さを使って頂点の揺れを計算
    float3 offset = normal * g_WaveStrength * 0.1f;

    // 頂点位置にオフセットを適用して、世界座標に変換
    float3 displacedPos = input.position + offset;
    float4 worldPos = mul(float4(displacedPos, 1.0f), g_World);

    // ビュー空間・射影空間へ変換して最終出力
    float4 viewPos = mul(worldPos, View);
    output.position = mul(viewPos, Projection);

    // 世界座標も出力（後で使うため）
    output.worldPos = worldPos.xyz;

    // UVはそのまま出力
    output.uv = baseUV;

    return output;
}

//=============================================================================
// ピクセルシェーダ
//=============================================================================
float4 PS(VS_OUTPUT input) : SV_TARGET
{
    // 計算用UV（時間に応じてスクロール）
    float waveSpeed = (1.0f - g_WaveStrength) * 0.015f + 0.005f;
    float2 uv1 = input.uv + float2(g_Time * waveSpeed, g_Time * waveSpeed * 1.2f);
    float2 uv2 = input.uv + float2(-g_Time * waveSpeed * 0.8f, g_Time * waveSpeed * 1.1f);

    // 法線マップの読み込み・合成
    float3 n1 = g_NormalMap1.Sample(g_SamplerLinear, uv1).xyz * 2.0f - 1.0f;
    float3 n2 = g_NormalMap2.Sample(g_SamplerLinear, uv2).xyz * 2.0f - 1.0f;
    float3 finalNormal = normalize(n1 + n2);
    finalNormal = normalize(lerp(float3(0, 1, 0), finalNormal, g_WaveStrength));

    // フレネル計算
    float3 viewDir = normalize(g_CameraPos - input.worldPos);
    float fresnel = g_FresnelBias + pow(1.0f - saturate(dot(viewDir, finalNormal)), g_FresnelPower);
    fresnel = saturate(fresnel);

    float alpha = lerp(0.5, 1.0, fresnel);
    
    //// 環境反射
    //float3 reflectDir = reflect(viewDir, finalNormal);
    //float3 reflection = g_ReflectionMap.Sample(g_SamplerLinear, reflectDir).rgb;

    // 水の色と合成
    float3 baseColor = g_WaterColor;
    float3 highlight = float3(0.6f, 0.8f, 1.0f);
    float3 finalColor = lerp(baseColor, highlight, fresnel);

    return float4(finalColor, alpha);
}