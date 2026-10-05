//*****************************************************************************
// 定数バッファ
//*****************************************************************************

struct LightViewProjBuffer
{
    matrix ViewProj[5];
    int LightIndex;
    int padding[3];
};

// マトリクスバッファ
cbuffer WorldBuffer : register(b0)
{
    matrix World;
}

cbuffer ViewBuffer : register(b1)
{
    matrix View;
}

cbuffer ProjectionBuffer : register(b2)
{
    matrix Projection;
}

cbuffer ProjViewBuffer : register(b8)
{
    LightViewProjBuffer lightViewProj;
}


cbuffer PerFrameBuffer : register(b12)
{
    float Time;
    float3 WindDirection;
    float WindStrength;
    float padding1;
    float2 NoiseTextureResolution;
    float2 padding2;
}


struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

//*****************************************************************************
// グローバル変数
//*****************************************************************************
Texture2D g_DiffuseTexture : register(t0);
SamplerState g_SamplerState : register(s0);
//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUTPUT VS(VS_INPUT input)
{
    matrix WorldViewProjection;
    WorldViewProjection = mul(World, View);
    WorldViewProjection = mul(WorldViewProjection, Projection);
    
    VS_OUTPUT output;

    output.position = mul(float4(input.position, 1.0), WorldViewProjection);

    //float2 frameUV = float2(fmod(frameIndex, 4) * frameSize.x, floor(frameIndex / 4) * frameSize.y);
    //output.uv = input.uv * frameSize + frameUV;

    output.color = input.color;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 texColor = g_DiffuseTexture.Sample(g_SamplerState, input.uv);

    float4 finalColor = texColor * input.color;
    
    //clip(finalColor.a - 0.1);

    return float4(1, 0, 0, 0.3); //finalColor;
}


//----------------------------------
// シャドウデプスパス (HLSL)
//----------------------------------

//=============================================================================
// 頂点シェーダ
//=============================================================================
//VS_OUTPUT VSShadow(VS_INPUT input)
//{

//}
