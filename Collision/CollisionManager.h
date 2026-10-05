#pragma once
//=============================================================================
//
// 八分木と動的衝突体による当たり判定管理 [CollisionManager.h]
// Author : 
// 静的オブジェクト（八分木）と動的衝突体の交差判定、
// 衝突イベントの発行・購読システムを統合的に管理する
// 
//=============================================================================
#include "Collision/OctreeNode.h"
#include "Collision/VoxelGrid.h"
#include "Utility/SingletonBase.h"
#include "Utility/Debug/Debugproc.h"
#include "Collision/Collider.h"
#include "Model/Model.h"
#include "Utility/Debug/DebugTriangleRenderer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
enum class HitColliderType
{
    None,
    Enemy,
    Environment,
};

//*****************************************************************************
// 構造体定義
//*****************************************************************************
// コライダーのペア構造体（常に小さいアドレスが first）
struct ColliderPair
{
    const Collider* collider1;
    const Collider* collider2;

    ColliderPair(const Collider* col1, const Collider* col2)
    {
        if (col1 < col2)
        {
            collider1 = col1;
            collider2 = col2;
        }
        else
        {
            collider1 = col2;
            collider2 = col1;
        }
    }
};

// コライダーペアのハッシュ関数
struct HashColliderPair
{
    size_t operator()(const ColliderPair& pair) const
    {
        size_t h1 = reinterpret_cast<size_t>(pair.collider1);
        size_t h2 = reinterpret_cast<size_t>(pair.collider2);
        return h1 ^ (h2 << 1); // シンプルなXOR合成
    }
};

// コライダーペアの比較関数
struct EqualColliderPair
{
    bool operator()(const ColliderPair& lhs, const ColliderPair& rhs) const
    {
        return lhs.collider1 == rhs.collider1 && lhs.collider2 == rhs.collider2;
    }
};


// CollisionManager クラス：静的オブジェクト（八分木）と動的オブジェクトの衝突管理
class CollisionManager : public SingletonBase<CollisionManager>, IDebugUI
{
public:
    void Update(void);
    void Init(void);
	// 静的オブジェクトの八分木を初期化する
    void InitOctree(const BOUNDING_BOX& boundingBox);
    bool IsInitializedOctree(void) const { return m_staticOctree != nullptr; }
    bool OctreeInsertTriangle(const Triangle* tri, bool insertWithHandle = false);

    // 静的オブジェクトのボクセルグリッドを初期化する
    void InitVoxelGrid(const SimpleArray<Triangle*>& triangles, float voxelSize = 1.0f);
	// ボクセルグリッドに三角形を挿入する
    void VoxelGridInsertTriangle(const Triangle* tri);
    void VoxelGridInsertTriangles(const SimpleArray<Triangle*>& triangles);

    // 動的衝突体の登録&解除
    void RegisterDynamicCollider(const Collider* collider) { m_dynamicColliders.push_back(collider); }
    void UnregisterDynamicCollider(const Collider* collider);
    // 衝突イベントを発行するためのコライダーを登録&解除
	void RegisterEventCollider(Collider* collider) { m_triggerEventColliders.push_back(collider); }
	void UnregisterEventCollider(Collider* collider);

    // 動的衝突体リストのクリア
    void ClearDynamicColliders(void) { m_dynamicColliders.clear(); }
	// 衝突イベントコライダーリストのクリア
	void ClearEventColliders(void) { m_triggerEventColliders.clear(); }

    void RemoveTriangleFromStaticOctree(const Triangle* tri) { m_staticOctree->RemoveTriangle(tri); }
    void RemoveTriangleFromVoxelGrid(const Triangle* tri) { m_voxelGrid->RemoveTriangleIncremental(tri); }

	// レイキャスト関数（rayOrigin から rayDir 方向に伸びるレイと静的オブジェクトの交差判定）
    bool RaycastForTargetPoint(const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDir, XMFLOAT3* targetHitPosOut) const;

    // シーン切り替え時に呼び出すVoxelMapping登録関数
    void PrepareVoxelMappingAfterSceneChange(void);

    const VoxelGrid* GetVoxelGrid(void) { return m_voxelGrid; }

private:
	void BlockColliderMovement(const Collider* collider, const Collider* collider2);
    void HandleStaticCollision(const Collider* dynamicCol);
    void HandleDynamicCollision(const Collider* dynamicCol, UINT startIdx);
    void HandleEventTriggerCollision(const Collider* dynamicCol);

    // 動的オブジェクト同士の衝突判定
	bool IsSelfCollision(const Collider* collider1, const Collider* collider2);
	// イベント衝突判定（衝突イベントの発行・購読）
	void CheckEventCollision(const Collider* dynamicCol, Collider* triggerCol);
    // 精密衝突判定関数（Collider と Triangle 間の衝突判定）
    bool IsPointInTriangle(XMVECTOR p, XMVECTOR a, XMVECTOR b, XMVECTOR c);
    // 三角形上の最近点を求める関数
    void ClosestPointOnTriangle(const XMFLOAT3& p, const XMFLOAT3& a,
        const XMFLOAT3& b, const XMFLOAT3& c, XMFLOAT3& out);
    // 壁との衝突判定のためのイプシロン値を計算
	float ComputeWallCollisionEpsilon(const BOUNDING_BOX& box);

    void TriggerOnExit(void);
    virtual void RenderDebugInfo(void) override;

	bool m_initialized = false; // 初期化フラグ
    bool m_drawBoundingBox = true;
    BOUNDING_BOX m_curCollisionBox; // 現在のバウンディングボックス（デバッグ用）
    DebugBoundingBoxRenderer m_debugBoundingBoxRenderer; // デバッグ用のバウンディングボックス描画
	DebugTriangleRenderer m_debugTriangleRenderer; // デバッグ用の三角形描画
	SimpleArray<Triangle> m_debugTriangles; // デバッグ用の三角形リスト

    // 静的オブジェクト（例：地面モデル）の八分木
    OctreeNode* m_staticOctree = nullptr;
    // 動的オブジェクトのボクセルグリッド（例：パーティクル）
    VoxelGrid* m_voxelGrid = nullptr;

    // 動的オブジェクト（プレイヤー、敵、その他）の衝突体リスト
    SimpleArray<const Collider*> m_dynamicColliders;

	// 衝突イベントを発行するためのコライダーリスト
    SimpleArray<Collider*> m_triggerEventColliders;

    // 毎フレームの接触ペア記録（フレーム前後の比較に使用）
    HashMap<ColliderPair, bool, HashColliderPair, EqualColliderPair>* m_currentFramePairs = nullptr;
    HashMap<ColliderPair, bool, HashColliderPair, EqualColliderPair>* m_lastFramePairs = nullptr;

    // 実体（2つのバッファ）
    HashMap<ColliderPair, bool, HashColliderPair, EqualColliderPair> m_framePairsA;
    HashMap<ColliderPair, bool, HashColliderPair, EqualColliderPair> m_framePairsB;

    // イベントバスのインスタンス
    EventBus eventBus;
};


