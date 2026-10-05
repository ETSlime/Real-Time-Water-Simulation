//=============================================================================
//
// Hilichurl処理 [Hilichurl.cpp]
// Author : 
//
//=============================================================================
#include "Scene/Character/Hilichurl.h"
#include "Utility/InputManager.h"
#include "Utility/Debug/Debugproc.h"
#include "Scene/Player.h"
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

Hilichurl::Hilichurl(Transform transform, EnemyState initState):Enemy(EnemyType::Hilichurl, transform)
{
	SkinnedGameObjectConfig config;
	config.modelPath = "data/MODEL/enemy/Hilichurl";
	config.modelName = "Character_output.fbx";
	config.modelType = SkinnedModelType::Hilichurl;
	config.drawWorldAABB = false;
	config.collisionType = ObjectCollisionType::COLLIDER_BOUNDING_BOX;
	config.colliderTag = ColliderTag::ENEMY;
	Instantiate(config);

	LoadWeapon("data/MODEL/enemy/Hilichurl", "Stick.fbx");


	m_initState = initState;
	m_playAnimSpeed = PLAY_ANIM_SPD;
}

void Hilichurl::Initialize(void)
{
	Enemy::Initialize();
	if (instance.pModel)
		instance.pModel->ResetCurrentAnim();
	if (m_stateMachine)
		m_stateMachine->SetCurrentState(STATE(m_enemyAttr.initState));
}

Hilichurl::~Hilichurl()
{
	SAFE_DELETE(m_stateMachine);
}

void Hilichurl::PostInit(void)
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
	options.debugName = "AsyncFbxModelLoader_InitializeAnim_Hilichurl"; // デバッグ用の名前
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
}

void Hilichurl::InitializeAnimation(void)
{
	AddAnimation("data/MODEL/enemy/Hilichurl/", "Happy Idle.fbx", AnimClipName::ANIM_IDLE);
	AddAnimation("data/MODEL/enemy/Hilichurl/", "Walking.fbx", AnimClipName::ANIM_WALK);
	AddAnimation("data/MODEL/enemy/Hilichurl/", "Running.fbx", AnimClipName::ANIM_RUN);
	AddAnimation("data/MODEL/enemy/Hilichurl/", "Standing Melee Attack Horizontal.fbx", AnimClipName::ANIM_STANDING_MELEE_ATTACK, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Hilichurl/", "Falling Back Death.fbx", AnimClipName::ANIM_FALLING_BACK_DEATH, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Hilichurl/", "Flying Back Death.fbx", AnimClipName::ANIM_FLYING_BACK_DEATH, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Hilichurl/", "Getting Up.fbx", AnimClipName::ANIM_GETTING_UP, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Hilichurl/", "Hit Reaction 1.fbx", AnimClipName::ANIM_HIT_REACTION_1, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Hilichurl/", "Hit Reaction 2.fbx", AnimClipName::ANIM_HIT_REACTION_2, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Hilichurl/", "Sit.fbx", AnimClipName::ANIM_SIT);
	AddAnimation("data/MODEL/enemy/Hilichurl/", "Surprised.fbx", AnimClipName::ANIM_SURPRISED, AnimPlayMode::ONCE);
	AddAnimation("data/MODEL/enemy/Hilichurl/", "Swing Dancing.fbx", AnimClipName::ANIM_DANCE);
	AddAnimation("data/MODEL/enemy/Hilichurl/", "Falling Back Death.fbx", AnimClipName::ANIM_DIE, AnimPlayMode::ONCE);

	SetupAnimStateMachine(m_initState);
	InitAnimInfo();
}

void Hilichurl::AddAnimation(char* animPath, char* animName, AnimClipName clipName, AnimPlayMode animPlayMode)
{
	if (instance.pModel)
		m_fbxLoader.LoadAnimation(instance.pModel, instance.pModel->GetModelData(), animPath, animName, clipName, animPlayMode);
}

