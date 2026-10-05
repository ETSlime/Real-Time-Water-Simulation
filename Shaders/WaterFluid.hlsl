#include "SharedParticleUtil.hlsl"
#include "VoxelCollisionUtil.hlsl"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SLOT_CB_EFFECT_EMIT                 b10

//*****************************************************************************
// 構造体定義
//*****************************************************************************

// 粒子構造体
struct WaterFluidParticle
{
    uint particleID; // パーティクルID（ユニークな識別子）
    float3 position; // 現在位置
    float3 prevPosition; // 前フレーム位置
    float density; // 密度（周囲の粒子から算出）
    

    float3 velocity; // 速度
    float lambda; // 拘束条件解（位置補正のための係数）
    float pressure; // 圧力（密度から算出される）
    
    float3 predictedPosition; // 予測位置（次の位置、拘束前）
    float lifeRemaining; // 寿命（将来拡張用、今は1で固定でも可）

    float4 color; // デバッグ用カラー（密度に応じた可視化など）
    
    float size; // サイズ
};

struct GS_OUTPUT_FLUID
{
    float4 PosH : SV_POSITION;
    float2 TexCoord : TEXCOORD;
    float4 Color : COLOR0;
    float3 ViewDir : TEXCOORD1;
};

struct PS_INPUT_FLUID
{
    float4 PosH : SV_POSITION;
    float2 TexCoord : TEXCOORD;
    float4 Color : COLOR0;
    float3 ViewDir : TEXCOORD1;
};

//*****************************************************************************
// 定数バッファ
//*****************************************************************************

// 水の流体パーティクル用の定数バッファ
cbuffer CBWaterFluid : register(SLOT_CB_EFFECT_PARTICLE)
{
    float g_SupportRadius;
    float g_RestDensity;
    float g_ParticleMass;
    float g_Stiffness;
    float g_Friction;
    float3 waterFluidPadding;
};

// パーティクルエミット用の定数バッファ
cbuffer CBEmit : register(SLOT_CB_EFFECT_EMIT)
{
    uint g_EmitCount;
    float3 g_EmitCenter;
    float g_EmitRadius;
    float3 g_EmitDirection;
    float g_EmitSpeed;
    float3 _padding;
};

//*****************************************************************************
// グローバル変数
//*****************************************************************************

// バッファ
RWStructuredBuffer<WaterFluidParticle> g_ParticlesUAV : register(SLOT_UAV_PARTICLE); // 書き込み用パーティクルデータバッファ
StructuredBuffer<WaterFluidParticle> g_ParticlesSRV : register(SLOT_SRV_PARTICLE); // 読み取り用パーティクルデータバッファ


