#pragma once
//=============================================================================
//
// AABB による三角形空間分割構造（八分木） [OctreeNode.h]
// Author : 
// 三角形集合を AABB 単位で再帰的に空間分割し、
// 高速な領域クエリと交差判定を可能にするデータ構造を提供する
// 
//=============================================================================
#include "main.h"
#include "Core/Async/LightweightMutex.h"
#include "Utility/SimpleArray.h"
#include "Utility/HashMap.h"
#include "Collision/AABBUtils.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_TRIANGLES           10 // 1ノードあたりの最大三角形数
#define CHILD_COUNT             8  // 子ノード数
#define MAX_TRIANGLE_COUNT      10000 // 八分木に格納する最大三角形数

// 前方宣言
class OctreeNode;
struct Triangle;

//*********************************************************
// 構造体
//*********************************************************
struct OctreeRef
{
    OctreeNode* node = nullptr; // 格納ノード
    int indexInNode = -1;       // node->m_triangles 内のインデックス
}; // 三角形が格納されているノードとインデックスを保持する参照構造体


// OctreeNode* 専用ハッシュ関数
struct OctreeNodePtrHash
{
    unsigned int operator()(const OctreeNode* ptr) const
    {
        if (ptr == nullptr)
            return 0xFFFFFFFFu; // 特殊値

        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

        // Knuth multiplicative hash
        return static_cast<unsigned int>((addr >> 4) ^ (addr * 2654435761u));
    }
};

// OctreeNode* 専用等価比較関数
struct OctreeNodePtrEquals
{
    bool operator()(const OctreeNode* a, const OctreeNode* b) const
    {
        return a == b; // ポインタアドレスが同じなら等しい
    }
};

//=============================================================================
// OctreeHandle
//  - 1つの三角形がオクツリー内のどこに格納されているかの参照一覧を保持。
//  - refs: 参照配列（複数ノードに跨る可能性あり）
//  - nodeToRefIndex: ノード→refs 内の位置（O(1) で該当参照を見つけるため）
//=============================================================================
struct OctreeHandle
{
    SimpleArray<OctreeRef> refs;

    // ノード→refs 配列インデックス（O(1) 検索用）
    HashMap<OctreeNode*, int, OctreeNodePtrHash, OctreeNodePtrEquals> nodeToRefIndex =
        HashMap<OctreeNode*, int, OctreeNodePtrHash, OctreeNodePtrEquals>(
        8192, // 初期バケット数
        OctreeNodePtrHash(), // ハッシュ関数
        OctreeNodePtrEquals() // 等価比較関数
    );

    // 参照を新規追加
    void AddRef(OctreeNode* node, int indexInNode)
    {
        int idx = refs.getSize();
        refs.push_back(OctreeRef{ node, indexInNode });
        nodeToRefIndex.insert(node, idx);
    }

    // ノード内インデックスを更新（swap-pop により index が変わった時に呼ぶ）
    void UpdateIndex(OctreeNode* node, int newIndexInNode)
    {
        auto it = nodeToRefIndex.find(node);
        if (it == nodeToRefIndex.end()) return;
        int ri = it->value;
        refs[ri].indexInNode = newIndexInNode;
    }

    // 指定ノードの参照を削除（refs 側も swap-pop）
    void RemoveRef(OctreeNode* node)
    {
        auto it = nodeToRefIndex.find(node);
        if (it == nodeToRefIndex.end()) return;

        int removeIdx = it->value;
        int last = refs.getSize() - 1;

        if (removeIdx != last)
        {
            // 後方要素をスワップして詰める
            OctreeRef moved = refs[last];
            refs[removeIdx] = moved;
            nodeToRefIndex[moved.node] = removeIdx;
        }
        refs.pop_back();
        nodeToRefIndex.erase(it);
    }

    // クリア
    void Clear()
    {
        refs.clear();
        nodeToRefIndex.clear();
    }

    bool Empty() const { return refs.getSize() == 0; }
};


// 八分木ノードの定義。各ノードは自作の AABB 境界を持ち、Triangle* を格納する。
class OctreeNode 
{
public:
    OctreeNode(const BOUNDING_BOX& boundary, bool threadSafe = true, int depth = 0, int maxDepth = 8)
        : m_boundary(boundary), m_threadSafe(threadSafe), m_depth(depth), m_maxDepth(maxDepth) {}

    ~OctreeNode();

    // 三角形をこの八分木に挿入する関数
    bool Insert(const Triangle* tri);
    // 三角形 tri を挿入し、outHandle に参照位置を収集する。
    bool InsertWithHandle(const Triangle* tri);

    // 指定した範囲 (AABB) と交差する三角形をクエリする関数
    void QueryRange(const BOUNDING_BOX& range, SimpleArray<const Triangle*>& result);

    const BOUNDING_BOX& GetBoundary(void) const { return m_boundary; }

	// 三角形をこの八分木から削除する関数
	bool RemoveTriangle(const Triangle* tri);

private:
    // 現在のノードを8つの子ノードに分割する関数
    void Subdivide(void);
    // ハンドルが保持する全参照を辿り、各ノードの配列から O(1) swap-pop で削除
    bool RemoveByHandle(OctreeHandle& handle);

    BOUNDING_BOX m_boundary; // このノードがカバーする領域（ワールド座標系）
    SimpleArray<const Triangle*> m_triangles;   // ノードに格納される三角形ポインタのリスト
    OctreeNode* m_children[CHILD_COUNT] = { nullptr }; // 8つの子ノードへのポインタ
    int m_depth;
    int m_maxDepth;
    bool m_threadSafe = true; // デフォルトでスレッドセーフ有効

	LightweightMutex m_lock; // ノード単位のロック

    // 三角形→ハンドルの関連を保持（swap-pop のインデックス回写に必要）
    static HashMap<const Triangle*, OctreeHandle*, TrianglePtrHash, TrianglePtrEquals> s_registry;

    static LightweightMutex s_lock; // 各ノードの互互掛かりを防ぐ
};