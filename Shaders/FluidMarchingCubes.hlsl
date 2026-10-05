#include "MarchingCubesClusterPartition.hlsl"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SLOT_SAMPLER_DEFAULT                    s0

#define SLOT_CB_FLUID_THICKNESS                 b9
#define SLOT_UAV_FLUID_THICKNESS                u7
#define SLOT_SRV_FLUID_THICKNESS                t17

#define SLOT_CB_SPH_VOLUM_PARAMS                b11

#define SLOT_SRV_VOLUME_SCALAR_FIELD            t18
#define SLOT_UAV_VOLUME_SCALAR_FIELD            u7

#define SLOT_SRV_MARCHING_CUBES_VERTEX          t19
#define SLOT_UAV_MARCHING_CUBES_VERTEX          u1
#define SLOT_SRV_MARCHING_CUBES_INDEX           t20
#define SLOT_UAV_MARCHING_CUBES_INDEX           u2
#define SLOT_UAV_MARCHING_CUBES_DRAWARGS        u3
#define SLOT_UAV_MARCHING_CUBES_EDGE_CATCH      u4
#define SLOT_UAV_MARCHING_CUBES_VERTEX_COUNTER  u5

#define SLOT_SRV_EDGE_TABLE                     t25
#define SLOT_SRV_TRI_TABLE                      t26

#define SCALAR_FIELD_CONTRIBUTION_SCALE         65536.0f // スカラー場の値を16ビット整数にスケーリングするための係数

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct VS_INPUT_MARCHING_CUBES
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
};


struct VS_OUTPUT_MARCHING_CUBES
{
    float4 Position : SV_POSITION;
    float3 NormalW : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
    float2 ScreenUV : TEXCOORD2;
};

// マーチングキューブ用の頂点構造体
struct MarchingCubesVertex
{
    float3 position; // 頂点位置
    float3 normal; // 法線ベクトル
};

//struct DrawIndexedInstancedArgs
//{
//    uint IndexCountPerInstance;
//    uint InstanceCount;
//    uint StartIndexLocation;
//    int BaseVertexLocation;
//    uint StartInstanceLocation;
//};

struct DrawInstancedArgs
{
    uint VertexCountPerInstance;
    uint InstanceCount;
    uint StartVertexLocation;
    uint StartInstanceLocation;
};

struct DebugVoxelInfo
{
    uint3 gridCoord; // グリッド座標（ボクセル位置）
    float cornerScalar[8]; // 8つのコーナーのスカラー値（密度など）
    int cubeIndex; // cubeIndex
    bool hasMesh; // メッシュが生成されたかどうか
};

//*****************************************************************************
// 定数バッファ
//*****************************************************************************
cbuffer CBVolumeParams : register(SLOT_CB_SPH_VOLUM_PARAMS)
{
    float3 g_FieldOrigin;
    float g_CellSize;
    uint3 g_GridDim;
    float g_DensityScale;
    float g_IsoRadius;
    float g_InvIsoRadius;
    float2 VolumParamsPadding;
};

cbuffer CBFluidThickness : register(SLOT_CB_FLUID_THICKNESS)
{
    float g_ThicknessPerParticle; // 粒子1つあたりの厚度値
    float2 g_ScreenSize; // スクリーンサイズ
    float thicknessPadding;
};


//*****************************************************************************
// グローバル変数
//*****************************************************************************
Texture2D<uint> g_ThicknessTex : register(SLOT_SRV_FLUID_THICKNESS);
SamplerState g_SamplerLinear : register(SLOT_SAMPLER_DEFAULT);

RWTexture2D<uint> g_ThicknessUAV : register(SLOT_UAV_FLUID_THICKNESS); // 厚度図出力
RWStructuredBuffer<uint> g_ScalarFieldUAV : register(SLOT_UAV_VOLUME_SCALAR_FIELD); // スカラー場（密度）出力
StructuredBuffer<uint> g_ScalarFieldSRV : register(SLOT_SRV_VOLUME_SCALAR_FIELD); // スカラー場（密度）
RWStructuredBuffer<MarchingCubesVertex> g_VertexBufferUAV : register(SLOT_UAV_MARCHING_CUBES_VERTEX); // 頂点バッファ出力
StructuredBuffer<MarchingCubesVertex> g_VertexBufferSRV : register(SLOT_SRV_MARCHING_CUBES_VERTEX); // 頂点バッファ
RWStructuredBuffer<uint> g_IndexBufferUAV : register(SLOT_UAV_MARCHING_CUBES_INDEX); // インデックスバッファ出力
StructuredBuffer<uint> g_IndexBufferSRV : register(SLOT_SRV_MARCHING_CUBES_INDEX); // インデックスバッファ
RWStructuredBuffer<DrawInstancedArgs> g_DrawArgsUAV : register(SLOT_UAV_MARCHING_CUBES_DRAWARGS); // 描画引数
RWStructuredBuffer<int> g_EdgeVertexCacheUAV : register(SLOT_UAV_MARCHING_CUBES_EDGE_CATCH); // EdgeCache: voxelごとの12エッジに対するvertex index記録
RWStructuredBuffer<uint> g_VertexCounterUAV : register(SLOT_UAV_MARCHING_CUBES_VERTEX_COUNTER); // 頂点カウンター（vertex indexの生成用）