void Hilichurl::LoadWeapon(char* modelPath, char* modelName)
{
	SkinnedGameObjectConfig config;
	config.modelPath = modelPath;
	config.modelName = modelName;
	config.modelType = SkinnedModelType::Weapon;
	config.collisionType = ObjectCollisionType::COLLIDER_BOUNDING_BOX;
	config.colliderTag = ColliderTag::ENEMY_ATTACK;
	config.scale = XMFLOAT3(WEAPON_SIZE, WEAPON_SIZE, WEAPON_SIZE);
	config.position = XMFLOAT3(WEAPON_ON_HANDS_POS_OFFSET_X, WEAPON_ON_HANDS_POS_OFFSET_Y, WEAPON_ON_HANDS_POS_OFFSET_Z);
	config.rotation = XMFLOAT3(WEAPON_ON_HANDS_ROT_OFFSET_X, WEAPON_ON_HANDS_ROT_OFFSET_Y, WEAPON_ON_HANDS_ROT_OFFSET_Z);
	m_weapon.Instantiate(config);
}

void Hilichurl::Update(void)
{
	Enemy::Update();

	if (!instance.load || !m_stateMachine) return;

	m_stateMachine->Update(ANIM_BLEND_SPD, dynamic_cast<ISkinnedMeshModelChar*>(this));

	switch (m_stateMachine->GetCurrentState())
	{
	case STATE(EnemyState::IDLE):
		PlayIdleAnim();
		break;
	case STATE(EnemyState::WALK):
		PlayWalkAnim();
		break;
	case STATE(EnemyState::RUN):
		PlayRunAnim();
		break;
	case STATE(EnemyState::ATTACK):
		PlayAttackAnim();
		break;
	case STATE(EnemyState::HIT1):
		PlayHitAnim();
		break;
	case STATE(EnemyState::HIT2):
		PlayHit2Anim();
		break;
	case STATE(EnemyState::SIT):
		PlaySitAnim();
		break;
	case STATE(EnemyState::SURPRISED):
		PlaySurprisedAnim();
		break;
	case STATE(EnemyState::DIE):
		PlayDieAnim();
		break;
	case STATE(EnemyState::DANCE):
		PlayDanceAnim();
		break;
	default:
		break;
	}

	instance.pModel->UpdateBoneTransform(m_stateMachine->GetBoneMatrices());

	UpdateWeapon();

}

void Hilichurl::Draw(void)
{
	Enemy::Draw();
	m_weapon.Draw();
}

void Hilichurl::InitAnimInfo(void)
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

void Hilichurl::UpdateWeapon(void)
{
	if (m_enemyAttr.isDead)
		m_weapon.SetCastShadow(false);
	else
		m_weapon.SetCastShadow(true);

	m_weapon.Update();
	XMMATRIX weaponMtx = instance.pModel->GetBodyTransformMtx(BodyTransformIndex::RightHand);
	weaponMtx = XMMatrixMultiply(m_weapon.GetWorldMatrix(), weaponMtx);
	weaponMtx = XMMatrixMultiply(weaponMtx, instance.transform.mtxWorld);

	m_weapon.SetWorldMatrix(weaponMtx);

	// ローカル空間のAABBを取得
	BOUNDING_BOX weaponBB = m_weapon.GetSkinnedMeshModel()->GetBoundingBox();
	XMFLOAT3 localMin = weaponBB.minPoint;
	XMFLOAT3 localMax = weaponBB.maxPoint;

	// ローカルAABBの8頂点を生成
	XMVECTOR localCorners[8] = {
		XMVectorSet(localMin.x, localMin.y, localMin.z, 1.0f),
		XMVectorSet(localMax.x, localMin.y, localMin.z, 1.0f),
		XMVectorSet(localMin.x, localMax.y, localMin.z, 1.0f),
		XMVectorSet(localMax.x, localMax.y, localMin.z, 1.0f),
		XMVectorSet(localMin.x, localMin.y, localMax.z, 1.0f),
		XMVectorSet(localMax.x, localMin.y, localMax.z, 1.0f),
		XMVectorSet(localMin.x, localMax.y, localMax.z, 1.0f),
		XMVectorSet(localMax.x, localMax.y, localMax.z, 1.0f),
	};

	// 武器の変換行列（アニメーション＋ワールド行列）
	weaponMtx = m_weapon.GetWorldMatrix(); // ※ここがボーン変形やアタッチ先の変換を含むこと！

	// AABB初期化（最大／最小を初期値に設定）
	XMVECTOR worldMin = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
	XMVECTOR worldMax = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);

	// 各頂点を変換してAABB範囲を更新
	for (int i = 0; i < 8; ++i)
	{
		XMVECTOR transformed = XMVector3TransformCoord(localCorners[i], weaponMtx);
		worldMin = XMVectorMin(worldMin, transformed);
		worldMax = XMVectorMax(worldMax, transformed);
	}

	// ワールド空間のAABBを格納
	XMStoreFloat3(&weaponBB.minPoint, worldMin);
	XMStoreFloat3(&weaponBB.maxPoint, worldMax);

	// コライダーに反映
	m_weapon.SetColliderBoundingBox(weaponBB);
}

