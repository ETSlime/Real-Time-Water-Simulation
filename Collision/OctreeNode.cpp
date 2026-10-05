//=============================================================================
//
// AABB による三角形空間分割構造（八分木） [OctreeNode.cpp]
// Author : 
// 三角形集合を AABB 単位で再帰的に空間分割し、
// 高速な領域クエリと交差判定を可能にするデータ構造を提供する
// 
//=============================================================================
#include "Collision/OctreeNode.h"

// 全ノード共通の静的レジストリ
HashMap<const Triangle*, OctreeHandle*, TrianglePtrHash, TrianglePtrEquals> OctreeNode::s_registry =
HashMap<const Triangle*, OctreeHandle*, TrianglePtrHash, TrianglePtrEquals>(
    MAX_TRIANGLE_COUNT, // 初期バケット数
    TrianglePtrHash(), // ハッシュ関数
    TrianglePtrEquals() // 等価比較関数
);
LightweightMutex OctreeNode::s_lock;

OctreeNode::~OctreeNode()
{
    for (int i = 0; i < 8; i++)
    {
        SAFE_DELETE(m_children[i]);
    }
}

bool OctreeNode::Insert(const Triangle* tri)
{
    // ノードの基本情報に対してロックをかける
    OptionalLockGuard guard(&m_lock, m_threadSafe);

    // 三角形の AABB がこのノードの境界と交差しなければ挿入しない
    if (!m_boundary.intersects(tri->aabb))
        return false;

    // 現在のノードが葉で、容量があるかまたは最大深さに達している場合はここに格納する
    if (m_triangles.getSize() < MAX_TRIANGLES || m_depth == m_maxDepth)
    {
        m_triangles.push_back(tri);
        return true;
    }

    // subdivide() が必要か一時判定（ダブルチェック用）
    bool needSubdivide = (m_children[0] == nullptr);

    // 最初のロックを解除して subdivide() によるデッドロックを防止
    guard.~OptionalLockGuard();

    // subdivide() を改めてロックのもと実行（ダブルチェックあり）
    if (needSubdivide)
    {
        OptionalLockGuard subdivideGuard(&m_lock, m_threadSafe);
        if (m_children[0] == nullptr)
            Subdivide(); // 子ノードに分割されていなければ、分割する
    }


    // 子ノードに挿入を試みる
    bool inserted = false;
    for (int i = 0; i < CHILD_COUNT; i++)
    {
        if (m_children[i]->Insert(tri))
            inserted = true;
    }

    // もし子ノードのどこにも完全に収まらない場合は、現在のノードに保持する
    if (!inserted)
    {
        OptionalLockGuard fallback(&m_lock, m_threadSafe);
        m_triangles.push_back(tri);
    }
    return true;
}

void OctreeNode::QueryRange(const BOUNDING_BOX& range, SimpleArray<const Triangle*>& result)
{
    OptionalLockGuard guard(const_cast<LightweightMutex*>(&m_lock), m_threadSafe);

    // 現在のノードと範囲が交差しなければ、何もせずリターン
    if (!m_boundary.intersects(range))
        return;

    // 現在のノードに格納された各三角形について、AABB の交差を確認する
    int size = m_triangles.getSize();
    for (int i = 0; i < size; i++)
    {
        if (m_triangles[i]->aabb.intersects(range))
            result.push_back(m_triangles[i]);
    }

    // ロック解除後も完全な同期性を保証
    OctreeNode* childrenCopy[CHILD_COUNT];
    for (int i = 0; i < CHILD_COUNT; ++i)
        childrenCopy[i] = m_children[i];

    // 子ノードが存在する場合、再帰的にクエリを実行する
    if (m_children[0] != nullptr)
    {
        for (int i = 0; i < CHILD_COUNT; i++)
        {
            m_children[i]->QueryRange(range, result);
        }
    }
}

