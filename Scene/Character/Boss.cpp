//=============================================================================
//
// Boss処理 [Boss.cpp]
// Author : 
//
//=============================================================================
#include "Scene/Character/Boss.h"
#include "Scene/Player.h"
#include "Scene/Ground.h"
#include "Utility/InputManager.h"
#include "Core/GameSystem.h"
#include "Core/AudioManager.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define PLAY_ANIM_SPD				1.0f
#define ANIM_BLEND_SPD				0.032f

#define WEAPON_SIZE						35.0f
#define WEAPON_ON_HANDS_POS_OFFSET_X	-55.0f
#define WEAPON_ON_HANDS_POS_OFFSET_Y	45.0f
#define WEAPON_ON_HANDS_POS_OFFSET_Z	-35.0f
#define WEAPON_ON_HANDS_ROT_OFFSET_X	0.0f
#define WEAPON_ON_HANDS_ROT_OFFSET_Y	XM_PI * 0.5f
#define WEAPON_ON_HANDS_ROT_OFFSET_Z	XM_PI * 0.5f

Boss::Boss(Transform transform, EnemyState initState):Enemy(EnemyType::Boss, transform)
{
	SkinnedGameObjectConfig config;
	config.modelPath = "data/MODEL/enemy/Boss";
	config.modelName = "Character_output.fbx";
	config.modelType = SkinnedModelType::Boss;
	config.drawWorldAABB = false;
	config.collisionType = ObjectCollisionType::COLLIDER_BOUNDING_BOX;
	config.colliderTag = ColliderTag::ENEMY;
	Instantiate(config);

	m_initState = initState;
	m_playAnimSpeed = PLAY_ANIM_SPD;

	GameObjectConfig iceConfig;
	iceConfig.modelPath = "data/MODEL/enemy/Boss/ice_only.obj";
	iceConfig.collisionType = ObjectCollisionType::COLLIDER_BOUNDING_BOX;
	iceConfig.colliderTag = ColliderTag::BOSS_ICE;
	iceConfig.scale = XMFLOAT3(25.0f, 65.0f, 185.0f);
	iceConfig.position = transform.pos;
	iceConfig.position.z += 300.0f;
	iceConfig.rotation = XMFLOAT3(XM_PI * 0.5f, 0.0f, 0.0f);
	iceConfig.castShadow = true;
	iceConfig.drawWorldAABB = true;
	m_ice.Instantiate(iceConfig);
	m_ice.SetRenderProgress(1.0f);
	m_ice.AddCollisionCallback(OnIceMeltCallback, this, CollisionCallbackID::PlayerAttackHit);

	m_enemyAttr.turnOnBehaviorTree = false;
}

bool Boss::PerformAttack(BossAttackPattern pattern)
{
	if (m_currentAttackPattern != pattern)
	{
		//switch (pattern)
		//{
		//case BossAttackPattern::MeleeAttackCombo2:
		//case BossAttackPattern::MeleeAttackCombo3:
		//	m_finishMeleeCombo = false;
		//	instance.attributes.isAttacking = true;
		//	break;
		//case BossAttackPattern::IceShard:
		//default:
		//	break;
		//}

		if (pattern == BossAttackPattern::DashToPlayer ||
			pattern == BossAttackPattern::BackStep)
		{
			m_prevPlayerPos = GameSystem::get_instance().GetPlayer()->GetTransform().pos;
			m_startDashPos = instance.transform.pos;
		}



		m_currentAttackPattern = pattern;
		instance.attributes.isAttacking = true;
	}
	

	return instance.attributes.isAttacking;
}

void Boss::Initialize(void)
{
	Enemy::Initialize();
	if (instance.pModel)
		instance.pModel->ResetCurrentAnim();
	if (m_stateMachine)
		m_stateMachine->SetCurrentState(STATE(m_enemyAttr.initState));
}

Boss::~Boss()
{
	SAFE_DELETE(m_stateMachine);
}

void Boss::PostInit(void)
{
	instance.transform.pos = m_enemyAttr.initTrans.pos;
	instance.transform.rot = m_enemyAttr.initTrans.rot;
	instance.transform.scl = m_enemyAttr.initTrans.scl;

	InitHPGauge();

	// 遅延用コンテキスト構築
	auto* ctx = new InitContext{
		this,
	};
	DeferredTaskOptions options{};
	options.async = true; // 非同期実行
	options.debugName = "AsyncFbxModelLoader_InitializeAnim_Boss"; // デバッグ用の名前
	// タスクを DeferredTaskSystem に登録
	RawDeferredTask::Add(
		ctx,
		[](void* ptr)
		{
			auto* ctx = static_cast<InitContext*>(ptr);
			ctx->owner->InitializeAnimation();

			ctx->owner->GameObject::PostInit();
		},
		nullptr,
		options
	);

	InitializeBoneHitCollider(
		m_attackCollider, 
		XMFLOAT3(0.5f, 0.7f, 0.5f), 
		XMFLOAT3(-50.0f, 20.0f, -10.0f), 
		ColliderTag::ENEMY_ATTACK
	);

	m_laserRenderer.Initialize();
}

