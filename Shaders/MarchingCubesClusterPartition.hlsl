#include "SPHSystem.hlsl"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SLOT_CB_FLUID_SURFACE                       b12     // 定数バッファ（流体表面用）
#define SLOT_SRV_VOXEL_CLUSTER_LABELS               t21     // ラベル伝播用のバッファ（読み取り用）
#define SLOT_UAV_VOXEL_CLUSTER_LABELS               u0      // ラベル伝播用のバッファ（書き込み用）

#define SLOT_SRV_VOXEL_CLUSTER_INDEX                t27     // voxelID → clusterID のマップ（読み取り用）
#define SLOT_UAV_VOXEL_CLUSTER_INDEX                u1      // voxelID → clusterID のマップ（書き込み用）

#define SLOT_SRV_CLUSTER_AABB_MIN                   t22     // clusterID → AABB最小（読み取り用）
#define SLOT_UAV_CLUSTER_AABB_MIN                   u2      // clusterID → AABB最小（書き込み用）

#define SLOT_SRV_CLUSTER_AABB_MAX                   t23     // clusterID → AABB最大（読み取り用）
#define SLOT_UAV_CLUSTER_AABB_MAX                   u3      // clusterID → AABB最大（書き込み用）

#define SLOT_SRV_CLUSTER_COUNT                      t24     // クラスタごとの書き込み回数（読み取り用）
#define SLOT_UAV_CLUSTER_COUNT                      u4      // クラスタごとの書き込み回数（書き込み用）

#define SLOT_UAV_CLUSTER_REMAP                      u5      // クラスタIDのリマップ（書き込み用）
#define SLOT_SRV_CLUSTER_REMAP                      t28     // クラスタIDのリマップ（読み取り用）

#define SLOT_UAV_CLUSTER_VALID_COUNTER              u6      // 有効なクラスタ数のカウンタ（書き込み用）

#define SLOT_SRV_CLUSTER_ISVALID_FLAG               t29     // 各ラベルが有効かどうかのフラグ（読み取り用）
#define SLOT_UAV_CLUSTER_ISVALID_FLAG               u7

#define SLOT_UAV_REMAP_PREFIX_SUM                   u2      // ラベルごとのPrefixSum（局所、書き込み用）
#define SLOT_SRV_REMAP_PREFIX_SUM                   t30     // ラベルごとのPrefixSum（局所）

#define SLOT_UAV_REMAP_LOCAL_SUMS                   u3      // 各スレッドグループの合計（書き込み用）   
#define SLOT_SRV_REMAP_LOCAL_SUMS                   t31     // 各スレッドグループの合計（読み取り用）

#define SLOT_UAV_REMAP_GLOBAL_OFFSETS               u4      // グローバルオフセット（書き込み用）
#define SLOT_SRV_REMAP_GLOBAL_OFFSETS               t32     // グローバルオフセット（読み取り用）

#define SLOT_CB_CLUSTER_REMAP                       b13     // クラスタリマップ用の定数バッファ

//*****************************************************************************
// 定数バッファ
//*****************************************************************************
cbuffer CBMarchingCubes : register(SLOT_CB_FLUID_SURFACE)
{
    float3 g_ShallowColor;
    float g_FresnelPower;
    
    float3 g_DeepColor;
    float g_AlphaScale;
    
    float g_ThicknessScale; // 厚度強度の倍率
    float g_ThicknessBias; // 最小透明度調整用
    float g_ThicknessNormalizeFactor; // 厚度の正規化係数（テクスチャの最大値で割る）
    float g_IsoLevel;
    float2 g_MCScreenSize;
    float2 g_InvScreenSize;
    
    uint g_NumActiveVoxels; // アクティブなボクセルの数
    float3 padding;
    //uint3 g_EdgeCacheDim; // エッジキャッシュの次元
}

cbuffer CBPrefixSumInfo : register(SLOT_CB_CLUSTER_REMAP)
{
    uint g_NumGroups; // g_NumGroups（Dispatch 時の Group 数）
    float3 prefixSumPadding; // 16バイトアライメントのためのパディング
};

//*****************************************************************************
// グローバル変数
//*****************************************************************************

RWStructuredBuffer<uint> g_LabelBufferUAV : register(SLOT_UAV_VOXEL_CLUSTER_LABELS);
StructuredBuffer<uint> g_LabelBufferSRV : register(SLOT_SRV_VOXEL_CLUSTER_LABELS);

// voxelID → clusterID のマップ（反向索引）
RWStructuredBuffer<uint> g_VoxelClusterIndexUAV : register(SLOT_UAV_VOXEL_CLUSTER_INDEX);
StructuredBuffer<uint> g_VoxelClusterIndexSRV : register(SLOT_SRV_VOXEL_CLUSTER_INDEX);

