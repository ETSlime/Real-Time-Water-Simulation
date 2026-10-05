#pragma once
//=============================================================================
//
// [OptimizedMarchingCubesMesher.h]
// Author : 
//
// カラー場から Marching Cubes の三角形メッシュを構築する CPU 実装。
// 最適化されたエッジキャッシュを使用し、同一エッジの頂点生成を防止することで、
// 頂点の重複を回避しパフォーマンスを向上させる。
//
// - GetEdgeCacheIndex により、flat 配列インデックスとしてのキャッシュ参照を実装
// - Sparse バージョンでは、HashMap による EdgeCacheKey（Voxel + EdgeIndex）で高速キャッシュ実装
// - BuildMesh() は稠密（3D配列）と稀疏（voxel map）両方に対応
// - 法線計算は周囲スカラー差分により生成（ComputeNormal / ComputeSparseNormal）
// - SetTargetBuffer() によって出力頂点／インデックスバッファの外部指定をサポート
// 
//=============================================================================
#include "Core/Graphics/VertexStructs.h"
#include "Effects/SPH/SPHVolumeBuilder.h"
#include "Utility/SimpleArray.h"

struct EdgeCacheKey
{
    VoxelCoord coord;   // voxel 座標
    int edgeIndex; // エッジ番号
};

struct HashEdgeCacheKey
{
    size_t operator()(const EdgeCacheKey& key) const
    {
        // Mortonコードを使って空間部分をエンコードし、
        // エッジインデックスを下位に混ぜる
        uint64_t morton = EncodeMorton3_64(
            static_cast<uint32_t>(key.coord.x),
            static_cast<uint32_t>(key.coord.y),
            static_cast<uint32_t>(key.coord.z));
        return static_cast<size_t>((morton << 4) ^ key.edgeIndex);
    }
};

struct EqualEdgeCacheKey
{
    bool operator()(const EdgeCacheKey& a, const EdgeCacheKey& b) const
    {
        return a.coord.x == b.coord.x && a.coord.y == b.coord.y && a.coord.z == b.coord.z && a.edgeIndex == b.edgeIndex;
    }
};

class OptimizedMarchingCubesMesher
{
public:
    OptimizedMarchingCubesMesher() = default;

    void Initialize(UINT maxParticles);
    void Clear(void);
    void BuildMesh(const SimpleArray<SimpleArray<SimpleArray<float>>>& scalarField,
        const VolumeReconstructionParams& params);
    void BuildMesh(const HashMap<VoxelCoord, float, HashVoxelKey64, EqualVoxelKey64>& scalarFieldMap,
        const VolumeReconstructionParams& params);

    // メッシュの頂点数とインデックス数を計算
    void ComputeMeshRequirements(
        const VolumeReconstructionParams& params,
        UINT* outVertexCount,
        UINT* outIndexCount);

    // メッシュの頂点とインデックスを設定
    void SetTargetBuffer(
        SimpleArray<MarchingCubesVertex>& vertices,
        SimpleArray<UINT>& indices);

    const SimpleArray<MarchingCubesVertex>& GetVertices(void) const { return *m_targetVertices; }
    const SimpleArray<UINT>& GetIndices(void) const { return *m_targetIndices; }

private:
    int GetEdgeCacheIndex(int x, int y, int z, int edge, const VolumeReconstructionParams& params) const;

    XMFLOAT3 InterpolateVertex(const XMFLOAT3& p1, const XMFLOAT3& p2, float valp1, float valp2);
    XMFLOAT3 ComputeNormal(const SimpleArray<SimpleArray<SimpleArray<float>>>& field,
        int x, int y, int z);

    XMFLOAT3 ComputeSparseNormal(
        const HashMap<VoxelCoord, float, HashVoxelKey64, EqualVoxelKey64>& scalarField,
        const VoxelCoord& coord,
        float cellSize);

    SimpleArray<MarchingCubesVertex>* m_targetVertices = nullptr;
    SimpleArray<UINT>* m_targetIndices = nullptr;
    SimpleArray<int> m_edgeVertexCache; // flat エッジキャッシュ

    HashMap<EdgeCacheKey, int, HashEdgeCacheKey, EqualEdgeCacheKey> m_edgeVertexCacheSparse;
    SimpleArray<EdgeCacheKey> m_keysInsertedThisFrame;
};
