//=============================================================================
//
// player処理 [Player.cpp]
// Author : 
//
//=============================================================================
#include "Utility/InputManager.h"
#include "Collision/OctreeNode.h"
#include "Core/Camera.h"
#include "Core/CursorManager.h"
#include "Core/Graphics/Renderer.h"
#include "Core/GameSystem.h"
#include "Core/SaveSystem.h"
#include "Core/AudioManager.h"
#include "Scene/Player.h"
#include "Scene/Weapon/ProjectileManager.h"
#include "Scene/EnterExitBehavior.h"


//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define ROTATION_SPEED				(0.18f)
#define MAX_FALL_SPEED 				(25.0f)
#define GRAVITY						(300.0f)
#define SPD_DECAY_RATE				(0.93f)
#define THROW_SPEED					(45.0f)

//=============================================================================
// 初期化処理
//=============================================================================
Player::Player()
{
#ifdef _DEBUG
	DebugProc::get_instance().Register(this);
#endif // DEBUG

	//sigewinne = new Sigewinne();

	//klee =  new Klee();

	lumine = new Lumine();
	lumine->SetPlayerComponent(this);

	//hilichurl = new Hilichurl();

	//mitachurl = new Mitachurl();

	m_playerAttr.allowControl = true;
	m_playerAttr.isMeleeMode = true; // 初期は近接モード
	m_playerAttr.seedCount = 0; // 初期は種を持っていない
	m_playerAttr.keyCount = 0;
	m_playerAttr.plantedSeed = false;

	Transform transform;
	transform.pos = PLAYER_INIT_POS;
	transform.scl = XMFLOAT3(1.2f, 1.2f, 1.2f);
	m_playerAttr.initPos = PLAYER_INIT_POS;
	lumine->SetTransform(transform);
	lumine->SetPlayerAttributes(&m_playerAttr);
	m_playerGO = lumine;
	m_playerGO->SetDrawWorldAABB(m_drawBoundingBox);
	m_playerGO->SetPosition(m_playerAttr.initPos);

	m_light = new DirectionalLight();
	m_light->SetEnable(true);
	m_light->BindToTransform(m_playerGO->GetTransformP());
	m_lightMgr.AddLight(m_light);

	m_previewRenderer.Initialize();
	m_predictor.SetCollisionVoxelGrid(CollisionManager::get_instance().GetVoxelGrid()); // 地形ボクセルをセット

}

Player::~Player()
{
	SAFE_DELETE(lumine);
}

void Player::Update(void)
{
	// 無敵時間のカウントダウン
	if (m_invincibleTimer > 0.0f) {
		m_invincibleTimer -= m_playerGO->m_timer.GetScaledDeltaTime();
	}

	if (!m_playerGO->GetInstance()->load)
	{
		m_playerGO->Update();
		return;
	}

	if (m_currentModelLoadCnt < m_totalModelLoadCnt && lumine->LoadFinished())
	{
		m_currentModelLoadCnt++;
	}
	
	m_attributes = m_playerGO->GetAttributes();
	m_transform = m_playerGO->GetTransform();

	if (m_inputManager.GetKeyboardTrigger(KEY_SWITCH_WEAPON) 
		&& m_playerGO->GetStateMachine()->GetCurrentState() != STATE(PlayerState::ATTACK_1)
		&& m_playerGO->GetStateMachine()->GetCurrentState() != STATE(PlayerState::ATTACK_2)
		&& m_playerGO->GetStateMachine()->GetCurrentState() != STATE(PlayerState::ATTACK_3))
	{
		m_playerAttr.isMeleeMode = !m_playerAttr.isMeleeMode;
		AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_switch_weapon);
		if (!m_playerAttr.isMeleeMode && !m_playerAttr.finishStoreWeapon && !m_playerAttr.weaponOnBack)
			m_playerAttr.storeWeaponOnBack = true;
	}

	if (m_playerAttr.climbMode)
		UpdateClimbMode();
	else
		UpdateNormalMode();
	
	HandlePlayerMove(m_transform);
	m_playerGO->SetTransform(m_transform);

	m_attributes->spd *= SPD_DECAY_RATE;
	m_playerGO->Update();

	m_projectileFollowerManager.UpdateAll();


