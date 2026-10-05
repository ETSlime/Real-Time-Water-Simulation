#pragma once
//=============================================================================
//
// 地形および環境オブジェクト管理[Ground.h]
// Author : 
// フィールド・町・環境オブジェクトの描画と更新を管理する
// 
//=============================================================================
#include "Utility/SimpleArray.h"
#include "Scene/Environment.h"
#include "Scene/Town.h"
#include "Scene/GameObject.h"
#include "Scene/Item/Item.h"
#include "Scene/EventTrigger.h"
#include "Scene/EnterExitBehavior.h"
#include "Effects/SpecialEffects/FallingParticleRenderer.h"
#include "Effects/Skybox.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_SCENE_NUM				15

#define WORLD_MAX					XMFLOAT3(50000.0f, 50000.0f, 50000.0f)
#define WORLD_MIN					XMFLOAT3(-50000.0f, -50000.0f, -50000.0f)

// 読み込むモデル名
#define MODEL_BANYAN_PATH			"data/MODEL/Environment/Banyan/Big_Banyan_121.obj"
#define	MODEL_ENVIRONMENT_PATH		"data/MODEL/Environment/"
#define	MODEL_FIELD_PATH			"data/MODEL/Environment/Land.obj"
#define	MODEL_TOWN_PATH				"data/MODEL/Environment/Knight"
#define	MODEL_TREE_NAME				"Tree.fbx"
#define	MODEL_FIELD_NAME			"Land.fbx"
#define MODEL_TERRAIN_PATH			"data/MODEL/Environment/Terrain"
#define MODEL_TERRAIN_NAME			"terrain_prototype2.fbx"
//#define MODEL_TERRAIN_NAME			"spaceland2.fbx"
#define MODEL_BONFIRE_PATH			"data/MODEL/Environment/bonfire.obj"
#define MODEL_LAND_PATH				"data/MODEL/Environment/terrain_prototype.obj"

#define MAX_SEED_NUM					99


enum class SceneID : uint64_t
{
	Spring_Day,
	Spring_Night,
	Summer_Day,
	Summer_Night,
	Autumn_Day,
	Autumn_Night,
	Winter_Day,
	Winter_Night,
	NONE,
	ALL,
};

// 前方宣言
class Ground;
// 各シーンに応じたモデルの生成処理などをまとめて呼び出す
typedef void (Ground::* SceneLoadFunc)();

class Ground : public SingletonBase<Ground>, public IDebugUI
{
public:

	void Initialize(void); // 初期化処理

	void Update(void);
	void Draw(void);

	void UnloadCurrentScene(void); // 現在のシーンをアンロード
	void LoadScene(SceneID id); // シーンをロード
	SceneID GetCurrentSceneID(void) { return m_currentSceneID; }

	void SetEnableBossfightRoom(bool enable);

	int GetTotalModelLoadCnt(void) const { return m_totalModelLoadCnt; } // 全モデルのロード数
	int GetCurrentModelLoadCnt(void) const { return m_currentModelLoadCnt; } // 現在ロードしたモデル数

	// 地形モデルのロードが完了しているか
	bool IsTerrainReady(void) const { return m_currentTerrainModelLoadCnt >= m_totalTerrainModelLoadCnt; }

	void ReinitializeSceneObjects();

private:
	void InitTerrain(void);
	GameObject<ModelInstance>* CreateTerrainGO(GameObjectConfig config);
	GameObject<ModelInstance>* CreateSceneGO(GameObjectConfig config, SceneID sceneID = SceneID::ALL);

	void CreateInstanceEnvironmentObj(InstanceParams& params, float startPosX, float startPosZ,
		int startNumX, int endNumX, int startNumZ, int endNumZ,
		float scale, float rotY, float offSetX, float offsetY, float offsetZ, float jitter = 0.0f);


	void LoadLakeAirWalls(SceneID sceneID); // エアウォールのロード

	// 環境オブジェクトの生成
	void GenerateEnvironmentObj(void*);
	bool DeferredIsFieldReady(void*) { return m_fieldGO->GetModel(); }


	// モデルのロードマップ登録
	void RegisterModelLoadMap(const char* modelPath, bool isPersistent = false, bool isTerrain = true);

	EventTrigger* RegisterEventTrigger(const BOUNDING_BOX& triggerBox, EventCallbackAction action, SceneID sceneID);

	// ゲームオブジェクトの更新と描画
	template<typename T>
	void UpdateGameObjectArray(const SimpleArray<T*>& array);
	template<typename T>
	void DrawGameObjectArray(const SimpleArray<T*>& array);

	virtual void RenderImGui(void) override;
	virtual const char* GetPanelName(void) const override { return "Map Model Manager"; }

	int m_currentModelLoadCnt = 0; // 現在ロードしたモデル数
	int m_totalModelLoadCnt = 0; // 全モデルのロード数
	int m_persistentModelLoadCnt = 0; // 永続的なモデルのロード数
	int m_persistentTerrainModelLoadCnt = 0; // 永続的な地形モデルのロード数
	int m_currentTerrainModelLoadCnt = 0; // 現在ロードした地形モデル数
	int m_totalTerrainModelLoadCnt = 0; // 全地形モデルのロード数

