#pragma once
//=============================================================================
//
// エネミーの行動管理クラス [Enemy.h]
// Author : 
// 敵キャラのHP・移動・攻撃・クールダウンなどをぜんぶ面倒みるAI担当クラスですっ！
// ビヘイビアツリーを使って、驚いたり座ったり追いかけたり、いろんなモードで生きてます
// 
//=============================================================================
#include "Scene/GameObject.h"
#include "Utility/SimpleArray.h"
#include "Core/Timer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define ENEMY_HIT_WINDOW			(60)
#define ENEMY_RESPAWN_TIME			(2000.0f)

enum class CooldownState
{
	MOVE,
	WAIT,
};

enum class EnemyUIType
{
	HPGauge,
	HPGaugeCover,
};

enum class EnemyType
{
	Hilichurl,
	GBMonster,
	Boss,
	NPC_Fish,
	Max,
};

//*****************************************************************************
// 構造体定義
//*****************************************************************************

struct EnemyAttributes
{
	float				HP;
	float				maxHP;

	float				timer;

	float				moveDuration;	// 移動する時間
	float				moveTimer;		// 移動時間のカウンター
	float				waitTime;		// 待機時間
	float				distPlayerSq;

	bool				fixedDirMove;
	float				fixedDir;

	bool				isWaiting;
	bool				isSurprised;
	bool				isChasingPlayer;
	bool				isSitting;
	bool				isInCooldown;

	float				viewAngle;     // 視野角 (例: 60° = 60 * (PI / 180))
	float				viewDistance;  // 視認範囲
	float				chaseRange;    // 追跡範囲
	float				attackRange;   // 攻撃範囲
	float				attackCooldownTimer;
	float				attackCooldownMin;
	float				attackCooldownMax;

	float				cooldownWaitTimer;
	float				cooldownMoveDirection;
	float				cooldownOrbitRadius;
	float				cooldownMoveTimer;

	float				cooldownProbability;
	CooldownState		cooldownState;


	EnemyState			initState;
	Transform			initTrans;

	EnemyType			enemyType;

	bool				die;
	bool				isDead;
	bool				respawn;
	bool				isInvincible;
	bool				startFadeOut;
	bool				randomMove;
	bool				initialMove;
	float				respawnTimer;

	bool				disableGravity;
	bool				turnOnBehaviorTree;

	// 攻撃アニメーションの時間管理
	float attackTimer = 0.0f;        // 経過時間
	float attackHitDelay = 0.3f;     // 振りかぶりから当たり判定が出るまでの遅延
	float attackHitWindow = 0.2f;    // 当たり判定が有効な時間
	float attackTotal = 1.0f;        // 攻撃モーション全体の長さ（例: 1秒）
	bool  attackHitboxEnabled = false; // 当たり判定が有効かどうか
	bool hasDealtDamage = false; // この攻撃でダメージを与えたか？

	void Initialize()
	{
		isWaiting = false;
		isSurprised = false;
		isChasingPlayer = false;
		isSitting = false;
		isInCooldown = false;
		die = false;
		isDead = false;
		startFadeOut = false;
		randomMove = false;

		fixedDirMove = false;
		fixedDir = 0.0f;

		timer = 0.0f;
		moveDuration = 0.0f;
		moveTimer = 0.0f;
		distPlayerSq = 0.0f;
		waitTime = 0.0f;
		cooldownMoveDirection = 0.0f;
		cooldownWaitTimer = 0.0f;
		cooldownMoveTimer = 0.0f;
		cooldownOrbitRadius = 0.0f;
		cooldownProbability = 0.5f;

		disableGravity = false;
		respawn = true;
		isInvincible = false;
		turnOnBehaviorTree = true;
	}
};

struct EnemyUI
{
	XMFLOAT3	pos;			// 位置
	XMFLOAT3	scl;			// スケール
	XMMATRIX	rot;
	MATERIAL	material;		// マテリアル
	float		fWidth;			// 幅
	float		fHeight;		// 高さ
	BOOL		bUse;			// 使用しているかどうか

};

// 敵の各骨格部位に基づくコライダー（攻撃用 or 被弾用）
struct EnemyBoneHitCollider
{
	Collider collider;			// 実際の衝突形状
	BOUNDING_BOX localAABB;		// ローカル空間のAABB
	XMMATRIX boneMatrix;         // このコライダーが追従する骨の変換行列
	bool isHitbox;               // true: 攻撃判定, false: 被弾判定
};

//*****************************************************************************
// プロトタイプ宣言
//*****************************************************************************

// 前方宣言
class Player;
class BehaviorTree;
class BehaviorNode;

class Enemy : public GameObject<SkinnedMeshModelInstance>
{
public:

	friend class BehaviorNode;

	Enemy(EnemyType enemyType, Transform trans);
	~Enemy();

	virtual void Initialize(void);
	inline EnemyAttributes& GetEnemyAttribute(void) { return m_enemyAttr; }
	void Update(void) override;
	void Draw(void) override;
	void DrawUI(EnemyUIType type);

	void SetPlayer(const Player* player) { m_player = player; }

	void ReduceHP(float amount);

	void SetRandomMove(bool random) { m_enemyAttr.randomMove = random; }
	void SetRespawn(bool respawn) { m_enemyAttr.respawn = respawn; }
	void SetInvincible(bool invincible) { m_enemyAttr.isInvincible = invincible; }
	void SetChasePlayer(bool chase) { m_enemyAttr.isChasingPlayer = chase; }

	float DistanceToPlayer(void) const;
	float DistanceToPlayerSq(void) const;

	void InitHPGauge(void);
	void UpdateHPGauge(void);

	void ChasePlayer(void);
	void ChaseAndAttackPlayer(void);
	void AttackPlayer(void);
	void Patrol(void);
	void CooldownMove(void);
	void CooldownWait(void);
	bool DetectPlayer(void);
	bool CheckAvailableToMove(void);


protected:
	// コリジョンイベントコールバック関数
	static void OnPlayerHitCallback(const CollisionEvent& event, void* context);
	static void OnItemHitCallback(const CollisionEvent& event, void* context);

	void InitializeBoneHitCollider(EnemyBoneHitCollider& hitCollider, const XMFLOAT3& scale, const XMFLOAT3& pos, ColliderTag tag);
	void UpdateBoneHitCollider(EnemyBoneHitCollider& hitCollider, BodyTransformIndex transformIdx);
	EnemyAttributes m_enemyAttr;
	const Player* m_player;
private:
	void SetNewPosTarget(void);
	void StartWaiting(void);

	bool UpdateAliveState(void);

	HRESULT MakeVertexHPGauge(float w, float h);
	void DrawHPGauge(void);
	void DrawHPGaugeCover(void);

	BehaviorTree*				m_behaviorTree;  // 敵AIの行動ツリー

	// テクスチャ情報
	ID3D11ShaderResourceView*	m_HPGaugeTex = nullptr;
	ID3D11ShaderResourceView*	m_HPGaugeCoverTex = nullptr;

	// 頂点バッファ
	ID3D11Buffer*				m_HPGaugeVertexBuffer = nullptr;
	ID3D11Buffer*				m_HPGaugeCoverVertexBuffer = nullptr;
	EnemyUI						m_HPGauge;

	Collider* m_attackCollider = nullptr;  // 攻撃用コライダー
	void EnableEnemyAttackCollider(bool enable);

	//void UpdateEditorSelect(int sx, int sy);
};