StructuredBuffer<uint> g_EdgeTableSRV : register(SLOT_SRV_EDGE_TABLE); // 各キューブインデックスに対するエッジフラグ
StructuredBuffer<int> g_TriTableSRV : register(SLOT_SRV_TRI_TABLE); // 各キューブインデックスに対する三角形の頂点インデックス

RWStructuredBuffer<DebugVoxelInfo> g_DebugVoxelInfoUAV : register(u7);

// 8つのコーナーのオフセット 
static const int3 cornerOffset[8] =
{
    int3(0, 0, 0), int3(1, 0, 0), int3(1, 0, 1), int3(0, 0, 1),
    int3(0, 1, 0), int3(1, 1, 0), int3(1, 1, 1), int3(0, 1, 1)
};

// エッジ接続情報（各エッジの2つのコーナーインデックス）
static const int2 edgeConnection[12] =
{
    int2(0, 1), int2(1, 2), int2(2, 3), int2(3, 0),
    int2(4, 5), int2(5, 6), int2(6, 7), int2(7, 4),
    int2(0, 4), int2(1, 5), int2(2, 6), int2(3, 7)
};

//=============================================================================
// コンピュートシェーダ
//=============================================================================
[numthreads(THREAD_COUNT, 1, 1)]
void FluidThicknessCS(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    // ディスパッチIDから粒子インデックスを取得
    uint aliveID = dispatchThreadId.x;
    // アライブリストから粒子インデックスを取得
    uint particleIndex = g_AliveListSRV[aliveID];
    // パーティクルデータを取得
    WaterFluidParticle p = g_ParticlesUAV[particleIndex];
    float3 pos = p.position;
    float size = p.size;

    // View行列からright/upベクトルを取得
    float3 right = normalize(float3(g_View._11, g_View._21, g_View._31));
    float3 up = normalize(float3(g_View._12, g_View._22, g_View._32));

    // ワールド空間でのビルボード中央と右上コーナーの座標を計算
    float3 cornerWS = mul(float4(pos + (right + up) * size, 1.0f), g_World).xyz;
    float3 centerWS = mul(float4(pos, 1.0f), g_World).xyz;

    // クリップ空間に変換
    float4 clipA = mul(float4(centerWS, 1.0f), g_ViewProj);
    float4 clipB = mul(float4(cornerWS, 1.0f), g_ViewProj);

    if (clipA.w <= 0.0f || clipB.w <= 0.0f)
        return;
    
    // w=0 or 負は無効（除算を防ぐ）
    if (clipA.w <= 1e-6f || clipB.w <= 1e-6f)
        return;

    // NDC 変換
    float2 ndcA = clipA.xy / clipA.w;
    float2 ndcB = clipB.xy / clipB.w;

    // NDC [-1,1] → [0,1]
    float2 uv = ndcA * 0.5f + 0.5f;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return;
    
    // スクリーン空間に変換
    float2 screenPos = uv * g_ScreenSize;
    float2 screenCorner = (ndcB * 0.5f + 0.5f) * g_ScreenSize;
    
    // 粒子の半径をピクセル単位で計算
    float radius = length(screenCorner - screenPos);
    // 整数に丸めた半径
    int radiusPixels = int(radius + 0.5f);
    
    // 厚度を加算する
    [allow_uav_condition]
    for (int dy = -radiusPixels; dy <= radiusPixels; ++dy)
    {
        [allow_uav_condition]
        for (int dx = -radiusPixels; dx <= radiusPixels; ++dx)
        {
            int2 ipos = int2(screenPos) + int2(dx, dy);
            
            // 範囲外はスキップ
            if (ipos.x < 0 || ipos.y < 0 || ipos.x >= (int) g_ScreenSize.x || ipos.y >= (int) g_ScreenSize.y)
                continue;

            float2 offset = float2(dx, dy);
            float dist2 = dot(offset, offset);
            
            // 円形の範囲内のみ影響を与える
            if (dist2 > radius * radius)
                continue;
            
            // 厚度を累加（簡易に一定値）
            InterlockedAdd(g_ThicknessUAV[ipos], asuint(g_ThicknessPerParticle));
        }
    }
}

