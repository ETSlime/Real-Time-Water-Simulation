#pragma once
//=============================================================================
//
// 空間加速構造体ちゃん [VoxelGrid.h]
// Author : 
// ボクセルベースの空間分割によって、三角形メッシュを3Dグリッドに整理し、
// 高速なクエリ（衝突判定・視界探索・地形サンプリングなど）を可能にする
// 
// 主な機能：
//   - モートンコード（Morton Encoding）による高速ハッシュ
//   - スレッドセーフ対応（SpinLockでガード）
//   - 三角形の一括登録／動的追加
//   - 指定半径内の高速検索クエリ(Query)
//
//=============================================================================
#include "main.h"
#include "Utility/HashMap.h"
#include "Collision/AABBUtils.h"
#include "Core/Async/SpinLock.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_VOXEL_NUM               20000000    // ボクセルグリッドの最大サイズ
#define MAX_VOXEL_TRIANGLE_NUM      5000000     // ボクセルに属する最大三角形数

// ボクセルキーのハッシュ関数
inline uint32_t Part1By2(uint32_t x)
{
    x &= 0x3ff; // 限定10位
    x = (x | (x << 16)) & 0x30000ff;
    x = (x | (x << 8)) & 0x300f00f;
    x = (x | (x << 4)) & 0x30c30c3;
    x = (x | (x << 2)) & 0x9249249;
    return x;
}

// 3次元座標をモートンコードにエンコードする関数
inline uint32_t EncodeMorton3(uint32_t x, uint32_t y, uint32_t z)
{
    //安全チェック
    assert(x < 1024 && "MortonX must be < 1024");
    assert(y < 1024 && "MortonY must be < 1024");
    assert(z < 1024 && "MortonZ must be < 1024");

    return (Part1By2(z) << 2) | (Part1By2(y) << 1) | Part1By2(x);
}

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct VoxelKey
{
    int x, y, z;
}; // ボクセルのキーを表す構造体

// VoxelKey のハッシュ関数と等価比較関数を定義
struct HashVoxelKey
{
    size_t operator()(const VoxelKey& key) const
    {
        return static_cast<size_t>(EncodeMorton3(key.x, key.y, key.z));
    }
};

// VoxelKey の等価比較関数
struct EqualVoxelKey
{
    bool operator()(const VoxelKey& a, const VoxelKey& b) const
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
};

//=============================================================================
// VoxelRef
//  - ある三角形が格納されている「ボクセルキー＋そのボクセル内インデックス」を表す参照。
//=============================================================================
struct VoxelRef
{
    VoxelKey key;      // 体素キー（x,y,z）
    int indexInCell;   // VoxelEntry::triangles 内のインデックス
};

//=============================================================================
// VoxelHandle
//  - 1つの三角形がどのボクセルに入っているかの参照一覧を保持。
//  - AddRef/UpdateIndex/RemoveRef は O(1) で操作できるように key→refIndex を持つ。
//=============================================================================
struct VoxelHandle
{
    SimpleArray<VoxelRef> refs;

    // 参照検索を O(1) にするため、キーを 64bit にパックして索引化
    // （Morton でも可。ここでは簡単に (x,y,z) を 21bit+21bit+21bit に詰める例）
    HashMap<uint64_t, int, HashUInt64, EqualUInt64> keyToRefIndex =
        HashMap<uint64_t, int, HashUInt64, EqualUInt64>(
            MAX_VOXEL_TRIANGLE_NUM,
            HashUInt64(),
            EqualUInt64()
        );

    static uint64_t PackKey(const VoxelKey& k)
    {
        // 注意：値域チェックはプロジェクト要件に合わせて調整
        uint64_t xx = static_cast<uint64_t>(static_cast<uint32_t>(k.x) & 0x1FFFFF);
        uint64_t yy = static_cast<uint64_t>(static_cast<uint32_t>(k.y) & 0x1FFFFF);
        uint64_t zz = static_cast<uint64_t>(static_cast<uint32_t>(k.z) & 0x1FFFFF);
        return (xx << 42) | (yy << 21) | zz;
    }

    void AddRef(const VoxelKey& key, int indexInCell)
    {
        int idx = refs.getSize();
        refs.push_back(VoxelRef{ key, indexInCell });
        keyToRefIndex[PackKey(key)] = idx;
    }

    void UpdateIndex(const VoxelKey& key, int newIndexInCell)
    {
        auto it = keyToRefIndex.find(PackKey(key));
        if (it == keyToRefIndex.end()) return;
        refs[it->value].indexInCell = newIndexInCell;
    }

    void RemoveRef(const VoxelKey& key)
    {
        auto it = keyToRefIndex.find(PackKey(key));
        if (it == keyToRefIndex.end()) return;
        int removeIdx = it->value;
        int last = refs.getSize() - 1;

        if (removeIdx != last)
        {
            VoxelRef moved = refs[last];
            refs[removeIdx] = moved;
            keyToRefIndex[PackKey(moved.key)] = removeIdx;
        }
        refs.pop_back();
        keyToRefIndex.erase(it);
    }