AnimStateMachine* Hilichurl::GetStateMachine(void)
{
	return m_stateMachine;
}

void Hilichurl::SetupAnimStateMachine(EnemyState initState)
{
	m_stateMachine = new AnimStateMachine(dynamic_cast<ISkinnedMeshModelChar*>(this));

	m_stateMachine->AddState(STATE(EnemyState::IDLE), instance.pModel->GetAnimationClip(AnimClipName::ANIM_IDLE));
	m_stateMachine->AddState(STATE(EnemyState::SIT), instance.pModel->GetAnimationClip(AnimClipName::ANIM_SIT));
	m_stateMachine->AddState(STATE(EnemyState::ATTACK), instance.pModel->GetAnimationClip(AnimClipName::ANIM_STANDING_MELEE_ATTACK));
	m_stateMachine->AddState(STATE(EnemyState::WALK), instance.pModel->GetAnimationClip(AnimClipName::ANIM_WALK));
	m_stateMachine->AddState(STATE(EnemyState::RUN), instance.pModel->GetAnimationClip(AnimClipName::ANIM_RUN));
	m_stateMachine->AddState(STATE(EnemyState::HIT1), instance.pModel->GetAnimationClip(AnimClipName::ANIM_HIT_REACTION_1));
	m_stateMachine->AddState(STATE(EnemyState::HIT2), instance.pModel->GetAnimationClip(AnimClipName::ANIM_HIT_REACTION_2));
	m_stateMachine->AddState(STATE(EnemyState::SURPRISED), instance.pModel->GetAnimationClip(AnimClipName::ANIM_SURPRISED));
	instance.pModel->GetAnimationClip(AnimClipName::ANIM_DIE)->SetAnimPlayMode(AnimPlayMode::ONCE);
	m_stateMachine->AddState(STATE(EnemyState::DIE), instance.pModel->GetAnimationClip(AnimClipName::ANIM_DIE));
	m_stateMachine->AddState(STATE(EnemyState::DANCE), instance.pModel->GetAnimationClip(AnimClipName::ANIM_DANCE));

	//状態遷移
	m_stateMachine->AddTransition(STATE(EnemyState::IDLE), STATE(EnemyState::WALK), &ISkinnedMeshModelChar::CanWalk);
	m_stateMachine->AddTransition(STATE(EnemyState::IDLE), STATE(EnemyState::ATTACK), &ISkinnedMeshModelChar::CanAttack);
	m_stateMachine->AddTransition(STATE(EnemyState::IDLE), STATE(EnemyState::HIT1), &ISkinnedMeshModelChar::CanHit);
	m_stateMachine->AddTransition(STATE(EnemyState::IDLE), STATE(EnemyState::SURPRISED), &ISkinnedMeshModelChar::CanSurprised);
	m_stateMachine->AddTransition(STATE(EnemyState::IDLE), STATE(EnemyState::DIE), &ISkinnedMeshModelChar::CanDie);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::RUN), &ISkinnedMeshModelChar::CanRun);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::CanStopMoving);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::SURPRISED), &ISkinnedMeshModelChar::CanSurprised);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::ATTACK), &ISkinnedMeshModelChar::CanAttack);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::HIT1), &ISkinnedMeshModelChar::CanHit);
	m_stateMachine->AddTransition(STATE(EnemyState::WALK), STATE(EnemyState::DIE), &ISkinnedMeshModelChar::CanDie);
	m_stateMachine->AddTransition(STATE(EnemyState::RUN), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::CanStopMoving);
	m_stateMachine->AddTransition(STATE(EnemyState::RUN), STATE(EnemyState::WALK), &ISkinnedMeshModelChar::CanStopRunning);
	m_stateMachine->AddTransition(STATE(EnemyState::RUN), STATE(EnemyState::WALK), &ISkinnedMeshModelChar::CanHit);
	m_stateMachine->AddTransition(STATE(EnemyState::RUN), STATE(EnemyState::DIE), &ISkinnedMeshModelChar::CanDie);
	m_stateMachine->AddTransition(STATE(EnemyState::HIT1), STATE(EnemyState::HIT2), &ISkinnedMeshModelChar::CanHit2, false);
	m_stateMachine->AddTransition(STATE(EnemyState::HIT1), STATE(EnemyState::DIE), &ISkinnedMeshModelChar::CanDie);
	m_stateMachine->AddTransition(STATE(EnemyState::HIT2), STATE(EnemyState::HIT1), &ISkinnedMeshModelChar::CanHit, false);
	m_stateMachine->AddTransition(STATE(EnemyState::HIT2), STATE(EnemyState::DIE), &ISkinnedMeshModelChar::CanDie);
	m_stateMachine->AddTransition(STATE(EnemyState::HIT1), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::AlwaysTrue, true);
	m_stateMachine->AddTransition(STATE(EnemyState::HIT2), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::AlwaysTrue, true);
	m_stateMachine->AddTransition(STATE(EnemyState::SURPRISED), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::AlwaysTrue, true);
	m_stateMachine->AddTransition(STATE(EnemyState::SURPRISED), STATE(EnemyState::HIT1), &ISkinnedMeshModelChar::CanHit, true);
	m_stateMachine->AddTransition(STATE(EnemyState::SURPRISED), STATE(EnemyState::DIE), &ISkinnedMeshModelChar::CanDie);
	m_stateMachine->AddTransition(STATE(EnemyState::ATTACK), STATE(EnemyState::IDLE), &ISkinnedMeshModelChar::AlwaysTrue, true);
	m_stateMachine->AddTransition(STATE(EnemyState::ATTACK), STATE(EnemyState::HIT1), &ISkinnedMeshModelChar::CanHit, true);
	m_stateMachine->AddTransition(STATE(EnemyState::ATTACK), STATE(EnemyState::DIE), &ISkinnedMeshModelChar::CanDie);
	m_stateMachine->AddTransition(STATE(EnemyState::DIE), STATE(EnemyState::DIE), &ISkinnedMeshModelChar::AlwaysTrue, true);
	m_stateMachine->AddTransition(STATE(EnemyState::DANCE), STATE(EnemyState::SURPRISED), &ISkinnedMeshModelChar::CanSurprised);
	m_stateMachine->AddTransition(STATE(EnemyState::DANCE), STATE(EnemyState::HIT1), &ISkinnedMeshModelChar::CanHit);
	m_stateMachine->AddTransition(STATE(EnemyState::HIT1), STATE(EnemyState::SURPRISED), &ISkinnedMeshModelChar::CanHit);

	m_stateMachine->SetEndCallback(STATE(EnemyState::ATTACK), &ISkinnedMeshModelChar::OnAttackAnimationEnd);
	m_stateMachine->SetEndCallback(STATE(EnemyState::HIT1), &ISkinnedMeshModelChar::OnHitAnimationEnd);
	m_stateMachine->SetEndCallback(STATE(EnemyState::HIT2), &ISkinnedMeshModelChar::OnHitAnimationEnd);
	m_stateMachine->SetEndCallback(STATE(EnemyState::SURPRISED), &ISkinnedMeshModelChar::OnSurprisedEnd);
	m_stateMachine->SetEndCallback(STATE(EnemyState::DIE), &ISkinnedMeshModelChar::OnDieAnimationEnd);
	

	m_enemyAttr.initState = initState;
	m_stateMachine->SetCurrentState(STATE(initState));
}