[numthreads(THREAD_COUNT, 1, 1)]
void GenerateScalarFieldCSS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint id = dispatchThreadID.x;
    if (id >= g_MaxParticleCount)
        return;

    WaterFluidParticle p = g_ParticlesSRV[id];
    float3 pos = p.position;
    
    // グリッド座標（粒子位置から）
    float3 rel = pos - g_FieldOrigin;
    uint3 gridCoord = (uint3) (rel / g_CellSize);
    
    // 近傍の 3x3x3 セルを走査
   [allow_uav_condition]
    for (int z = -1; z <= 1; ++z)
    {
        [allow_uav_condition]
        for (int y = -1; y <= 1; ++y)
        {
            [allow_uav_condition]
            for (int x = -1; x <= 1; ++x)
            {
                int3 neighborCoordInt = int3(gridCoord) + int3(x, y, z);
                if (any(neighborCoordInt < 0)) // 負の座標は無視
                    continue;
                
                // モートンキーを生成し、Page HeaderからTripletを取得
                uint3 neighborCoord = uint3(neighborCoordInt);
                uint2 morton = EncodeMorton3_64_GPU(neighborCoord.x, neighborCoord.y, neighborCoord.z);
                
                int found = FindGridPageHeader(morton);
                if (found < 0)
                    continue;
                
                SPHIndexTriplet cell = g_IndexTripletsSRV[found];
                
                // セル内の全粒子を評価
                [allow_uav_condition]
                for (uint i = 0; i < cell.count; ++i)
                {
                    uint neighborID = g_SortedIndicesSRV[cell.startOffset + i];
                    WaterFluidParticle q = g_ParticlesSRV[neighborID];

                    float3 r = (g_FieldOrigin + ((float3)neighborCoordInt + 0.5f) * g_CellSize) - q.position;
                    float dist2 = dot(r, r);
                    if (dist2 >= g_IsoRadius * g_IsoRadius)
                        continue;
                    
                    float dist = sqrt(dist2);
                    float q_norm = dist * g_InvIsoRadius;

                    float w = pow(1.0f - q_norm, 4.0f) * (1.0f + 4.0f * q_norm); // Wendland C2
                    float contribution = w * g_DensityScale;
                    
                    
                    // スカラー場に蓄積（intにスケーリング）
                    float scaled = contribution * SCALAR_FIELD_CONTRIBUTION_SCALE;
                    uint intScaled = (uint) (scaled + 0.5f);

                    // flatIndexの計算（前提: active voxel は dense buffer に詰められている）
                    uint flatIndex = found;
                    InterlockedAdd(g_ScalarFieldUAV[flatIndex], intScaled);
                }
            }
        }
    }
}

[numthreads(THREAD_COUNT, 1, 1)]
void GenerateScalarFieldCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint id = dispatchThreadID.x;
    if (id >= g_MaxParticleCount)
        return;

    WaterFluidParticle p = g_ParticlesSRV[id];

    float radius2 = g_IsoRadius * g_IsoRadius;
    float invRadius = 1.0f / g_IsoRadius;

    float3 minPos = p.position - g_IsoRadius;
    float3 maxPos = p.position + g_IsoRadius;

    int3 minVoxel = floor((minPos - g_FieldOrigin) / g_CellSize);
    int3 maxVoxel = floor((maxPos - g_FieldOrigin) / g_CellSize);

    [loop]
    for (int x = minVoxel.x; x <= maxVoxel.x; ++x)
        for (int y = minVoxel.y; y <= maxVoxel.y; ++y)
            for (int z = minVoxel.z; z <= maxVoxel.z; ++z)
            {
                int3 coord = int3(x, y, z);
                float3 voxelCenter = g_FieldOrigin + (coord + 0.5f) * g_CellSize;

                float3 diff = p.position - voxelCenter;
                float dist2 = dot(diff, diff);
                if (dist2 > radius2)
                    continue;

                float q = sqrt(dist2) * invRadius;
                float w = pow(1.0 - q, 4.0) * (1.0 + 4.0 * q); // Wendland C2

                uint scalarInt = (uint) (w * g_DensityScale * SCALAR_FIELD_CONTRIBUTION_SCALE + 0.5f);

                uint2 morton = EncodeMorton3_64_GPU(coord.x, coord.y, coord.z);
                int found = FindGridPageHeader(morton);

                if (found >= 0)
                {
                    InterlockedAdd(g_ScalarFieldUAV[found], scalarInt);
                }
            }
}