    void Clear()
    {
        refs.clear();
        keyToRefIndex.clear();
    }

    bool Empty() const { return refs.getSize() == 0; }
};


class VoxelGrid
{
	friend class VoxelBufferUploader; // VoxelBufferUploaderからアクセス可能にする
public:
    VoxelGrid(bool threadSafe = true) :m_threadSafe(threadSafe) {}

    ~VoxelGrid(void);

    // ボクセルグリッドの初期化
    void Initialize(float voxelSize = 100.0f);
	// ボクセルグリッドの初期化（スレッドセーフ）
    void BuildTriangles(const SimpleArray<Triangle*>& triangles);
    // 三角形を動的に追加（物理オブジェクトや生成地形など、スレッドセーフ）
    void InsertTriangle(const Triangle* tri);
	// ボクセルグリッドを三角形から構築（初期化後に呼び出す）
    void BuildVoxelMapByTriangles(void);
    // グリッドが初期化済みかどうか
	bool IsReady(void) const { return m_isReady; } 

	// 指定された中心と半径に基づいて、ボクセルグリッドから三角形をクエリする関数
    void Query(const XMFLOAT3& center, float radius, SimpleArray<const Triangle*>& outTriangles) const;
    // レイキャストによる衝突判定
	bool Raycast(const XMFLOAT3& start, const XMFLOAT3& end, XMFLOAT3* hitPos, XMFLOAT3* hitNormal) const;

	// 三角形の動的削除（スレッドセーフ）：tri → handle を引き、各セルから swap - pop で削除し、moved のインデックスを回写
    bool RemoveTriangleIncremental(const Triangle* tri);

    // デバッグ用の統計情報を出力
    void print_debug_stats(void) const;

private:
	// レイと三角形の交差判定（Moller Trumboreアルゴリズム）
    bool RayIntersectsTriangle(
        const XMFLOAT3& rayOrigin,
        const XMFLOAT3& rayDir,
        const Triangle* tri,
        float* outT,
        XMFLOAT3* outHit,
        XMFLOAT3* outHitNormal) const;

    // 既に m_isReady == true の状態で、追加の三角形を必要なボクセルにのみ反映
    // 成功時、三角形→ハンドルをレジストリに登録（重複は拒否）
	bool VoxelGrid::InsertTriangleIncremental(const Triangle* tri);

    // 単一ボクセルへ tri を追加し、handle に参照を登録
    bool AddTriToCell(const Triangle* tri, const VoxelKey& key, VoxelHandle* handle);

    // 単一ボクセルから tri を O(1) で削除（swap-pop）し、moved のインデックスを回写
    // true なら削除実施。moved と newIdx は呼び出し側でインデックス回写に使用
    bool RemoveTriFromCell(const VoxelKey& key, int indexInCell, const Triangle*& outMoved, int& outNewIdx);

    // 各Voxelに登録された三角形
    struct VoxelEntry
    {
        SimpleArray<const Triangle*> triangles;
    };

    // 位置ベクトルから所属するVoxelキーを算出
    VoxelKey GetVoxelKey(const XMFLOAT3& pos) const;
	// 三角形のAABBを計算し、ボクセルグリッド全体のAABBを更新
    void ComputeVoxelBounds(const Triangle* tri);

	BOUNDING_BOX m_voxelBounds; // ボクセルグリッド全体のAABB
	float m_voxelSize = 1.0f; // ボクセルのサイズ（1辺の長さ）
	XMFLOAT3 m_origin = { 0.0f, 0.0f, 0.0f }; // グリッドの原点
	bool m_isReady = false; // グリッドが初期化済みかどうか
    bool m_threadSafe = true; // スレッドセーフかどうか
	mutable SpinLock m_lock; // スレッドセーフ用のスピンロック
	SimpleArray<const Triangle*> m_triangles; // 登録された三角形のリスト

	// ボクセルキーと三角形のマッピングを保持するハッシュマップ
	HashMap<VoxelKey, VoxelEntry, HashVoxelKey, EqualVoxelKey> m_voxelMap =
        HashMap<VoxelKey, VoxelEntry, HashVoxelKey, EqualVoxelKey>(
        MAX_VOXEL_NUM, // 初期バケット数
		HashVoxelKey(), // ハッシュ関数
		EqualVoxelKey() // 等価比較関数
        );

    // 静的登録用のスピンロック
	static SpinLock s_registryLock;
    // 三角形→ハンドルの静的登録マップ
	static HashMap<const Triangle*, VoxelHandle*, TrianglePtrHash, TrianglePtrEquals> s_registry;
};