#ifdef _DEBUG
	m_debugProc.PrintDebugProc("PosX: %f PosY: %f, PosZ: %f\n", m_transform.pos.x, m_transform.pos.y, m_transform.pos.z);
	m_debugProc.PrintDebugProc("HP: %f\n", m_currentHP);
#endif
}

void Player::Draw(void)
{
	m_playerGO->Draw();

}

void Player::DrawEffect(void)
{
	m_playerGO->DrawEffect();

	if (m_playerAttr.throwMode)
	{
		m_previewRenderer.Draw();
	}
}

void Player::HandlePlayerMove(Transform& transform)
{
	float deltaDir = m_attributes->targetDir - m_attributes->dir;
	if (deltaDir > XM_PI) deltaDir -= XM_2PI;
	if (deltaDir < -XM_PI) deltaDir += XM_2PI;
	m_attributes->dir += deltaDir * ROTATION_SPEED * m_playerGO->m_timer.GetScaledDeltaTime();

	transform.rot.y = m_attributes->dir;

	if (m_attributes->isMoveBlocked)
		return;

	if (m_playerAttr.aimMode || m_playerAttr.throwMode)
	{
		// カメラ基準のローカル移動方向を計算
		XMFLOAT3 moveDir = { 0, 0, 0 };

		if (m_inputManager.GetKeyboardPress(DIK_W)) moveDir.z += 1.0f; // 前
		if (m_inputManager.GetKeyboardPress(DIK_S)) moveDir.z -= 1.0f; // 後
		if (m_inputManager.GetKeyboardPress(DIK_D)) moveDir.x += 1.0f; // 右
		if (m_inputManager.GetKeyboardPress(DIK_A)) moveDir.x -= 1.0f; // 左

		// 正規化（斜め移動で速くならないように）
		XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&moveDir));

		// カメラのright/forwardを使ってワールド方向に変換
		XMVECTOR forward = XMLoadFloat3(&m_playerGO->GetTransform().GetForward());
		forward = XMVectorSetY(forward, 0.0f); // 水平に限定
		forward = XMVector3Normalize(forward);

		XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), forward));

		XMVECTOR worldMove = right * moveDir.x + forward * moveDir.z;
		worldMove = XMVector3Normalize(worldMove);

		// 速度適用
		XMFLOAT3 finalMove;
		XMStoreFloat3(&finalMove, worldMove * m_attributes->spd);

		transform.pos.x += finalMove.x;
		transform.pos.z += finalMove.z;

	}
	else
	{
		// 入力のあった方向へプレイヤーを向かせて移動させる
		float speed = m_attributes->spd;
		if (m_attributes->isRunning)
			speed *= VALUE_PLAYEER_RUN_FACTOR;

		if (m_playerAttr.climbMode)
		{
			transform.pos.y += speed;
		}
		else
		{
			transform.pos.x += sinf(transform.rot.y) * speed;
			transform.pos.z += cosf(transform.rot.y) * speed;
		}
	}



#ifdef _DEBUG
	if (m_inputManager.GetKeyboardPress(DIK_TAB))
	{
		transform.pos.x += sinf(transform.rot.y) * m_attributes->spd * 5 * m_playerGO->m_timer.GetScaledDeltaTime();
		transform.pos.z += cosf(transform.rot.y) * m_attributes->spd * 5 * m_playerGO->m_timer.GetScaledDeltaTime();
	}
#endif // DEBUG

	if (m_inputManager.GetKeyboardPress(DIK_TAB))
	{
		transform.pos.x += sinf(transform.rot.y) * m_attributes->spd * 5 * m_playerGO->m_timer.GetScaledDeltaTime();
		transform.pos.z += cosf(transform.rot.y) * m_attributes->spd * 5 * m_playerGO->m_timer.GetScaledDeltaTime();
	}

}