//========================= ユーティリティ関数 =========================
uint FlattenIndex(uint3 coord, uint3 dim)
{
    return coord.x + coord.y * dim.x + coord.z * dim.x * dim.y;
}

uint GetEdgeCacheIndex(uint3 voxelCoord, int edgeIdx, uint3 dim)
{
    return (((voxelCoord.z * dim.y + voxelCoord.y) * dim.x + voxelCoord.x) * 12 + edgeIdx);
}

float3 Interpolate(float3 p1, float3 p2, float v1, float v2)
{
    float denom = v2 - v1;
    float t = (abs(denom) < 1e-6f) ? 0.5f : saturate((g_IsoLevel - v1) / denom);
    return lerp(p1, p2, t);
}

[numthreads(THREAD_COUNT, 1, 1)]
void MarchingCubesCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // 現在の活性ボクセルインデックスを取得
    if (dispatchThreadID.x >= g_NumActiveVoxels)
        return;
    
    DebugVoxelInfo debugInfo = (DebugVoxelInfo) 0;

    // 64bit Morton を 3D 座標に戻す（marching cubes 基点）
    uint3 id = g_IndexTripletsSRV[dispatchThreadID.x].gridCoord;
    debugInfo.gridCoord = id;
    
    // 8個のコーナースカラー値（uint から float に復元）
    float corner[8];
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        uint3 coord = id + cornerOffset[i];
        uint2 morton = EncodeMorton3_64_GPU(coord.x, coord.y, coord.z);
        int found = FindGridPageHeader(morton);
        if (found < 0)
        {
            corner[i] = 0.0f; // 無効ボクセルの場合は0
            debugInfo.cornerScalar[i] = 0.0f;
        }
        else
        {
            // スカラー値を整数から float にスケーリングして復元
            float value = (float) (g_ScalarFieldSRV[found]) / SCALAR_FIELD_CONTRIBUTION_SCALE;
            corner[i] = value;
            debugInfo.cornerScalar[i] = value;
            //corner[i] = (float) (g_ScalarFieldSRV[found]) / SCALAR_FIELD_CONTRIBUTION_SCALE;
        }
    }

    // cube index の構築
    int cubeIndex = 0;
    [unroll]
    for (int j = 0; j < 8; ++j)
        if (corner[j] < g_IsoLevel)
            cubeIndex |= (1 << j);
    
    debugInfo.cubeIndex = cubeIndex;

    int edgeFlags = g_EdgeTableSRV[cubeIndex];
    debugInfo.hasMesh = (edgeFlags != 0);
    g_DebugVoxelInfoUAV[dispatchThreadID.x] = debugInfo;
    
    if (edgeFlags == 0)
        return;
    
    // クラスタIDとリマップIDを取得
    uint rawLabel = g_VoxelClusterIndexSRV[dispatchThreadID.x];
    uint compactID = g_ClusterRemapSRV[rawLabel];
    if (compactID == 0xFFFFFFFF)
        return; // 無効クラスタなのでスキップ

    // 頂点補間とキャッシュによる再利用
    int edgeVertexIndex[12];
    [unroll]
    for (int k = 0; k < 12; ++k)
    {
        edgeVertexIndex[k] = -1;
        
        if ((edgeFlags & (1 << k)) == 0)
            continue;

        int a = edgeConnection[k][0];
        int b = edgeConnection[k][1];

        float3 p1 = g_FieldOrigin + g_CellSize * (id + cornerOffset[a]);
        float3 p2 = g_FieldOrigin + g_CellSize * (id + cornerOffset[b]);
        float v1 = corner[a];
        float v2 = corner[b];

        //uint clusterID = g_VoxelClusterIndexSRV[dispatchThreadID.x];
        //uint3 edgeCacheMin = g_ClusterMinSRV[clusterID];
        //uint3 edgeCacheDim = g_ClusterMaxSRV[clusterID];
        uint3 edgeCacheMin = g_ClusterMinSRV[compactID];
        uint3 edgeCacheMax = g_ClusterMaxSRV[compactID];
        
        uint3 edgeCacheDim = edgeCacheMax - edgeCacheMin + 1;

        uint3 localVoxelCoord = id - edgeCacheMin;
        uint cacheIdx = GetEdgeCacheIndex(localVoxelCoord, k, g_GridDim);
        
        int cached = g_EdgeVertexCacheUAV[cacheIdx];
        if (cached >= 0)
        {
            edgeVertexIndex[k] = cached;
        }
        else
        {
            float3 pos = Interpolate(p1, p2, v1, v2);
            
            MarchingCubesVertex v;
            
            v.position = pos;
            v.normal = float3(0, 1, 0); // 必要なら後で法線計算を追加
            
            uint vertexIdx;
            InterlockedAdd(g_VertexCounterUAV[0], 1, vertexIdx);
            g_VertexBufferUAV[vertexIdx] = v;
            
            int oldValue;
            InterlockedCompareExchange(g_EdgeVertexCacheUAV[cacheIdx], vertexIdx, -1, oldValue);
            edgeVertexIndex[k] = (oldValue == -1) ? vertexIdx : oldValue;

            //g_EdgeVertexCacheUAV[cacheIdx] = vertexIdx;
            //edgeVertexIndex[k] = vertexIdx;
        }
    }
    
    // トライアングル構築（三角形インデックスを格納）
    [loop]
    for (int ii = 0; g_TriTableSRV[cubeIndex * 16 + ii] != -1; ii += 3)
    {
        uint baseIndex;
        InterlockedAdd(g_DrawArgsUAV[0].VertexCountPerInstance, 3, baseIndex);

        [unroll]
        for (int jj = 0; jj < 3; ++jj)
        {
            int edgeIdx = g_TriTableSRV[cubeIndex * 16 + ii + jj];
            uint vertexIdx = edgeVertexIndex[edgeIdx];
            g_IndexBufferUAV[baseIndex + jj] = vertexIdx;
        }
    }

    // DrawArgs を設定（1回だけでも安全）
    g_DrawArgsUAV[0].InstanceCount = 1;
}

