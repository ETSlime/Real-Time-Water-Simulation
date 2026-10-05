#pragma once
//=============================================================================
//
// [ExitBehavior.h]
// Author : 
// 
//=============================================================================
#include "UI/Base/UIInteractPrompt.h"
#include "UI/Base/UIManager.h"
#include "Scene/Player.h"
#include "Scene/Enemy.h"
#include "Scene/EventTrigger.h"

// 前方宣言
class PlayerEnterExitBehavior;

//*****************************************************************************
// 構造体定義
//*****************************************************************************

struct PlayerEnterExitBehaviorContext
{
	// このクライムコンテキストを所有するオーナー
	PlayerEnterExitBehavior* owner = nullptr;
	// メイン処理後に呼ばれるコールバック関数ポインタ
	void (*postCallback)(PlayerEnterExitBehaviorContext* ctx) = nullptr;
	PlayerEnterExitBehaviorContext() : owner(nullptr), postCallback(nullptr) {}
	PlayerEnterExitBehaviorContext(PlayerEnterExitBehavior* owner, void (*callback)(PlayerEnterExitBehaviorContext* ctx) = nullptr)
		: owner(owner), postCallback(callback) {
	}
	virtual ~PlayerEnterExitBehaviorContext() {};
};

struct PlaceItemContext : PlayerEnterExitBehaviorContext
{
	Player* player; // プレイヤーキャラクターへのポインタ
	ItemType itemType; // 配置するアイテムの種類
};

struct ClimbContext : PlayerEnterExitBehaviorContext
{
	Player* player; // プレイヤーキャラクターへのポインタ
	PlayerState climbState; // クライム状態
};

enum class EventCallbackAction
{
	PlayerEventCallback = 0, // プレイヤーイベントコールバック
	PlaceItem,		// アイテムを配置するアクション
	StartClimbUp,	// クライムアップアクション
	StopClimbUp,	// クライムアップ停止アクション
	StartClimbDown, // クライムダウンアクション
	StopClimbDown,	// クライムダウン停止アクション
	ResetPosition,	// プレイヤー位置リセットアクション

	EnemyEventCallback, // エネミーイベントコールバック
	SuddenDeath,	// 突然死アクション
	Max				// 最大値
};

class PlayerEnterExitBehavior : public IEventBehavior
{
public:
	PlayerEnterExitBehavior() = default;
	PlayerEnterExitBehavior(EventCallbackAction action)
		: m_actionType(action) {}

	void Initialize(void) override;
	void SetAction(EventCallbackAction action) { m_actionType = action; }
	void SetTriggerAABB(BOUNDING_BOX* aabb) override { m_triggerAABB = aabb; }
    void OnTrigger(const Collider* activator) override;

    void OnExit(const Collider* activator) override;

private:
	bool m_interacted = false; // インタラクションが行われたかどうか
	EventCallbackAction m_actionType = EventCallbackAction::PlayerEventCallback; // 現在のアクションタイプ
	Player* m_player = nullptr; // プレイヤーキャラクターへのポインタ
	UIInteractPrompt* m_prompt = nullptr; // インタラクションプロンプトへのポインタ
	BOUNDING_BOX* m_triggerAABB = nullptr; // 
};

class EnemyEnterExitBehavior : public IEventBehavior
{
public:
	EnemyEnterExitBehavior() = default;
	EnemyEnterExitBehavior(EventCallbackAction action)
		: m_actionType(action) {}

	void SetAction(EventCallbackAction action) { m_actionType = action; }
	void SetTriggerAABB(BOUNDING_BOX* aabb) override { m_triggerAABB = aabb; }
	void OnTrigger(const Collider* activator) override;

	void OnExit(const Collider* activator) override;

private:
	bool m_interacted = false; // インタラクションが行われたかどうか
	EventCallbackAction m_actionType = EventCallbackAction::EnemyEventCallback; // 現在のアクションタイプ
	BOUNDING_BOX* m_triggerAABB = nullptr; // 
};