void Boss::InitializeAnimation(void)
{
	AddAnimation("data/MODEL/enemy/Boss/", "Idle.fbx", AnimClipName::ANIM_IDLE);
	AddAnimation("data/MODEL/enemy/Boss/", "Standing Up.fbx", AnimClipName::ANIM_BOSS_STANDING_UP, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Boss/", "Sword And Shield Power Up.fbx", AnimClipName::ANIM_BOSS_FIGHT_READY, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Boss/", "Walking.fbx", AnimClipName::ANIM_WALK);
	AddAnimation("data/MODEL/enemy/Boss/", "Standing Melee Attack Downward.fbx", AnimClipName::ANIM_STANDING_MELEE_ATTACK, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Boss/", "Standing Melee Attack Backhand.fbx", AnimClipName::ANIM_STANDING_MELEE_ATTACK2, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Boss/", "Standing Melee Attack 360 High.fbx", AnimClipName::ANIM_STANDING_MELEE_ATTACK3, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Boss/", "Sword And Shield Death.fbx", AnimClipName::ANIM_DIE, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Boss/", "Standing 1H Cast Spell 01.fbx", AnimClipName::ANIM_BOSS_FIGHT_CAST_ICE, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Boss/", "Standing 2H Magic Attack 03.fbx", AnimClipName::ANIM_BOSS_FIGHT_LASER, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Boss/", "Getting Up.fbx", AnimClipName::ANIM_GETTING_UP, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Boss/", "Falling Down.fbx", AnimClipName::ANIM_FALLING_DOWN, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Boss/", "Mutant Jumping.fbx", AnimClipName::ANIM_BOSS_DASH_TO_PLAYER, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Boss/", "Jump Backward.fbx", AnimClipName::ANIM_BOSS_BACK_STEP, AnimPlayMode::ONCE);

	SetupAnimStateMachine(m_initState);
	InitAnimInfo();
}

void Boss::AddAnimation(char* animPath, char* animName, AnimClipName clipName, AnimPlayMode animPlayMode)
{
	if (instance.pModel)
		m_fbxLoader.LoadAnimation(instance.pModel, instance.pModel->GetModelData(), animPath, animName, clipName, animPlayMode);
}



void Boss::Update(void)
{
	if (!m_bossFightStarted)
	{
		m_enemyAttr.turnOnBehaviorTree = false;
		SetColliderEnable(false);
		SetInvincible(true);
	}

	Enemy::Update();

	if (!instance.load || !m_stateMachine) return;


	m_stateMachine->Update(ANIM_BLEND_SPD, dynamic_cast<ISkinnedMeshModelChar*>(this));

	uint64_t state = m_stateMachine->GetCurrentState();
	if (state != STATE(EnemyState::ATTACK))
		m_attackCollider.collider.enable = false;

	float animSpeed = m_playAnimSpeed * m_timer.GetScaledDeltaTime() * 1.2f;
	if (!m_bossFightStarted)
		animSpeed = 0.0f;
	switch (m_stateMachine->GetCurrentState())
	{
	case STATE(EnemyState::IDLE):
		PlayGeneralAnim(AnimClipName::ANIM_IDLE, animSpeed);
		break;
	case STATE(EnemyState::WALK):
		PlayGeneralAnim(AnimClipName::ANIM_WALK, animSpeed);
		break;
	case STATE(EnemyState::RUN):
		PlayGeneralAnim(AnimClipName::ANIM_RUN, animSpeed);
		break;
	case STATE(EnemyState::ATTACK):
		PlayAttackAnim(ANIM_STANDING_MELEE_ATTACK);
		break;
	case STATE(EnemyState::ATTACK2):
		PlayAttackAnim(ANIM_STANDING_MELEE_ATTACK2);
		break;
	case STATE(EnemyState::ATTACK3):
		PlayAttackAnim(ANIM_STANDING_MELEE_ATTACK3);
		break;
	case STATE(EnemyState::DIE):
		PlayGeneralAnim(AnimClipName::ANIM_DIE, animSpeed);
		break;
	case STATE(EnemyState::BOSS_STAND_UP):
		PlayGeneralAnim(AnimClipName::ANIM_BOSS_STANDING_UP, animSpeed * 1.5f);
		break;
	case STATE(EnemyState::GET_UP):
		PlayGeneralAnim(AnimClipName::ANIM_GETTING_UP, animSpeed);
		break;
	case STATE(EnemyState::FALL_DOWN):
		PlayGeneralAnim(AnimClipName::ANIM_FALLING_DOWN, animSpeed);
		break;
	case STATE(EnemyState::BOSS_FIGHT_READY):
		PlayGeneralAnim(AnimClipName::ANIM_BOSS_FIGHT_READY, animSpeed);
		break;
	case STATE(EnemyState::BOSS_FIGHT_CAST_ICE):
		PlayCastIceAnim();
		break;
	case STATE(EnemyState::BOSS_FIGHT_LASER):
		PlayLaserAnim();
		break;
	case STATE(EnemyState::BOSS_FIGHT_DASH_TO_PLAYER):
		PlayDashToPlayerAnim();
		break;
	case STATE(EnemyState::BOSS_FIGHT_BACK_STEP):
		PlayBackStepAnim();
		break;
	default:
		break;
	}

	instance.pModel->UpdateBoneTransform(m_stateMachine->GetBoneMatrices());

	UpdateIceShard();

	m_ice.Update();
}

void Boss::Draw(void)
{
	Enemy::Draw();

	if (!m_bossFightStarted)
	{
		if (m_iceStage > 0 && m_iceStage < 4)
		{
			float currentRenderProgress = m_ice.GetRenderProgress() - m_timer.GetDeltaTime();
			float minProgress = 1.0f - m_iceStage * 0.34f;
			if (currentRenderProgress < minProgress)
				currentRenderProgress = minProgress;
			m_ice.SetRenderProgress(currentRenderProgress);
			renderer.SetRenderProgress(m_ice.GetInstance()->renderProgress);
			if (currentRenderProgress < 0.0f)
			{
				m_iceStage = 4;
				m_bossFightStarted = true;
				m_ice.SetColliderEnable(false);
				SetColliderEnable(true);
				Ground::get_instance().SetEnableBossfightRoom(true);
			}
		}

		renderer.SetStaticModelInputLayout(); // モデルの入力レイアウトを設定
		renderer.SetRenderObject(); // モデルの描画を設定
		Transform iceTrans = m_ice.GetTransform();
		Transform tempTrans = iceTrans;
		tempTrans.pos.z = iceTrans.pos.z + 60.0f;
		m_ice.SetTransform(tempTrans);
		m_ice.Update();
		m_ice.Draw();
		tempTrans.pos.z = iceTrans.pos.z - 60.0f;
		m_ice.SetTransform(tempTrans);
		m_ice.Update();
		m_ice.Draw();
		m_ice.SetTransform(iceTrans);
		m_ice.Update();
		m_ice.Draw();
		renderer.SetSkinnedMeshInputLayout(); // スキニングメッシュの入力レイアウトを設定
		renderer.SetRenderSkinnedMeshModel(); // スキニングメッシュモデルの描画を設定

		if (m_iceStage > 0 && m_iceStage < 4)
		{
			RenderProgressCBuffer defaultRenderProgress;
			defaultRenderProgress.isRandomFade = false;
			defaultRenderProgress.progress = 1.0f;
			renderer.SetRenderProgress(defaultRenderProgress);
		}
	}

}

void Boss::DrawEffect(void)
{
	m_laserRenderer.Draw();
}

void Boss::InitAnimInfo(void)
{
	AnimationClip* attack1 = instance.pModel->GetAnimationClip(AnimClipName::ANIM_STANDING_MELEE_ATTACK);
	if (attack1)
	{
		attack1->animInfo.animPhase.startMoveFraction = 0.0f;
		attack1->animInfo.animPhase.endMoveFraction = 0.3f;
		attack1->animInfo.animPhase.startAttackFraction = 0.3f;
		attack1->animInfo.animPhase.endAttackFraction = 0.6f;
	}
}



void Boss::UpdateIceShard(void)
{
	if (m_iceShardMiddle && m_iceShardLeft && m_iceShardRight)
	{
		float currentRenderProgress = m_iceShardMiddle->GetRenderProgress();
		if (currentRenderProgress < m_timer.GetDeltaTime())
			AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_boss_cast);

		// 中央の氷柱を表示
		if (currentRenderProgress < 1.0f)
			m_iceShardMiddle->SetRenderProgress(currentRenderProgress + m_timer.GetDeltaTime());
		else
		{
			currentRenderProgress = m_iceShardRight->GetRenderProgress();
			if (currentRenderProgress < m_timer.GetDeltaTime())
				AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_boss_cast);

			// 右の氷柱を表示
			if (currentRenderProgress < 1.0f)
				m_iceShardRight->SetRenderProgress(currentRenderProgress + m_timer.GetDeltaTime());
			else
			{
				currentRenderProgress = m_iceShardLeft->GetRenderProgress();
				if (currentRenderProgress < m_timer.GetDeltaTime())
					AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_boss_cast);

				// 左の氷柱を表示
				if (currentRenderProgress < 1.0f)
					m_iceShardLeft->SetRenderProgress(currentRenderProgress + m_timer.GetDeltaTime());
				else
				{
					if (m_iceShardMiddle->GetShot())
						return;

					// 全て表示し終わったら発射
					m_iceShardMiddle->SetShot(true);
					m_iceShardRight->SetShot(true);
					m_iceShardLeft->SetShot(true);

					XMFLOAT3 playerPos = GameSystem::get_instance().GetPlayer()->GetTransform().pos;

					// プレイヤーの方向に向ける
					XMFLOAT3 initDir = DirNorm(playerPos, m_iceShardMiddle->GetTransform().pos);
					m_iceShardMiddle->SetDir(initDir);
					XMFLOAT3 initDirR = DirNorm(playerPos, m_iceShardRight->GetTransform().pos);
					m_iceShardRight->SetDir(initDirR);
					XMFLOAT3 initDirL = DirNorm(playerPos, m_iceShardLeft->GetTransform().pos);
					m_iceShardLeft->SetDir(initDirL);
				}
			}
		}
	}
}

