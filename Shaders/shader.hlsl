//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define LIGHT_MAX_NUM   5
#define LIGHT_MAX_NUM_CASCADE 4
#define SCREEN_WIDTH    1920
#define SHADOWMAP_SIZE  2048

//*****************************************************************************
// 定数バッファ
//*****************************************************************************

struct WorldMatrixBuffer
{
    matrix world;
    matrix invWorld;
};

// マテリアルバッファ
struct MATERIAL
{
    float4 Ambient;
    float4 Diffuse;
    float4 Specular;
    float4 Emission;
    float Shininess;
    int noTexSampling;
    int lightmapSampling;
    int normalMapSampling;
    int bumpMapSampling;
    int opacityMapSampling;
    int reflectMapSampling;
    int translucencyMapSampling;
    //float Dummy[1]; //16byte境界用
};

struct LightFlags
{
    int Type;
    int OnOff;
    int Dummy1;
    int Dummy2;
};

// ライト用バッファ
struct LIGHT
{
    float4 Direction[LIGHT_MAX_NUM];
    float4 Position[LIGHT_MAX_NUM];
    float4 Diffuse[LIGHT_MAX_NUM];
    float4 Ambient[LIGHT_MAX_NUM];
    float4 Attenuation[LIGHT_MAX_NUM];
    LightFlags Flags[LIGHT_MAX_NUM];
    matrix LightViewProj[LIGHT_MAX_NUM];
    int Enable;
    int3 Dummy; //16byte境界用
};

struct FOG
{
    float4 Distance;
    float4 FogColor;
    int Enable;
    float Dummy[3]; //16byte境界用
};

struct MODE
{
    int mode;
    int padding[3];
};

struct SkinnedMeshVertexInputType
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;
    float4 weights : BLENDWEIGHT;
    float4 boneIndices : BLENDINDICES;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float3 viewPos : POSITIONT1;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR;
    float4 worldPos : POSITION;
    float4 shadowCoord[LIGHT_MAX_NUM_CASCADE] : TEXCOORD1;
};

struct CascadeData
{
    float4x4 lightViewProj;
    float4x4 lightView;
    float4x4 lightProj;
    float splitDepth;
    float3 padding; // 16バイトのアライメントを維持するため
};

// マトリクスバッファ
cbuffer WorldBuffer : register( b0 )
{
    WorldMatrixBuffer WorldBuffer;
}

cbuffer ViewBuffer : register( b1 )
{
	matrix View;
}

cbuffer ProjectionBuffer : register( b2 )
{
	matrix Projection;
}

cbuffer MaterialBuffer : register(b3)
{
    MATERIAL Material;
}

cbuffer LightBuffer : register(b4)
{
    LIGHT Light;
}

// フォグ用バッファ
cbuffer FogBuffer : register(b5)
{
    FOG Fog;
};

cbuffer CB_CascadeData : register(b8)
{
    CascadeData g_CascadeData;
};

cbuffer CameraPosBuffer : register(b7)
{
    float4 CameraPos;
}

cbuffer CB_CascadeDataMainPass : register(b6)
{
    CascadeData g_CascadeArray[LIGHT_MAX_NUM_CASCADE];
    float4 g_CascadeSplits;
}

cbuffer ModeBuffer : register(b10)
{
    MODE md;
}


cbuffer BoneTransformBuffer : register(b11)
{
    matrix BoneTransforms[256];
}

cbuffer ProgressBuffer : register(b13)
{
    float progress;
    int isRandomFade;
    float2 padding;
}



//=============================================================================
// 頂点シェーダ
//=============================================================================
void VertexShaderPolygon( in  float3 inPosition		: POSITION0,
						  in  float3 inNormal		: NORMAL0,
						  in  float4 inDiffuse		: COLOR0,
						  in  float2 inTexCoord		: TEXCOORD0,
                          in  float3 inTangent      : TANGENT,

						  out float4 outPosition	: SV_POSITION,
                          out float3 outViewPos     : POSITIONT1,
						  out float3 outNormal		: NORMAL0,
						  out float2 outTexCoord	: TEXCOORD0,
						  out float4 outDiffuse		: COLOR0,
						  out float4 outWorldPos    : POSITION0,
                          out float3 outTangent     : TANGENT,
						  out float4 outshadowCoord[LIGHT_MAX_NUM_CASCADE] : TEXCOORD1)
{
	matrix wvp;
    wvp = mul(WorldBuffer.world, View);
	wvp = mul(wvp, Projection);
    outPosition = mul(float4(inPosition, 1.0f), wvp);

    outNormal = normalize(mul(inNormal, (float3x3) WorldBuffer.invWorld)); // w = 0
    outTangent = normalize(mul(inTangent, (float3x3) WorldBuffer.world)); // w = 0
	//outNormal = normalize(mul(float4(inNormal.xyz, 0.0f), World));
    //outTangent = normalize(mul(float4(inTangent.xyz, 0.0f), World));

	outTexCoord = inTexCoord;

    outWorldPos = mul(float4(inPosition, 1.0f), WorldBuffer.world);
    outViewPos = mul(outWorldPos, View).xyz;
    for (int i = 0; i < LIGHT_MAX_NUM_CASCADE; ++i)
    {
        outshadowCoord[i] = mul(outWorldPos, g_CascadeArray[i].lightViewProj);
    }

	outDiffuse = inDiffuse;

}