void Player::AlignPlayerYawToCamera(void)
{
	// カメラの向き
	float targetYaw = m_camera.GetRotation().y;
	// 補間速度（値を上げると追従が速い）
	float lerpSpeed = 10.0f;
	float t = min(1.0f, m_playerGO->m_timer.GetDeltaTime() * lerpSpeed);

	// 線形補間で滑らかに回転を追従
	m_attributes->dir = SmoothAngleLerp(m_attributes->dir, targetYaw, t);

	// targetDir は常に目標値として更新
	m_attributes->targetDir = targetYaw;
}

Transform Player::GetTransform(void) const
{
	return m_playerGO->GetTransform();
}

void Player::InitializePlayerStatus(void)
{
	if (lumine)
	{
		Transform transform = lumine->GetTransform();
		transform.pos = PLAYER_INIT_POS;
		lumine->SetTransform(transform);
		lumine->InitializeStatus();
	}

	m_maxHP = 999999.0f;
	m_currentHP = 99999.0f;

	m_playerAttr.seedCount = 0;
	m_playerAttr.keyCount = 0;
	m_playerAttr.plantedSeed = false;

	Update();
}

void Player::LoadSaveData(const SaveData& saveData)
{
	SetPosition(saveData.pos);
	m_playerAttr.seedCount = saveData.seedCount;
	m_playerAttr.bulletCount = saveData.bulletCount;
	m_playerAttr.keyCount = saveData.keyCount;
	m_playerAttr.hasBullet = saveData.hasBullet;
}

void Player::SetClimbMode(PlayerState climbState)
{
	m_playerAttr.drawGun = false;

	if (climbState == PlayerState::START_CLIMBING_UP ||
		climbState == PlayerState::START_CLIMBING_DOWN)
	{
		// クライムモードを有効にする
		m_playerAttr.climbMode = true;
		m_playerAttr.enableClimbUp = false;
		m_playerAttr.enableClimbDown = false;

		// クライムモードではコリジョンを無効化
		m_playerGO->SetStaticCollisionEnable(false);
		m_playerGO->SetDynamicCollisionEnable(false);

		DeferredTaskOptions options{};
		options.async = false; // 同期で実行
		options.debugName = "ClimbFade"; // デバッグ用の名前
		options.delay = 2.5f; // 遅延時間を設定
		options.pauseDelay = true; // 一時停止中でも遅延を実行する

		RawDeferredTask::Add(
			nullptr,
			[](void* ptr)
			{
				UIManager::get_instance().GetFader().StartFadeOut();
			},
			nullptr,
			options
		);
	}
	else if (climbState == PlayerState::STOP_CLIMBING_UP ||
		climbState == PlayerState::STOP_CLIMBING_DOWN)
	{
		// クライムモードを無効にする
		m_playerAttr.climbMode = false;
		m_attributes->isMoveBlocked = true;

		DeferredTaskOptions options{};
		options.async = false; // 同期で実行
		options.debugName = "ClimbFade"; // デバッグ用の名前
		options.delay = 2.5f; // 遅延時間を設定
		options.pauseDelay = true; // 一時停止中でも遅延を実行する

		RawDeferredTask::Add(
			nullptr,
			[](void* ptr)
			{
				UIManager::get_instance().GetFader().StartFadeOut();
			},
			nullptr,
			options
		);
	}
	else
		return;

	// クライム状態を設定
	dynamic_cast<Lumine*>(m_playerGO)->SetClimb(climbState);
}

void Player::PlaceItem(ItemType type)
{
	m_playerAttr.drawGun = false;

	Item* item = m_itemManager.GetItem(type);
	if (item == nullptr || item->GetUsed())
		return;

	//item->Use();
	lumine->PlaceItem(type);
}

