//=============================================================================
//
// EnemyManager処理 [EnemyManager.cpp]
// Author : 
//
//=============================================================================
#include "Scene/EnemyManager.h"
#include "Scene/Character/Hilichurl.h"
#include "Scene/Character/GBMonster.h"
#include "Scene/Character/Boss.h"
#include "Scene/Character/Enemy_NPC.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define BOSS_INIT_POS				XMFLOAT3(9354.0f, 6420.0f, 2096.0f)

EnemyManager::EnemyManager()
{
	m_player = nullptr;
}

void EnemyManager::Init(const Player* player)
{
#ifdef _DEBUG
	DebugProc::get_instance().Register(this);
#endif // DEBUG

	m_player = player;
	Transform trans{};
	trans.pos = XMFLOAT3(7289.0f, -1400.0f, -18958.0f);

	for (int i = 0; i < 6; i++)
	{
		float radius = 6000.f;
		float r = GetRandFloat(2000.0f, radius);
		float angle = GetRandFloat(0.0f, 2.0f * 3.14159265359f);
		float x = trans.pos.x + r * cos(angle);
		float z = trans.pos.z + r * sin(angle);
		Transform t = trans;
		t.pos.x = x;
		t.pos.z = z;

		SpawnEnemy(EnemyType::Hilichurl, t, EnemyState::IDLE);
	}

	trans.pos = XMFLOAT3(6938.5f, -2152.0f, -18322.56f);
	SpawnEnemy(EnemyType::Hilichurl, trans, EnemyState::DANCE);

	trans.pos = XMFLOAT3(6583.5f, -2152.0f, -18322.56f);
	SpawnEnemy(EnemyType::Hilichurl, trans, EnemyState::DANCE);

	trans.pos = XMFLOAT3(6406.0f, -2152.0f, -18630.0f);
	SpawnEnemy(EnemyType::Hilichurl, trans, EnemyState::DANCE);

	trans.pos = XMFLOAT3(6583.5f, -2152.0f, -18937.43f);
	SpawnEnemy(EnemyType::Hilichurl, trans, EnemyState::DANCE);

	trans.pos = XMFLOAT3(6938.5f, -2152.0f, -18937.43f);
	SpawnEnemy(EnemyType::Hilichurl, trans, EnemyState::DANCE);
}



Enemy* EnemyManager::SpawnEnemy(EnemyType enemyType, Transform trans, EnemyState initState, bool initialMove)
{
	Enemy* enemy = nullptr;
	switch (enemyType)
	{
	case EnemyType::Hilichurl:
		enemy = new Hilichurl(trans, initState);
		break;
	case EnemyType::Boss:
		enemy = new Boss(trans, initState);
		break;
	case EnemyType::NPC_Fish:
		enemy = new Enemy_NPC(trans, EnemyType::NPC_Fish, initState);
		break;
	default:
		return nullptr;
	}

	enemy->GetEnemyAttribute().initialMove = initialMove;

	if (!enemyLoadMap.contains(TO_UINT64(enemyType)))
	{
		enemyLoadMap[TO_UINT64(enemyType)] = 0;
		m_totalEnemyLoadCnt++;
	}

	enemy->SetPlayer(m_player);
	enemy->SetDrawWorldAABB(m_drawBoundingBox);
	m_enemyList.push_back(enemy);

	return enemy;
}

void EnemyManager::InitializeEnemyStatus(void)
{
	Node<Enemy*>* cur = m_enemyList.getHead();
	while (cur != nullptr)
	{
		cur->data->Initialize();
		cur = cur->next;
	}
}

void EnemyManager::Draw(void)
{
	// カリング無効
	m_renderer.SetCullingMode(CULL_MODE_NONE);

	Node<Enemy*>* cur = m_enemyList.getHead();
	while (cur != nullptr)
	{
		// モデル描画
		if (cur->data->GetInstance()->renderProgress.progress < 1.0f)
			m_renderer.SetRenderProgress(cur->data->GetInstance()->renderProgress);

		if (!cur->data->GetEnemyAttribute().isDead)
			cur->data->Draw();

		if (cur->data->GetInstance()->renderProgress.progress < 1.0f)
		{
			RenderProgressCBuffer defaultRenderProgress;
			defaultRenderProgress.isRandomFade = false;
			defaultRenderProgress.progress = 1.0f;
			m_renderer.SetRenderProgress(defaultRenderProgress);
		}

		cur = cur->next;
	}

	// カリング設定を戻す
	m_renderer.SetCullingMode(CULL_MODE_BACK);
}

