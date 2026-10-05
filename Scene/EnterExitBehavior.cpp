//=============================================================================
//
// [EnterExitBehavior.cpp]
// Author : 
// 
//=============================================================================
#include "Scene/EnterExitBehavior.h"
#include "Core/GameSystem.h"

void PlayerEnterExitBehavior::Initialize(void)
{
    m_prompt = new UIInteractPrompt();
    m_prompt->Initialize("data/TEXTURE/UI/interactUI.png");
    m_prompt->SetPosition(1200, 500);
    m_player = GameSystem::get_instance().GetPlayer();
}

void PlayerEnterExitBehavior::OnTrigger(const Collider* activator)
{
    if (activator->tag != ColliderTag::PLAYER)
        return;

    switch (m_actionType)
    {
    case EventCallbackAction::PlaceItem:
    {
        if (!m_player->HasSeed())
            return;

        PlaceItemContext* ctx = new PlaceItemContext();
		ctx->player = m_player;
		ctx->itemType = ItemType::Seed; // ここでは種を配置すること
        ctx->owner = this;
		ctx->postCallback = [](PlayerEnterExitBehaviorContext* ctx)
			{
                ctx->owner->m_actionType = EventCallbackAction::PlayerEventCallback;
			};

        m_prompt->SetCallback(m_player->OnPlaceItemPromptTriggered, ctx);
        UIManager::get_instance().SetActivePrompt(m_prompt);
        break;
    }
    case EventCallbackAction::StartClimbUp:
    {
        ClimbContext* climbCtx = new ClimbContext();
		climbCtx->player = m_player;
		climbCtx->climbState = PlayerState::START_CLIMBING_UP; // クライムアップ状態
        climbCtx->owner = this;

		// コールバック関数を設定
        climbCtx->postCallback = [](PlayerEnterExitBehaviorContext* ctx)
			{
                ctx->owner->m_actionType = EventCallbackAction::StopClimbDown; // 反転させる
                //ctx->owner->m_triggerAABB->minPoint = XMFLOAT3(-1501.6f, -4975.0f, 583.6f);
                //ctx->owner->m_triggerAABB->maxPoint = XMFLOAT3(-1319.2f, -4938.0f, 764.0f);
			};

        m_prompt->SetCallback(m_player->OnClimbPromptTriggered, climbCtx);
        UIManager::get_instance().SetActivePrompt(m_prompt);
        break;
    }
    case EventCallbackAction::StopClimbUp:
    {
        ClimbContext* climbCtx = new ClimbContext();
        climbCtx->player = m_player;
        climbCtx->climbState = PlayerState::STOP_CLIMBING_UP; // クライムアップ状態
        climbCtx->owner = this;

        // コールバック関数を設定
        climbCtx->postCallback = [](PlayerEnterExitBehaviorContext* ctx)
            {
                ctx->owner->m_actionType = EventCallbackAction::StartClimbDown; // 反転させる
            };
        m_player->GetPlayerAttributes().enableClimbUp = false; // クライムダウンを無効化

        m_prompt->SetCallback(m_player->OnClimbPromptTriggered, climbCtx);
        UIManager::get_instance().SetActivePrompt(m_prompt);
        break;
    }
    case EventCallbackAction::StartClimbDown:
    {
        ClimbContext* climbCtx = new ClimbContext();
        climbCtx->player = m_player;
        climbCtx->climbState = PlayerState::START_CLIMBING_DOWN; // クライムアップ状態
        climbCtx->owner = this;

        // コールバック関数を設定
        climbCtx->postCallback = [](PlayerEnterExitBehaviorContext* ctx)
            {
                ctx->owner->m_actionType = EventCallbackAction::StopClimbUp; // 反転させる
            };

        m_prompt->SetCallback(m_player->OnClimbPromptTriggered, climbCtx);
        UIManager::get_instance().SetActivePrompt(m_prompt);
        break;
    }
    case EventCallbackAction::StopClimbDown:
    {
        ClimbContext* climbCtx = new ClimbContext();
        climbCtx->player = m_player;
        climbCtx->climbState = PlayerState::STOP_CLIMBING_DOWN; // クライムアップ状態
        climbCtx->owner = this;
        // コールバック関数を設定
        climbCtx->postCallback = [](PlayerEnterExitBehaviorContext* ctx)
            {
                ctx->owner->m_actionType = EventCallbackAction::StartClimbUp; // 反転させる
                //ctx->owner->m_triggerAABB->minPoint = XMFLOAT3(-1501.6f, -4914.0f, 583.6f);
                //ctx->owner->m_triggerAABB->maxPoint = XMFLOAT3(-1319.2f, -4877.0f, 764.0f);
            };
        m_player->GetPlayerAttributes().enableClimbDown = false; // クライムダウンを無効化

        m_prompt->SetCallback(m_player->OnClimbPromptTriggered, climbCtx);
        UIManager::get_instance().SetActivePrompt(m_prompt);
        break;
    }
    case EventCallbackAction::ResetPosition:
    {
		struct ResetPositionContext
		{
            Player* player;
		};
        auto& fader = UIManager::get_instance().GetFader();
        fader.StartFadeOut();

        DeferredTaskOptions options{};
        options.async = false; // 同期で実行
        options.debugName = "Reset Position"; // デバッグ用の名前
        auto* ctx = new ResetPositionContext{ m_player }; // コンテキストを作成して渡す
        RawDeferredTask::Add(
            ctx, // コンテキストを渡す
            [](void* ptr)
            {
				Player* player = static_cast<ResetPositionContext*>(ptr)->player;
				player->ResetPlayerPosition(); // プレイヤー位置をリセット
                UIManager::get_instance().GetFader().StartFadeIn(); // タイトル画面をフェードアウト表示
            },
            [](void* ptr)
            {
                return UIManager::get_instance().GetFader().IsFullyHidden(); // 完全に暗くなったら実行する♪
            },
            options
        );

        break;
    }
    default:
        return;
    }
}

void PlayerEnterExitBehavior::OnExit(const Collider* activator)
{
    if (activator->tag != ColliderTag::PLAYER)
        return;

    switch (m_actionType)
    {
    case EventCallbackAction::StopClimbUp:
        m_player->GetPlayerAttributes().enableClimbUp = true; // クライムアップを有効化
        break;
    case EventCallbackAction::StopClimbDown:
		m_player->GetPlayerAttributes().enableClimbDown = true; // クライムダウンを有効化
        break;
    default:
        break;
    }

    m_interacted = false;
    UIManager::get_instance().ClearActivePrompt();
}

void EnemyEnterExitBehavior::OnTrigger(const Collider* activator)
{
    if (activator->tag != ColliderTag::ENEMY)
        return;

	Enemy* enemy = static_cast<Enemy*>(activator->owner);

    switch (m_actionType)
    {
    case EventCallbackAction::SuddenDeath:
        enemy->ReduceHP(999999.0f); // 即死
        break;
    default:
        break;
    }
}

void EnemyEnterExitBehavior::OnExit(const Collider* activator)
{
}