AnimStateMachine* Boss::GetStateMachine(void)
{
	return m_stateMachine;
}

bool Boss::CanActNow(void)
{
	return m_stateMachine && (
		m_stateMachine->GetCurrentState() == STATE(EnemyState::IDLE) ||
		m_stateMachine->GetCurrentState() == STATE(EnemyState::WALK)
		);
}

void Boss::PlayGeneralAnim(AnimClipName clipName, float speed)
{
	if (instance.pModel->GetCurrentAnim() != clipName)
		instance.pModel->SetCurrentAnim(m_stateMachine->GetCurrentAnimClip());
	instance.pModel->PlayCurrentAnim(speed);
}

void Boss::SetupAnimStateMachine(EnemyState initState)
{
	m_stateMachine = new AnimStateMachine(dynamic_cast<ISkinnedMeshModelChar*>(this));

	m_stateMachine->AddState(STATE(EnemyState::IDLE), instance.pModel->GetAnimationClip(AnimClipName::ANIM_IDLE));
	m_stateMachine->AddState(STATE(EnemyState::ATTACK), instance.pModel->GetAnimationClip(AnimClipName::ANIM_STANDING_MELEE_ATTACK));
	m_stateMachine->AddState(STATE(EnemyState::ATTACK2), instance.pModel->GetAnimationClip(AnimClipName::ANIM_STANDING_MELEE_ATTACK2));
	m_stateMachine->AddState(STATE(EnemyState::ATTACK3), instance.pModel->GetAnimationClip(AnimClipName::ANIM_STANDING_MELEE_ATTACK3));
	m_stateMachine->AddState(STATE(EnemyState::WALK), instance.pModel->GetAnimationClip(AnimClipName::ANIM_WALK));
	instance.pModel->GetAnimationClip(AnimClipName::ANIM_DIE)->SetAnimPlayMode(AnimPlayMode::ONCE);
	m_stateMachine->AddState(STATE(EnemyState::DIE), instance.pModel->GetAnimationClip(AnimClipName::ANIM_DIE));

	m_stateMachine->AddState(STATE(EnemyState::BOSS_STAND_UP), instance.pModel->GetAnimationClip(AnimClipName::ANIM_BOSS_STANDING_UP));
	m_stateMachine->AddState(STATE(EnemyState::BOSS_FIGHT_READY), instance.pModel->GetAnimationClip(AnimClipName::ANIM_BOSS_FIGHT_READY));
	m_stateMachine->AddState(STATE(EnemyState::BOSS_FIGHT_LASER), instance.pModel->GetAnimationClip(AnimClipName::ANIM_BOSS_FIGHT_LASER));
	m_stateMachine->AddState(STATE(EnemyState::BOSS_FIGHT_CAST_ICE), instance.pModel->GetAnimationClip(AnimClipName::ANIM_BOSS_FIGHT_CAST_ICE));
	m_stateMachine->AddState(STATE(EnemyState::GET_UP), instance.pModel->GetAnimationClip(AnimClipName::ANIM_GETTING_UP));
	m_stateMachine->AddState(STATE(EnemyState::FALL_DOWN), instance.pModel->GetAnimationClip(AnimClipName::ANIM_FALLING_DOWN));
	m_stateMachine->AddState(STATE(EnemyState::BOSS_FIGHT_DASH_TO_PLAYER), instance.pModel->GetAnimationClip(AnimClipName::ANIM_BOSS_DASH_TO_PLAYER));
	m_stateMachine->AddState(STATE(EnemyState::BOSS_FIGHT_BACK_STEP), instance.pModel->GetAnimationClip(AnimClipName::ANIM_BOSS_BACK_STEP));

	//状態遷移
	m_stateMachine->AddTransition(STATE(EnemyState::IDLE), STATE(EnemyState::WALK), &ISkinnedMeshModelChar::CanWalk);
	m_stateMachine->AddTransition(STATE(EnemyState::IDLE), STATE(EnemyState::ATTACK), &ISkinnedMeshModelChar::CanAttack);
	m_stateMachine->AddTransition(STATE(EnemyState::IDLE), STATE(EnemyState::BOSS_FIGHT_CAST_ICE), &ISkinnedMeshModelChar::CanCastIce);
	m_stateMachine->AddTransition(STATE(EnemyState::IDLE), STATE(EnemyState::BOSS_FIGHT_DASH_TO_PLAYER), &ISkinnedMeshModelChar::CanDash);
	m_stateMachine->AddTransition(STATE(EnemyState::IDLE), STATE(EnemyState::BOSS_FIGHT_BACK_STEP), &ISkinnedMeshModelChar::CanBackStep);
	m_stateMachine->AddTransition(STATE(EnemyState::IDLE), STATE(EnemyState::BOSS_FIGHT_LASER), &ISkinnedMeshModelChar::CanLaser);
	m_stateMachine->AddTransition(STATE(EnemyState::IDLE), STATE(EnemyState::DIE), &ISkinnedMeshModelChar::CanDie);
	m_stateMachine->AddTransition(STATE(EnemyState::IDLE), STATE(EnemyState::FALL_DOWN), &ISkinnedMeshModelChar::CanFallDown);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::CanStopMoving);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::ATTACK), &ISkinnedMeshModelChar::CanAttack);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::DIE), &ISkinnedMeshModelChar::CanDie);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::BOSS_FIGHT_DASH_TO_PLAYER), &ISkinnedMeshModelChar::CanDash);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::BOSS_FIGHT_BACK_STEP), &ISkinnedMeshModelChar::CanBackStep);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::BOSS_FIGHT_CAST_ICE), &ISkinnedMeshModelChar::CanCastIce);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::BOSS_FIGHT_LASER), &ISkinnedMeshModelChar::CanLaser);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::FALL_DOWN), &ISkinnedMeshModelChar::CanFallDown);
	m_stateMachine->AddTransition(STATE(EnemyState::ATTACK), STATE(EnemyState::ATTACK2), &ISkinnedMeshModelChar::CanAttack2, true);
	m_stateMachine->AddTransition(STATE(EnemyState::ATTACK2), STATE(EnemyState::ATTACK3), &ISkinnedMeshModelChar::CanAttack3, true);
	m_stateMachine->AddTransition(STATE(EnemyState::ATTACK2), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::CanStopAttacking3, true);
	m_stateMachine->AddTransition(STATE(EnemyState::ATTACK3), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::AlwaysTrue, true);
	m_stateMachine->AddTransition(STATE(EnemyState::ATTACK), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::CanStopAttacking2, true);
	m_stateMachine->AddTransition(STATE(EnemyState::BOSS_FIGHT_LASER), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::AlwaysTrue, true);
	m_stateMachine->AddTransition(STATE(EnemyState::BOSS_FIGHT_CAST_ICE), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::AlwaysTrue, true);
	m_stateMachine->AddTransition(STATE(EnemyState::BOSS_FIGHT_CAST_ICE), STATE(EnemyState::FALL_DOWN), &ISkinnedMeshModelChar::CanFallDown);
	m_stateMachine->AddTransition(STATE(EnemyState::ATTACK), STATE(EnemyState::DIE), &ISkinnedMeshModelChar::CanDie);

	m_stateMachine->AddTransition(STATE(EnemyState::BOSS_STAND_UP), STATE(EnemyState::BOSS_FIGHT_READY), &ISkinnedMeshModelChar::AlwaysTrue, true);
	m_stateMachine->AddTransition(STATE(EnemyState::BOSS_FIGHT_READY), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::AlwaysTrue, true);
	m_stateMachine->AddTransition(STATE(EnemyState::FALL_DOWN), STATE(EnemyState::GET_UP), &ISkinnedMeshModelChar::AlwaysTrue, true);
	m_stateMachine->AddTransition(STATE(EnemyState::GET_UP), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::AlwaysTrue, true);

	m_stateMachine->AddTransition(STATE(EnemyState::BOSS_FIGHT_DASH_TO_PLAYER), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::AlwaysTrue, true);
	m_stateMachine->AddTransition(STATE(EnemyState::BOSS_FIGHT_BACK_STEP), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::AlwaysTrue, true);

	m_stateMachine->SetEndCallback(STATE(EnemyState::ATTACK), &ISkinnedMeshModelChar::OnAttackAnimationEnd);
	m_stateMachine->SetEndCallback(STATE(EnemyState::ATTACK2), &ISkinnedMeshModelChar::OnAttackAnimationEnd);
	m_stateMachine->SetEndCallback(STATE(EnemyState::ATTACK3), &ISkinnedMeshModelChar::OnAttackAnimationEnd);
	m_stateMachine->SetEndCallback(STATE(EnemyState::BOSS_FIGHT_CAST_ICE), &ISkinnedMeshModelChar::OnAttackAnimationEnd);
	m_stateMachine->SetEndCallback(STATE(EnemyState::BOSS_FIGHT_LASER), &ISkinnedMeshModelChar::OnAttackAnimationEnd);
	m_stateMachine->SetEndCallback(STATE(EnemyState::BOSS_FIGHT_DASH_TO_PLAYER), &ISkinnedMeshModelChar::OnAttackAnimationEnd);
	m_stateMachine->SetEndCallback(STATE(EnemyState::BOSS_FIGHT_BACK_STEP), &ISkinnedMeshModelChar::OnAttackAnimationEnd);
	m_stateMachine->SetEndCallback(STATE(EnemyState::DIE), &ISkinnedMeshModelChar::OnDieAnimationEnd);
	m_stateMachine->SetEndCallback(STATE(EnemyState::BOSS_STAND_UP), &ISkinnedMeshModelChar::OnBossStandUp);
	m_stateMachine->SetEndCallback(STATE(EnemyState::BOSS_FIGHT_READY), &ISkinnedMeshModelChar::OnBossfightReady);
	m_stateMachine->SetEndCallback(STATE(EnemyState::GET_UP), &ISkinnedMeshModelChar::OnGetUpAnimationEnd);
	

	m_enemyAttr.initState = initState;
	m_stateMachine->SetCurrentState(STATE(initState));
}