void EnemyManager::DrawUI(EnemyUIType type)
{
	Node<Enemy*>* cur = m_enemyList.getHead();
	while (cur != nullptr)
	{
		// モデル描画
		if (cur->data->GetInstance()->renderProgress.progress < 1.0f)
			m_renderer.SetRenderProgress(cur->data->GetInstance()->renderProgress);

		if (!cur->data->GetEnemyAttribute().isDead)
			cur->data->DrawUI(type);

		if (cur->data->GetInstance()->renderProgress.progress < 1.0f)
		{
			RenderProgressCBuffer defaultRenderProgress;
			defaultRenderProgress.isRandomFade = false;
			defaultRenderProgress.progress = 1.0f;
			m_renderer.SetRenderProgress(defaultRenderProgress);
		}

		cur = cur->next;
	}
}

void EnemyManager::Update(void)
{
	Node<Enemy*>* cur = m_enemyList.getHead();
	while (cur != nullptr)
	{
		cur->data->Update();

		if (m_currentEnemyLoadCnt < m_totalEnemyLoadCnt)
		{
			EnemyType enemyType = cur->data->GetEnemyAttribute().enemyType;
			if (enemyLoadMap.contains(TO_UINT64(enemyType)))
			{
				if (cur->data->GetLoad() && enemyLoadMap[TO_UINT64(enemyType)] == 0)
				{
					enemyLoadMap[TO_UINT64(enemyType)] = 1;
					m_currentEnemyLoadCnt++;
				}
			}
		}

		if (cur->data->GetUse() == FALSE)
		{
			Node<Enemy*>* toDelete = cur;
			cur = cur->next;
			SAFE_DELETE(toDelete->data);
			m_enemyList.remove(toDelete);
		}
		else
			cur = cur->next;
	}

#if _DEBUG
	if (debugEnemy)
	{
		debugEnemy->SetPosition(XMFLOAT3(debugEnemyPosX, debugEnemyPosY, debugEnemyPosZ));
		debugEnemy->SetScale(XMFLOAT3(debugEnemyScaleX, debugEnemyScaleY, debugEnemyScaleZ));
		debugEnemy->SetRotation(XMFLOAT3(0.0f, debugEnemyRotY, 0.0f));
	}
#endif // _DEBUG

}

void EnemyManager::RenderImGui(void)
{
	if (ImGui::Checkbox("Draw Enemy Bounding Box", &m_drawBoundingBox))
	{
		Node<Enemy*>* cur = m_enemyList.getHead();
		while (cur != nullptr)
		{
			cur->data->SetDrawWorldAABB(m_drawBoundingBox);
			cur = cur->next;
		}
	}

	if (ImGui::CollapsingHeader("Debug Enemy"))
	{
		ImGui::SliderFloat("PosX: ", &debugEnemyPosX, 1133.67f, 11111.67f, "%.1f");
		ImGui::SliderFloat("PosY: ", &debugEnemyPosY, 1131.0f, 11111.0f, "%.1f");
		ImGui::SliderFloat("PosZ: ", &debugEnemyPosZ, 1418.0f, 11111.0f, "%.1f");
		ImGui::SliderFloat("ScaleX: ", &debugEnemyScaleX, 0.1f, 5.0f, "%.1f");
		ImGui::SliderFloat("ScaleY: ", &debugEnemyScaleY, 0.1f, 5.0f, "%.1f");
		ImGui::SliderFloat("ScaleZ: ", &debugEnemyScaleZ, 0.1f, 5.0f, "%.1f");
		ImGui::SliderFloat("RotY: ", &debugEnemyRotY, -3, 3, "%.1f");
	}
}