void Hilichurl::PlayWalkAnim(void)
{
	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_WALK)
		instance.pModel->SetCurrentAnim(m_stateMachine->GetCurrentAnimClip());
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed);
}

void Hilichurl::PlayRunAnim(void)
{
	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_RUN)
		instance.pModel->SetCurrentAnim(m_stateMachine->GetCurrentAnimClip());
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed);
}

void Hilichurl::PlayIdleAnim(void)
{
	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_IDLE)
		instance.pModel->SetCurrentAnim(m_stateMachine->GetCurrentAnimClip());
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed);
}

void Hilichurl::PlayAttackAnim(void)
{
	float curAnimTime = m_stateMachine->GetCurrentAnimTime();
	if (curAnimTime >= m_stateMachine->GetCurrentAnimClip()->animInfo.animPhase.startAttackFraction
		&& curAnimTime <= m_stateMachine->GetCurrentAnimClip()->animInfo.animPhase.endAttackFraction)
	{
		m_weapon.SetColliderEnable(true);
	}
	else
	{
		m_weapon.SetColliderEnable(false);
	}

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


	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_STANDING_MELEE_ATTACK)
		instance.pModel->SetCurrentAnim(m_stateMachine->GetCurrentAnimClip());
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed);
}

void Hilichurl::PlayHitAnim(void)
{
	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_HIT_REACTION_1)
		instance.pModel->SetCurrentAnim(m_stateMachine->GetCurrentAnimClip());
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed);
}

