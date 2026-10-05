//=============================================================================
//
// モデル処理 [Enemy_NPC.cpp]
// Author : 
//
//=============================================================================
#include "Enemy_NPC.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define PLAY_ANIM_SPD				1.0f
#define ANIM_BLEND_SPD				0.032f

Enemy_NPC::Enemy_NPC(Transform transform, EnemyType type, EnemyState initState) :Enemy(type, transform)
{
	SkinnedGameObjectConfig config;
	config.modelPath = "data/MODEL/enemy/NPC";
	config.modelName = "Character_output.fbx";
	config.modelType = SkinnedModelType::NPC;
	config.drawWorldAABB = false;
	config.collisionType = ObjectCollisionType::COLLIDER_BOUNDING_BOX;
	config.colliderTag = ColliderTag::NPC;
	Instantiate(config);

	m_initState = initState;
	m_playAnimSpeed = PLAY_ANIM_SPD;
}

Enemy_NPC::~Enemy_NPC()
{
	SAFE_DELETE(m_stateMachine);
}

void Enemy_NPC::Initialize(void)
{
	Enemy::Initialize();
	if (instance.pModel)
		instance.pModel->ResetCurrentAnim();
	if (m_stateMachine)
		m_stateMachine->SetCurrentState(STATE(m_enemyAttr.initState));
}

void Enemy_NPC::PostInit(void)
{
	instance.transform.pos = m_enemyAttr.initTrans.pos;
	instance.transform.rot = m_enemyAttr.initTrans.rot;
	instance.transform.scl = m_enemyAttr.initTrans.scl;

	// 遅延用コンテキスト構築
	auto* ctx = new InitContext{
		this,
	};
	DeferredTaskOptions options{};
	options.async = true; // 非同期実行
	options.debugName = "AsyncFbxModelLoader_InitializeAnim_NPC"; // デバッグ用の名前
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

void Enemy_NPC::InitializeAnimation(void)
{
	AddAnimation("data/MODEL/enemy/NPC/", "Animation_Walking.fbx", AnimClipName::ANIM_IDLE);

	SetupAnimStateMachine(m_initState);
	InitAnimInfo();
}

void Enemy_NPC::AddAnimation(char* animPath, char* animName, AnimClipName clipName, AnimPlayMode animPlayMode)
{
	if (instance.pModel)
		m_fbxLoader.LoadAnimation(instance.pModel, instance.pModel->GetModelData(), animPath, animName, clipName, animPlayMode);
}

void Enemy_NPC::Update(void)
{
	Enemy::Update();

	if (!instance.load || !m_stateMachine) return;

	m_stateMachine->Update(ANIM_BLEND_SPD, dynamic_cast<ISkinnedMeshModelChar*>(this));

	float animSpeed = m_playAnimSpeed * m_timer.GetScaledDeltaTime();
	switch (m_stateMachine->GetCurrentState())
	{
	case STATE(EnemyState::IDLE):
		PlayGeneralAnim(AnimClipName::ANIM_IDLE, animSpeed);
		break;
	default:
		break;
	}

	instance.pModel->UpdateBoneTransform(m_stateMachine->GetBoneMatrices());
}

void Enemy_NPC::Draw(void)
{
	Enemy::Draw();
}

AnimStateMachine* Enemy_NPC::GetStateMachine(void)
{
	return m_stateMachine;
}

void Enemy_NPC::PlayGeneralAnim(AnimClipName clipName, float speed)
{
	if (instance.pModel->GetCurrentAnim() != clipName)
		instance.pModel->SetCurrentAnim(m_stateMachine->GetCurrentAnimClip());
	instance.pModel->PlayCurrentAnim(speed);
}

void Enemy_NPC::SetupAnimStateMachine(EnemyState initState)
{
	m_stateMachine = new AnimStateMachine(dynamic_cast<ISkinnedMeshModelChar*>(this));

	m_stateMachine->AddState(STATE(EnemyState::IDLE), instance.pModel->GetAnimationClip(AnimClipName::ANIM_IDLE));

	m_enemyAttr.initState = initState;
	m_stateMachine->SetCurrentState(STATE(initState));
}

bool Enemy_NPC::CanWalk(void) const
{
	return instance.attributes.isMoving || instance.attributes.isRotating;
}

bool Enemy_NPC::CanStopMoving() const
{
	return !instance.attributes.isMoving;
}

void Enemy_NPC::RenderDebugInfo(void)
{
}
