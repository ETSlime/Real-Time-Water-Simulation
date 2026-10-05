#pragma once
//=============================================================================
//
// プレイヤー操作＆主役ちゃん管理クラス [Player.h]
// Author : 
// プレイヤーの行動・移動・描画・エフェクトを一括で担当する主人公ちゃん管理クラスですっ！
// SigewinneやKleeたちの切り替えもここで制御して、アクションキューで動きを可愛く整列します
// 
//=============================================================================
#include "Scene/Character/Lumine.h"
#include "Scene/Character/Hilichurl.h"
#include "Scene/Item/Item.h"
#include "Scene/Item/Interactable/Interactable.h"
#include "Scene/Item/ItemManager.h"
#include "Scene/Weapon/Projectile.h"
#include "Core/LightManager.h"
#include "Utility/InputManager.h"
#include "Effects/ProjectilePreview/ProjectilePreviewRenderer.h"
#include "Effects/ProjectilePreview/ArcLengthPathFollower.h"
#include "Effects/ProjectilePreview/ProjectileFollowerManager.h"
#include "UI/Base/UIManager.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define	VALUE_PLAYER_MOVE			(7.5f)							// 移動量
#define VALUE_PLAYER_CLIMB			(3.0f)							// クライム移動量
#define VALUE_PLAYEER_RUN_FACTOR	(2.5f)
#define	VALUE_PLAYER_ROTATE			(XM_PI * 0.02f)					// 回転量

//#define PLAYER_INIT_POS				XMFLOAT3(1932.0f, 6370.0f, 1904.0f)
#define PLAYER_INIT_POS		XMFLOAT3(4518.0f, -2060.0f, -16244.0f)

#define BULLET_FIRE_INTERVAL		(0.1f)							// 弾の発射間隔（秒）
#define SUN_BULLET_FIRE_INTERVAL	(0.5f)							// 弾の発射間隔（秒）

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct PlayerAttributes
{
	UINT			actionQueueClearTime;
	PlayerAction	actionQueue[ACTION_QUEUE_SIZE];
	UINT			actionQueueStart;
	UINT			actionQueueEnd;

	int				seedCount = 0;
	int				keyCount = 0;
	int				bulletCount = 0;
	bool			hasBullet = false;
	bool			plantedSeed = false;

	bool			isMeleeMode = true;
	bool			allowControl = true;
	bool			aimMode = false;	// エイムモード
	bool			throwMode = false;
	bool			confirmThrow = false;
	bool			confirmThrowFlag = false;
	bool			cancelThrowFlag = false;
	bool			climbMode = false; // クライムモード
	bool			climbUp = false; // クライムモードで上に移動中かどうか
	bool 			climbDown = false; // クライムモードで下に移動中かどうか
	bool			enableClimbUp = false; // クライムモードで上に移動できるかどうか
	bool			enableClimbDown = false; // クライムモードで下に移動できるかどうか

	float			weaponOnBackTimer = 0.0f;
	float			weaponOnHandTimer = 0.0f;
	bool			weaponOnBack = false;
	bool			storeWeaponOnBack = false;
	bool			storeWeapon = false;
	bool			finishStoreWeapon = true;

	bool			drawGun = false;
	float			fireIntervalSec = BULLET_FIRE_INTERVAL; // 射撃インターバル（秒）
	float			fireCooldownSec = 0.0f; // 現在のクールダウン残り時間（秒）: 0以下なら発射可能

	PlayerState		currentState = PlayerState::IDLE;
	ProjectileType	currentProjectile = ProjectileType::Bullet;

	float			groundY = 0.0f; // 地面のY座標
	XMFLOAT3		initPos{};
};

struct SaveData;

class Player : public IDebugUI
{
public:
	Player();
	~Player();
	void Update(void);
	void Draw(void);
	void DrawEffect(void);
	Transform GetTransform(void) const;

	void InitializePlayerStatus(void);
	void LoadSaveData(const SaveData& saveData);

