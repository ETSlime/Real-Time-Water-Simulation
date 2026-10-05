//=============================================================================
//
// Gound処理 [Ground.cpp]
// Author : 
//
//=============================================================================
#include "main.h"
#include "Core/Graphics/Renderer.h"
#include "Core/Async//DeferredTaskAPI.h"
#include "Model/Model.h"
#include "Utility/InputManager.h"
#include "Utility/Debug/Debugproc.h"
#include "Collision/CollisionManager.h"
#include "Effects/EffectSystem.h"
#include "Effects/SpecialEffects/FireBallEffectRenderer.h"
#include "Scene/Ground.h"
#include "Scene/Item/ItemManager.h"
#include "Scene/Item/Interactable/AutumnDoor.h"
#include "Scene/Item/Interactable/Gate.h"
#include "Core/GameSystem.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define TREE_SIZE			(350.0f)
#define KEY_SIZE			(5.0f)
#define SEED_SIZE			(0.2f)
#define SEEDSOIL_SIZE       (10)
#define AUTUMNDOOR_SIZE       (13)
#define SOIL_SIZE			(100.0f)
#define FIELD_SIZE			(4685.0f)
#define TERRAIN_SIZE		(12.0f)
#define BONFIRE_SIZE		(12.0f)

#define TERRAIN_POS			XMFLOAT3(5371.67f, 5775.0f, 5895.0f)

#define SOIL_POS			XMFLOAT3(8833.67f, 6229.0f, 8084.0f)
#define SOIL_SCALE			XMFLOAT3(48.0f, 31.0f, 46.0f)
#define SOIL_ROT_Y			(-0.55f)


//=============================================================================
// 初期化処理
//=============================================================================
void Ground::Initialize(void)
{
#ifdef _DEBUG
	DebugProc::get_instance().Register(this);
#endif // _DEBUG

	BOUNDING_BOX worldBB;
	worldBB.maxPoint = WORLD_MAX;
	worldBB.minPoint = WORLD_MIN;
	CollisionManager::get_instance().InitOctree(worldBB);

	//m_town = new Town();

	InitTerrain();


	DeferredTaskOptions options{};
	options.async = false; // 同期実行
	options.debugName = "Ground_GenerateEnvironmentObj"; // デバッグ用の名前
	// タスクを DeferredTaskSystem に登録
	DeferredTaskBinder<Ground>::Add(
		this,
		nullptr,
		&Ground::GenerateEnvironmentObj,
		&Ground::DeferredIsFieldReady,
		options
	);

	m_initialized = true;
}

//=============================================================================
// 更新処理
//=============================================================================
void Ground::Update(void)
{
	if (!m_initialized) return;

	UpdateGameObjectArray<GameObject<ModelInstance>>(m_staticGOs);
	UpdateGameObjectArray<GameObject<ModelInstance>>(m_persistentStaticGOs);
	UpdateGameObjectArray<GameObject<SkinnedMeshModelInstance>>(m_skinnedGOs);
	UpdateGameObjectArray<GameObject<SkinnedMeshModelInstance>>(m_persistentSkinnedGOs);

	UpdateGameObjectArray<GameObject<ModelInstance>>(m_tempGOs);

	if (m_town)
		m_town->Update();

	if (m_environment)
		m_environment->Update();

	if (m_currentSceneID == SceneID::Winter_Day || m_currentSceneID == SceneID::Winter_Night)
	{
		// 雪のパーティクルを更新
		Player* player = GameSystem::get_instance().GetPlayer();
		if (player)
		{
			XMFLOAT3 emitPos = player->GetTransform().pos;
			emitPos.y += 800.0f; // 少し上から
			if (m_fallingParticles)
			{
				if (m_fallingParticles && VoxelBufferUploader::get_instance().IsBuffersUploaded())
					m_fallingParticles->EmitParticles(7000.0f, 1.0f, emitPos);
			}
		}
	}


	/// INTERACTABLE-IMPLEMENTATION
	/*if (GetCurrentScene() == SceneID::SCENE_1) {
	
		if (m_sceneSoil != nullptr) {
		
			if (m_sceneSoil->GetItemAttributes().used && m_scenePSeed == nullptr)
			{
				Transform trans;
				trans.pos = m_sceneSoil->GetTransform().pos;
				//trans.pos.y -= 100;
				trans.rot = XMFLOAT3(0.0f, 0.0f, 0.0f);
				trans.scl = XMFLOAT3(SEED_SIZE, SEED_SIZE, SEED_SIZE);

				m_scenePSeed = ItemManager::get_instance().SpawnItem(ItemType::IntPSeed, trans);
			}

		}
	
	}*/
#ifdef _DEBUG
	if (debugGO)
	{
		debugGO->SetPosition(XMFLOAT3(debugGOPosX, debugGOPosY, debugGOPosZ));
		debugGO->SetScale(XMFLOAT3(debugGOScaleX, debugGOScaleY, debugGOScaleZ));
		debugGO->SetRotation(XMFLOAT3(0.0f, debugGORotY, 0.0f));
	}

	if (debugTrigger)
	{
		BOUNDING_BOX triggerBox;
		triggerBox.minPoint = XMFLOAT3(debugTriggerMinX, debugTriggerMinY, debugTriggerMinZ);
		triggerBox.maxPoint = XMFLOAT3(debugTriggerMaxX, debugTriggerMaxY, debugTriggerMaxZ);
		debugTrigger->SetBoundingBox(triggerBox);
	}

	if (debugItem)
	{
		debugItem->SetPosition(XMFLOAT3(debugItemPosX, debugItemPosY, debugItemPosZ));
		debugItem->SetScale(XMFLOAT3(debugItemScaleX, debugItemScaleY, debugItemScaleZ));
		debugItem->SetRotation(XMFLOAT3(debugItemRotX, debugItemRotY, debugItemRotZ));
	}
#endif // _DEBUG
}

