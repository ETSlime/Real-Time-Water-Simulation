//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SLOT_CB_VIEW_MATRIX                 b1
#define SLOT_CB_PROJECTION_MATRIX           b2
#define SLOT_CB_CAMERA_POS                  b7
#define SLOT_TEX_DIFFUSE                    t0
#define SLOT_SAMPLER_DEFAULT                s0

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct VS_INPUT
{
    float3 position : POSITION; // ローカル頂点位置（-0.5～+0.5 の平面）

    float3 instancePos : INSTANCEPOS; // インスタンスの中心位置（ワールド空間）
    float size : SCALE; // スケール（正方形の一辺）

    float3 normal : NORMAL; // 法線（貼り付け方向）
    float lifetime : LIFETIME; // 寿命（秒）

    float2 uvOffset : UVOFFSET; // UVアトラス or 回転オフセット
    float elapsed : ELAPSED; // 経過時間
    float rotation : ROTATION; // 回転（ラジアン）
    
    float alpha : ALPHA; // 透明度（0.0～1.0）
    float3 padding : PADDING; // パディング
};


struct VS_OUTPUT
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
    float elapsed : TEXCOORD1;
    float lifetime : TEXCOORD2;
    float alpha : TEXCOORD3;
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

cbuffer CameraPosBuffer : register(SLOT_CB_CAMERA_POS)
{
    float4 g_CameraPos;
}

//*****************************************************************************
// グローバル変数
//*****************************************************************************
Texture2D g_DiffuseTexture : register(SLOT_TEX_DIFFUSE);
SamplerState g_SamplerState : register(SLOT_SAMPLER_DEFAULT); // サンプラーステート


float3x3 RotationAroundAxis(float3 axis, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    float t = 1.0 - c;

    float x = axis.x;
    float y = axis.y;
    float z = axis.z;

    return float3x3(
        t * x * x + c, t * x * y - s * z, t * x * z + s * y,
        t * x * y + s * z, t * y * y + c, t * y * z - s * x,
        t * x * z - s * y, t * y * z + s * x, t * z * z + c
    );
}

//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // ノーマルベクトル
    float3 up = normalize(input.normal);
    float3 arbitrary = abs(up.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 right = normalize(cross(arbitrary, up));
    float3 forward = cross(up, right);
    
    float3x3 basis = float3x3(right, up, forward);

    // ローカルXZ平面での回転
    float s = sin(input.rotation);
    float c = cos(input.rotation);
    float3x3 inPlaneRot = RotationAroundAxis(up, input.rotation);
    
    // 頂点を回転 + スケール
    float3 local = input.position * input.size;
    float3 rotated = mul(local, inPlaneRot);
    float3 offset = mul(rotated, basis);
    
    // プレイヤーが見ている方向と法線の方向に応じて、オフセットの方向を調整
    float3 toCamera = normalize(g_CameraPos.xyz - (input.instancePos + offset));
    float dotView = dot(up, toCamera);
    
    // 法線と視線の内積が負なら法線方向にオフセット、正なら法線と逆方向にオフセット
    float directionSign = dotView > 0 ? 1.0f : -1.0f;
    
    // 最終ワールド位置
    float3 worldPos = input.instancePos + offset + up * (0.1f * directionSign); // 法線方向に微小オフセット（Z-fighting防止）

    // UVオフセット（4x2テクスチャアトラス）
    float2 tileSize = float2(1.0f / 4.0f, 1.0f / 2.0f);

    // [-0.5 ~ +0.5] のローカル頂点位置を [0 ~ 1] に変換
    float2 localUV = input.position.xz + float2(0.5f, 0.5f);

    // UVをアトラスの中の1区画にスケール
    float2 atlasUV = localUV * tileSize;

    // オフセットを加えて最終UVに
    output.uv = atlasUV + input.uvOffset;

    // View * Projection
    float4 viewPos = mul(float4(worldPos, 1.0f), g_View);
    output.posH = mul(viewPos, g_Projection);
    
    // 寿命と経過時間を渡す
    output.lifetime = input.lifetime;
    output.elapsed = input.elapsed;
    
    // 透明度を渡す
    output.alpha = input.alpha;
    
    return output;
}

//=============================================================================
// ピクセルシェーダ
//=============================================================================

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 texColor = g_DiffuseTexture.Sample(g_SamplerState, input.uv);
    
    // アルファフェード（寿命に応じた透明度）
    float fadeAlpha = saturate(1.0f - input.elapsed / input.lifetime);
    
    // 初期透明度と乗算して最終的なアルファ値にする
    texColor.a *= fadeAlpha * 0.1f;

    return texColor;
}