void Player::OnClimbPromptTriggered(void* userData)
{
	ClimbContext* ctx = static_cast<ClimbContext*>(userData);
	if (ctx == nullptr || ctx->player == nullptr)
		return;

	PlayerState climbState = ctx->climbState;
	Player* player = ctx->player;

	player->SetClimbMode(climbState);

	// コールバック関数が設定されている場合は実行
	if (ctx->postCallback)
	{
		ctx->postCallback(ctx);
	}
}

void Player::OnPlaceItemPromptTriggered(void* userData)
{
	PlaceItemContext* ctx = static_cast<PlaceItemContext*>(userData);
	if (ctx == nullptr || ctx->player == nullptr)
		return;

	ItemType itemType = ctx->itemType;
	Player* player = ctx->player;
	player->PlaceItem(itemType);
}

void Player::UpdateNormalMode(void)
{
	// アクションキューの更新
	UpdateActionQueue();

	if (m_playerAttr.allowControl)
	{
		// キャラクターの切り替え
		UpdateSwitchCharacter();

		// 移動させちゃう
		UpdatePlayerMove();
		
		// 攻撃の更新
		UpdatePlayerAttack();

		// 入力の処理
		HandleInput();

		if (m_playerAttr.throwMode)
		{
			Item* item = m_itemManager.GetPickedItem(ItemType::Seed);
			if (item)
			{
				XMMATRIX M_hand_world = m_playerGO->GetSkinnedMeshModel()->GetBodyTransformMtx(BodyTransformIndex::RightHand);
				XMMATRIX M_local_pos = XMMatrixTranslation(-38.6f, 84.6f, 1.8f);
				XMMATRIX M_local_rot = XMMatrixRotationRollPitchYaw(m_transform.rot.x, m_transform.rot.y, m_transform.rot.z);
				XMMATRIX M_local = M_local_rot * M_local_pos;

				XMMATRIX M_item_world = XMMatrixMultiply(M_local, M_hand_world);
				M_item_world = XMMatrixMultiply(M_item_world, m_playerGO->GetInstance()->transform.mtxWorld);

				XMVECTOR S, R, T;
				XMMatrixDecompose(&S, &R, &T, M_item_world);

				XMFLOAT3 startPos;
				XMStoreFloat3(&startPos, T);

				XMFLOAT3 rotEuler;
				{
					XMFLOAT4 q;
					XMStoreFloat4(&q, R);

					XMVECTOR quat = XMLoadFloat4(&q);
					XMFLOAT3 angles;
					XMStoreFloat3(&angles, XMQuaternionRotationMatrix(XMMatrixRotationQuaternion(quat)));

					XMFLOAT4X4 rotMtx;
					XMStoreFloat4x4(&rotMtx, XMMatrixRotationQuaternion(quat));

					rotEuler.y = atan2f(rotMtx._13, rotMtx._33);                        // Yaw
					rotEuler.x = asinf(-rotMtx._23);                                    // Pitch
					rotEuler.z = atan2f(rotMtx._21, rotMtx._22);                        // Roll
				}

				item->SetPosition(startPos);
				XMFLOAT3 itemScale = XMFLOAT3(SEED_SIZE * 0.5f, SEED_SIZE * 0.5f, SEED_SIZE * 0.5f);
				item->SetScale(itemScale);
				item->SetRotation(XMFLOAT3(m_transform.rot.x - 2.0f, m_transform.rot.y + 0.2f, m_transform.rot.z + 0.3f));
				item->GetAttributes()->use = true;

				XMFLOAT3 hitPos{}, hitNormal{};
				m_predictor.ComputeTrajectory(
					startPos,
					m_camera.GetThrowModeDirection(),
					THROW_SPEED,
					75.0f,		// 最大予測時間
					0.15f,		// サンプリング間隔
					&hitPos,	// ヒット位置
					&hitNormal	// ヒット法線
				);

				m_previewRenderer.Update(m_predictor.GetTrajectoryPoints(), hitPos, hitNormal);
			}
		}
	}

	// プレイヤーの状態を更新
	UpdatePlayerStatus();

#ifdef _DEBUG
	if (m_inputManager.GetKeyboardPress(DIK_UP))
	{
		m_transform.pos.y += 200;
	}
	if (m_inputManager.GetKeyboardPress(DIK_DOWN))
	{
		m_transform.pos.y -= 50;
	}
#endif

	if (m_inputManager.GetKeyboardPress(DIK_UP))
	{
		m_transform.pos.y += 200;
	}
	if (m_inputManager.GetKeyboardPress(DIK_DOWN))
	{
		m_transform.pos.y -= 50;
	}

	/// INTERACTABLE-IMPLEMENTATION
	/*if (m_inputManager.GetKeyboardTrigger(DIK_SPACE))
	{
		m_attributes.isJumping = true;
	}

	*/
	if (m_linkedInteractable != nullptr) {

		if (m_InteractPromptTexLoc != m_linkedInteractable->GetPromptTextureLocation()) {
		
			UIManager::get_instance().ClearActivePrompt();

			delete m_InteractPrompt;

			m_InteractPrompt = nullptr;
		
			m_InteractPromptTexLoc = m_linkedInteractable->GetPromptTextureLocation();
		}

		if (m_InteractPrompt == nullptr && m_InteractPromptTexLoc != nullptr) {
			m_InteractPrompt = new UIInteractPrompt();

			const char* promptTexture = m_InteractPromptTexLoc != nullptr? m_InteractPromptTexLoc : "data/TEXTURE/UI/interactUI.png";

			m_InteractPrompt->Initialize(promptTexture);
			m_InteractPrompt->SetPosition(1200, 500); // 画面中央に配置
			UIManager::get_instance().SetActivePrompt(m_InteractPrompt);
		}

		if (m_inputManager.GetKeyboardTrigger(DIK_F)) {
			
			/*if (m_linkedInteractable->GetItemType() == ItemType::IntSoil && m_attributes.hasSeed) {
				m_linkedInteractable->Use();
				m_linkedInteractable->SetActive(false);
			}
			else if (m_linkedInteractable->GetItemType() == ItemType::IntVines) {
				m_transform.pos.y += 500;
			}*/

			m_linkedInteractable->Interact();

		}

		m_linkedInteractable = nullptr;

	}
	else {

		if (m_InteractPrompt != nullptr) {

			UIManager::get_instance().ClearActivePrompt();

			delete m_InteractPrompt;

			m_InteractPrompt = nullptr;
		}
	
	}

}

