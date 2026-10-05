#pragma once
//=============================================================================
//
// モデル処理 [Enemy_NPC.h]
// Author : 
//
//=============================================================================
#include "Scene/Enemy.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define HILI_VIEW_ANGLE					(XM_PI * 0.3f)
#define HILI_VIEW_DISTANCE				(800.0f)
#define HILI_CHASING_RANGE				(1500.0f)
#define HILI_ATTACK_RANGE				(140.0f)
#define HILI_MAX_ATTACK_STEP			(520.0f)
#define HILI_MIN_ATTACK_CDTIME			(180.0f)
#define HILI_MAX_ATTACK_CDTIME			(360.0f)
#define	HILI_MAX_COOLDOWN_WAIT_TIME		(0.4f)
#define HILI_MIN_COOLDOWN_WAIT_TIME		(0.2f)
#define	HILI_MAX_COOLDOWN_MOVE_TIME		(0.2f)
#define HILI_MIN_COOLDOWN_MOVE_TIME		(0.1f)

#define Enemy_NPC_SIZE					XMFLOAT3(1.3f, 1.3f, 1.3f)


class Enemy_NPC : public Enemy, public ISkinnedMeshModelChar
{
public:
	Enemy_NPC(Transform transform, EnemyType type, EnemyState initState = EnemyState::IDLE);
	~Enemy_NPC();

	// アニメーションやスクリプトなど、モデルに依存する処理をここで設定する
	virtual void PostInit(void) override;
	virtual void InitializeAnimation(void) override;

	void Update(void) override;
	void Draw(void) override;

	void SetupAnimStateMachine(EnemyState initState);

	AnimStateMachine* GetStateMachine(void) override;

	void PlayGeneralAnim(AnimClipName clipName, float speed = 1.0f);

	virtual void Initialize(void) override;

	virtual bool CanWalk(void) const override;
	virtual bool CanStopMoving() const override;

	virtual const char* GetPanelName(void) const override { return "Enemy_NPC"; }
private:
	virtual void RenderDebugInfo(void) override;
	struct InitContext
	{
		Enemy_NPC* owner;
	};
	void AddAnimation(char* animPath, char* animName, AnimClipName clipName, AnimPlayMode animPlayMode = AnimPlayMode::LOOP) override;


	AnimStateMachine* m_stateMachine = nullptr;
	EnemyState m_initState;
	float m_playAnimSpeed;
	FBXLoader& m_fbxLoader = FBXLoader::get_instance();
};