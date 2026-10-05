//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SLOT_CB_VOXEL_META                  b13
#define SLOT_SRV_VOXEL_TRIANGLE_INDEX       t8
#define SLOT_SRV_VOXEL_TRIANGLE_STRUCT      t9
#define SLOT_SRV_VOXEL_IDX_TRIPLET          t10
#define SLOT_SRV_VOXEL_IDX_TRIPLET_PAGE     t11

#define MAX_TRI_PER_VOXEL                   256  
#define MAX_INDEX_TRIPLET_PAGES             1024


//*****************************************************************************
// 構造体定義
//*****************************************************************************

// 三角形構造体：頂点3つと法線
struct TriangleStruct
{
    float3 v0;
    float pad0;
    float3 v1;
    float pad1;
    float3 v2;
    float pad2;
    float3 normal;
    float pad3;
};

// IndexTriplet：voxel index → 三角形の開始オフセットと数
struct IndexTriplet
{
    uint voxelIndex;
    uint startOffset;
    uint count;
    uint pad; // パディング（cbuffer整列のため）
};

// IndexTripletPageHeader：IndexTripletのページヘッダー
struct IndexTripletPageHeader
{
    uint pageStartMorton; // ページ最小モートン番号
    uint pageEndMorton; // ページ最大モートン番号
    uint pageOffset; // g_IndexTripletsでのオフセット
    uint pageCount; // このページのトリプレット数
};

//*****************************************************************************
// 定数バッファ
//*****************************************************************************

// ボクセルメタデータ用の定数バッファ
cbuffer CBVoxelMeta : register(SLOT_CB_VOXEL_META)
{
    float3 g_VoxelOrigin; // ワールド空間での原点
    float g_VoxelSize; // 各ボクセルの1辺の長さ

    uint g_TotalIndexTripletSize; // g_IndexTripletsの総数
    uint g_TotalPageHeaderSize; // g_PageHeadersの総数
    uint2 padVoxelMeta; // パディングで16バイト揃え
};

//*****************************************************************************
// グローバル変数
//*****************************************************************************
StructuredBuffer<uint> g_VoxelTriangleIndices           : register(SLOT_SRV_VOXEL_TRIANGLE_INDEX); // 三角形インデックス
StructuredBuffer<TriangleStruct> g_Triangles            : register(SLOT_SRV_VOXEL_TRIANGLE_STRUCT); // 三角形構造体
StructuredBuffer<IndexTriplet> g_IndexTriplets          : register(SLOT_SRV_VOXEL_IDX_TRIPLET); // インデックス三つ組（モートンコード用）
StructuredBuffer<IndexTripletPageHeader> g_PageHeaders  : register(SLOT_SRV_VOXEL_IDX_TRIPLET_PAGE); // ページヘッダー

//================== 補助関数 ==================
int3 GetVoxelCoord(float3 worldPos)
{
    float3 local = (worldPos - g_VoxelOrigin) / g_VoxelSize;
    return int3(floor(local)); // ボクセル座標は整数値
}

uint Part1By2(uint x)
{
    x &= 0x3ff;
    x = (x | (x << 16)) & 0x30000ff;
    x = (x | (x << 8)) & 0x300f00f;
    x = (x | (x << 4)) & 0x30c30c3;
    x = (x | (x << 2)) & 0x9249249;
    return x;
}

uint EncodeMorton3(uint x, uint y, uint z)
{
    return (Part1By2(z) << 2) | (Part1By2(y) << 1) | Part1By2(x);
}

// 二分探索で voxelIndex に一致する IndexTriplet を探す
int FindIndexTriplet(uint voxelIndex)
{
    uint left = 0;
    uint right = g_TotalPageHeaderSize - 1;

    // ページヘッダーの二分探索
    [allow_uav_condition]
    while (left <= right)
    {
        uint mid = (left + right) / 2;
        IndexTripletPageHeader pageHeader = g_PageHeaders[mid];
        
        if (voxelIndex < pageHeader.pageStartMorton)
        {
            right = mid - 1;
        }
        else if (voxelIndex > pageHeader.pageEndMorton)
        {
            left = mid + 1;
        }
        else
        {
            // ページの範囲に含まれているから、内部を探索する
            [allow_uav_condition]
            for (uint j = 0; j < pageHeader.pageCount; ++j)
            {
                IndexTriplet triplet = g_IndexTriplets[pageHeader.pageOffset + j];
                if (triplet.voxelIndex == voxelIndex)
                    return pageHeader.pageOffset + j;
            }

            // でも実際には見つからなかったら…？
            // 範囲は合っていたけど、ページに含まれていないパターン！
            // **探索は続けるべきじゃなくて、これはありえない状態かも！**
            return -2; // 一応保険
        }
    }

    return -1; // 完全に見つからない
}

//================== コリジョンチェック ==================
bool RayIntersectsTriangle(float3 origin, float3 dir, TriangleStruct tri, out float3 hitPoint, out float rayDistance)
{
    float3 edge1 = tri.v1 - tri.v0;
    float3 edge2 = tri.v2 - tri.v0;
    float3 h = cross(dir, edge2);
    float a = dot(edge1, h);

    if (abs(a) < 1e-6f)
        return false;

    float f = 1.0f / a;
    float3 s = origin - tri.v0;
    float u = f * dot(s, h);
    if (u < 0.0f || u > 1.0f)
        return false;

    float3 q = cross(s, edge1);
    float v = f * dot(dir, q);
    if (v < 0.0f || u + v > 1.0f)
        return false;

    rayDistance = f * dot(edge2, q);

    if (rayDistance > 1e-6f)
    {
        hitPoint = origin + rayDistance * dir;
        return true;
    }

    return false;
}

float3 ApplyCollisionSlideVelocity(float3 velocity, float3 normal, float friction, float minStopSpeed)
{
    float3 tangent = velocity - dot(velocity, normal) * normal;
    return (length(tangent) < minStopSpeed) ? float3(0, 0, 0) : tangent * friction; // 摩擦力を適用
}

float3 ApplyCollisionResponseVelocity(float3 velocity, float3 normal, float restitution, float friction, float minStopSpeed)
{
    float3 normalComponent = dot(velocity, normal) * normal;
    float3 tangentialComponent = velocity - normalComponent;

    // 反発成分を計算
    float3 bounce = -restitution * normalComponent;

    // 摩擦成分を計算
    if (length(tangentialComponent) < minStopSpeed)
        tangentialComponent = float3(0, 0, 0);
    else
        tangentialComponent *= saturate(1.0f - friction);

    return bounce + tangentialComponent;
}