void Hilichurl::PlayHit2Anim(void)
{
	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_HIT_REACTION_2)
		instance.pModel->SetCurrentAnim(m_stateMachine->GetCurrentAnimClip());
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed);
}

void Hilichurl::PlaySitAnim(void)
{
	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_SIT)
		instance.pModel->SetCurrentAnim(m_stateMachine->GetCurrentAnimClip());
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed);
}

void Hilichurl::PlaySurprisedAnim(void)
{
	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_SURPRISED)
		instance.pModel->SetCurrentAnim(m_stateMachine->GetCurrentAnimClip());
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed * 1.2f);
}

void Hilichurl::PlayDieAnim(void)
{
	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_DIE)
		instance.pModel->SetCurrentAnim(m_stateMachine->GetCurrentAnimClip());
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed);
}

void Hilichurl::PlayDanceAnim(void)
{
	if (instance.pModel->GetCurrentAnim() != AnimClipName::ANIM_DANCE)
		instance.pModel->SetCurrentAnim(m_stateMachine->GetCurrentAnimClip());
	instance.pModel->PlayCurrentAnim(m_playAnimSpeed);
}

bool Hilichurl::CanWalk(void) const
{
	return (instance.attributes.isMoving || instance.attributes.isRotating) && !instance.attributes.isAttacking;
}

bool Hilichurl::CanStopMoving() const
{
	return !instance.attributes.isMoving;
}

bool Hilichurl::CanStopRunning() const
{
	return !instance.attributes.isRunning && instance.attributes.isMoving;
}

bool Hilichurl::CanAttack() const
{
	return instance.attributes.isAttacking;
}

bool Hilichurl::CanRun(void) const
{
	return instance.attributes.isRunning;
}

bool Hilichurl::CanHit(void) const
{
	return instance.attributes.isHit1;
}

bool Hilichurl::CanHit2(void) const
{
	return instance.attributes.isHit2;
}

bool Hilichurl::CanSurprised(void) const
{
	return m_enemyAttr.isSurprised;
}

bool Hilichurl::CanDie(void) const
{
	return m_enemyAttr.die;
}

void Hilichurl::OnAttackAnimationEnd(void)
{
	m_weapon.SetColliderEnable(false);

	instance.attributes.isAttacking = false;

	m_enemyAttr.isInCooldown = true;
	m_enemyAttr.attackCooldownTimer = GetRandFloat(HILI_MIN_ATTACK_CDTIME, HILI_MAX_ATTACK_CDTIME);
	m_enemyAttr.cooldownProbability = 0.5f;
	instance.attributes.isMoving = false;
}

void Hilichurl::OnHitAnimationEnd(void)
{
	instance.attributes.isHit1 = false;
	instance.attributes.isHit2 = false;
	instance.attributes.hitTimer = 0;

	m_enemyAttr.isChasingPlayer = true;
}

void Hilichurl::OnSurprisedEnd(void)
{
	m_enemyAttr.isSurprised = false;
	m_enemyAttr.isChasingPlayer = true;
	m_enemyAttr.randomMove = true;
}

void Hilichurl::OnDieAnimationEnd()
{
	m_enemyAttr.startFadeOut = true;
}
