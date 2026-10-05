#pragma once
//=============================================================================
//
// [Collider.h]
// Author : 
// 
//=============================================================================
#include "main.h"
#include "Utility/SimpleArray.h"
#include "Utility/Debug/DebugBoundingBoxRenderer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************

// 衝突器の種類を表す列挙型
enum class ColliderTag
{
    DEFAULT,
    PLAYER,
    ENEMY,
    NPC,
    TRIGGER,
    WALL,
	TRANSPARENT_WALL,
    AIR_WALL,
    ICE_WALL,
    TREE,
    ITEM,
    STATIC_OBJECT,
    BOSS_ICE,
    TELEPORTER,
    PLAYER_ATTACK,
    ENEMY_ATTACK,
    PLAYER_PROJECTILE,
	ENEMY_PROJECTILE,
    INTERACTABLE
};

enum class CollisionCallbackID
{
    Item,
    EnemyAttackHit,
    PlayerAttackHit,
    ProjectileHit,
};

// 前方宣言
class Collider;

//*****************************************************************************
// 構造体定義
//*****************************************************************************

// 衝突イベント構造体：衝突時に発行されるイベントメッセージ
struct CollisionEvent
{
    const Collider* colliderA;
    const Collider* colliderB;
	const void* additionalData; // 追加情報（必要に応じて使用）
};

// 通常のコライダーにコールバックを追加
typedef void (*CollisionCallback)(const CollisionEvent& event, void* context);
struct CollisionCallbackEntry
{
    //void (*callback)(const CollisionEvent& event, void* context, CollisionCallbackID callbackID);
    CollisionCallback callback;
    void* context;
    CollisionCallbackID callbackID; // コールバック識別ID（種類を区別するため）
};

class Collider
{
public:
    BOUNDING_BOX aabb; // ワールド座標での衝突判定用
    ColliderTag tag; // 衝突器の種類
    void* owner;
    bool enable;
	bool enableStaticCollision;         // 静的オブジェクトとの衝突を有効にするか
	bool enableDynamicCollision;        // 動的オブジェクトとの衝突を有効にするか
	bool enableTriggerEventCollision;   // イベント衝突を有効にするか
    bool isInitialized;

    // 通常コールバック（接触時に呼ばれる）
    SimpleArray<CollisionCallbackEntry> collisionCallbacks; // 複数コールバックを保持する配列
    //CollisionCallback onCollisionCallback = nullptr;
    //void* collisionContext = nullptr;

    Collider()
    {
        aabb = BOUNDING_BOX();
        tag = ColliderTag::DEFAULT;
        enable = false;
		enableStaticCollision = true;       // デフォルトでは静的オブジェクトとの衝突を有効
		enableDynamicCollision = true;      // デフォルトでは動的オブジェクトとの衝突を有効
        enableTriggerEventCollision = true; // デフォルトではイベント衝突を有効
        isInitialized = false;
        owner = nullptr;
    }

    void UpdateBoundingBox(const XMVECTOR& min, const XMVECTOR& max)
    {
        XMStoreFloat3(&aabb.minPoint, min);
        XMStoreFloat3(&aabb.maxPoint, max);
        isInitialized = true;
    }

    //void SetCollisionCallback(CollisionCallback callback, void* context)
    //{
    //    onCollisionCallback = callback;
    //    collisionContext = context;
    //}

    void AddCollisionCallback(CollisionCallback callback, void* context, CollisionCallbackID callbackID)
    {
        CollisionCallbackEntry entry;
        entry.callback = callback;
        entry.context = context;
        entry.callbackID = callbackID;
        collisionCallbacks.push_back(entry);
    }

    void CallCollisionCallbacks(const CollisionEvent& event, CollisionCallbackID callbackID) const
    {
        for (UINT i = 0; i < collisionCallbacks.getSize(); ++i)
        {
            const auto& entry = collisionCallbacks[i];
            if (entry.callback && entry.callbackID == callbackID)
                entry.callback(event, entry.context);
        }
    }

    //void CallCollisionCallback(const CollisionEvent& event) const
    //{
    //    if (onCollisionCallback)
    //    {
    //        onCollisionCallback(event, collisionContext);
    //    }
    //}
};

// コールバック関数
typedef void (*CollisionListener)(const CollisionEvent& event, void* context);

// 衝突イベントリスナーのエントリ構造体
struct CollisionListenerEntry
{
    CollisionListener listener; // コールバック関数ポインタ
    void* context;              // 任意のコンテキスト情報（呼び出し側で利用可能）
};

// EventBus クラス：イベントを購読・発行するシンプルなメッセージシステム
class EventBus
{
public:
    // 衝突イベントリスナーのリストを保持する
    SimpleArray<CollisionListenerEntry> collisionListeners;

    // 衝突イベントの購読を登録する（関数ポインタとコンテキストを登録）
    void subscribeCollision(CollisionListener listener, void* context)
    {
        CollisionListenerEntry entry;
        entry.listener = listener;
        entry.context = context;
        collisionListeners.push_back(entry);
    }

    // 衝突イベントを発行する（登録された全リスナーに通知する）
    void publishCollision(const CollisionEvent& event)
    {
        for (UINT i = 0; i < collisionListeners.getSize(); ++i)
        {
            collisionListeners[i].listener(event, collisionListeners[i].context);
        }
    }
};

// トリガーイベントのコールバック関数型
typedef void (*TriggerCallback)(const Collider* activator, void* context);


class TriggerEventCollider : public Collider
{
public:
    TriggerCallback onTriggerEnter; // 衝突時のイベント関数
	TriggerCallback onTriggerExit;  // 衝突終了時のイベント関数（必要に応じて）
    void* triggerContext;           // コールバック用の任意データ
    bool hasTriggered;              // 一度だけのトリガー用（再入防止など）

    TriggerEventCollider()
    {
        tag = ColliderTag::TRIGGER; // 固有タグで区別
        onTriggerEnter = nullptr;
		onTriggerExit = nullptr;
        triggerContext = nullptr;
        hasTriggered = false;
    }

    // トリガーイベントを設定する
    void SetTriggerCallback(TriggerCallback enter, TriggerCallback exit, void* context)
    {
        onTriggerEnter = enter;
        onTriggerExit = exit;
        triggerContext = context;
    }

	// トリガーを発火
    void TriggerEnter(const Collider* activator)
    {
        if (onTriggerEnter)
            onTriggerEnter(activator, triggerContext);
    }

	// トリガー終了を発火
    void TriggerExit(const Collider* activator)
    {
        if (onTriggerExit)
            onTriggerExit(activator, triggerContext);
    }

    // 再度トリガーできるようにする（任意）
    void ResetTrigger(void)
    {
        hasTriggered = false;
    }
};