//=============================================================================
// コンピュートシェーダ
//=============================================================================
[numthreads(THREAD_COUNT, 1, 1)]
void UpdateCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint id = dispatchThreadID.x;
    if (id >= g_MaxParticleCount)
        return;

    WaterFluidParticle p = g_ParticlesUAV[id];
    
    // ==== 生存している場合：更新処理 ====
    if (p.lifeRemaining > 0.0f)
    {
        // --- 予測位置更新（単純な速度積分）
        float3 accel = g_Acceleration;
        p.velocity += accel * g_DeltaTime; // 重力加速度 × Δt
        float3 deltaPos = p.velocity * g_DeltaTime;
        p.predictedPosition = p.position + deltaPos;
        
        // Voxel 座標 → Morton Index
        float4 worldPredicted = mul(float4(p.predictedPosition, 1.0f), g_World);
        uint3 voxelCoord = GetVoxelCoord(worldPredicted.xyz);
        uint mortonIndex = EncodeMorton3(voxelCoord.x, voxelCoord.y, voxelCoord.z);
        
        
        int found = FindIndexTriplet(mortonIndex);
        
        
        if (found >= 0)
        {
            IndexTriplet t = g_IndexTriplets[found];
            
            float closestRayDistance = 1e20f;
            float3 bestHitPoint;
            
            [allow_uav_condition] // ループ展開のためのアトミック制御
            for (uint i = 0; i < t.count; ++i)
            {   
                
                uint triIndex = g_VoxelTriangleIndices[t.startOffset + i];
                TriangleStruct tri = g_Triangles[triIndex];

                float3 hitPoint = float3(0, 0, 0);
                float3 dir = deltaPos;
                
                float4 rayStartWS = mul(float4(p.position, 1.0f), g_World);
                float4 rayDirWS = mul(float4(deltaPos, 0.0f), g_World);
                
                float3 rayDir = normalize(rayDirWS.xyz);
                float rayRange = length(rayDirWS.xyz);
                float3 rayStart = rayStartWS.xyz + rayDir * 0.001f;
                
                
                float rayDistance = 0;
                if (RayIntersectsTriangle(rayStart, rayDir, tri, hitPoint, rayDistance))
                {
                    if (rayDistance <= rayRange && rayDistance < closestRayDistance)
                    {
                        closestRayDistance = rayDistance;
                        bestHitPoint = hitPoint;
                    
                    // --- 簡易的な反射処理っ！（滑る感じで跳ね返す）
                    //float3 n = normalize(tri.normal);
                    //float3 reflected = reflect(p.velocity, n);
                    //p.velocity = reflected * 0.4f;
                    
                    //// predictedPositionを更新だけにして、最後に一括反映
                    //p.predictedPosition = hitPoint + n * 0.01f;
                    
                        p.velocity = 0;
                        p.color = float4(1, 0, 0, 1); // 衝突時は赤くする
                    
                        break;
                    }
                }
            }
        }

        p.position = p.predictedPosition;

        // データ書き戻し
        g_ParticlesUAV[id] = p;
    
        // 活発な粒子を AliveList に登録
        g_AliveListUAV.Append(id);
    }
    else
    {
        // 死亡した場合は FreeList に登録
        g_FreeListUAV.Append(id);
    }


}

//================== 補助関数 ==================
float3 SafeDirectionFromSeed(uint seed)
{
    float x = frac((seed * 16807u) / 4294967296.0f) * 2.0f - 1.0f;
    float y = frac((seed * 48271u) / 4294967296.0f) * 2.0f - 1.0f;
    float z = frac((seed * 69691u) / 4294967296.0f) * 2.0f - 1.0f;

    float3 dir = float3(x, y, z);
    float len = length(dir);

    if (len == 0.0f)
        return float3(0.0f, 1.0f, 0.0f);
    return normalize(dir);
}


float3 RandomInsideUnitSphere(uint seed)
{
    for (int i = 0; i < 3; ++i)
    {
        uint state = seed * 747796405u + 2891336453u;
        state = state * 747796405u + 2891336453u;
        float x = frac(state / 4294967296.0);

        state = state * 747796405u + 2891336453u;
        float y = frac(state / 4294967296.0);

        state = state * 747796405u + 2891336453u;
        float z = frac(state / 4294967296.0);

        x = x * 2.0f - 1.0f;
        y = y * 2.0f - 1.0f;
        z = z * 2.0f - 1.0f;

        float3 v = float3(x, y, z);
        if (length(v) <= 1.0f)
            return normalize(v);

        seed += 17; // 再試行用に乱数種をずらす
    }

    // fallback にほんの少しノイズを足す（絶対 0 にしない）
    return SafeDirectionFromSeed(seed);
}