PixelInputType SkinnedMeshVertexShaderPolygon(SkinnedMeshVertexInputType input)
{
    PixelInputType output;
    matrix boneTransform = 0;

    // Calculate the bone transformation matrix based on the weights and indices
    for (int i = 0; i < 4; ++i)
    {
        if (input.weights[i] > 0)
            boneTransform += input.weights[i] * BoneTransforms[input.boneIndices[i]];
    }
	
    // Calculate the final world position for the vertex
    float4 worldPosition = mul(float4(input.position, 1.0f), boneTransform);
    
    // Transform the vertex position into the homogeneous clip space
    matrix wvp = mul(WorldBuffer.world, View); // Assume World[0] is used for non-skinned objects
    wvp = mul(wvp, Projection);
    output.position = mul(worldPosition, wvp);

    // Normal transformation (only transform by the world matrix part)
    output.normal = normalize(mul(float4(input.normal, 0.0), boneTransform)).xyz;
    output.tangent = normalize(mul(float4(input.tangent, 0.0), boneTransform)).xyz;
    output.bitangent = normalize(mul(float4(input.bitangent, 0.0), boneTransform)).xyz;

    // Pass through the texture coordinates and color
    output.texcoord = input.texcoord;
    output.color = input.color;

    // Compute shadow coordinates for multiple light sources
    output.worldPos = mul(float4(input.position, 1.0f), WorldBuffer.world);
    output.viewPos = mul(output.worldPos, View).xyz;
    for (int j = 0; j < LIGHT_MAX_NUM_CASCADE; ++j)
    {
        output.shadowCoord[j] = mul(output.worldPos, g_CascadeArray[j].lightViewProj);
    }

    return output;
}


//*****************************************************************************
// グローバル変数
//*****************************************************************************
Texture2D g_Texture : register( t0 );
Texture2D g_ShadowMap[LIGHT_MAX_NUM] : register(t1);
Texture2D g_TextureSmall : register(t7);
Texture2D g_LightMap : register(t8);
Texture2D g_NormalMap : register(t9);
Texture2D g_BumpMap : register(t10);
Texture2D g_OpacityMap : register(t11);
Texture2D g_ReflectMap : register(t12);
Texture2D g_TranslucencyMap : register(t13);
SamplerState g_SamplerState : register(s0);
SamplerComparisonState g_ShadowSampler : register(s1);
SamplerState g_SamplerStateOpacity : register(s2);

float ToonLighting(float NdotL)
{
    float levels = 3.0;
    return floor(NdotL * levels) / (levels - 1);
}

float3 ToonShading(float3 normal, float3 lightDir, float3 lightColor)
{
    float NdotL = max(dot(normal, lightDir), 0.0);
    float toonIntensity = ToonLighting(NdotL);
    return toonIntensity * lightColor;
}

float HalfLambert(float NdotL)
{
    return (NdotL * 0.5 + 0.5);
}

float3 HalfLambertToon(float3 normal, float3 lightDir, float3 lightColor)
{
    float NdotL = max(dot(normal, lightDir), 0.0);
    float halfLambert = HalfLambert(NdotL);
    float toonIntensity = ToonLighting(halfLambert);
    return toonIntensity * lightColor;
}

float3 RimLight(float3 normal, float3 viewDir, float rimIntensity, float rimThreshold)
{
    float rim = 1.0 - max(dot(normal, viewDir), 0.0);
    rim = smoothstep(rimThreshold, 1.0, rim);
    return rim * rimIntensity;
}

