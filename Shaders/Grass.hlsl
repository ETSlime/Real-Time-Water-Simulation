#define WindTiling  float2(0.08f, 0.08f) // 相位の空間周波数
#define BendAngleMaxRad  0.15f // 小回転（扇形曲げ）強度
#define BaseSpeed        0.45f

// 陣風（振幅エンベロープ）用の超低周波ノイズ
// 256×256固定テクスチャを Load+手動バイリニアで参照
#define GustTiling      float2(0.004f, 0.004f) // さらに長い波長
#define GustScroll      0.06f // 陣風の移動速度
#define GustAmount      0.95f // 0..1：振幅にどれだけ効かせるか

//*****************************************************************************
// 定数バッファ
//*****************************************************************************

struct WorldMatrixBuffer
{
    matrix world;
    matrix invWorld;
};

struct LightViewProjBuffer
{
    matrix ViewProj[5];
    int LightIndex;
    int padding[3];
};

// マトリクスバッファ
cbuffer WorldBuffer : register(b0)
{
    WorldMatrixBuffer WorldBuffer;
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
    float3 Position : POSITION; // 頂点位置
    float3 Normal : NORMAL; // 法線
    float2 TexCoord : TEXCOORD; // テクスチャ座標
    float Weight : TEXCOORD1; // 風影響重み
    float3 Tangent : TANGENT;

    float3 OffsetPosition : POSITION1; // インスタンス位置オフセット
    float4 Rotation : TEXCOORD2; // クォータニオンでの回転
    float4 initialBillboardRot : TEXCOORD3; // 初期ビルボード回転角度
    float Scale : TEXCOORD4; // インスタンススケール
    float Type : TEXCOORD5; // インスタンスタイプ
};


struct VS_OUTPUT
{
    float4 Position : SV_POSITION;  // 出力位置 (クリップ空間)
    float2 TexCoord : TEXCOORD;     // テクスチャ座標
    float4 ShadowCoord : TEXCOORD1; // 影計算用座標
    float3 Normal : NORMAL;         // 出力法線
};

//*****************************************************************************
// グローバル変数
//*****************************************************************************
Texture2D DiffuseTexture : register(t0);
Texture2D NoiseTexture : register(t7); // ノイズテクスチャ (風アニメーション用)
Texture2D g_NormalMap : register(t9);
Texture2D g_BumpMap : register(t10);
Texture2D g_OpacityMap : register(t11);
Texture2D g_ReflectMap : register(t12);
Texture2D g_TranslucencyMap : register(t13);
Texture2D ShadowMap : register(t2);
SamplerState SampleType : register(s0); // サンプラーステート
SamplerComparisonState ShadowSampler : register(s1);