void Boss::PlayAttackAnim(AnimClipName clipName)
{
	float curAnimTime = m_stateMachine->GetCurrentAnimTime();

	float dist = m_enemyAttr.distPlayerSq;

	if (dist > m_enemyAttr.attackRange * m_enemyAttr.attackRange)
	{
		const AnimationClip* currentAnimClip = m_stateMachine->GetCurrentAnimClip();

		// アニメーションの再生進捗を取得
		float attackAnimTime = m_stateMachine->GetCurrentAnimTime();
		// 最大移動距離
		float moveStep = min(dist - m_enemyAttr.attackRange, HILI_MAX_ATTACK_STEP);
		// どのくらいの割合の時間が移動に使われるか計算
		float moveTimeFraction = currentAnimClip->animInfo.animPhase.endMoveFraction - currentAnimClip->animInfo.animPhase.startMoveFraction;

		// `num_steps` を自動調整
		int num_steps = max(50, min(200, (int)(moveStep / 5.0f)));

		// `sum_speed_factors` を数値積分で計算
		float sum_speed_factors = 0.0f;
		for (int i = 0; i <= num_steps; i++)
		{
			float phase = (float)i / num_steps;
			sum_speed_factors += phase * (2.0f - phase);
		}

		// MAX_SPEED を計算
		float MAX_SPEED = moveStep / sum_speed_factors;

		// 現在の移動フェーズの正規化値を計算
		float attackPhase = (attackAnimTime - currentAnimClip->animInfo.animPhase.startMoveFraction) / moveTimeFraction;

		// 指定された時間範囲内で移動
		if (attackAnimTime >= currentAnimClip->animInfo.animPhase.startMoveFraction
			&& attackAnimTime <= currentAnimClip->animInfo.animPhase.endMoveFraction)
		{
			// 速度補正係数を計算 (S 曲線補間)
			float speedFactor = attackPhase * (2.0f - attackPhase);
			instance.attributes.spd = MAX_SPEED * speedFactor;

			// すでに攻撃範囲内にいる場合、速度を急激に減衰させる
			if (dist <= m_enemyAttr.attackRange * m_enemyAttr.attackRange)
			{
				instance.attributes.spd *= (1.0f - attackPhase * attackPhase); // 速度を急激に減少
			}
		}
		else
		{
			instance.attributes.spd = 0.0f; // 移動を停止
		}
	}

	AnimationClip* clip = m_stateMachine->GetCurrentAnimClip();
	if (instance.pModel->GetCurrentAnim() != clipName)
		instance.pModel->SetCurrentAnim(clip);
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed);

	float ip;
	float currentTime = modff(clip->GetCurrentPlayTime(), &ip);
	if (currentTime > 0.1f && currentTime < 0.9f)
		m_attackCollider.collider.enable = true;
	else
		m_attackCollider.collider.enable = false;

	UpdateBoneHitCollider(m_attackCollider, BodyTransformIndex::RightHand);

}