bool OctreeNode::RemoveTriangle(const Triangle* tri)
{
    if (!tri) return false;

    OctreeHandle* h = nullptr;

    // tri→handle を取得しつつ、同一クリティカルセクションでエントリを消す
    {
        OptionalLockGuard regGuard(&s_lock, /*enable*/ true);
        auto it = s_registry.find(tri);
        if (it == s_registry.end()) return false;
        h = it->value;
        s_registry.erase(it);          // ここで窓を閉じる（以降、他スレッドは tri を引けない）
    }
    if (!h) return false;

    // ノード配列からの実削除（swap-pop + moved のインデックス回写）
    bool ok = RemoveByHandle(*h);

    // ハンドルをクリア
    h->Clear();
    SAFE_DELETE(h); // ← Insert 時に new しているならここで解放（プール管理なら返却）

    return ok;
}

void OctreeNode::Subdivide(void)
{
    XMFLOAT3 pMin = m_boundary.minPoint;
    XMFLOAT3 pMax = m_boundary.maxPoint;

    // 境界の中心点を計算する
    XMFLOAT3 center;
    center.x = (pMin.x + pMax.x) * 0.5f;
    center.y = (pMin.y + pMax.y) * 0.5f;
    center.z = (pMin.z + pMax.z) * 0.5f;

    // 8つの子ノードの境界をそれぞれ計算する
    // 子ノード 0: (min, center)
    m_children[0] = new OctreeNode(BOUNDING_BOX(pMin, center), m_depth + 1, m_maxDepth);
    // 子ノード 1: ( (center.x, min.y, min.z), (max.x, center.y, center.z) )
    m_children[1] = new OctreeNode(BOUNDING_BOX(XMFLOAT3(center.x, pMin.y, pMin.z), XMFLOAT3(pMax.x, center.y, center.z)), m_depth + 1, m_maxDepth);
    // 子ノード 2: ( (min.x, center.y, min.z), (center.x, max.y, center.z) )
    m_children[2] = new OctreeNode(BOUNDING_BOX(XMFLOAT3(pMin.x, center.y, pMin.z), XMFLOAT3(center.x, pMax.y, center.z)), m_depth + 1, m_maxDepth);
    // 子ノード 3: ( (center.x, center.y, min.z), (max.x, max.y, center.z) )
    m_children[3] = new OctreeNode(BOUNDING_BOX(XMFLOAT3(center.x, center.y, pMin.z), XMFLOAT3(pMax.x, pMax.y, center.z)), m_depth + 1, m_maxDepth);
    // 子ノード 4: ( (min.x, min.y, center.z), (center.x, center.y, max.z) )
    m_children[4] = new OctreeNode(BOUNDING_BOX(XMFLOAT3(pMin.x, pMin.y, center.z), XMFLOAT3(center.x, center.y, pMax.z)), m_depth + 1, m_maxDepth);
    // 子ノード 5: ( (center.x, min.y, center.z), (max.x, center.y, max.z) )
    m_children[5] = new OctreeNode(BOUNDING_BOX(XMFLOAT3(center.x, pMin.y, center.z), XMFLOAT3(pMax.x, center.y, pMax.z)), m_depth + 1, m_maxDepth);
    // 子ノード 6: ( (min.x, center.y, center.z), (center.x, max.y, max.z) )
    m_children[6] = new OctreeNode(BOUNDING_BOX(XMFLOAT3(pMin.x, center.y, center.z), XMFLOAT3(center.x, pMax.y, pMax.z)), m_depth + 1, m_maxDepth);
    // 子ノード 7: ( (center.x, center.y, center.z), (max.x, max.y, max.z) )
    m_children[7] = new OctreeNode(BOUNDING_BOX(center, pMax), m_depth + 1, m_maxDepth);
}

