#pragma once
//=============================================================================
//
// モデル処理 [Lumine.h]
// Author : 
//
//=============================================================================
#include "Scene/GameObject.h"
#include "Effects/SpecialEffects/SwordTrail.h"
#include "Utility/InputManager.h"
#include "Effects/SpecialEffects/Water/WaterFluidParticleRenderer.h"
#include "Scene/Item/Item.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define ACTION_QUEUE_SIZE		(4)
#define ACTION_QUEUE_CLEAR_WAIT	(40)
#define PLAYER_HIT_WINDOW		(90)
#define Action(action)			static_cast<UINT>(action)

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct PlayerAction
{
	ActionEnum	actionType;
	float		liveTime;
};

struct PlayerAttributes;

class Lumine : public GameObject<SkinnedMeshModelInstance>, public ISkinnedMeshModelChar
{
public:

	Lumine();
	~Lumine();

	void InitializeStatus(void);

	// アニメーションやスクリプトなど、モデルに依存する処理をここで設定する
	virtual void PostInit(void) override;
	virtual void InitializeAnimation(void) override;

	void Update(void) override;
	void Draw(void) override;
	void DrawEffect(void) override;
	bool LoadFinished(void) { return this->GetLoad() && m_weapon.GetLoad(); }
	void SetPlayerAttributes(PlayerAttributes* attr) { m_playerAttr = attr; }

	void PlaceItem(ItemType type);
	void SetClimb(PlayerState state);

	AnimStateMachine* GetStateMachine(void) override;

	void PlayGeneralAnim(AnimClipName clipName, float speed = 1.0f);

	PlayerState GetCurrentState(void) const { return static_cast<PlayerState>(m_stateMachine->GetCurrentState()); }

	virtual bool ExecuteAction(ActionEnum action) override;

	void PlayAttackAnim(float animSpeed);
	void PlayAttack2Anim(float animSpeed);
	void PlayAttack3Anim(float animSpeed);
	void PlayJumpAnim(float animSpeed);
	void PlayClimbAnim(float animSpeed);
	void PlayThrowAnim(float animSpeed);

	void InitEffect(void);

	virtual bool CanWalk(void) const override;
	virtual bool CanStopMoving(void) const override;
	virtual bool CanAttack(void) const override;
	virtual bool CanAttack2(void) const override;
	virtual bool CanAttack3(void) const override;
	virtual bool CanRun(void) const override;
	virtual bool CanStopRunning(void) const override;
	virtual bool CanHit(void) const override;
	virtual bool CanJump(void) const override;
	virtual bool CanAim(void) const override;
	virtual bool CanWalkAim(void) const override;
	virtual bool CanWalkAimToWalk(void) const override;
	virtual bool CanRunAim(void) const override;
	virtual bool CanRunAimToRun(void) const override;
	virtual bool CanStopAiming(void) const override;
	virtual bool CanJumpAim(void) const override;
	virtual bool CanRunJumpAim(void) const override;
	virtual bool CanClimb(void) const override;
	virtual bool CanGlide(void) const override;
	virtual bool CanStopGliding(void) const override;
	virtual bool CanThrow(void) const override;
	virtual bool CanStopThrowing(void) const override;

	virtual void OnAttackAnimationEnd(void) override;
	virtual void OnHitAnimationEnd(void) override;
	virtual void OnJumpAnimationEnd(void) override;
	virtual void OnPlantSeedAnimationEnd(void) override;
	virtual void OnClimbAnimationEnd(void) override;
	virtual void OnThrowAnimationEnd(void) override;
	//virtual const char* GetPanelName(void) const override { return "Lumine"; };
	
	void SetPlayerComponent(void* player);
	void* GetPlayerComponent(void);
	
	void ForwardEnemyAttackHit(const CollisionEvent& event);

private:
	// コリジョンイベントコールバック関数
	static void OnItemPickupCallback(const CollisionEvent& event, void* context);
	static void OnHitCallback(const CollisionEvent& event, void* context);

	//virtual void RenderImGui(void) override;
	struct InitContext
	{
		Lumine* owner;
	};
	void HandleInput(void);
	void AddAnimation(char* animPath, char* animName, AnimClipName clipName, AnimPlayMode animPlayMode = AnimPlayMode::LOOP) override;
	void LoadWeapon(char* modelPath, char* modelName = nullptr);
	void SetupAnimStateMachine(void);
	void InitAnimInfo(void) override;
	void ResetStatus(void);
	void FaceToNearestEnemy(void);
	void UpdateWeapon(void);

	SwordTrail* swordTrail;
	WaterFluidParticleRenderer* m_waterFluid = nullptr;
	PlayerAttributes* m_playerAttr = nullptr;
	GameObject<SkinnedMeshModelInstance> m_weapon;
	GameObject<ModelInstance> m_gun;

	AnimStateMachine* m_stateMachine;
	float m_playAnimSpeed;
	float m_gunPosX, m_gunPosY, m_gunPosZ, m_gunRotX, m_gunRotY, m_gunRotZ;

	FBXLoader& m_fbxLoader = FBXLoader::get_instance();
	InputManager& m_inputManager = InputManager::get_instance();
	Renderer& m_renderer = Renderer::get_instance();
	void* m_player;
	Camera& m_camera = Camera::get_instance();
};