[numthreads(THREAD_COUNT, 1, 1)]
void EmitCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint id = dispatchThreadID.x;
    if (id >= g_EmitCount)
        return;

    uint slotID = g_FreeListConsumeUAV.Consume();
    WaterFluidParticle p = (WaterFluidParticle) 0; // 明示的に全フィールドを初期化

    //uint seed = asuint(g_TotalTime * 1000.0f) + dispatchThreadID.x;
    //float3 offset = RandomInsideUnitSphere(seed);
    //offset.y = 0;
    //p.position = g_EmitCenter + offset * g_EmitRadius;
    //p.velocity = float3(0, 0, 0);
    
    p.position = g_EmitCenter;
    p.velocity = g_EmitDirection * g_EmitSpeed;
    
    p.predictedPosition = p.position;

    p.density = g_RestDensity;
    p.lambda = 0.0f;
    p.lifeRemaining = g_LifeMax; // 寿命は最大値で初期化

    p.color = g_startColor; // 初期カラー
    p.size = g_Scale;
    

    g_ParticlesUAV[slotID] = p;
    g_AliveListUAV.Append(slotID);
}

//=============================================================================
// 頂点シェーダ：パーティクルIDをジオメトリシェーダーに渡すだけ
//=============================================================================
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // インスタンスIDを使って、実際の粒子インデックスを取得
    output.ParticleID = g_AliveListSRV[input.instanceID];

    return output;
}

//=============================================================================
// ジオメトリシェーダ：粒子をビルボード化して四角形に展開
//=============================================================================
[maxvertexcount(6)]
void GS(point GS_INPUT input[1], inout TriangleStream<GS_OUTPUT_FLUID> stream)
{
    WaterFluidParticle p = g_ParticlesSRV[input[0].ParticleID];

    float3 pos = p.position;
    float size = p.size;

    // ビルボード展開（Viewの1列目と2列目から）
    float3 right = normalize(float3(g_View._11, g_View._21, g_View._31));
    float3 up = normalize(float3(g_View._12, g_View._22, g_View._32));

    // 左上 → 右上 → 右下 → 左下 順に展開
    float3 corners[4] =
    {
        pos + (-right + up) * size,
        pos + (right + up) * size,
        pos + (right - up) * size,
        pos + (-right - up) * size
    };

    float2 texcoords[4] =
    {
        float2(0.0f, 0.0f),
        float2(1.0f, 0.0f),
        float2(1.0f, 1.0f),
        float2(0.0f, 1.0f)
    };

    int triangleIndices[6] =
    {
        0, 1, 2, // 第1三角形
        2, 3, 0 // 第2三角形
    };
    
    for (int i = 0; i < 6; ++i)
    {
        int idx = triangleIndices[i];
        GS_OUTPUT_FLUID o;
        float4 worldPos = mul(float4(corners[idx], 1.0f), g_World);
        o.PosH = mul(worldPos, g_ViewProj);
        o.TexCoord = texcoords[idx];
        o.Color = p.color;
        float3 viewDir = normalize(g_CameraPos - pos);
        o.ViewDir = viewDir;
        stream.Append(o);

        // 3個ごとにRestartStrip()
        if ((i + 1) % 3 == 0)
        {
            stream.RestartStrip();
        }
    }
}

//=============================================================================
// ピクセルシェーダ：ノイズで破れた煙を表現
//=============================================================================
float4 PS(PS_INPUT_FLUID input) : SV_Target
{    
    // UV座標を [-1,1] 範囲に変換
    float2 uv = input.TexCoord * 2.0f - 1.0f; // [0,1] → [-1,1]
    float distSq = dot(uv, uv);

    // 中心からの距離が1.0以上なら破棄（透明化）
    if (distSq > 1.0f)
        discard;

    // 透明度を距離に基づいて計算
    float alpha = smoothstep(1.0f, 0.0f, distSq);
    alpha = saturate(alpha * 0.3);
    float3 baseColor = input.Color.rgb;
    
    // フレネル効果を追加
    float3 normal = float3(0, 0, 1);
    float3 viewDir = normalize(input.ViewDir);
    float fresnel = pow(1.0f - abs(dot(viewDir, normal)), 4.0f);
    float3 finalColor = baseColor + fresnel * 0.5;

    return float4(finalColor, alpha * input.Color.a);
}