//=============================================================================
// 描画処理
//=============================================================================
void Ground::Draw(void)
{
	if (!m_initialized) return;

	m_renderer.SetLightModeBuffer(1);

	if (m_town)
		m_town->Draw();

	if (m_renderer.GetRenderMode() == RenderMode::INSTANCE)
	{
		if (m_environment)
			m_environment->Draw();
	}
	else if (m_renderer.GetRenderMode() == RenderMode::OBJ)
	{
		DrawGameObjectArray<GameObject<ModelInstance>>(m_staticGOs);
		DrawGameObjectArray<GameObject<ModelInstance>>(m_persistentStaticGOs);
		DrawGameObjectArray<GameObject<ModelInstance>>(m_tempGOs);
	}
	else if (m_renderer.GetRenderMode() == RenderMode::SKINNED_MESH)
	{
		DrawGameObjectArray<GameObject<SkinnedMeshModelInstance>>(m_skinnedGOs);
		DrawGameObjectArray<GameObject<SkinnedMeshModelInstance>>(m_persistentSkinnedGOs);
	}

	m_renderer.SetLightModeBuffer(0);
}

void Ground::UnloadCurrentScene(void)
{
	if (!m_initialized) return;

	for (auto it = m_modelLoadMap.begin(); it != m_modelLoadMap.end();)
	{
		// モデルロード済みのものは削除
		if ((*it).value == 1)
			it = m_modelLoadMap.erase(it);
		else
			++it;
	}

	// 現在のシーンの GameObject をクリア
	m_staticGOs.clear();
	m_skinnedGOs.clear();

	// 現在のシーンのモデルロードカウントをリセット
	m_totalModelLoadCnt = m_persistentModelLoadCnt;
	m_totalTerrainModelLoadCnt = m_persistentTerrainModelLoadCnt;
	m_currentModelLoadCnt = 0;
	m_currentTerrainModelLoadCnt = 0;

	// 現在のシーンが存在しない場合は何もしない
	if (!m_sceneEntityMap.contains(TO_UINT64(m_currentSceneID)))
		return;

	auto& sceneData = m_sceneEntityMap[TO_UINT64(m_currentSceneID)];

	// まずはシーンから登録解除
	for (ISceneEntity* entity : sceneData)
		m_scene.UnregisterEntity(entity);

	// オブジェクトを破棄（Destroy）
	// Unregister() 中で m_renderableObjects や m_entities を操作するため、
	// その後 Destroy() で同じリストを操作するとメモリ破壊やクラッシュ
	for (ISceneEntity* entity : sceneData)
		entity->Destroy();

	// GameObject の破棄は Scene が一括で処理する
	// - GameObject は ShadowRenderer などで使われるため、描画中に削除されると危険！
	// - 安全のため、事前に Unregister してから delete する必要があるのっ！
	// - だから Scene::DestroyAllMarkedGameObjects() で一括破棄を行うよ～！
	m_scene.DestroyAllMarkedGameObjects();

	sceneData.clear();

	m_currentSceneID = SceneID::NONE;
}

void Ground::LoadScene(SceneID sceneID)
{
	if (!m_initialized) return;

	// 現在のシーンと同じ場合は何もしない
	if (m_currentSceneID == sceneID)
		return;

	UnloadCurrentScene(); // 現在のシーンを破棄

	// シーンに対応するロード関数を呼び出す
	auto it = m_sceneLoadMap.find(TO_UINT64(sceneID));
	if (it != m_sceneLoadMap.end())
	{
		SceneLoadFunc func = it->value;
		(this->*func)(); // 登録されたロード関数を呼び出す
		m_currentSceneID = sceneID;
	}

	// "ALL" シーンのロード関数も呼び出す
	auto all = m_sceneLoadMap.find(TO_UINT64(SceneID::ALL));
	if (all != m_sceneLoadMap.end())
	{
		SceneLoadFunc func = all->value;
		(this->*func)(); // 登録されたロード関数を呼び出す
	}
}

void Ground::SetEnableBossfightRoom(bool enable)
{
	if (enable)
	{
		GameObjectConfig airwallConfig{};
		airwallConfig.modelPath = "data/MODEL/Environment/Cube.obj";
		airwallConfig.castShadow = false;
		airwallConfig.drawWorldAABB = false;
		airwallConfig.collisionType = ObjectCollisionType::COLLIDER_BOUNDING_BOX;
		airwallConfig.colliderTag = ColliderTag::AIR_WALL;
		airwallConfig.position = XMFLOAT3(5180.0f, 6488.0f, 2400.0f);
		airwallConfig.scale = XMFLOAT3(23.0f, 25.0f, 410.0f);
		CreateSceneGO(airwallConfig, SceneID::NONE);

		airwallConfig.position = XMFLOAT3(12637.0f, 6488.0f, 2400.0f);
		airwallConfig.scale = XMFLOAT3(23.0f, 25.0f, 410.0f);
		CreateSceneGO(airwallConfig, SceneID::NONE);

		airwallConfig.position = XMFLOAT3(9407.0f, 6488.0f, 6378.0f);
		airwallConfig.scale = XMFLOAT3(400.0f, 25.0f, 33.0f);
		CreateSceneGO(airwallConfig, SceneID::NONE);

		airwallConfig.position = XMFLOAT3(9407.0f, 6488.0f, -962.0f);
		airwallConfig.scale = XMFLOAT3(400.0f, 25.0f, 36.0f);
		CreateSceneGO(airwallConfig, SceneID::NONE);
	}
	else
	{
		auto& sceneData = m_sceneEntityMap[TO_UINT64(SceneID::NONE)];

		m_tempGOs.clear();

		// まずはシーンから登録解除
		for (ISceneEntity* entity : sceneData)
			m_scene.UnregisterEntity(entity);


		for (ISceneEntity* entity : sceneData)
			entity->Destroy();

		m_scene.DestroyAllMarkedGameObjects();

		sceneData.clear();
	}
}