bool OctreeNode::RemoveByHandle(OctreeHandle& handle)
{
    if (handle.Empty()) return false;

    SimpleArray<OctreeRef> refsCopy = handle.refs; // スナップショット
    bool removedAny = false;

    for (UINT r = 0; r < refsCopy.getSize(); ++r)
    {
        OctreeNode* node = refsCopy[r].node;
        int idx = refsCopy[r].indexInNode;
        if (!node) continue;

        const Triangle* moved = nullptr;
        int last = -1;

        // ノード側：swap-pop（ノードロックのみ）
        {
            OptionalLockGuard g(&node->m_lock, node->m_threadSafe);
            last = node->m_triangles.getSize() - 1;
            if (idx >= 0 && idx <= last)
            {
                moved = node->m_triangles[last];
                node->m_triangles[idx] = moved; // idx==last の場合は自己代入
                node->m_triangles.pop_back();
                removedAny = true;
            }
        }

        // moved のインデックス回写（必要な場合のみ）
        // 条件:
        //  - idx が有効だった
        //  - idx != last（位置が変わった時だけ）
        //  - moved のハンドルが「削除対象 handle」とは別（同一なら回写不要）
        if (moved != nullptr)
        {
            OptionalLockGuard regGuard(&s_lock, /*enable*/ true);
            auto it = s_registry.find(moved);
            if (it != s_registry.end() && it->value)
            {
                OctreeHandle* movedHandle = it->value;
                if (movedHandle != &handle) // ← 自分自身の削除ハンドルなら回写不要
                {
                    movedHandle->UpdateIndex(node, idx);
                }
            }
        }

        // 自身のハンドル（削除対象）からも、この node 参照を除去（swap-pop）
        handle.RemoveRef(node);
    }

    return removedAny;
}