// clusterID → AABB最小/最大
RWStructuredBuffer<float3> g_ClusterMinUAV : register(SLOT_UAV_CLUSTER_AABB_MIN);
StructuredBuffer<float3> g_ClusterMinSRV : register(SLOT_SRV_CLUSTER_AABB_MIN);
RWStructuredBuffer<float3> g_ClusterMaxUAV : register(SLOT_UAV_CLUSTER_AABB_MAX);
StructuredBuffer<float3> g_ClusterMaxSRV : register(SLOT_SRV_CLUSTER_AABB_MAX);

// クラスタごとの書き込み回数
RWStructuredBuffer<uint> g_ClusterCountUAV : register(SLOT_UAV_CLUSTER_COUNT); // ラベルごとのボクセル数（書き込み用）
StructuredBuffer<uint> g_ClusterCountSRV : register(SLOT_SRV_CLUSTER_COUNT); // ラベルごとのボクセル数

RWStructuredBuffer<uint> g_ClusterRemapUAV : register(SLOT_UAV_CLUSTER_REMAP); // ラベル → コンパクトなクラスタID への変換マップ
StructuredBuffer<uint> g_ClusterRemapSRV : register(SLOT_SRV_CLUSTER_REMAP); // ラベル → コンパクトなクラスタID への変換マップ

RWStructuredBuffer<uint> g_ValidClusterCounterUAV : register(SLOT_UAV_CLUSTER_VALID_COUNTER); // 有効クラスタの個数（Interlocked加算）

RWStructuredBuffer<uint> g_IsValidClusterUAV : register(SLOT_UAV_CLUSTER_ISVALID_FLAG); // 1 = 有効, 0 = 無効
StructuredBuffer<uint> g_IsValidClusterSRV : register(SLOT_SRV_CLUSTER_ISVALID_FLAG); // 各ラベルが有効か（0 or 1）

RWStructuredBuffer<uint> g_RemapPrefixSumUAV : register(SLOT_UAV_REMAP_PREFIX_SUM); // 出力：各ラベルのPrefixSum（局所）


RWStructuredBuffer<uint> g_RemapLocalSumsUAV : register(SLOT_UAV_REMAP_LOCAL_SUMS); // 各Groupの合計（Group数分）
StructuredBuffer<uint> g_RemapLocalSumsSRV : register(SLOT_SRV_REMAP_LOCAL_SUMS);

RWStructuredBuffer<uint> g_RemapGlobalOffsetsUAV : register(SLOT_UAV_REMAP_GLOBAL_OFFSETS); // グローバルオフセット（各ラベルの開始位置）
StructuredBuffer<uint> g_RemapGlobalOffsetsSRV : register(SLOT_SRV_REMAP_GLOBAL_OFFSETS); // グローバルオフセット（各ラベルの開始位置）

// グループ内で共有する有効クラスタ判定バッファ（スキャン用）
groupshared uint groupScanBuffer[THREAD_COUNT];

//=============================================================================
// コンピュートシェーダ
//=============================================================================
// アクティブ体素のラベル伝播（3x3x3 近傍で最小ラベルに更新）
[numthreads(THREAD_COUNT, 1, 1)]
void LabelPropagationCS(uint dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID >= g_NumActiveVoxels)
        return;
    
    // 現在の voxel の Morton 座標を復元
    uint tripletIndex = g_SortedIndicesSRV[dispatchThreadID];
    SPHIndexTriplet triplet = g_IndexTripletsSRV[tripletIndex];
    uint3 coord = triplet.gridCoord;

    //uint centerLabel = g_LabelBufferUAV[dispatchThreadID];
    uint centerLabel = g_LabelBufferSRV[dispatchThreadID];
    uint minLabel = centerLabel;

    // 3x3x3 の周辺をチェック
    [allow_uav_condition]
    for (int z = -1; z <= 1; ++z)
    {
        [allow_uav_condition]
        for (int y = -1; y <= 1; ++y)
        {
            [allow_uav_condition]
            for (int x = -1; x <= 1; ++x)
            {
                uint3 neighborCoord = coord + uint3(x, y, z);
                uint2 neighborMorton = EncodeMorton3_64_GPU(neighborCoord.x, neighborCoord.y, neighborCoord.z);

                int found = FindGridPageHeader(neighborMorton);
                if (found < 0)
                    continue;
                
                //uint neighborLabel = g_LabelBufferUAV[found];
                uint neighborLabel = g_LabelBufferSRV[found];
                minLabel = min(minLabel, neighborLabel);
            }
        }
    }
    
    // ラベル更新（既に最小なら変化なし）
    g_LabelBufferUAV[dispatchThreadID] = minLabel;
}