void Boss::PlayBackStepAnim(void)
{
	AnimationClip* clip = m_stateMachine->GetCurrentAnimClip();
	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_BOSS_BACK_STEP)
		instance.pModel->SetCurrentAnim(clip);
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed);

	XMFLOAT3 playerPos = GameSystem::get_instance().GetPlayer()->GetTransform().pos;
	XMVECTOR vPlayer = XMLoadFloat3(&playerPos);
	XMVECTOR vPos = XMLoadFloat3(&m_startDashPos);

	// プレイヤー方向ベクトル
	XMVECTOR toPlayer = XMVectorSubtract(vPlayer, vPos);

	// 正規化して反対方向を計算
	XMVECTOR awayDir = XMVector3Normalize(XMVectorNegate(toPlayer));

	// 退避する目標距離
	constexpr float desiredDistance = 400.0f;

	// アニメ再生時間
	float t = clip->GetCurrentPlayTime();

	// 反対方向に補間移動
	XMVECTOR vNewPos = XMVectorAdd(vPos, XMVectorScale(awayDir, desiredDistance * t));

	// 位置を更新
	XMStoreFloat3(&instance.transform.pos, vNewPos);
}

void Boss::PlayCastIceAnim(void)
{
	AnimationClip* clip = m_stateMachine->GetCurrentAnimClip();
	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_BOSS_FIGHT_CAST_ICE)
		instance.pModel->SetCurrentAnim(clip);
	instance.pModel->PlayCurrentAnim(0.5f);

	if (!m_spawnIceShard)
	{
		const float yawRad = instance.transform.rot.y; // Y軸回転（ラジアン）
		const XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f); // 上方向ベクトル

		// 前方向ベクトルを計算
		const float sy = sinf(yawRad);
		const float cy = cosf(yawRad);
		const XMVECTOR forward = XMVectorSet(sy, 0.f, cy, 0.f);

		// 右方向ベクトルを計算
		XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, up));
		// ベース位置
		XMVECTOR base = XMLoadFloat3(&instance.transform.pos);

		float middleHeight = 400.0f, sideHeightOffset = -50.0f, sideSpacing = 150.0f, forwardOffset = 120.0f;

		// オフセットベクトルを計算
		XMVECTOR forwardOffsetV = XMVectorScale(forward, forwardOffset);
		XMVECTOR upOffset = XMVectorScale(up, middleHeight);
		XMVECTOR sideUpOffset = XMVectorScale(up, sideHeightOffset);
		XMVECTOR rightOffset = XMVectorScale(right, sideSpacing);

		XMFLOAT3 outUpLeft, outUpMiddle, outUpRight;

		// 中央の位置を計算
		XMVECTOR middlePosV = XMVectorAdd(base, upOffset);
		middlePosV = XMVectorAdd(middlePosV, forwardOffsetV);
		XMStoreFloat3(&outUpMiddle, middlePosV);

		// 左右の位置を計算
		middlePosV = XMVectorAdd(middlePosV, sideUpOffset);
		XMVECTOR leftPosV = XMVectorSubtract(middlePosV, rightOffset);
		XMVECTOR rightPosV = XMVectorAdd(middlePosV, rightOffset);


		XMStoreFloat3(&outUpRight, rightPosV);
		XMStoreFloat3(&outUpLeft, leftPosV);

		Transform shootTrans;
		shootTrans.pos = outUpMiddle;
		shootTrans.scl = XMFLOAT3(2.0f, 3.0f, 2.0f);
		m_iceShardMiddle = ProjectileManager::get_instance().SpawnProjectile(ProjectileType::IceShard, shootTrans, XMFLOAT3(), 15.0f);
		m_iceShardMiddle->SetHoming(true);
		m_iceShardMiddle->SetShot(false);
		m_iceShardMiddle->SetRenderProgress(0.0f); // 描画進捗をリセット

		shootTrans.pos = outUpRight;
		m_iceShardRight = ProjectileManager::get_instance().SpawnProjectile(ProjectileType::IceShard, shootTrans, XMFLOAT3(), 15.0f);
		m_iceShardRight->SetHoming(true);
		m_iceShardRight->SetShot(false);
		m_iceShardRight->SetRenderProgress(0.0f); // 描画進捗をリセット

		shootTrans.pos = outUpLeft;
		m_iceShardLeft = ProjectileManager::get_instance().SpawnProjectile(ProjectileType::IceShard, shootTrans, XMFLOAT3(), 15.0f);
		m_iceShardLeft->SetHoming(true);
		m_iceShardLeft->SetShot(false);
		m_iceShardLeft->SetRenderProgress(0.0f); // 描画進捗をリセット

		m_spawnIceShard = true;
	}

}