bool OctreeNode::InsertWithHandle(const Triangle* tri)
{
    if (tri == nullptr) return false;

    // すでに登録済みかをチェック（静的レジストリ側のロック）
    {
        OptionalLockGuard reg(&s_lock, /*enable*/ true);
        auto it = s_registry.find(tri);
        if (it != s_registry.end())
        {
            // 既に登録済み → 二重挿入は拒否
            return false;
        }
    }
    // 新規ハンドルを確保（成功したらレジストリに登録する／失敗なら破棄）
    OctreeHandle* handle = new OctreeHandle();

    // まずはこのノード範囲との交差を確認（ノード側ロック）
    {
        OptionalLockGuard guard(&m_lock, m_threadSafe);
        if (!m_boundary.intersects(tri->aabb))
        {
            SAFE_DELETE(handle);
            return false;
        }
    }

    // 葉条件 or 容量未満 or 最大深度 到達 → 現ノードへ格納
    {
        bool placedHere = false;

        {
            OptionalLockGuard guard(&m_lock, m_threadSafe);
            if (m_triangles.getSize() < MAX_TRIANGLES || m_depth == m_maxDepth)
            {
                const int indexInNode = m_triangles.getSize();
                m_triangles.push_back(tri);
                handle->AddRef(this, indexInNode);
                placedHere = true;
            }
        }

        if (placedHere)
        {
            // ノードロックのスコープはここで終了 → 次にレジストリを更新（全域ロック）
            OptionalLockGuard reg(&s_lock, /*enable*/ true);
            s_registry[tri] = handle;
            return true;
        }

    }

    // 子ノードが未生成なら分割（ダブルチェック・ロック）
    {
        bool needSubdivide = false;
        {
            OptionalLockGuard guard(&m_lock, m_threadSafe);
            needSubdivide = (m_children[0] == nullptr);
        }
        if (needSubdivide)
        {
            OptionalLockGuard guard(&m_lock, m_threadSafe);
            if (m_children[0] == nullptr)
                Subdivide();
        }
    }

    // 子ノード配列のスナップショットを取得してロックを外す
    OctreeNode* childrenCopy[CHILD_COUNT];
    {
        OptionalLockGuard guard(&m_lock, m_threadSafe);
        for (int i = 0; i < CHILD_COUNT; ++i)
            childrenCopy[i] = m_children[i];
    }

    // 交差する子へ再帰的に挿入（複数の子に跨る可能性あり）
    bool insertedToChild = false;
    if (childrenCopy[0] != nullptr)
    {
        for (int i = 0; i < CHILD_COUNT; ++i)
        {
            OctreeNode* c = childrenCopy[i];
            if (!c) continue;

            // 子境界と tri->aabb が交差するかは子ノード側のロック下で判定
            bool hitChild = false;
            {
                OptionalLockGuard guard(&c->m_lock, c->m_threadSafe);
                hitChild = c->m_boundary.intersects(tri->aabb);
            }
            if (!hitChild) continue;

            // 再帰呼び出しは「この outHandle なし版」では参照蓄積ができないため、
            // 同関数を直接呼ぶのではなく、子側でも同様に新規ハンドルを作らないよう工夫する必要がある。
            // → 解決策：子に対しても「同一の handle に参照を追加」するヘルパが必要。
            // ここではローカル再帰ラムダで handle をキャプチャして参照を蓄積する。

            // ---- 再帰ラムダ（このスコープ内で一度だけ定義）----
            auto insertRec = [&](auto&& self, OctreeNode* node) -> bool
                {
                    // 子境界チェック（node ロック）
                    {
                        OptionalLockGuard g(&node->m_lock, node->m_threadSafe);
                        if (!node->m_boundary.intersects(tri->aabb))
                            return false;
                    }

                    // 葉条件 or 容量未満 or 最大深度 → 当該ノードへ格納
                    {
                        OptionalLockGuard g(&node->m_lock, node->m_threadSafe);
                        if (node->m_triangles.getSize() < MAX_TRIANGLES || node->m_depth == node->m_maxDepth)
                        {
                            const int idx = node->m_triangles.getSize();
                            node->m_triangles.push_back(tri);
                            handle->AddRef(node, idx);
                            return true;
                        }
                    }

                    // 子未生成なら分割
                    {
                        bool needSub = false;
                        {
                            OptionalLockGuard g(&node->m_lock, node->m_threadSafe);
                            needSub = (node->m_children[0] == nullptr);
                        }
                        if (needSub)
                        {
                            OptionalLockGuard g(&node->m_lock, node->m_threadSafe);
                            if (node->m_children[0] == nullptr)
                                node->Subdivide();
                        }
                    }

                    // 子スナップショット
                    OctreeNode* cc[CHILD_COUNT];
                    {
                        OptionalLockGuard g(&node->m_lock, node->m_threadSafe);
                        for (int k = 0; k < CHILD_COUNT; ++k)
                            cc[k] = node->m_children[k];
                    }

                    bool insChild = false;
                    if (cc[0] != nullptr)
                    {
                        for (int k = 0; k < CHILD_COUNT; ++k)
                        {
                            OctreeNode* ch = cc[k];
                            if (!ch) continue;

                            bool hit = false;
                            {
                                OptionalLockGuard gg(&ch->m_lock, ch->m_threadSafe);
                                hit = ch->m_boundary.intersects(tri->aabb);
                            }
                            if (!hit) continue;

                            if (self(self, ch)) insChild = true;
                        }
                    }

                    // 子に入らなかった場合は当該ノードへフォールバック
                    if (!insChild)
                    {
                        OptionalLockGuard g(&node->m_lock, node->m_threadSafe);
                        const int idx = node->m_triangles.getSize();
                        node->m_triangles.push_back(tri);
                        handle->AddRef(node, idx);
                        return true;
                    }
                    return true;
                };
            // ---- 再帰ラムダ end ----

            if (insertRec(insertRec, c))
                insertedToChild = true;
        }
    }

    // どの子にも入らなかった場合は現在ノードへ格納（フォールバック）
    if (!insertedToChild)
    {
        OptionalLockGuard guard(&m_lock, m_threadSafe);
        const int indexInNode = m_triangles.getSize();
        m_triangles.push_back(tri);
        handle->AddRef(this, indexInNode);

        // 成功したので tri→handle を静的レジストリに登録
        OptionalLockGuard reg(&s_lock, /*enable*/ true);
        s_registry[tri] = handle;
        return true;
    }

    // 子への挿入に成功している場合、参照は handle に蓄積済み。
    //    tri→handle を静的レジストリに最終登録する。
    {
        OptionalLockGuard reg(&s_lock, /*enable*/ true);
        s_registry[tri] = handle;
    }

    return true;
}