void Ground::ReinitializeSceneObjects()
{

}

void Ground::InitTerrain(void)
{
	GameObject<ModelInstance>* terrainGO = new GameObject<ModelInstance>();
	GameObjectConfig terrainBuildConfig{};
	GameObjectConfig airwallConfig{};
	//terrainBuildConfig.modelPath = "data/MODEL/Level/terrain/ground_new.obj";
	//terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	//terrainBuildConfig.scale = XMFLOAT3(TERRAIN_SIZE, TERRAIN_SIZE, TERRAIN_SIZE);
	//terrainBuildConfig.position = TERRAIN_POS;
	//terrainBuildConfig.drawWorldAABB = false;
	//terrainBuildConfig.flip = AxisFlip::FlipX;
	//terrainBuildConfig.loadAsync = false;
	//terrainGO->Instantiate(terrainBuildConfig);
	//m_persistentStaticGOs.push_back(terrainGO);
	//m_fieldGO = terrainGO;

	terrainBuildConfig.modelPath = MODEL_FIELD_PATH;
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.scale = XMFLOAT3(FIELD_SIZE, FIELD_SIZE, FIELD_SIZE);
	terrainBuildConfig.position = XMFLOAT3(0.0f, -650.0f, 0.0f);
	terrainBuildConfig.drawWorldAABB = false;
	terrainBuildConfig.loadAsync = false;
	terrainGO->Instantiate(terrainBuildConfig);
	m_persistentStaticGOs.push_back(terrainGO);
	m_fieldGO = terrainGO;


	if (!m_modelLoadMap.contains(terrainBuildConfig.modelPath))
	{
		m_modelLoadMap[terrainBuildConfig.modelPath] = 0;
		m_totalModelLoadCnt++;
		m_persistentModelLoadCnt++;
	}


	terrainBuildConfig.modelPath = MODEL_BONFIRE_PATH;
	terrainBuildConfig.collisionType = ObjectCollisionType::OBJECT_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(6761.0f, -2023.0f, -18630.0f);
	terrainBuildConfig.scale = XMFLOAT3(BONFIRE_SIZE, BONFIRE_SIZE, BONFIRE_SIZE);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 2.0f, 0.0f);
	terrainBuildConfig.drawWorldAABB = false;
	terrainBuildConfig.flip = AxisFlip::None;
	terrainBuildConfig.loadAsync = false;
	CreateTerrainGO(terrainBuildConfig);


	terrainBuildConfig.modelPath = "data/MODEL/Environment/Cottage/Cottage2.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(7289.0f, -1559.0f, -19558.0f);
	terrainBuildConfig.scale = XMFLOAT3(55.0f, 55.0f, 55.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, -0.77f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);


	terrainBuildConfig.modelPath = "data/MODEL/Environment/Church/Church.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(4223.0f, -627.0f, -3050.0f);
	terrainBuildConfig.scale = XMFLOAT3(200.0f, 154.0f, 195.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	//airwallConfig.modelPath = "data/MODEL/Environment/Cube.obj";
	//airwallConfig.castShadow = false;
	//airwallConfig.drawWorldAABB = false;
	//airwallConfig.collisionType = ObjectCollisionType::OBJECT_BOUNDING_BOX;
	//airwallConfig.colliderTag = ColliderTag::AIR_WALL;
	//airwallConfig.position = XMFLOAT3(4223.0f, -627.0f, -3050.0f);
	//airwallConfig.scale = XMFLOAT3(113.0f, 154.0f, 144.0f);
	//airwallConfig.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	//CreateTerrainGO(airwallConfig);


	terrainBuildConfig.modelPath = "data/MODEL/Environment/House/House.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(8163.0f, -734.0f, -6838.0f);
	terrainBuildConfig.scale = XMFLOAT3(177.0f, 143.0f, 156.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 1.57f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	//airwallConfig.modelPath = "data/MODEL/Environment/Cube.obj";
	//airwallConfig.castShadow = false;
	//airwallConfig.drawWorldAABB = false;
	//airwallConfig.collisionType = ObjectCollisionType::OBJECT_BOUNDING_BOX;
	//airwallConfig.colliderTag = ColliderTag::AIR_WALL;
	//airwallConfig.position = XMFLOAT3(8163.0f, -734.0f, -6838.0f);
	//airwallConfig.scale = XMFLOAT3(177.0f, 143.0f, 156.0f);
	//airwallConfig.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	//debugGO = CreateTerrainGO(airwallConfig);

	//debugGOPosX = terrainBuildConfig.position.x;
	//debugGOPosY = terrainBuildConfig.position.y;
	//debugGOPosZ = terrainBuildConfig.position.z;
	//debugGOScaleX = terrainBuildConfig.scale.x;
	//debugGOScaleY = terrainBuildConfig.scale.y;
	//debugGOScaleZ = terrainBuildConfig.scale.z;
	//debugGORotY = terrainBuildConfig.rotation.y;

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Inn/Inn.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(8473.0f, -558.0f, -10578.0f);
	terrainBuildConfig.scale = XMFLOAT3(174.0f, 161.0f, 166.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 1.57f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);


	terrainBuildConfig.modelPath = "data/MODEL/Environment/Barn/Barn.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(1332.0f, -1520.0f, -5590.0f);
	terrainBuildConfig.scale = XMFLOAT3(80.0f, 80.0f, 97.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Watchtower/Watchtower.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(8471.0f, -903.0f, -13119.0f);
	terrainBuildConfig.scale = XMFLOAT3(128.0f, 128.0f, 128.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 3.14f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);


	terrainBuildConfig.modelPath = "data/MODEL/Environment/Cottage/Cottage3.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(2011.0f, -1562.0f, -10086.0f);
	terrainBuildConfig.scale = XMFLOAT3(55.0f, 55.0f, 55.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, -1.57f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);


	terrainBuildConfig.modelPath = "data/MODEL/Environment/Cottage/Cottage4.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(2088.0f, -1498.0f, -12433.0f);
	terrainBuildConfig.scale = XMFLOAT3(55.0f, 55.0f, 55.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, -1.98f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Townhouse/Townhouse.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(1156.0f, -1349.0f, -8131.0f);
	terrainBuildConfig.scale = XMFLOAT3(78.0f, 78.0f, 78.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, -1.98f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Townhouse2/Townhouse2.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(2165.0f, -1412.0f, -9011.0f);
	terrainBuildConfig.scale = XMFLOAT3(75.0f, 62.0f, 75.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, -2.03f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Townhouse2/Townhouse2.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(2397.0f, -1433.0f, -11162.0f);
	terrainBuildConfig.scale = XMFLOAT3(75.0f, 62.0f, 75.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, -2.03f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Cottage/Cottage5.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(2864.0f, -1540.0f, -13410.0f);
	terrainBuildConfig.scale = XMFLOAT3(55.0f, 55.0f, 55.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, -3.0f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);


	terrainBuildConfig.modelPath = "data/MODEL/Environment/Windmill/Windmill.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(3283.0f, -273.0f, 3958.0f);
	terrainBuildConfig.scale = XMFLOAT3(70.0f, 70.0f, 70.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Windmill/Windmill2.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(10267.0f, 152.0f, 4251.0f);
	terrainBuildConfig.scale = XMFLOAT3(83.0f, 83.0f, 83.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Windmill/Windmill3.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(7007.0f, 130.0f, 7708.0f);
	terrainBuildConfig.scale = XMFLOAT3(83.0f, 83.0f, 83.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Windmill/Windmill4.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(1108.0f, 300.0f, 7708.0f);
	terrainBuildConfig.scale = XMFLOAT3(83.0f, 83.0f, 83.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Stall1/Stall1.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(3253.0f, -1946.0f, -8033.0f);
	terrainBuildConfig.scale = XMFLOAT3(24.0f, 24.0f, 24.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 1.37f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Stall2/Stall2.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(3408.0f, -1946.0f, -8913.0f);
	terrainBuildConfig.scale = XMFLOAT3(24.0f, 24.0f, 24.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 1.37f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Stall3/Stall3.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(3485.0f, -1882.0f, -9402.0f);
	terrainBuildConfig.scale = XMFLOAT3(24.0f, 24.0f, 24.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 1.37f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Stall1/Stall11.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(3641.0f, -1903.0f, -10282.0f);
	terrainBuildConfig.scale = XMFLOAT3(24.0f, 24.0f, 24.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 1.37f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Stall2/Stall22.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(5581.0f, -1924.0f, -8228.0f);
	terrainBuildConfig.scale = XMFLOAT3(24.0f, 24.0f, 24.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, -1.59f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Stall3/Stall33.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(5658.0f, -1902.0f, -9303.0f);
	terrainBuildConfig.scale = XMFLOAT3(24.0f, 24.0f, 24.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, -1.59f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Stall3/Stall333.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(5658.0f, -1838.0f, -10383.0f);
	terrainBuildConfig.scale = XMFLOAT3(24.0f, 24.0f, 24.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, -1.59f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Stall1/Stall111.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(5658.0f, -1795.0f, -10872.0f);
	terrainBuildConfig.scale = XMFLOAT3(24.0f, 24.0f, 24.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, -1.59f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Sculpture/Sculpture.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(4183.0f, -1454.0f, -7351.0f);
	terrainBuildConfig.scale = XMFLOAT3(66.0f, 66.0f, 66.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Well/Well.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(6280.0f, -1890.0f, -3341.0f);
	terrainBuildConfig.scale = XMFLOAT3(14.0f, 14.0f, 14.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Scarecrow/Scarecrow.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(8142.0f, -1756.0f, -3536.0f);
	terrainBuildConfig.scale = XMFLOAT3(30.0f, 30.0f, 30.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);

	terrainBuildConfig.modelPath = "data/MODEL/Environment/Entrance/Entrance.obj";
	terrainBuildConfig.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	terrainBuildConfig.position = XMFLOAT3(4571.0f, -1411.0f, -14783.0f);
	terrainBuildConfig.scale = XMFLOAT3(121.0f, 98.0f, 123.0f);
	terrainBuildConfig.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	CreateTerrainGO(terrainBuildConfig);



	ParticleEffectParams smokeParams;
	smokeParams.type = EffectType::Smoke;
	smokeParams.duration = 9999.0f;
	smokeParams.position = XMFLOAT3(6761.0f, -2023.0f, -18630.0f);
	smokeParams.position.y += 90.0f; // 少し上に配置
	smokeParams.scale = 175.0f;
	smokeParams.acceleration = XMFLOAT3(0.0f, 25.0f, 0.0f);
	smokeParams.spawnRateMin = 2.0f;
	smokeParams.spawnRateMax = 3.0f;
	smokeParams.lifeMin = 5.5f;
	smokeParams.lifeMax = 7.0f;
	smokeParams.startColor = XMFLOAT4(0.5f, 0.2f, 0.2f, 0.3f);

	EffectSystem::get_instance().SpawnParticleEffect(smokeParams);


	smokeParams.position = XMFLOAT3(980.0f, -615.0f, -7659.0f);
	smokeParams.lifeMin = 6.5f;
	smokeParams.lifeMax = 9.0f;
	smokeParams.scale = 215.0f;
	EffectSystem::get_instance().SpawnParticleEffect(smokeParams);

	smokeParams.position = XMFLOAT3(2025.0f, -952.0f, -9665.0f);
	EffectSystem::get_instance().SpawnParticleEffect(smokeParams);

	smokeParams.position = XMFLOAT3(1916.0f, -922.0f, -12047.0f);
	EffectSystem::get_instance().SpawnParticleEffect(smokeParams);

	smokeParams.position = XMFLOAT3(2405.0f, -942.0f, -13336.0f);
	EffectSystem::get_instance().SpawnParticleEffect(smokeParams);

	smokeParams.position = XMFLOAT3(7600.0f, -919.0f, -19226.0f);
	EffectSystem::get_instance().SpawnParticleEffect(smokeParams);

	smokeParams.position = XMFLOAT3(8263.0f, 757.0f, -8161.0f);
	smokeParams.scale = 255.0f;
	smokeParams.lifeMin = 8.5f;
	smokeParams.lifeMax = 12.0f;
	EffectSystem::get_instance().SpawnParticleEffect(smokeParams);

	FireBallEffectParams fireparams;
	fireparams.type = EffectType::FireBall;
	fireparams.duration = 9999.0f;
	fireparams.position = XMFLOAT3(6761.0f, -2023.0f, -18630.0f);
	fireparams.scale = 23.0f;
	fireparams.lifeMin = 1.0f;
	fireparams.lifeMax = 1.0f;
	fireparams.spawnRateMin = 0.0f;
	fireparams.spawnRateMax = 10.0f;
	fireparams.acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);
	fireparams.startColor = XMFLOAT4(1.0f, 0.5f, 0.2f, 1.0f);
	fireparams.endColor = XMFLOAT4(1.0f, 0.2f, 0.2f, 0.0f);
	fireparams.tilesX = FIREBALL_ANIM_TILE_X;
	fireparams.tilesY = FIREBALL_ANIM_TILE_Y;
	fireparams.coneAngleDegree = 25.0f;
	fireparams.coneRadius = 0.6f;
	fireparams.coneLength = 5.0f;
	fireparams.frameLerpCurve = 1.0f;
	fireparams.rotationSpeed = 0.0f;
	fireparams.startSpeedMin = 2.0f;
	fireparams.startSpeedMax = 3.0f;

	IEffectRenderer* effect = EffectSystem::get_instance().SpawnParticleEffect(fireparams);

	//terrainBuildConfig.modelPath = "data/MODEL/Level/terrain/cubes.obj";
	//CreateTerrainGO(terrainBuildConfig);


	//terrainBuildConfig.modelPath = "data/MODEL/Level/addon/Fence.obj";
	//terrainBuildConfig.position = XMFLOAT3(1178.0f, 6415.0f, 3239.6f);
	//terrainBuildConfig.scale = XMFLOAT3(21.4f, 15.0f, 15.0f);
	//terrainBuildConfig.rotation = XMFLOAT3(0.0f, 2.0f, 0.0f);
	//terrainBuildConfig.drawWorldAABB = false;
	//terrainBuildConfig.flip = AxisFlip::None;
	//terrainBuildConfig.loadAsync = false;
	//CreateTerrainGO(terrainBuildConfig);
}

GameObject<ModelInstance>* Ground::CreateTerrainGO(GameObjectConfig config)
{
	GameObject<ModelInstance>* terrainGO = new GameObject<ModelInstance>();
	terrainGO->Instantiate(config);
	m_persistentStaticGOs.push_back(terrainGO);

	RegisterModelLoadMap(config.modelPath, true, true);

	return terrainGO;
}

GameObject<ModelInstance>* Ground::CreateSceneGO(GameObjectConfig config, SceneID sceneID)
{
	GameObject<ModelInstance>* staticGO = new GameObject<ModelInstance>();
	staticGO->Instantiate(config);
	if (sceneID == SceneID::NONE)
		m_tempGOs.push_back(staticGO);
	else
		m_staticGOs.push_back(staticGO);

	RegisterModelLoadMap(config.modelPath, false, false);
	if (sceneID != SceneID::ALL)
		m_sceneEntityMap[TO_UINT64(sceneID)].push_back(staticGO);

	return staticGO;
}

void Ground::CreateInstanceEnvironmentObj(InstanceParams& params, float startPosX, float startPosZ,
	int startNumX, int endNumX, int startNumZ, int endNumZ,
	float scale, float rotY, float offSetX, float offsetY, float offsetZ, float jitter)
{

	float offsets = 0.0f;

	for (int x = startNumX; x < endNumX; x++)
	{
		for (int z = startNumZ; z < endNumZ; z++)
		{
			if (jitter > 0.0f)
			{
				offsets = GetRandFloat(-jitter, jitter);
			}

			params.transformArray.push_back(InstanceTransformInfo(
				startPosX - (x + offsets) * offSetX,		// posX
				startPosZ + (z + offsets) * offsetZ,		// posZ
				rotY,							// rotY
				scale,							// scl
				offsetY							// offsetY
			));
		}
	}
}

void Ground::GenerateEnvironmentObj(void*)
{
	// 環境オブジェクト管理クラスのインスタンス生成
	m_environment = new Environment();

	InstanceParams grassInstanceParams;
	grassInstanceParams.type = EnvironmentObjectType::Grass_1;

	float jitter = 0.3f;

	float grassPosX = 894.0f, grassPosZ = 3351.0f;
	CreateInstanceEnvironmentObj(grassInstanceParams, grassPosX, grassPosZ, 0, 3, 0, 7, 20.0f, 0.0f, 350.0f, 25.0f, 350.0f, jitter);

	grassPosX = 8270.0f, grassPosZ = -3600.0f;
	CreateInstanceEnvironmentObj(grassInstanceParams, grassPosX, grassPosZ, -5, 5, -5, 5, 20.0f, 0.0f, 350.0f, 25.0f, 350.0f, jitter);

	grassPosX = 11506.0f, grassPosZ = 1471.0f;
	CreateInstanceEnvironmentObj(grassInstanceParams, grassPosX, grassPosZ, -3, 5, -4, 3, 20.0f, 0.0f, 350.0f, 25.0f, 350.0f, jitter);

	grassPosX = 6817.0f, grassPosZ = -1219.0f;
	CreateInstanceEnvironmentObj(grassInstanceParams, grassPosX, grassPosZ, -6, 5, -5, 7, 20.0f, 0.0f, 350.0f, 25.0f, 350.0f, jitter);

	grassPosX = 4089.0f, grassPosZ = 8911.0f;
	CreateInstanceEnvironmentObj(grassInstanceParams, grassPosX, grassPosZ, -8, 8, -8, 8, 20.0f, 0.0f, 350.0f, 25.0f, 350.0f, jitter);

	grassPosX = -1458.0f, grassPosZ = -23485.0f;
	CreateInstanceEnvironmentObj(grassInstanceParams, grassPosX, grassPosZ, -4, 5, -5, 4, 20.0f, 0.0f, 350.0f, 25.0f, 350.0f, jitter);

	grassPosX = 11134.0f, grassPosZ = -24325.0f;
	CreateInstanceEnvironmentObj(grassInstanceParams, grassPosX, grassPosZ, -6, 3, -4, 5, 20.0f, 0.0f, 350.0f, 25.0f, 350.0f, jitter);

	const char* environmentModelPath = Environment::GetModelPathByType(grassInstanceParams.type);

	// モデルが既にロード完了している場合は、即座にインスタンス生成を実行
	if (Model::GetModel(environmentModelPath))
	{
		m_environment->GenerateInstanceByParams(
			grassInstanceParams,
			m_fieldGO->GetModel(),
			m_fieldGO->GetBoundingBoxWorld());
	}
	else // モデルがロードされていない場合は非同期でロードしてから実行
	{
		// 遅延用コンテキスト構築
		auto* ctx = new GenerateInstanceContext{
			m_environment,
			grassInstanceParams,
			m_fieldGO,
			environmentModelPath
		};
		DeferredTaskOptions options{};
		options.async = false; // 非同期実行
		options.debugName = "Ground_GenerateInstanceByParams"; // デバッグ用の名前

		RawDeferredTask::Add(
			ctx,
			[](void* ptr)
			{
				auto* ctx = static_cast<GenerateInstanceContext*>(ptr);
				GameObject<ModelInstance>* fieldGO = dynamic_cast<GameObject<ModelInstance>*>(ctx->fieldGO);
				if (!fieldGO) return;
				// 環境オブジェクトのインスタンスを生成
				ctx->environment->GenerateInstanceByParams(
					ctx->instanceParams,
					fieldGO->GetModel(),
					ctx->fieldGO->GetBoundingBoxWorld()
				);
			},
			[](void* ptr)
			{
				// モデルのロード要求を発行（非同期ロードを開始）
				auto* ctx = static_cast<GenerateInstanceContext*>(ptr);
				GameObject<ModelInstance>* fieldGO = dynamic_cast<GameObject<ModelInstance>*>(ctx->fieldGO);
				Model* instanceModel = Model::StoreModel(ctx->modelPath);
				return instanceModel != nullptr && fieldGO->GetCollider().isInitialized;
			},
			options
		);
	}

	//InstanceParams bushInstanceParams;
	//bushInstanceParams.type = EnvironmentObjectType::Bush_1;
	//float bushPosX = -82.0f, bushPosZ = 7591.0f;
	//CreateInstanceEnvironmentObj(bushInstanceParams, bushPosX, bushPosZ, 0, 1, 0, 1, 75.0f, 0.0f, 300.0f, 0.0f, 300.0f);

	//environmentModelPath = Environment::GetModelPathByType(bushInstanceParams.type);

	//// モデルが既にロード完了している場合は、即座にインスタンス生成を実行
	//if (Model::GetModel(environmentModelPath))
	//{
	//	m_environment->GenerateInstanceByParams(
	//		bushInstanceParams,
	//		m_fieldGO->GetModel(),
	//		m_fieldGO->GetBoundingBoxWorld());
	//}
	//else // モデルがロードされていない場合は非同期でロードしてから実行
	//{
	//	// 遅延用コンテキスト構築
	//	auto* ctx = new GenerateInstanceContext{
	//		m_environment,
	//		bushInstanceParams,
	//		m_fieldGO,
	//		environmentModelPath
	//	};
	//	DeferredTaskOptions options{};
	//	options.async = false; // 非同期実行
	//	options.debugName = "Ground_GenerateInstanceByParams"; // デバッグ用の名前

	//	RawDeferredTask::Add(
	//		ctx,
	//		[](void* ptr)
	//		{
	//			auto* ctx = static_cast<GenerateInstanceContext*>(ptr);
	//			GameObject<ModelInstance>* fieldGO = dynamic_cast<GameObject<ModelInstance>*>(ctx->fieldGO);
	//			if (!fieldGO) return;
	//			// 環境オブジェクトのインスタンスを生成
	//			ctx->environment->GenerateInstanceByParams(
	//				ctx->instanceParams,
	//				fieldGO->GetModel(),
	//				ctx->fieldGO->GetBoundingBoxWorld()
	//			);
	//		},
	//		[](void* ptr)
	//		{
	//			// モデルのロード要求を発行（非同期ロードを開始）
	//			auto* ctx = static_cast<GenerateInstanceContext*>(ptr);
	//			GameObject<ModelInstance>* fieldGO = dynamic_cast<GameObject<ModelInstance>*>(ctx->fieldGO);
	//			Model* instanceModel = Model::StoreModel(ctx->modelPath);
	//			return instanceModel != nullptr && fieldGO->GetCollider().isInitialized;
	//		},
	//		options
	//	);
	//}

	//InstanceParams bushInstanceParams2;
	//bushInstanceParams2.type = EnvironmentObjectType::Bush_2;

	//float bush2PosX = 4885.0f, bush2PosZ = 4100.0f;

	//for (int i = 0; i < 5; i++)
	//{
	//	bushInstanceParams2.transformArray.push_back(InstanceTransformInfo(
	//		bush2PosX,					// posX
	//		bush2PosZ - i * 800.0f,		// posZ
	//		XM_PI * 0.5f,				// rotY
	//		0.2f,						// scl
	//		5.0f						// offsetY
	//	));
	//}

	//environmentModelPath = Environment::GetModelPathByType(bushInstanceParams2.type);

	//// モデルが既にロード完了している場合は、即座にインスタンス生成を実行
	//if (Model::GetModel(environmentModelPath))
	//{
	//	m_environment->GenerateInstanceByParams(
	//		bushInstanceParams2,
	//		m_fieldGO->GetModel(),
	//		m_fieldGO->GetBoundingBoxWorld());
	//}
	//else // モデルがロードされていない場合は非同期でロードしてから実行
	//{
	//	// 遅延用コンテキスト構築
	//	auto* ctx = new GenerateInstanceContext{
	//		m_environment,
	//		bushInstanceParams2,
	//		m_fieldGO,
	//		environmentModelPath
	//	};
	//	DeferredTaskOptions options{};
	//	options.async = false; // 非同期実行
	//	options.debugName = "Ground_GenerateInstanceByParams"; // デバッグ用の名前

	//	RawDeferredTask::Add(
	//		ctx,
	//		[](void* ptr)
	//		{
	//			auto* ctx = static_cast<GenerateInstanceContext*>(ptr);
	//			GameObject<ModelInstance>* fieldGO = dynamic_cast<GameObject<ModelInstance>*>(ctx->fieldGO);
	//			if (!fieldGO) return;
	//			// 環境オブジェクトのインスタンスを生成
	//			ctx->environment->GenerateInstanceByParams(
	//				ctx->instanceParams,
	//				fieldGO->GetModel(),
	//				ctx->fieldGO->GetBoundingBoxWorld()
	//			);
	//		},
	//		[](void* ptr)
	//		{
	//			// モデルのロード要求を発行（非同期ロードを開始）
	//			auto* ctx = static_cast<GenerateInstanceContext*>(ptr);
	//			GameObject<ModelInstance>* fieldGO = dynamic_cast<GameObject<ModelInstance>*>(ctx->fieldGO);
	//			Model* instanceModel = Model::StoreModel(ctx->modelPath);
	//			return instanceModel != nullptr && fieldGO->GetCollider().isInitialized;
	//		},
	//		options
	//	);
	//}
}


void Ground::RegisterModelLoadMap(const char* modelPath, bool isPersistent, bool isTerrain)
{
	if (!m_modelLoadMap.contains(modelPath))
	{
		m_modelLoadMap[modelPath] = 0; // 初期値は0
		m_totalModelLoadCnt++; // 全体のロード数を更新
		if (isPersistent)
		{
			m_persistentModelLoadCnt++; // 永続的なモデルロード数も更新
			if (isTerrain)
				m_persistentTerrainModelLoadCnt++; // 永続的な地形モデルのロード数も更新
		}
		if (isTerrain)
			m_totalTerrainModelLoadCnt++; // 地形モデルのロード数も更新
	}
}

EventTrigger* Ground::RegisterEventTrigger(const BOUNDING_BOX& triggerBox, EventCallbackAction action, SceneID sceneID)
{
	EventTrigger* eventTriger = new EventTrigger();
	eventTriger->SetBoundingBox(triggerBox);

	// イベントの内容を設定
	if (action < EventCallbackAction::EnemyEventCallback)
	{
		// プレイヤー用
		auto* behavior = new PlayerEnterExitBehavior();
		behavior->SetAction(action);
		eventTriger->SetEventBehavior(behavior);
		eventTriger->Init();
	}
	else
	{
		// 敵用
		auto* behavior = new EnemyEnterExitBehavior();
		behavior->SetAction(action);
		eventTriger->SetEventBehavior(behavior);
		eventTriger->Init();
	}


	// 登録
	m_sceneEntityMap[TO_UINT64(sceneID)].push_back(eventTriger);
	CollisionManager::get_instance().RegisterEventCollider(eventTriger->GetCollider());

	return eventTriger;
}

void Ground::RenderImGui(void)
{
	if (ImGui::Checkbox("Draw Scene Objct Bounding Box", &m_drawBoundingBox))
	{
		UINT size = m_staticGOs.getSize();
		for (UINT i = 0; i < size; i++)
		{
			if (m_staticGOs[i]->GetUse() == false) continue;
			m_staticGOs[i]->GetModel()->SetDrawBoundingBox(m_drawBoundingBox);
		}

		size = m_persistentStaticGOs.getSize();
		for (UINT i = 0; i < size; i++)
		{
			if (m_persistentStaticGOs[i]->GetUse() == false) continue;
			m_persistentStaticGOs[i]->GetModel()->SetDrawBoundingBox(m_drawBoundingBox);
		}

		size = m_skinnedGOs.getSize();
		for (UINT i = 0; i < size; i++)
		{
			if (m_skinnedGOs[i]->GetUse() == false) continue;
			m_skinnedGOs[i]->GetSkinnedMeshModel()->SetDrawBoundingBox(m_drawBoundingBox);
		}

		size = m_persistentSkinnedGOs.getSize();
		for (UINT i = 0; i < size; i++)
		{
			if (m_persistentSkinnedGOs[i]->GetUse() == false) continue;
			m_persistentSkinnedGOs[i]->GetSkinnedMeshModel()->SetDrawBoundingBox(m_drawBoundingBox);
		}
	}

	if (m_environment)
		m_environment->SetDrawBoundingBox(m_drawBoundingBox);


	if (ImGui::CollapsingHeader("Debug GameObject"))
	{
		//ImGui::SliderFloat("PosX: ", &debugGOPosX, 10182.0f, 14182.0f, "%.1f");
		//ImGui::SliderFloat("PosY: ", &debugGOPosY, -981.0f, -2181.0f, "%.1f");
		//ImGui::SliderFloat("PosZ: ", &debugGOPosZ, -15485.0f, -21485.0f, "%.1f");
		//ImGui::SliderFloat("ScaleX: ", &debugGOScaleX, 5.0f, 399.0f, "%.1f");
		//ImGui::SliderFloat("ScaleY: ", &debugGOScaleY, 5.0f, 99.0f, "%.1f");
		//ImGui::SliderFloat("ScaleZ: ", &debugGOScaleZ, 5.0f, 399.0f, "%.1f");
		//ImGui::SliderFloat("RotY: ", &debugGORotY, -3, 3, "%.2f");


		ImGui::SliderFloat("PosX: ", &debugGOPosX, -5283.0f, 15283.0f, "%.1f");
		ImGui::SliderFloat("PosY: ", &debugGOPosY, -2821.0f, 2821.0f, "%.1f");
		ImGui::SliderFloat("PosZ: ", &debugGOPosZ, -18958.0f, 9958.0f, "%.1f");
		ImGui::SliderFloat("ScaleX: ", &debugGOScaleX, 5.0f, 299.0f, "%.1f");
		ImGui::SliderFloat("ScaleY: ", &debugGOScaleY, 5.0f, 299.0f, "%.1f");
		ImGui::SliderFloat("ScaleZ: ", &debugGOScaleZ, 5.0f, 299.0f, "%.1f");
		ImGui::SliderFloat("RotY: ", &debugGORotY, -3, 3, "%.2f");
	}

	if (ImGui::CollapsingHeader("Debug EventTrigger"))
	{
		ImGui::SliderFloat("MinX: ", &debugTriggerMinX, 1133.67f, 11111.0f, "%.1f");
		ImGui::SliderFloat("MinY: ", &debugTriggerMinY, 1133.67f, 11111.0f, "%.1f");
		ImGui::SliderFloat("MinZ: ", &debugTriggerMinZ, 1133.67f, 11111.0f, "%.1f");
		ImGui::SliderFloat("MaxX: ", &debugTriggerMaxX, 1133.67f, 11111.0f, "%.1f");
		ImGui::SliderFloat("MaxY: ", &debugTriggerMaxY, 1133.67f, 11111.0f, "%.1f");
		ImGui::SliderFloat("MaxZ: ", &debugTriggerMaxZ, 1133.67f, 11111.0f, "%.1f");
	}

	if (ImGui::CollapsingHeader("Debug Item"))
	{
		ImGui::SliderFloat("PosX: ", &debugItemPosX, 1133.67f, 11111.67f, "%.1f");
		ImGui::SliderFloat("PosY: ", &debugItemPosY, 1131.0f, 11111.0f, "%.1f");
		ImGui::SliderFloat("PosZ: ", &debugItemPosZ, 1418.0f, 11111.0f, "%.1f");
		ImGui::SliderFloat("ScaleX: ", &debugItemScaleX, 0.1f, 5.0f, "%.1f");
		ImGui::SliderFloat("ScaleY: ", &debugItemScaleY, 0.1f, 5.0f, "%.1f");
		ImGui::SliderFloat("ScaleZ: ", &debugItemScaleZ, 0.1f, 5.0f, "%.1f");
		ImGui::SliderFloat("RotX: ", &debugItemRotX, -3, 3, "%.1f");
		ImGui::SliderFloat("RotY: ", &debugItemRotX, -3, 3, "%.1f");
		ImGui::SliderFloat("RotZ: ", &debugItemRotZ, -3, 3, "%.1f");
	}
}