void Player::UpdateClimbMode(void)
{
	if (m_playerAttr.enableClimbDown && m_inputManager.GetKeyboardPress(DIK_S))
	{	
		// 下へ移動
		m_attributes->spd = -VALUE_PLAYER_CLIMB;
		m_playerAttr.climbDown = true;
		m_playerAttr.climbUp = false;

	}
	else if (m_playerAttr.enableClimbUp && m_inputManager.GetKeyboardPress(DIK_W))
	{
		// 上へ移動
		m_attributes->spd = VALUE_PLAYER_CLIMB;
		m_playerAttr.climbUp = true;
		m_playerAttr.climbDown = false;
	}
	else
	{
		m_playerAttr.climbUp = false;
		m_playerAttr.climbDown = false;
	}
}

void Player::UpdateActionQueue(void)
{
	m_playerAttr.actionQueueClearTime++;
	for (UINT i = m_playerAttr.actionQueueStart; i < m_playerAttr.actionQueueEnd; i++)
	{
		m_playerAttr.actionQueue[i].liveTime += m_playerGO->m_timer.GetScaledDeltaTime();
		if (m_playerAttr.actionQueue[i].liveTime >= ACTION_QUEUE_CLEAR_WAIT)
		{
			m_playerAttr.actionQueueStart++;
			continue;
		}

		if (dynamic_cast<ISkinnedMeshModelChar*>(m_playerGO)->ExecuteAction(m_playerAttr.actionQueue[i].actionType))
		{
			m_playerAttr.actionQueueStart++;
		}
		else
		{
			break;
		}
	}
	if (m_playerAttr.actionQueueStart >= m_playerAttr.actionQueueEnd)
	{
		m_playerAttr.actionQueueStart = m_playerAttr.actionQueueEnd = 0;
	}
}