	int GetTotalModelLoadCnt(void) const { return m_totalModelLoadCnt; }
	int GetCurrentModelLoadCnt(void) const { return m_currentModelLoadCnt; }

	void SetPosition(const XMFLOAT3& pos) { if (m_playerGO) m_playerGO->SetPosition(pos); }
	void ResetPlayerPosition(void) { if (m_playerGO) m_playerGO->SetPosition(m_playerAttr.initPos); }

	void SetClimbMode(PlayerState climbState);
	void PlaceItem(ItemType type);

	BOUNDING_BOX GetPlayerAABB(void) const { return m_playerGO->GetCollider().aabb; }

	static void OnClimbPromptTriggered(void* userData);

	static void OnPlaceItemPromptTriggered(void* userData);

	bool GetMeleeMode(void) const { return m_playerAttr.isMeleeMode; }
	bool HasSeed(void) const { return m_playerAttr.seedCount > 0; }
	bool HasBullet(void) const { return m_playerAttr.hasBullet; }
	PlayerAttributes& GetPlayerAttributes(void) { return m_playerAttr; }
	const BOUNDING_BOX& GetBoundingBox(void) const { return m_playerGO->GetCollider().aabb; }

	PlayerState GetCurrentState(void) const { return static_cast<PlayerState>(m_playerGO->GetStateMachine()->GetCurrentState()); }

	float GetCurrentHP() const { return m_currentHP; }
	float GetMaxHP() const { return m_maxHP; }

	void TakeDamage(int damage);
	void Heal(int amount);
	void OnEnemyAttackHit(const CollisionEvent& e);

	void LinkInteractable(Interactable* interactable);

private:
	void UpdateNormalMode(void);
	void UpdateClimbMode(void);
	void UpdateActionQueue(void);
	void UpdatePlayerStatus(void);
	void UpdatePlayerMove(void);
	void UpdatePlayerAttack(void);
	void UpdateSwitchCharacter(void);
	void HandleInput(void);
	void HandlePlayerMove(Transform& transform);
	void AlignPlayerYawToCamera(void); // プレイヤーのYawをカメラのYawに補間して合わせる関数
	void ConfirmThrow(void);
	void CancelThrow(void);

	virtual void RenderImGui(void) override;
	virtual const char* GetPanelName(void) const override { return "Player"; };

	Attributes* m_attributes = nullptr;
	Transform m_transform;
	PlayerAttributes m_playerAttr;

	Lumine* lumine = nullptr;


	bool m_drawBoundingBox = true;
	int m_currentModelLoadCnt = 0;
	int m_totalModelLoadCnt = 1;
	bool m_hasWeapon = false;

	GameObject<SkinnedMeshModelInstance>* m_playerGO = nullptr;

	Light* m_light = nullptr;

	ProjectilePreviewRenderer m_previewRenderer;
	ProjectilePredictor m_predictor;


	Renderer& m_renderer = Renderer::get_instance();
	LightManager& m_lightMgr = LightManager::get_instance();
	Camera& m_camera = Camera::get_instance();
	DebugProc& m_debugProc = DebugProc::get_instance();
	InputManager& m_inputManager = InputManager::get_instance();

	Interactable*   m_linkedInteractable = nullptr;

	ItemManager& m_itemManager = ItemManager::get_instance();
	ProjectileFollowerManager& m_projectileFollowerManager = ProjectileFollowerManager::get_instance();

	float m_currentHP = 100;
	float m_maxHP = 100;        // 最大HP

	float m_hpDrainTimer = 0.0f;
	bool m_autoHPDrain = true;

	float m_invincibleTimer = 0.0f;         // 無敵時間カウント
	const float INVINCIBLE_TIME = 0.5f;     // 0.5秒間無敵

	UIInteractPrompt* m_InteractPrompt = nullptr;
	char* m_InteractPromptTexLoc = nullptr;
};