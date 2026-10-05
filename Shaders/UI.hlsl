//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SLOT_TEX_DIFFUSE            t0
#define SLOT_SAMPLER_DEFAULT        s0
#define SCREEN_WIDTH                1920.0f
#define SCREEN_HEIGHT               1080.0f

//*****************************************************************************
// 定数バッファ
//*****************************************************************************
struct VS_INPUT
{
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // 変換行列を適用
    float2 ndc = input.pos / float2(SCREEN_WIDTH, SCREEN_HEIGHT) * 2.0f - 1.0f;
    ndc.y *= -1.0f;
    output.pos = float4(ndc, 0.0f, 1.0f);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

//*****************************************************************************
// グローバル変数
//*****************************************************************************
Texture2D g_Texture : register(SLOT_TEX_DIFFUSE);
SamplerState g_SamplerState : register(SLOT_SAMPLER_DEFAULT);

//=============================================================================
// ピクセルシェーダ
//=============================================================================
float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 texColor = g_Texture.Sample(g_SamplerState, input.uv);
    
    if (texColor.a == 0.0f)
        return float4(0, 0, 0, 0);
    
    return texColor * input.color;
}