void Player::UpdatePlayerStatus(void)
{
	if (m_playerAttr.cancelThrowFlag)
	{
		CancelThrow();
		m_playerAttr.cancelThrowFlag = false;
	}

	// 地面にいるかどうか
	if (!m_attributes->isGrounded)
	{
		// 滑空時間を更新
		m_attributes->glideDuration += m_playerGO->m_timer.GetDeltaTime();

		// 落下処理
		m_attributes->fallSpeed += GRAVITY * m_playerGO->m_timer.GetDeltaTime();
		// 最大落下速度を制限
		if (m_attributes->fallSpeed < MAX_FALL_SPEED)
		{
			m_attributes->fallSpeed = MAX_FALL_SPEED;
		}

		// 位置を更新
		m_transform.pos.y -= m_attributes->fallSpeed * m_playerGO->m_timer.GetDeltaTime();
	}

	// ヒット中かどうか
	if (m_attributes->isHit1 || m_attributes->hitTimer > 0)
	{
		m_attributes->hitTimer -= m_playerGO->m_timer.GetScaledDeltaTime();
		if (m_attributes->hitTimer < 0)
		{
			m_attributes->isHit1 = false;
		}
	}

	if (m_attributes->lastFireTimeCountDown > 0)
	{
		m_attributes->lastFireTimeCountDown -= m_playerGO->m_timer.GetDeltaTime();
	}

	if (m_playerAttr.throwMode)
		AlignPlayerYawToCamera();

	if (m_playerAttr.fireCooldownSec > 0.0f)
		m_playerAttr.fireCooldownSec -= m_playerGO->m_timer.GetDeltaTime();
}

void Player::UpdatePlayerMove(void)
{
	if (!m_attributes->isAttacking && !m_attributes->isAttacking2 && !m_attributes->isAttacking3 && !m_attributes->isHit1)
	{
		if (m_inputManager.GetKeyboardPress(DIK_A))
		{	// 左へ移動
			m_attributes->spd = VALUE_PLAYER_MOVE;
			m_attributes->isMoving = true;

			if (m_inputManager.GetKeyboardPress(DIK_W))
				m_attributes->targetDir = -XM_PI * 3 / 4 + m_camera.GetRotation().y;
			else if (m_inputManager.GetKeyboardPress(DIK_S))
				m_attributes->targetDir = -XM_PI / 4 + m_camera.GetRotation().y;
			else
				m_attributes->targetDir = -XM_PI / 2 + m_camera.GetRotation().y;
		}
		if (m_inputManager.GetKeyboardPress(DIK_D))
		{	// 右へ移動
			m_attributes->spd = VALUE_PLAYER_MOVE;
			m_attributes->isMoving = true;

			if (m_inputManager.GetKeyboardPress(DIK_W))
				m_attributes->targetDir = XM_PI * 3 / 4 + m_camera.GetRotation().y;
			else if (m_inputManager.GetKeyboardPress(DIK_S))
				m_attributes->targetDir = XM_PI / 4 + m_camera.GetRotation().y;
			else
				m_attributes->targetDir = XM_PI / 2 + m_camera.GetRotation().y;
		}
		if (m_inputManager.GetKeyboardPress(DIK_S))
		{	// 下へ移動
			m_attributes->spd = VALUE_PLAYER_MOVE;
			m_attributes->isMoving = true;

			if (m_inputManager.GetKeyboardPress(DIK_A))
				m_attributes->targetDir = -XM_PI * 3 / 4 + m_camera.GetRotation().y;
			else if (m_inputManager.GetKeyboardPress(DIK_D))
				m_attributes->targetDir = -XM_PI * 5 / 4 + m_camera.GetRotation().y;
			else
				m_attributes->targetDir = -XM_PI + m_camera.GetRotation().y;
		}
		if (m_inputManager.GetKeyboardPress(DIK_W))
		{	// 上へ移動
			m_attributes->spd = VALUE_PLAYER_MOVE;
			m_attributes->isMoving = true;

			if (m_inputManager.GetKeyboardPress(DIK_A))
				m_attributes->targetDir = -XM_PI / 4 + m_camera.GetRotation().y;
			else if (m_inputManager.GetKeyboardPress(DIK_D))
				m_attributes->targetDir = XM_PI / 4 + m_camera.GetRotation().y;
			else
				m_attributes->targetDir = 0.0f + m_camera.GetRotation().y;
		}
	}

	if (m_inputManager.GetKeyboardTrigger(DIK_SPACE) && !m_attributes->isJumping && m_attributes->isGrounded)
	{
		m_attributes->isJumping = true;
		m_playerAttr.groundY = m_transform.pos.y;
	}
}