// クォータニオン回転（ベクトル用）
//------------------------------------------------------------
// v' = v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v)
float3 RotateByQuat(float3 v, float4 q)
{
    return v + 2.0f * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

// ロドリゲスの回転公式（軸周りに角度回転）
float3 RotateAroundAxis(float3 p, float3 axis, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    float3 t = cross(axis, p);
    float d = dot(axis, p);
    return p * c + t * s + axis * d * (1.0f - c);
}

// ノイズ（VSではLoadのみのため、手動バイリニア補間）
//   - texサイズ256固定 → 0..255 にマッピング
//   - fracでラップ、4サンプルで平滑化
float SampleNoiseBilinear256(float2 uv01)
{
    float2 tex = frac(uv01) * 255.0f; // 0..255
    int2 p = (int2) floor(tex);
    float2 f = frac(tex);

    int2 p00 = clamp(p, int2(0, 0), int2(255, 255));
    int2 p10 = clamp(p + int2(1, 0), int2(0, 0), int2(255, 255));
    int2 p01 = clamp(p + int2(0, 1), int2(0, 0), int2(255, 255));
    int2 p11 = clamp(p + int2(1, 1), int2(0, 0), int2(255, 255));

    float n00 = NoiseTexture.Load(int3(p00, 0)).r;
    float n10 = NoiseTexture.Load(int3(p10, 0)).r;
    float n01 = NoiseTexture.Load(int3(p01, 0)).r;
    float n11 = NoiseTexture.Load(int3(p11, 0)).r;

    float nx0 = lerp(n00, n10, f.x);
    float nx1 = lerp(n01, n11, f.x);
    return lerp(nx0, nx1, f.y); // [0,1]
}

// 安定ハッシュ（インスタンスごとに固定の乱数を得る：0..1）
// ※ OffsetPosition.xz に基づく → カメラや頂点に依らない
float Hash01(float2 v)
{
    // 有名な一行ハッシュ
    float h = dot(v, float2(127.1f, 311.7f));
    return frac(sin(h) * 43758.5453f);
}

//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUTPUT VS(VS_INPUT input)
{
    matrix WorldViewProjection;
    WorldViewProjection = mul(WorldBuffer.world, View);
    WorldViewProjection = mul(WorldViewProjection, Projection);
    
    VS_OUTPUT output;
    
    // クォータニオンを使用して回転を適用
    float4 q = input.Rotation;
    float3 rotatedLocal = RotateByQuat(input.Position, q);
    
    // スケールを適用
    rotatedLocal *= input.Scale;
    
    // ローカル法線 → 回転（等方スケールのためそのまま）
    float3 nLocal = RotateByQuat(input.Normal, q);
    
    // ワールド空間の頂点（インスタンス原点で平行移動）
    float3 worldPos = rotatedLocal + input.OffsetPosition;
    
    
    float2 worldXZ = input.OffsetPosition.xz;

    // 個体差：各株で位相オフセット＆微妙な周波数差を付与
    float rndPhase = Hash01(worldXZ); // 0..1
    float rndFreq = Hash01(worldXZ.yx); // 0..1
    float phase0 = dot(worldXZ, WindTiling)
                     + Time * (BaseSpeed * lerp(0.9f, 1.12f, rndFreq))
                     + rndPhase * 6.2831853f;

    // 陣風：超低周波ノイズで「振幅」をゆっくり上下させる
    float2 gustUV = worldXZ * GustTiling + Time * GustScroll;
    float gustN = SampleNoiseBilinear256(gustUV); // 0..1
    float ampScale = lerp(1.0f - GustAmount, 1.0f, gustN);
    // 例：GustAmount=0.45 → 振幅が 0.55..1.0 の間でゆっくり呼吸する

    // 最終角度：根0→先端1、低周波ベース＋陣風包絡
    float bendWeight = saturate(input.Weight);
    float angle = bendWeight * (BendAngleMaxRad * ampScale) * sin(phase0);
    // 角度はスケールを掛けない（巻き込み防止）
    
    // 回転軸：風向とYの直交（横振り）
    float3 windDirWS = normalize(WindDirection);
    float3 axisWS = normalize(float3(-windDirWS.z, 0.0f, windDirWS.x));

    // ピボット＝株の原点（OffsetPosition）
    float3 vecFromPivot = worldPos - input.OffsetPosition;
    float3 rotatedAround = RotateAroundAxis(vecFromPivot, axisWS, angle);
    float3 deformedWorld = rotatedAround + input.OffsetPosition;

    // 法線も同じ小角回転（近似）
    float3 nWS = RotateAroundAxis(nLocal, axisWS, angle);
    nWS = normalize(nWS);

    // Type 分岐（0=草のみ風適用）
    float3 finalPosWS = (input.Type < 0.5f) ? deformedWorld : worldPos;
    float3 finalNrmWS = (input.Type < 0.5f) ? nWS : nLocal;

    output.Position = mul(float4(finalPosWS, 1.0f), WorldViewProjection);
    output.ShadowCoord = mul(float4(finalPosWS, 1.0f), lightViewProj.ViewProj[0]);
    output.TexCoord = input.TexCoord;
    output.Normal = finalNrmWS;
    
    return output;
}

//=============================================================================
// ピクセルシェーダ
//=============================================================================
float CalculateShadow(float4 shadowCoord)
{
    float2 shadowTexCoord = shadowCoord.xy / shadowCoord.w;
    float depth = shadowCoord.z / shadowCoord.w;
    if (shadowTexCoord.x < 0.0 || shadowTexCoord.x > 1.0 || shadowTexCoord.y < 0.0 || shadowTexCoord.y > 1.0)
        return 1.0;
    return ShadowMap.SampleCmpLevelZero(ShadowSampler, shadowTexCoord, depth);
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 baseColor = DiffuseTexture.Sample(SampleType, input.TexCoord);
    
    if (baseColor.a == 0.0f)
        discard;
    
    float3 lightDir = normalize(float3(0.5, -1.0, 0.5));
    float NdotL = saturate(dot(input.Normal, -lightDir));
    float shadowFactor = CalculateShadow(input.ShadowCoord);
    float3 lighting = baseColor.rgb * (0.3 + 0.7 * NdotL * shadowFactor);
    return float4(lighting, baseColor.a);
}


//----------------------------------
// シャドウデプスパス (HLSL)
//----------------------------------

//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUTPUT VSShadow(VS_INPUT input)
{
    VS_OUTPUT output;

    //// 影パスではパフォーマンス向上のため風偏移を省略可能
    //float4 a = float4((input.Position * input.Scale + input.OffsetPosition), 1.f); // 最終頂点位置計算

    // ライト空間への変換
    output.Position = mul(float4((input.Position * input.Scale + input.OffsetPosition), 1.f), lightViewProj.ViewProj[0]);
    return output;
}