void Boss::PlayLaserAnim(void)
{
	AnimationClip* clip = m_stateMachine->GetCurrentAnimClip();
	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_BOSS_FIGHT_LASER)
		instance.pModel->SetCurrentAnim(clip);
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed * 0.7f);

	constexpr float laserStartTime = 0.2f;
	constexpr float laserEndTime = 0.8f;

	// レーザーの描画開始・終了タイミングを制御
	float ip;
	float currentTime = modff(clip->GetCurrentPlayTime(), &ip);
	if (currentTime < laserStartTime || currentTime > laserEndTime)
	{
		m_laserRenderer.End();
		AudioManager::get_instance().StopSound(SoundLabel::SOUND_LABEL_SE_boss_laser);
		return;
	}
	else
		AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_boss_laser);

	// レーザーの方向と位置を計算
	XMFLOAT3 playerPos = GameSystem::get_instance().GetPlayer()->GetTransform().pos;
	XMFLOAT3 targetPos = Sub(playerPos, XMFLOAT3(0.0f, 260.0f, 0.0f));
	XMVECTOR targetPosV = XMLoadFloat3(&targetPos);
	XMVECTOR enemyPosV = XMLoadFloat3(&instance.transform.pos);

	// プレイヤーへの方向ベクトルを計算
	XMVECTOR toPlayerV = XMVectorSubtract(targetPosV, enemyPosV);
	XMVECTOR dirV = XMVector3Normalize(toPlayerV);

	// レーザーの開始位置を調整（敵の少し上、前方にオフセット）
	XMVECTOR posV = XMVectorAdd(
		XMVectorMultiplyAdd(dirV, XMVectorReplicate(50.0f), enemyPosV), // a*b + c
		XMVectorSet(0.0f, 260.0f, 0.0f, 0.0f)
	);
	XMFLOAT3 laserDir, laserPos;
	XMStoreFloat3(&laserDir, dirV);
	XMStoreFloat3(&laserPos, posV);


	LaserParams params{};
	m_laserRenderer.Begin(
		laserPos,
		laserDir,
		params,
		XMFLOAT4(1, 0.4f, 0.9f, 1)
	);

	BOUNDING_BOX playerBB = GameSystem::get_instance().GetPlayer()->GetPlayerAABB();
	m_laserRenderer.UpdateDirection(laserDir, m_timer.GetDeltaTime());
	m_laserRenderer.Advance(m_timer.GetDeltaTime(), &playerBB);

	instance.attributes.targetDir = atan2f(laserDir.x, laserDir.z);

}