void Player::UpdatePlayerAttack(void)
{
	// 発射
	if (m_inputManager.IsMouseLeftTriggered() && !CursorManager::get_instance().IsMouseFreeMode())
	{

		if (m_playerAttr.throwMode)
		{
			ConfirmThrow();
			AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_throw);
		}


	}

	if (m_inputManager.IsMouseRightTriggered())
	{
		if (m_playerAttr.throwMode)
			CancelThrow();
	}


	// 攻撃
	if (m_inputManager.IsMouseLeftTriggered() && !CursorManager::get_instance().IsMouseFreeMode())
	{
		if (m_playerAttr.isMeleeMode)
		{
			PlayerAction action;
			action.actionType = ActionEnum::ATTACK;
			action.liveTime = 0;
			m_playerAttr.actionQueue[m_playerAttr.actionQueueEnd] = action;
			m_playerAttr.actionQueueEnd = (m_playerAttr.actionQueueEnd + 1) % ACTION_QUEUE_SIZE;
			m_playerAttr.finishStoreWeapon = false;

			// キューのオーバーフローを防止
			if (m_playerAttr.actionQueueEnd == m_playerAttr.actionQueueStart)
			{
				m_playerAttr.actionQueueStart = (m_playerAttr.actionQueueStart + 1) % ACTION_QUEUE_SIZE; // 最も古いアクションを破棄
			}

		}
	}
}

void Player::UpdateSwitchCharacter(void)
{
	if (lumine && m_inputManager.GetKeyboardTrigger(DIK_1))
	{
		lumine->SetTransform(m_transform);
		//lumine->SetRenderProgress(0.0f);
		//lumine->SetSwitchCharEffect(true);
		m_playerGO = lumine;
		m_light->BindToTransform(m_playerGO->GetTransformP());
	}
	//if (klee && m_inputManager.GetKeyboardTrigger(DIK_2))
	//{
	//	klee->SetTransform(m_transform);
	//	//klee->SetRenderProgress(0.0f);
	//	//klee->SetSwitchCharEffect(true);
	//	m_playerGO = klee;
	//	m_light->BindToTransform(m_playerGO->GetTransformP());
	//}
	//if (sigewinne && m_inputManager.GetKeyboardTrigger(DIK_3))
	//{
	//	sigewinne->SetTransform(m_transform);
	//	//sigewinne->SetRenderProgress(0.0f);
	//	//sigewinne->SetSwitchCharEffect(true);
	//	m_playerGO = sigewinne;
	//	m_light->BindToTransform(m_playerGO->GetTransformP());
	//}
}

