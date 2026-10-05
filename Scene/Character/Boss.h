#pragma once
//=============================================================================
//
// モデル処理 [Boss.h]
// Author : 
//
//=============================================================================
#include "Scene/Enemy.h"
#include "Scene/Weapon/Projectile.h"
#include "Effects/SpecialEffects/LaserRenderer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define HILI_VIEW_ANGLE					(XM_PI * 0.3f)
#define HILI_VIEW_DISTANCE				(800.0f)
#define HILI_CHASING_RANGE				(1500.0f)
#define HILI_ATTACK_RANGE				(140.0f)
#define HILI_MAX_ATTACK_STEP			(520.0f)
#define BOSS_MIN_ATTACK_CDTIME			(80.0f)
#define BOSS_MAX_ATTACK_CDTIME			(160.0f)
#define	BOSS_MAX_COOLDOWN_WAIT_TIME		(0.4f)
#define BOSS_MIN_COOLDOWN_WAIT_TIME		(0.2f)
#define	BOSS_MAX_COOLDOWN_MOVE_TIME		(0.2f)
#define BOSS_MIN_COOLDOWN_MOVE_TIME		(0.1f)

#define BOSS_SIZE					XMFLOAT3(2.8f, 2.8f, 2.8f)

#define BOSS_MAX_HP					(3500.0f)

enum class BossAttackPattern
{
	None,
	Laser,
	IceShard,
	MeleeAttackCombo2,
	MeleeAttackCombo3,
	DashToPlayer,
	BackStep,
};

class Boss : public Enemy, public ISkinnedMeshModelChar
{
public:
	Boss(Transform transform, EnemyState initState = EnemyState::IDLE);
	~Boss();

	// アニメーションやスクリプトなど、モデルに依存する処理をここで設定する
	virtual void PostInit(void) override;
	virtual void InitializeAnimation(void) override;

	void Update(void) override;
	void Draw(void) override;
	void DrawEffect(void);
	void SetupAnimStateMachine(EnemyState initState);

	AnimStateMachine* GetStateMachine(void) override;

	bool CanActNow(void);

	void PlayGeneralAnim(AnimClipName clipName, float speed = 1.0f);
	void PlayAttackAnim(AnimClipName clipName);
	void PlayBackStepAnim(void);
	void PlayCastIceAnim(void);
	void PlayLaserAnim(void);
	void PlayDashToPlayerAnim(void);
	void SetInterruptedIceCast(bool interrupted) { m_interruptedIceCast = interrupted; }
	void ProcessIceStage(void) { m_iceStage++; }

	bool GetBossFightStarted(void) const { return m_bossFightStarted; }
	int GetBossPhase(void) const { return m_bossPhase; }
	void EnterPhase2(void) { m_bossPhase = 2; m_startPhase2 = true; }
	float GetConsecutiveMeleeCount(void) const { return m_consecutiveMeleeCount; }
	float GetLastMeleeTime(void) const { return m_lastMeleeTime; }
	void AddConsecutiveMeleeCount(void) { m_consecutiveMeleeCount++; }
	void SetLastMeleeTime(float time) { m_lastMeleeTime = time; }
	void SetConsecutiveMeleeCount(int count) { m_consecutiveMeleeCount = count; }
	bool PerformAttack(BossAttackPattern pattern);

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
	virtual bool CanFallDown(void) const override;
	virtual bool CanCastIce(void) const override { return m_currentAttackPattern == BossAttackPattern::IceShard; }
	virtual bool CanAttack2(void) const override 
	{
		return m_currentAttackPattern == BossAttackPattern::MeleeAttackCombo2
			|| m_currentAttackPattern == BossAttackPattern::MeleeAttackCombo3;
	}
	virtual bool CanAttack3(void) const override { return m_currentAttackPattern == BossAttackPattern::MeleeAttackCombo3; }
	virtual bool CanStopAttacking2(void) const override { return !CanAttack2(); }
	virtual bool CanStopAttacking3(void) const override { return !CanAttack3(); }
	virtual bool CanDash(void) const override { return m_currentAttackPattern == BossAttackPattern::DashToPlayer; }
	virtual bool CanBackStep(void) const override { return m_currentAttackPattern == BossAttackPattern::BackStep; }
	virtual bool CanLaser(void) const override { return m_currentAttackPattern == BossAttackPattern::Laser; }

	virtual void OnAttackAnimationEnd(void) override;
	virtual void OnHitAnimationEnd(void) override;
	virtual void OnSurprisedEnd(void) override;
	virtual void OnDieAnimationEnd() override;
	virtual void OnGetUpAnimationEnd() override;
	virtual void OnBossfightReady() override;
	virtual void OnBossStandUp() override;
	virtual const char* GetPanelName(void) const override { return "Boss"; }
private:
	// コリジョンイベントコールバック関数
	static void OnIceMeltCallback(const CollisionEvent& event, void* context);
	virtual void RenderDebugInfo(void) override;
	struct InitContext
	{
		Boss* owner;
	};
	void AddAnimation(char* animPath, char* animName, AnimClipName clipName, AnimPlayMode animPlayMode = AnimPlayMode::LOOP) override;
	void InitAnimInfo(void) override;

	void UpdateIceShard(void);

	GameObject<ModelInstance> m_ice;
	EnemyBoneHitCollider m_attackCollider;
	AnimStateMachine* m_stateMachine = nullptr;
	EnemyState m_initState;
	float m_playAnimSpeed;
	bool m_bossFightStarted = false;
	bool m_spawnIceShard = false;
	bool m_interruptedIceCast = false;
	int m_iceStage = 0; // 氷の段階
	int m_bossPhase = 1; // ボスのフェーズ
	bool m_startPhase2 = false;
	bool m_finishMeleeCombo = true;
	int m_consecutiveMeleeCount = 0; // 連続近接攻撃回数
	float m_lastMeleeTime = 0.0f; // 最後に近接攻撃を行った時間
	XMFLOAT3 m_startDashPos = { 0,0,0 };
	XMFLOAT3 m_prevPlayerPos = { 0,0,0 };
	BossAttackPattern m_currentAttackPattern = BossAttackPattern::None;
	Projectile* m_iceShardMiddle = nullptr;
	Projectile* m_iceShardLeft = nullptr;
	Projectile* m_iceShardRight = nullptr;

	LaserRenderer m_laserRenderer;

	FBXLoader& m_fbxLoader = FBXLoader::get_instance();
};