	SimpleArray<GameObject<ModelInstance>*>				m_staticGOs;
	SimpleArray<GameObject<ModelInstance>*>             m_persistentStaticGOs;
	SimpleArray<GameObject<ModelInstance>*>             m_tempGOs;
	SimpleArray<GameObject<SkinnedMeshModelInstance>*>	m_skinnedGOs;
	SimpleArray<GameObject<SkinnedMeshModelInstance>*>  m_persistentSkinnedGOs;

	SimpleArray<EventTrigger*> m_eventTriggers; // イベントトリガーの配列

	GameObject<ModelInstance>* m_fieldGO = nullptr;
	bool m_drawBoundingBox = true;

	GameObject<ModelInstance>* debugGO = nullptr;
	float debugGOPosX = 0.0f;
	float debugGOPosY = 0.0f;
	float debugGOPosZ = 0.0f;
	float debugGOScaleX = 1.0f;
	float debugGOScaleY = 1.0f;
	float debugGOScaleZ = 1.0f;
	float debugGORotY = 0.0f;

	EventTrigger* debugTrigger = nullptr;
	float debugTriggerMinX = -5000.0f;
	float debugTriggerMaxX = 5000.0f;
	float debugTriggerMinY = -5000.0f;
	float debugTriggerMaxY = 5000.0f;
	float debugTriggerMinZ = -5000.0f;
	float debugTriggerMaxZ = 5000.0f;

	Item* debugItem = nullptr;
	float debugItemPosX = 0.0f;
	float debugItemPosY = 0.0f;
	float debugItemPosZ = 0.0f;
	float debugItemScaleX = 1.0f;
	float debugItemScaleY = 1.0f;
	float debugItemScaleZ = 1.0f;
	float debugItemRotZ = 0.0f;
	float debugItemRotX = 0.0f;
	float debugItemRotY = 0.0f;

	bool m_initialized = false; // 初期化フラグ

	Town* m_town = nullptr;
	Environment* m_environment = nullptr;
	FallingParticleRenderer* m_fallingParticles = nullptr;

	SceneID m_currentSceneID = SceneID::NONE;
	Scene& m_scene = Scene::get_instance();
	Renderer& m_renderer = Renderer::get_instance();

	HashMap<const char*, int, CharPtrHash, CharPtrEquals> m_modelLoadMap =
		HashMap<const char*, int, CharPtrHash, CharPtrEquals>(
			MAX_MODEL_TYPE,
			CharPtrHash(),
			CharPtrEquals()
		);

	HashMap<uint64_t, SceneLoadFunc, HashUInt64, EqualUInt64> m_sceneLoadMap = 
		HashMap<uint64_t, SceneLoadFunc, HashUInt64, EqualUInt64>(
			MAX_SCENE_NUM,
			HashUInt64(),
			EqualUInt64()
		);

	HashMap<uint64_t, SimpleArray<ISceneEntity*>, HashUInt64, EqualUInt64> m_sceneEntityMap =
		HashMap<uint64_t, SimpleArray<ISceneEntity*>, HashUInt64, EqualUInt64>(
			MAX_SCENE_NUM,
			HashUInt64(),
			EqualUInt64()
		);

	Skybox& m_skybox = Skybox::get_instance();
	Renderer& m_Renderer = Renderer::get_instance();
};

template<typename T>
inline void Ground::UpdateGameObjectArray(const SimpleArray<T*>& array)
{
	UINT size = array.getSize();
	for (UINT i = 0; i < size; i++)
	{
		auto& gameObject = array[i]; // GameObjectのポインタを取得

		if (m_currentModelLoadCnt < m_totalModelLoadCnt)
		{
			const char* modelPath = gameObject->GetInstance()->modelName;
			// モデルのロードが必要な場合はチェック
			if (m_modelLoadMap.contains(modelPath))
			{
				// モデルのロードが完了していない場合はスキップ
				if (gameObject->GetLoad() && m_modelLoadMap[modelPath] == 0)
				{
					if (gameObject->GetCollisionBuilt())
					{
						m_currentModelLoadCnt++; // モデルのロードカウントを増やす

						if (gameObject->GetCollisionType() != ObjectCollisionType::NONE)
						{
							m_currentTerrainModelLoadCnt++; // 地形モデルのロードカウントを増やす
						}

						m_modelLoadMap[modelPath] = 1; // モデルのロードが完了したことを記録
					}
				}
			}
		}

		if (gameObject->GetUse() == false) 
			continue;

		gameObject->Update();
	}
}

template<typename T>
inline void Ground::DrawGameObjectArray(const SimpleArray<T*>& array)
{
	UINT size = array.getSize();
	for (UINT i = 0; i < size; i++)
	{
		auto& gameObject = array[i]; // GameObjectのポインタを取得

		if (gameObject->GetUse() == false) continue;
		//if (gameObject->GetCollider().tag == ColliderTag::AIR_WALL || gameObject->GetCollider().tag == ColliderTag::ICE_WALL)
		//{
		//	continue;
		//}

		gameObject->Draw();
	}
}