float3 ApplyRimLight(float3 normal, float3 viewDir, float3 lightColor)
{
    float3 rim = RimLight(normal, viewDir, 1.0, 0.5);
    return rim * lightColor;
}

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

// Perlin Noise
float PerlinNoise(float2 uv)
{
    float2 i = floor(uv);
    float2 f = frac(uv);
    
    float a = Hash(i);
    float b = Hash(i + float2(1.0, 0.0));
    float c = Hash(i + float2(0.0, 1.0));
    float d = Hash(i + float2(1.0, 1.0));

    float2 u = f * f * (3.0 - 2.0 * f);
    
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float GetOpacity(float2 uv)
{
    return g_OpacityMap.Sample(g_SamplerStateOpacity, uv).r;
}

float3 GetBumpNormal(float2 uv, float3 normal, float3 tangent)
{
    float3 bumpNormal = g_BumpMap.Sample(g_SamplerState, uv).rgb;
    
    bumpNormal = normalize(bumpNormal * 2.0 - 1.0);

    float3 bitangent = cross(normal, tangent);

    float3x3 TBN = float3x3(tangent, bitangent, normal);
    
    return normalize(mul(bumpNormal, TBN));
}

int GetCascadeIndex(float viewDepth, float4 splits)
{
    if (viewDepth < splits.x)
        return 0;
    if (viewDepth < splits.y)
        return 1;
    if (viewDepth < splits.z)
        return 2;
    return 3;
}

//=============================================================================
// ピクセルシェーダ
//=============================================================================
void PixelShaderPolygon( in  float4 inPosition		: SV_POSITION,
                         in  float3 inViewPos       : POSITIONT1,
						 in  float3 inNormal		: NORMAL0,
						 in  float2 inTexCoord		: TEXCOORD0,
						 in  float4 inDiffuse		: COLOR0,
						 in  float4 inWorldPos      : POSITION0,
                         in  float3 inTangent       : TANGENT,
						 in float4 inShadowCoord[LIGHT_MAX_NUM_CASCADE] : TEXCOORD1,

						 out float4 outDiffuse		: SV_Target )
{
    float4 color;
	

	if (Material.noTexSampling == 0)
	{
		color = g_Texture.Sample(g_SamplerState, inTexCoord);

		color *= inDiffuse;
	}
	else
	{
		color = inDiffuse;
    }
    
    float opacity = 1.0;
    if (Material.opacityMapSampling)
    {
        float opacity = GetOpacity(inTexCoord);
        clip(opacity - 0.5);
    }

	if (Light.Enable == 0)
	{
		color = color * Material.Diffuse;
	}
	else
	{
		float4 tempColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
		float4 outColor = float4(0.0f, 0.0f, 0.0f, 0.0f);


        for (int i = 0; i < LIGHT_MAX_NUM; i++)
		{
			float3 lightDir;
			float light;
            tempColor = float4(1.0f, 0.0f, 0.0f, 1.0f);
			if (Light.Flags[i].OnOff == 1)
			{
                float4 ambient = color * Material.Diffuse * Light.Ambient[i];
                
                float3 normal = inNormal;
                if (Material.bumpMapSampling)
                    normal = GetBumpNormal(inTexCoord, inNormal, inTangent);
                
                //if (Material.reflectMapSampling)
                //{
                //    float3 reflectVector = reflect(-input.ViewDir, normal);
                //    float3 reflectionColor = g_ReflectMap.Sample(g_SamplerState, reflectVector).rgb;
                //}
                
				if (Light.Flags[i].Type == 1)
				{
					lightDir = normalize(Light.Direction[i].xyz);
                        light = saturate(dot(lightDir, normal));
                    float backlightFactor = saturate(dot(-lightDir, normal));

					light = 0.5 - 0.5 * light;
					tempColor = color * Material.Diffuse * light * Light.Diffuse[i];
                    //tempColor += color * Material.Diffuse * backlightFactor * Light.Diffuse[i];
                    //tempColor = float4(1.0f, 0.0f, 0.0f, 1.0f);
                }
                else if (Light.Flags[i].Type == 2)
				{
					lightDir = normalize(Light.Position[i].xyz - normal);
					light = dot(lightDir, normal);

					tempColor = color * Material.Diffuse * light * Light.Diffuse[i];

					float distance = length(inWorldPos - Light.Position[i]);

					float att = saturate((Light.Attenuation[i].x - distance) / Light.Attenuation[i].x);
					tempColor *= att;
				}
				else
				{
					tempColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
				}
                
                
                float shadowFactor = 1.0f;
                if (Light.Flags[i].Type == 1)
                {
                    // ビュースペース深度を取得（通常は viewPos.z）
                    float viewDepth = inViewPos.z;
                    
                    // カスケードインデックスの決定
                    int cascadeIdx = GetCascadeIndex(viewDepth, g_CascadeSplits);
                    
                    // シャドウ座標の取得
                    float4 shadowCoord = inShadowCoord[cascadeIdx];
                    float2 shadowUV = shadowCoord.xy / shadowCoord.w * 0.5f + 0.5f;
                    shadowUV.y = 1.0f - shadowUV.y; // Y反転

                    float currentDepth = shadowCoord.z / shadowCoord.w;
                    currentDepth -= 0.005f; // バイアス

      //              if (shadowTexCoord.x >= 0.0f && shadowTexCoord.y >= 0.0f &&
						//shadowTexCoord.x <= 1.0f && shadowTexCoord.y <= 1.0f &&
						//currentDepth >= 0.0f && currentDepth <= 1.0f)
      //              {
      //                  tempColor = float4(1.0f, 0.0f, 0.0f, 1.0f);
      //              }
                    //tempColor = float4(currentDepth, currentDepth, currentDepth, 1.0f);
                    //float shadowMapValue = g_ShadowMap[2].Sample(g_SamplerState, shadowTexCoord).r;
                   // tempColor = float4(shadowMapValue, shadowMapValue, shadowMapValue, 1.0f);
                    
                    float2 shadowMapDimensions = float2(SHADOWMAP_SIZE, SHADOWMAP_SIZE);
                    float2 texelSize = 1.0 / shadowMapDimensions;
                    float shadow = 0.0;
                    float totalWeight = 0.0;
                    int kernelSize = 1;
                    
                    if (shadowUV.x >= 0.0f && shadowUV.y >= 0.0f &&
                        shadowUV.x <= 1.0f && shadowUV.y <= 1.0f &&
                        currentDepth >= 0.0f && currentDepth <= 1.0f)
                    {
                        for (int x = -kernelSize; x <= kernelSize; x++)
                        {
                            for (int y = -kernelSize; y <= kernelSize; y++)
                            {
                                float2 offset = float2(x, y) * texelSize;
                                
                                float weight = exp(-(x * x + y * y) / (2.0 * kernelSize * kernelSize)); // gaussian weight
                                float sampled = 0.0f;
                                if (cascadeIdx == 0)
                                    sampled = g_ShadowMap[0].SampleCmpLevelZero(g_ShadowSampler, shadowUV + offset, currentDepth);
                                else if (cascadeIdx == 1)
                                    sampled = g_ShadowMap[1].SampleCmpLevelZero(g_ShadowSampler, shadowUV + offset, currentDepth);
                                else if (cascadeIdx == 2)
                                    sampled = g_ShadowMap[2].SampleCmpLevelZero(g_ShadowSampler, shadowUV + offset, currentDepth);
                                else
                                    sampled = g_ShadowMap[3].SampleCmpLevelZero(g_ShadowSampler, shadowUV + offset, currentDepth);

                                shadow += sampled * weight;
                                totalWeight += weight;
                            }
                        }
                        //tempColor = float4(1.0f, 0.0f, 0.0f, 1.0f);
                    }
                    else
                    {
                        shadow = 1;
                        totalWeight = 1;
                    }

 
                    shadowFactor = shadow / totalWeight + 0.2f;
                }

                tempColor *= shadowFactor;
				
                outColor += tempColor + ambient;
                //outColor = float4(ambient.x, ambient.x, ambient.x, 1.f);

            }
		}

		color = outColor;
        if (Material.opacityMapSampling)
            color.a = opacity;
        else
		    color.a = inDiffuse.a * Material.Diffuse.a;
        
        
        if (isRandomFade == 1)
        {
            float noise = PerlinNoise(inTexCoord * 10.0);

            if (noise > progress)
                discard;

            if (noise > progress - 0.1)
                color.rgb *= 2.0;
        }

    }

	//フォグ
	if (Fog.Enable == 1)
	{
		float z = inPosition.z*inPosition.w;
		float f = (Fog.Distance.y - z) / (Fog.Distance.y - Fog.Distance.x);
		f = saturate(f);
		outDiffuse = f * color + (1 - f)*Fog.FogColor;
		outDiffuse.a = color.a;
	}
	else
	{
		outDiffuse = color;
	}
	
    //if (md.mode == 1)
    //{

    //    //color.a *= 0.5f;
    //    float2 centeredTexcoord = inTexCoord - float2(0.5, 0.5);

    //    float distanceFromCenter = length(centeredTexcoord);
    //    float angle = distanceFromCenter * 5;

    //    float sinAngle = sin(angle);
    //    float cosAngle = cos(angle);
    //    float2 rotatedTexcoord;
    //    rotatedTexcoord.x = centeredTexcoord.x * cosAngle - centeredTexcoord.y * sinAngle;
    //    rotatedTexcoord.y = centeredTexcoord.x * sinAngle + centeredTexcoord.y * cosAngle;

    //    rotatedTexcoord += float2(0.5, 0.5);
		
    //    color = g_TextureSmall.Sample(g_SamplerState, rotatedTexcoord);
    //    outDiffuse = color;
    //    return;

    //}
	
 

	
	//縁取り
	//if (fuchi == 1)
	//{
	//	float angle = dot(normalize(inWorldPos.xyz - Camera.xyz), normalize(inNormal));
	//	//if ((angle < 0.5f)&&(angle > -0.5f))
	//	if (angle > -0.3f)
	//	{
	//		outDiffuse.rb  = 1.0f;
	//		outDiffuse.g = 0.0f;			
	//	}
	//}
}


void SkinnedMeshPixelShader(PixelInputType input,
                            out float4 outDiffuse : SV_Target)
{
    float4 color;
    float4 lightColor;

    if (Material.noTexSampling == 0)
    {
        color = g_Texture.Sample(g_SamplerState, input.texcoord);

        color *= input.color;
        
        if (color.a <= 0.0f)
            discard;

    }
    else
    {
        color = input.color;
    }
	
    //if (Material.lightmapSampling == 1)
    //{
    //    color *= g_LightMap.Sample(g_SamplerState, inTexCoord);
    //}
    
    //  Sample the normal map
    float3 tangentNormal = g_NormalMap.Sample(g_SamplerState, input.texcoord).rgb;

    // Convert sampled normal from [0,1] range to [-1,1] range
    tangentNormal = tangentNormal * 2.0 - 1.0;

    // Construct TBN matrix
    float3x3 TBN = float3x3(input.tangent, input.bitangent, input.normal);

    // Transform the normal from tangent space to world space
    float3 worldNormal = normalize(mul(TBN, tangentNormal));



    if (Light.Enable == 0)
    {
        color = color * Material.Diffuse;
    }
    else
    {
        float4 tempColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
        float4 outColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
        
        float3 viewDir = normalize(CameraPos.xyz - input.position.xyz);

        for (int i = 0; i < LIGHT_MAX_NUM; i++)
        {
            float3 lightDir;
            float light;

            if (Light.Flags[i].OnOff == 1)
            {
                lightDir = normalize(Light.Direction[i].xyz);
                lightColor = Light.Diffuse[i];
                
                float4 ambient = color * Material.Diffuse * Light.Ambient[i];
                if (Light.Flags[i].Type == 1)
                {
                    
                    if (md.mode == 0 || md.mode == 2)
                    {
                        // Half-Lambert Toon
                        float3 toonColor = HalfLambertToon(worldNormal, lightDir, lightColor.xyz);
                    
                        // Rim Light
                        float3 rimColor = ApplyRimLight(worldNormal, viewDir, lightColor.xyz);

                        // Combine lighting effects
                        tempColor = color * ((toonColor + rimColor), 0.9f);
                        tempColor *= 0.5f;

                    }
                    else
                    {
                        lightDir = normalize(Light.Direction[i].xyz);
                        light = saturate(dot(lightDir, input.normal));
                        float backlightFactor = saturate(dot(-lightDir, input.normal));

                        light = 0.5 - 0.5 * light;
                        tempColor = color * Material.Diffuse * light * Light.Diffuse[i];
                    }
                    
                }
                else if (Light.Flags[i].Type == 2)
                {
                    lightDir = normalize(Light.Position[i].xyz - input.worldPos.xyz);
                    light = dot(lightDir, input.normal);

                    tempColor = color * Material.Diffuse * light * Light.Diffuse[i];

                    float distance = length(input.worldPos - Light.Position[i]);

                    float att = saturate((Light.Attenuation[i].x - distance) / Light.Attenuation[i].x);
                    tempColor *= att;
                }
                else
                {
                    tempColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
                }
                
                
                float shadowFactor = 1.0f;
                if (Light.Flags[i].Type == 1) // DirectionalLight
                {
                    // ビュースペース深度を取得（通常は viewPos.z）
                    float viewDepth = input.viewPos.z;
                    
                    // カスケードインデックスの決定
                    int cascadeIdx = GetCascadeIndex(viewDepth, g_CascadeSplits);
                    
                    // シャドウ座標の取得
                    float4 shadowCoord = input.shadowCoord[cascadeIdx];
                    float2 shadowUV = shadowCoord.xy / shadowCoord.w * 0.5f + 0.5f;
                    shadowUV.y = 1.0f - shadowUV.y; // Y反転

                    float currentDepth = shadowCoord.z / shadowCoord.w;
                    currentDepth -= 0.005f; // バイアス
                    
                    
                    float2 shadowMapDimensions = float2(SHADOWMAP_SIZE, SHADOWMAP_SIZE);
                    float2 texelSize = 1.0 / shadowMapDimensions;
                    float shadow = 0.0;
                    float totalWeight = 0.0;
                    int kernelSize = 1;

                    if (shadowUV.x >= 0.0f && shadowUV.y >= 0.0f &&
                        shadowUV.x <= 1.0f && shadowUV.y <= 1.0f &&
                        currentDepth >= 0.0f && currentDepth <= 1.0f)
                    {
                        for (int x = -kernelSize; x <= kernelSize; x++)
                        {
                            for (int y = -kernelSize; y <= kernelSize; y++)
                            {
                                float2 offset = float2(x, y) * texelSize;
                                
                                float weight = exp(-(x * x + y * y) / (2.0 * kernelSize * kernelSize)); // gaussian weight
                                float sampled = 0.0f;
                                if (cascadeIdx == 0)
                                    sampled = g_ShadowMap[0].SampleCmpLevelZero(g_ShadowSampler, shadowUV + offset, currentDepth);
                                else if (cascadeIdx == 1)
                                    sampled = g_ShadowMap[1].SampleCmpLevelZero(g_ShadowSampler, shadowUV + offset, currentDepth);
                                else if (cascadeIdx == 2)
                                    sampled = g_ShadowMap[2].SampleCmpLevelZero(g_ShadowSampler, shadowUV + offset, currentDepth);
                                else
                                    sampled = g_ShadowMap[3].SampleCmpLevelZero(g_ShadowSampler, shadowUV + offset, currentDepth);

                                shadow += sampled * weight;
                                totalWeight += weight;
                            }
                        }
                    }
                    else
                    {
                        shadow = 1;
                        totalWeight = 1;
                    }

 
                    shadowFactor = shadow / totalWeight + 0.2f; // 最終陰影係数（ソフトシャドウ補正）
                }

                if (md.mode != 2)
                    tempColor *= shadowFactor;
				
                outColor += tempColor + ambient;

            }
        }

        color = outColor;
        color.a = input.color.a * Material.Diffuse.a;
        
        
        if (isRandomFade == 1)
        {
            float noise = PerlinNoise(input.texcoord * 10.0);

            if (noise > progress)
                discard;

            if (noise > progress - 0.1)
                color.rgb *= 2.0;
        }

    }

	//フォグ
    //if (Fog.Enable == 1)
    //{
    //    float z = inPosition.z * inPosition.w;
    //    float f = (Fog.Distance.y - z) / (Fog.Distance.y - Fog.Distance.x);
    //    f = saturate(f);
    //    outDiffuse = f * color + (1 - f) * Fog.FogColor;
    //    outDiffuse.a = color.a;
    //}
    //else
    //{
    //    outDiffuse = color;
    //}
	
    //if (md.mode == 1)
    //{

    //    //color.a *= 0.5f;
    //    float2 centeredTexcoord = input.texcoord - float2(0.5, 0.5);

    //    float distanceFromCenter = length(centeredTexcoord);
    //    float angle = distanceFromCenter * 5;

    //    float sinAngle = sin(angle);
    //    float cosAngle = cos(angle);
    //    float2 rotatedTexcoord;
    //    rotatedTexcoord.x = centeredTexcoord.x * cosAngle - centeredTexcoord.y * sinAngle;
    //    rotatedTexcoord.y = centeredTexcoord.x * sinAngle + centeredTexcoord.y * cosAngle;

    //    rotatedTexcoord += float2(0.5, 0.5);
		
    //    color = g_TextureSmall.Sample(g_SamplerState, rotatedTexcoord);
    //    outDiffuse = color;
    //    return;

    //}


    outDiffuse = color;
}