//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUTPUT_MARCHING_CUBES VS(VS_INPUT_MARCHING_CUBES input)
{
    VS_OUTPUT_MARCHING_CUBES output;
    
    float4 worldPos = mul(float4(input.Position, 1.0f), g_World);
    output.Position = mul(worldPos, g_ViewProj);

    output.NormalW = mul(float4(input.Normal, 0.0f), g_World).xyz;
    output.ViewDir = g_CameraPos - worldPos.xyz;

    output.ScreenUV = output.Position.xy / output.Position.w;
    output.ScreenUV = output.ScreenUV * 0.5f + 0.5f;

    return output;
}

VS_OUTPUT_MARCHING_CUBES VS_SRV_DECODE(uint vertexID : SV_VertexID)
{
    VS_OUTPUT_MARCHING_CUBES output;
    
    // インデックスバッファから参照される頂点IDを取得
    uint index = g_IndexBufferSRV[vertexID];
    
    // 頂点データを読み出し
    MarchingCubesVertex v = g_VertexBufferSRV[index];

    // ワールド座標系へ変換
    float4 worldPos = mul(float4(v.position, 1.0f), g_World);
    output.Position = mul(worldPos, g_ViewProj);

    // ワールド法線方向
    output.NormalW = mul(float4(v.normal, 0.0f), g_World).xyz;

    // 視線方向
    output.ViewDir = g_CameraPos - worldPos.xyz;

    // スクリーンUV（NDC → [0,1]）
    output.ScreenUV = output.Position.xy / output.Position.w;
    output.ScreenUV = output.ScreenUV * 0.5f + 0.5f;

    return output;
}

//-----------------------------------------------------------------------------
// 厚度 + フレネル + アルファ合成用 PS
//-----------------------------------------------------------------------------
float4 PS(VS_OUTPUT_MARCHING_CUBES input) : SV_TARGET
{
    // 法線と視線方向を正規化
    float3 N = normalize(input.NormalW);
    float3 V = normalize(input.ViewDir);
    
    // フレネル効果
    float fresnel = pow(1.0f - abs(dot(N, V)), g_FresnelPower);

    // 厚度テクスチャから値を取得（整数形式）
    int2 pixelCoord = int2(input.ScreenUV * g_MCScreenSize);
    uint thicknessInt = g_ThicknessTex.Load(int3(pixelCoord, 0)).r;
    float thickness = float(thicknessInt) * g_ThicknessNormalizeFactor;

    // 厚度に応じた透明度と色の補間
    float t = saturate(thickness * g_ThicknessScale + g_ThicknessBias);
    float thicknessAlpha = smoothstep(0.0f, 1.0f, t);

    float3 thicknessColor = lerp(g_ShallowColor, g_DeepColor, 1.0f - thicknessAlpha);

    // 色にフレネルを加算
    float3 finalColor = thicknessColor + fresnel * 0.3f;

    // アルファ（厚度ベース、カスタムスケーリング）
    float alpha = g_AlphaScale * pow(thicknessAlpha, 1.5f);
    return float4(finalColor, alpha);
}