void Boss::PlayDashToPlayerAnim(void)
{
	// 現在の進捗 t [0,1]
	float currentTime = m_stateMachine->GetCurrentAnimTime();

	constexpr float startJumpTime = 0.4f;
	constexpr float endJumpTime = 0.6f;

	// ジャンプ中はスローモーションにする
	if (currentTime > startJumpTime && currentTime < endJumpTime)
		m_playAnimSpeed = 0.5f;
	else
		m_playAnimSpeed = 1.0f;

	AnimationClip* clip = m_stateMachine->GetCurrentAnimClip();
	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_BOSS_DASH_TO_PLAYER)
		instance.pModel->SetCurrentAnim(clip);
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed);

	// --- プレイヤー位置 ---
	const XMFLOAT3 playerPos = GameSystem::get_instance().GetPlayer()->GetTransform().pos;
	XMFLOAT3& pos = instance.transform.pos;

	// --- 開始位置 ---
	XMFLOAT3 startPos = m_startDashPos;

	// 区間 [tStart, tEnd]
	float tStart = startJumpTime;
	float tEnd = endJumpTime;

	// clamp
	if (currentTime < tStart) currentTime = tStart;
	if (currentTime > tEnd)   currentTime = tEnd;

	// 正規化 [0,1]
	float localT = (currentTime - tStart) / (tEnd - tStart);

	// --- ターゲット位置 ---
	// 少し下を目指す
	XMFLOAT3 targetPos = playerPos;
	targetPos.y -= 20.0f;


	// プレイヤーの移動方向を推定（今フレーム位置 - 前フレーム位置）
	XMFLOAT3 prevPlayerPos = m_prevPlayerPos; // 前フレーム保存しておく必要あり
	XMFLOAT3 vel = {
		playerPos.x - prevPlayerPos.x,
		0.0f,
		playerPos.z - prevPlayerPos.z
	};

	// 横方向オフセットを計算
	float len = sqrtf(vel.x * vel.x + vel.z * vel.z);
	XMFLOAT3 sideOffset = { 0, 0, 0 };
	if (len > 1e-3f) {
		// 移動方向に対して直交するベクトル
		sideOffset.x = -vel.z / len;
		sideOffset.z = vel.x / len;

		// 横オフセット量（例: ±100）
		float offsetScale = 100.0f;
		sideOffset.x *= offsetScale;
		sideOffset.z *= offsetScale;

		// 片側にずらす（必要なら乱数で左右を切替）
		targetPos.x += sideOffset.x;
		targetPos.z += sideOffset.z;
	}

	// --- 線形補間 ---
	XMVECTOR vStart = XMLoadFloat3(&startPos);
	XMVECTOR vTarget = XMLoadFloat3(&targetPos);
	XMVECTOR vNewPos = XMVectorLerp(vStart, vTarget, localT);

	// --- 高さの弧 ---
	float heightOffset = 150.0f;
	float parabola = 4.0f * localT * (1.0f - localT);

	XMFLOAT3 newPos;
	XMStoreFloat3(&newPos, vNewPos);
	newPos.y += parabola * heightOffset;

	// 位置反映
	pos = newPos;

	// --- Y軸回転だけ更新 ---
	float rotY = atan2f(playerPos.x - pos.x, playerPos.z - pos.z);
	instance.transform.rot.y = rotY;

	// 前フレームのプレイヤー位置を保存
	m_prevPlayerPos = playerPos;

}

bool Boss::CanWalk(void) const
{
	return (instance.attributes.isMoving || instance.attributes.isRotating) && !instance.attributes.isAttacking;
}

