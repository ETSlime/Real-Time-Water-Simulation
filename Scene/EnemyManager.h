#pragma once
//=============================================================================
//
// エネミーたちのまとめ役ちゃん [EnemyManager.h]
// Author : 
// 敵キャラの生成・更新・描画・UI表示までぜーんぶお世話してくれる管理クラスですっ！
// ダブルリンクリストで敵ちゃんたちをしっかり整列させてるお姉さんタイプかも…？
//
//=============================================================================
#include "Scene/Enemy.h"
#include "Scene/Character/Boss.h"
#include "Utility/DoubleLinkedList.h"
#include "Utility/SingletonBase.h"

//*****************************************************************************
// プロトタイプ宣言
//*****************************************************************************
class EnemyManager : public SingletonBase<EnemyManager>, public IDebugUI
{
public:
	EnemyManager();

	void Init(const Player* player = nullptr);
	void Draw(void);
	void Update(void);
	void DrawUI(EnemyUIType type);

	// 敵キャラの生成
	Enemy* SpawnEnemy(EnemyType enemType, Transform trans, EnemyState initState, bool initialMove = false);
	void InitializeEnemyStatus(void);

	int GetTotalEnemyLoadCnt(void) const { return m_totalEnemyLoadCnt; }
	int GetEnemyLoadedCnt(void) const { return m_currentEnemyLoadCnt; }

	const DoubleLinkedList<Enemy*>* GetEnemy() { return &m_enemyList; }
	Boss* GetBoss(void) { return m_boss; }

private:

	virtual void RenderImGui(void) override;
	virtual const char* GetPanelName(void) const override { return "Enemy Manager"; };

	int m_totalEnemyLoadCnt = 0;
	int m_currentEnemyLoadCnt = 0;

	DoubleLinkedList<Enemy*> m_enemyList;
	Renderer& m_renderer = Renderer::get_instance();
	const Player* m_player;
	bool m_drawBoundingBox = true;

	Boss* m_boss = nullptr;

	Enemy* debugEnemy = nullptr;
	float debugEnemyPosX = 0.0f;
	float debugEnemyPosY = 0.0f;
	float debugEnemyPosZ = 0.0f;
	float debugEnemyScaleX = 1.0f;
	float debugEnemyScaleY = 1.0f;
	float debugEnemyScaleZ = 1.0f;
	float debugEnemyRotY = 0.0f;

	HashMap<uint64_t, int, HashUInt64, EqualUInt64> enemyLoadMap =
		HashMap<uint64_t, int, HashUInt64, EqualUInt64>(
			static_cast<int>(EnemyType::Max),
			HashUInt64(),
			EqualUInt64()
		);
};