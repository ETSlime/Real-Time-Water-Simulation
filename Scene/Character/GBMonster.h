#pragma once
//=============================================================================
//
// モデル処理 [GBMonster.h]
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

#define GBMONSTER_SIZE					XMFLOAT3(0.9f, 0.9f, 0.9f)

#define GBMONSTER_MAX_HP				(600.0f)

class GBMonster : public Enemy, public ISkinnedMeshModelChar
{
public:
	GBMonster(Transform transform, EnemyState initState = EnemyState::IDLE);
	~GBMonster();

	// アニメーションやスクリプトなど、モデルに依存する処理をここで設定する
	virtual void PostInit(void) override;
	virtual void InitializeAnimation(void) override;

	void Update(void) override;
	void Draw(void) override;

	void SetupAnimStateMachine(EnemyState initState);

	AnimStateMachine* GetStateMachine(void) override;

	void PlayGeneralAnim(AnimClipName clipName, float speed = 1.0f);
	void PlayAttackAnim(void);

	virtual void Initialize(void) override;

	virtual bool CanWalk(void) const override;
	virtual bool CanStopMoving() const override;
	virtual bool CanStopRunning() const override;
	virtual bool CanAttack() const override;
	virtual bool CanRun(void) const override;
	virtual bool CanHit(void) const override;
	virtual bool CanHit2(void) const override;
	virtual bool CanSurprised(void) const override;
	virtual bool CanDie(void) const override;
	virtual void OnAttackAnimationEnd(void) override;
	virtual void OnHitAnimationEnd(void) override;
	virtual void OnSurprisedEnd(void) override;
	virtual void OnDieAnimationEnd() override;

	virtual const char* GetPanelName(void) const override { return "GBMonster"; }
private:
	virtual void RenderDebugInfo(void) override;
	struct InitContext
	{
		GBMonster* owner;
	};
	void AddAnimation(char* animPath, char* animName, AnimClipName clipName, AnimPlayMode animPlayMode = AnimPlayMode::LOOP) override;
	void LoadWeapon(char* modelPath, char* modelName);
	void InitAnimInfo(void) override;

	void UpdateWeapon(void);

	GameObject<SkinnedMeshModelInstance> m_weapon;
	EnemyBoneHitCollider m_attackCollider;
	AnimStateMachine* m_stateMachine;
	EnemyState m_initState;
	float m_playAnimSpeed;
	FBXLoader& m_fbxLoader = FBXLoader::get_instance();
};