bool Boss::CanStopMoving() const
{
	return !instance.attributes.isMoving;
}

bool Boss::CanStopRunning() const
{
	return !instance.attributes.isRunning && instance.attributes.isMoving;
}

bool Boss::CanAttack() const
{
	return instance.attributes.isAttacking && 
		(m_currentAttackPattern == BossAttackPattern::MeleeAttackCombo2 
			|| m_currentAttackPattern == BossAttackPattern::MeleeAttackCombo3);
}

bool Boss::CanRun(void) const
{
	return instance.attributes.isRunning;
}

bool Boss::CanHit(void) const
{
	return instance.attributes.isHit1;
}

bool Boss::CanHit2(void) const
{
	return instance.attributes.isHit2;
}

bool Boss::CanSurprised(void) const
{
	return m_enemyAttr.isSurprised;
}

bool Boss::CanDie(void) const
{
	return m_enemyAttr.die;
}

bool Boss::CanFallDown(void) const
{
	return m_interruptedIceCast || m_startPhase2;
}

void Boss::OnAttackAnimationEnd(void)
{
	//instance.attributes.isAttacking = false;

	bool attackEnded = false;

	attackEnded |= m_currentAttackPattern == BossAttackPattern::MeleeAttackCombo2
		&& m_stateMachine->GetCurrentState() == STATE(EnemyState::ATTACK2);

	attackEnded |= m_currentAttackPattern == BossAttackPattern::MeleeAttackCombo3
		&& m_stateMachine->GetCurrentState() == STATE(EnemyState::ATTACK3);

	attackEnded |= m_currentAttackPattern == BossAttackPattern::IceShard
		&& m_stateMachine->GetCurrentState() == STATE(EnemyState::BOSS_FIGHT_CAST_ICE);
	
	attackEnded |= m_currentAttackPattern == BossAttackPattern::Laser
		&& m_stateMachine->GetCurrentState() == STATE(EnemyState::BOSS_FIGHT_LASER);

	attackEnded |= m_currentAttackPattern == BossAttackPattern::DashToPlayer
		&& m_stateMachine->GetCurrentState() == STATE(EnemyState::BOSS_FIGHT_DASH_TO_PLAYER);

	attackEnded |= m_currentAttackPattern == BossAttackPattern::BackStep
		&& m_stateMachine->GetCurrentState() == STATE(EnemyState::BOSS_FIGHT_BACK_STEP);

	if (attackEnded)
	{
		if (m_currentAttackPattern == BossAttackPattern::IceShard)
			m_spawnIceShard = false;

		instance.attributes.isAttacking = false;
		m_currentAttackPattern = BossAttackPattern::None;

		if (m_currentAttackPattern != BossAttackPattern::DashToPlayer && 
			m_currentAttackPattern != BossAttackPattern::BackStep)
		{
			m_enemyAttr.isInCooldown = true;
			m_enemyAttr.attackCooldownTimer = GetRandFloat(BOSS_MIN_ATTACK_CDTIME, BOSS_MAX_ATTACK_CDTIME);
			m_enemyAttr.cooldownProbability = 0.5f;
			instance.attributes.isMoving = false;
		}
	}
}

void Boss::OnHitAnimationEnd(void)
{
	instance.attributes.isHit1 = false;
	instance.attributes.isHit2 = false;
	instance.attributes.hitTimer = 0;

	m_enemyAttr.isChasingPlayer = true;
}

void Boss::OnSurprisedEnd(void)
{
	m_enemyAttr.isSurprised = false;
	m_enemyAttr.isChasingPlayer = true;
	m_enemyAttr.randomMove = true;
}

void Boss::OnDieAnimationEnd()
{
	m_enemyAttr.startFadeOut = true;
	m_bossFightStarted = false;
	Ground::get_instance().SetEnableBossfightRoom(false);
	AudioManager::get_instance().StopSound(SoundLabel::SOUND_LABEL_BGM_boss);
	AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_BGM_winter);
}

void Boss::OnGetUpAnimationEnd()
{
	m_interruptedIceCast = false;
	m_startPhase2 = false;
	m_currentAttackPattern = BossAttackPattern::None;
	instance.attributes.isAttacking = false;
}

void Boss::OnBossfightReady()
{
	SetInvincible(false);
	instance.attributes.isAttacking = false;
	m_currentAttackPattern = BossAttackPattern::None;
	m_enemyAttr.turnOnBehaviorTree = true;
	AudioManager::get_instance().StopSound();
	AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_BGM_boss);
}

void Boss::OnBossStandUp()
{
	AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_boss_scream);
}

void Boss::OnIceMeltCallback(const CollisionEvent& event, void* context)
{
	const Collider* projectileCollider = event.colliderA->tag == ColliderTag::PLAYER_PROJECTILE ? event.colliderA : event.colliderB;
	Projectile* owner = static_cast<Projectile*>(projectileCollider->owner);
	owner->SetActive(false);
	if (owner->GetProjectileAttributes().projectileType != ProjectileType::SunBullet)
		return;

	Boss* self = static_cast<Boss*>(context);
	self->ProcessIceStage();
}

void Boss::RenderDebugInfo(void)
{
	GameObject::RenderDebugInfo();

#if DRAW_BOUNDING_BOX
	if (m_attackCollider.collider.enable)
	{
		XMFLOAT4 color = { 1.0f, 0.0f, 0.0f, 1.0f };
		m_debugBoundingBoxRenderer.DrawBox(m_attackCollider.collider.aabb, Camera::get_instance().GetViewProjMtx(), color);
	}
#endif
}
