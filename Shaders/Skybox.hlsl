//*****************************************************************************
// 定数バッファ
//*****************************************************************************
cbuffer SkyBoxBuffer : register(b9)
{
    matrix view;
    matrix projection;
    float blendFactor;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD0;
    int faceIndex : TEXCOORD1;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    int faceIndex : TEXCOORD1;
};

//*****************************************************************************
// グローバル変数
//*****************************************************************************
Texture2D skyboxDayTextures[6] : register(t15);
Texture2D skyboxNightTextures[6] : register(t21);
SamplerState samplerState : register(s2);

//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;

    float4 pos = float4(input.position, 1.0f);
    output.position = mul(pos, view);
    output.position = mul(output.position, projection);
    output.faceIndex = input.faceIndex;
    output.texCoord = input.texCoord;
    return output;
}

//=============================================================================
// ピクセルシェーダ
//=============================================================================
float4 PS(VS_OUTPUT input) : SV_TARGET
{
    int faceIndex = input.faceIndex;
    float4 dayColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 nightColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    switch (input.faceIndex)
    {
        case 0:
            dayColor = skyboxDayTextures[0].Sample(samplerState, input.texCoord);
            nightColor = skyboxNightTextures[0].Sample(samplerState, input.texCoord);
            break;
        case 1:
            dayColor = skyboxDayTextures[1].Sample(samplerState, input.texCoord);
            nightColor = skyboxNightTextures[1].Sample(samplerState, input.texCoord);
            break;
        case 2:
            dayColor = skyboxDayTextures[2].Sample(samplerState, input.texCoord);
            nightColor = skyboxNightTextures[2].Sample(samplerState, input.texCoord);
            break;
        case 3:
            dayColor = skyboxDayTextures[3].Sample(samplerState, input.texCoord);
            nightColor = skyboxNightTextures[3].Sample(samplerState, input.texCoord);
            break;
        case 4:
            dayColor = skyboxDayTextures[4].Sample(samplerState, input.texCoord);
            nightColor = skyboxNightTextures[4].Sample(samplerState, input.texCoord);
            break;
        case 5:
            dayColor = skyboxDayTextures[5].Sample(samplerState, input.texCoord);
            nightColor = skyboxNightTextures[5].Sample(samplerState, input.texCoord);
            break;
    }
    float4 finalColor = lerp(dayColor, nightColor, saturate(blendFactor));

    return finalColor;

}