// ラベル伝播後、各クラスターの AABB 情報を構築する
[numthreads(THREAD_COUNT, 1, 1)]
void ClusterPartitionCS(uint dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID >= g_NumActiveVoxels)
        return;

    uint label = g_LabelBufferSRV[dispatchThreadID];
    uint3 voxelCoord = g_IndexTripletsSRV[dispatchThreadID].gridCoord;

    // voxelID → clusterID を出力
    g_VoxelClusterIndexUAV[dispatchThreadID] = label;

    // 変換 float3
    float3 pos = float3(voxelCoord);

    // cluster 書き込み回数を InterlockedAdd（初回だけ原子ロックで初期化）
    uint count;
    InterlockedAdd(g_ClusterCountUAV[label], 1, count);

    if (count == 0)
    {
        // 最初のvoxel → Min/Maxを初期化
        g_ClusterMinUAV[label] = pos;
        g_ClusterMaxUAV[label] = pos;
    }
    else
    {
        // 以降のvoxel → Min/Maxを更新
        float3 prevMin = g_ClusterMinUAV[label];
        float3 prevMax = g_ClusterMaxUAV[label];

        g_ClusterMinUAV[label] = min(prevMin, pos);
        g_ClusterMaxUAV[label] = max(prevMax, pos);
    }
}

//// 残留ラベルを除外して、有効クラスタのコンパクトIDを割り当てる
//[numthreads(THREAD_COUNT, 1, 1)]
//void ClusterRemapCS(uint3 dispatchThreadID : SV_DispatchThreadID)
//{
//    uint label = dispatchThreadID.x;
//    if (label >= g_NumActiveVoxels)
//        return;
    
//    uint voxelCount = g_ClusterCountSRV[id];
//    g_IsValidClusterUAV[id] = (voxelCount > 0) ? 1 : 0;
    
//    // グループ内で g_IsValidClusterUAV 書き込み同期 
//    GroupMemoryBarrierWithGroupSync();

//    uint voxelCount = g_ClusterCountSRV[label];
//    if (voxelCount > 0)
//    {
//        // 有効ラベル → 新クラスタID を割り当て
//        uint compactID;
//        InterlockedAdd(g_ValidClusterCounterUAV[0], 1, compactID);
//        g_ClusterRemapUAV[label] = compactID;
//    }
//    else
//    {
//        g_ClusterRemapUAV[label] = 0xFFFFFFFF; // 無効ラベル
//    }
//}

// 各スレッドグループ内の有効クラスタ数のPrefix Sumを計算
[numthreads(THREAD_COUNT, 1, 1)]
void PrefixSumLocalCS(uint tid : SV_DispatchThreadID, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID)
{
    uint labelIndex = gid * THREAD_COUNT + gtid;

    // スレッドごとにクラスタの有効性を判定し、共有バッファに書き込む（1 = 有効クラスタ, 0 = 無効）
    if (labelIndex < g_NumActiveVoxels)
    {
        uint voxelCount = g_ClusterCountSRV[labelIndex];
        uint isValid = (voxelCount > 0) ? 1 : 0;
        
        g_IsValidClusterUAV[labelIndex] = isValid;
        groupScanBuffer[gtid] = isValid;
    }
    else
    {
        groupScanBuffer[gtid] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    // 並列 prefix sum (inclusive scan)
    for (uint offset = 1; offset < THREAD_COUNT; offset *= 2)
    {
        uint currentValue = groupScanBuffer[gtid];
        GroupMemoryBarrierWithGroupSync();
        
        if (gtid >= offset)
        {
            currentValue += groupScanBuffer[gtid - offset];
        }
        
        GroupMemoryBarrierWithGroupSync();
        groupScanBuffer[gtid] = currentValue;
    }

    GroupMemoryBarrierWithGroupSync();

    // コンパクトID（prefix sum 結果 - 1）を出力
    if (labelIndex < g_NumActiveVoxels)
    {
        g_RemapPrefixSumUAV[labelIndex] = groupScanBuffer[gtid] - 1; // compactID = 指数-1
    }

    // このスレッドグループの合計を保存（次段階で使う）
    if (gtid == THREAD_COUNT - 1)
    {
        g_RemapLocalSumsUAV[gid] = groupScanBuffer[gtid];
    }
}

// Group合計（g_RemapLocalSumsUAV）のPrefixSumを計算して g_RemapGlobalOffsetsUAV に保存
[numthreads(1, 1, 1)]
void PrefixSumGlobalCS(uint dispatchThreadID : SV_DispatchThreadID)
{
    uint sum = 0;
    for (uint i = 0; i < g_NumGroups; ++i)
    {
        g_RemapGlobalOffsetsUAV[i] = sum;
        sum += g_RemapLocalSumsSRV[i];
    }
    
    g_ValidClusterCounterUAV[0] = sum;
}

// 各GroupのPrefixSumにGlobal Offsetを加算し、g_RemapPrefixSumUAVを更新
[numthreads(THREAD_COUNT, 1, 1)]
void PrefixSumAddOffsetCS(uint tid : SV_DispatchThreadID, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID)
{
    uint labelIndex = gid * THREAD_COUNT + gtid;
    if (labelIndex >= g_NumActiveVoxels)
        return;

    uint baseOffset = g_RemapGlobalOffsetsSRV[gid];
    uint compactID = g_RemapPrefixSumUAV[labelIndex];

    if (g_IsValidClusterUAV[labelIndex] != 0)
    {
        g_RemapPrefixSumUAV[labelIndex] = baseOffset + compactID;
    }
    else
    {
        g_RemapPrefixSumUAV[labelIndex] = 0xFFFFFFFF;
    }
}