void Player::HandleInput(void)
{
	if (!m_inputManager.GetKeyboardPress(DIK_W)
		&& !m_inputManager.GetKeyboardPress(DIK_S)
		&& !m_inputManager.GetKeyboardPress(DIK_A)
		&& !m_inputManager.GetKeyboardPress(DIK_D))
		m_attributes->isMoving = false;

	if (m_inputManager.IsMouseRightPressed() && !m_playerAttr.isMeleeMode)
	{
		if (!m_playerAttr.throwMode && !m_playerAttr.climbMode && m_playerAttr.currentState != PlayerState::GLIDE)
		{
			m_playerAttr.aimMode = true;
			m_playerAttr.drawGun = true;
			m_camera.SetAimMode(CameraMode::Aiming, true);

			AlignPlayerYawToCamera();
		}
	}
	else
	{
		m_playerAttr.aimMode = false;
		m_camera.SetAimMode(CameraMode::Aiming, false);
	}

	if (m_inputManager.GetKeyboardRelease(KEY_ENTER_THROW_MODE) && !m_playerAttr.throwMode)
	{
		if (!m_playerAttr.aimMode && m_itemManager.GetPickedItem(ItemType::Seed))
		{
			m_playerAttr.throwMode = true;
			m_playerAttr.drawGun = false;
			m_camera.SetAimMode(CameraMode::ProjectileAiming, true);
		}
	}
	else if (m_inputManager.GetKeyboardRelease(KEY_ENTER_THROW_MODE) && m_playerAttr.throwMode)
	{
		ConfirmThrow();
		AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_throw);
	}
	else if (m_inputManager.GetKeyboardRelease(KEY_EXIT_THROW_MODE) && m_playerAttr.throwMode)
	{
		CancelThrow();
	}

	if (m_inputManager.GetKeyboardPress(KEY_RUN))
	{
		m_attributes->isRunning = true;
	}
	else
	{
		m_attributes->isRunning = false;
	}

	if (m_inputManager.GetKeyboardRelease(KEY_SWITCH_BULLET))
	{
		AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_switch_bullet);
		m_playerAttr.currentProjectile = m_playerAttr.currentProjectile == ProjectileType::Bullet ? ProjectileType::SunBullet : ProjectileType::Bullet;
	}
}

void Player::LinkInteractable(Interactable* interactable)
{
	m_linkedInteractable = interactable;
}

void Player::ConfirmThrow(void)
{
	m_playerAttr.throwMode = false;
	m_playerAttr.confirmThrow = true;
	m_playerAttr.allowControl = false;
	m_playerAttr.seedCount--;
	m_camera.SetAimMode(CameraMode::ProjectileAiming, false);

	Item* item = m_itemManager.GetPickedItem(ItemType::Seed);
	if (item)
	{
		item->SetColliderEnable(true);
		item->Use();
		item->SetIsThrew(true);
		m_projectileFollowerManager.Spawn(m_predictor.GetTrajectoryPoints(), THROW_SPEED, item);
	}
}

void Player::CancelThrow(void)
{
	m_playerAttr.throwMode = false;
	m_playerAttr.confirmThrow = false;
	m_camera.SetAimMode(CameraMode::ProjectileAiming, false);

	Item* item = m_itemManager.GetPickedItem(ItemType::Seed);
	if (item)
	{
		item->GetAttributes()->use = false;
	}
}

void Player::RenderImGui(void)
{
	if (ImGui::Checkbox("Draw Player Bounding Box", &m_drawBoundingBox))
	{
		if (m_playerGO)
		{
			m_playerGO->SetDrawWorldAABB(m_drawBoundingBox);
		}
	}
}



void Player::TakeDamage(int damage)
{
	m_currentHP -= damage;
	if (m_currentHP < 0) m_currentHP = 0;
}

void Player::Heal(int amount)
{
	m_currentHP += amount;
	if (m_currentHP > m_maxHP) m_currentHP = m_maxHP;

	float hpRatio = static_cast<float>(m_currentHP) / static_cast<float>(m_maxHP);
}

void Player::OnEnemyAttackHit(const CollisionEvent& e)
{
	if (m_invincibleTimer > 0.0f) {
		return; // 無敵中は無視
	}

	TakeDamage(10);

	m_invincibleTimer = 0.5f; // 0.5秒の無敵時間
}

