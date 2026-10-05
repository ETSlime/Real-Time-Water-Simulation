//=============================================================================
//
// GameObject処理 [GameObject.cpp]
// Author : 
//
//=============================================================================
#include "Scene/GameObject.h"

template <>
GameObject<SkinnedMeshModelInstance>::~GameObject()
{
	if (instance.pModel)
	{
		instance.load = false;
		SkinnedMeshModelPool* modelPool = SkinnedMeshModel::GetModel(instance.modelFullPath);
		if (modelPool)
		{
			modelPool->count--;
			if (modelPool->count == 0)
			{
				delete modelPool->pModel;
				Model::RemoveModel(instance.modelFullPath);
			}
		}
	}

	Scene::get_instance().UnregisterGameObject(this);
}


template <>
void GameObject<SkinnedMeshModelInstance>::Instantiate(const SkinnedGameObjectConfig& config)
{
	if (instance.load) return; // 既にロード済みなら何もしない

	if (!instance.modelFullPath)
	{
		char* modelFulllPath = new char[MODEL_NAME_LENGTH + MODEL_PATH_LENGTH] {};
		strcat_s(modelFulllPath, MODEL_NAME_LENGTH + MODEL_PATH_LENGTH, config.modelPath);

		size_t modelPathLength = strlen(config.modelPath);
		if (modelPathLength > 0 && config.modelPath[modelPathLength - 1] != '/' && config.modelPath[modelPathLength - 1] != '\\')
			strcat_s(modelFulllPath, MODEL_NAME_LENGTH + MODEL_PATH_LENGTH, "/");

		strcat_s(modelFulllPath, MODEL_NAME_LENGTH + MODEL_PATH_LENGTH, config.modelName);
		instance.modelFullPath = modelFulllPath;

		instance.modelPath = const_cast<char*>(config.modelPath);
		instance.modelName = const_cast<char*>(config.modelName);
	}

	if (!instance.pModel)
		instance.pModel = SkinnedMeshModel::StoreModel(config.modelPath, config.modelName,
			instance.modelFullPath, config.modelType, config.animClipName);


	// モデルのビルドタスクを登録
	if (!instance.modelBuildTaskRegistered)
	{
		m_skinnedConfig = config; // コンフィグを保存
		SetCollisionType(m_skinnedConfig.collisionType);
		SetScale(m_skinnedConfig.scale);
		SetPosition(m_skinnedConfig.position);
		SetRotation(m_skinnedConfig.rotation);
		SetCastShadow(m_skinnedConfig.castShadow);
		SetDrawWorldAABB(m_skinnedConfig.drawWorldAABB);

		instance.modelBuildTaskRegistered = true;

		DeferredTaskOptions options{};
		options.async = false; // postInit は同期で実行
		options.debugName = "GameObject_InstantiateSkinnedGameObject_PostInit"; // デバッグ用の名前
		auto* ctx = new InstantiateTaskContext<SkinnedMeshModelInstance>{ this }; // コンテキストを作成して渡す

		RawDeferredTask::Add(
			ctx, // コンテキストを渡す
			[](void* ptr) // タスクの実行関数
			{
				// コンテキストを取得
				auto* ctx = static_cast<InstantiateTaskContext<SkinnedMeshModelInstance>*>(ptr);
				SkinnedMeshModel* model = ctx->owner->instance.pModel;
				model->SetOverallDiffuseTexture();
				ctx->owner->PostInit();
			},
			[](void* ptr) // タスクの条件関数
			{
				// コンテキストを取得
				auto* ctx = static_cast<InstantiateTaskContext<SkinnedMeshModelInstance>*>(ptr);
				// モデルがロードされているかチェック
				return ctx->owner->DeferredCanRunModelReady();
			},
			options
		);

		// 非同期で実行するかどうかを決定
		if (config.loadAsync)
			options.async = instance.collisionType == ObjectCollisionType::MESH_BOUNDING_BOX ? true : false;
		else
			options.async = false;
		options.debugName = "GameObject_InstantiateSkinnedGameObject_Collision";
		options.callbackAsync = false; // コールバックは同期で実行
		options.onComplete = [](void* ptr) // タスク完了後のコールバック関数
			{
				// コンテキストを取得
				auto* ctx = static_cast<InstantiateTaskContext<SkinnedMeshModelInstance>*>(ptr);
				ctx->owner->instance.collisionBuilt = true; // 当たり判定のビルドが完了
			};
		options.callbackParam = ctx;

		if (instance.collisionType == ObjectCollisionType::MESH_BOUNDING_BOX)
		{
			RawDeferredTask::Add(
				ctx, // コンテキストを渡す
				[](void* ptr) // タスクの実行関数
				{
					// コンテキストを取得
					auto* ctx = static_cast<InstantiateTaskContext<SkinnedMeshModelInstance>*>(ptr);
					const XMMATRIX& worldMatrix = ctx->owner->GetWorldMatrix();
					SkinnedMeshModel* model = ctx->owner->instance.pModel;
					// モデルの三角形をビルド
					model->Deferred_BuildMeshTriangles(worldMatrix, true, ctx->owner->instance.drawWorldAABB);
					// モデルのボクセルグリッドをビルド
					model->Deferred_BuildMeshVoxelGrid(worldMatrix, true);
				},
				[](void* ptr) // タスクの条件関数
				{
					// コンテキストを取得
					auto* ctx = static_cast<InstantiateTaskContext<SkinnedMeshModelInstance>*>(ptr);
					// モデルがロードされているかチェック
					return ctx->owner->GetCollider().isInitialized;
				},
				options
			);
		}
		else if (instance.collisionType == ObjectCollisionType::OBJECT_BOUNDING_BOX)
		{
			RawDeferredTask::Add(
				ctx, // コンテキストを渡す
				[](void* ptr) // タスクの実行関数
				{
					// コンテキストを取得
					auto* ctx = static_cast<InstantiateTaskContext<SkinnedMeshModelInstance>*>(ptr);
					const BOUNDING_BOX& boundingBox = ctx->owner->GetCollider().aabb;
					SkinnedMeshModel* model = ctx->owner->instance.pModel;
					// モデルの三角形をビルド
					model->Deferred_BuildBoundingBoxTriangles(boundingBox, ctx->owner->instance.drawWorldAABB);
					// モデルのボクセルグリッドをビルド
					model->Deferred_BuildBoundingBoxVoxelGrid(boundingBox);
				},
				[](void* ptr) // タスクの条件関数
				{
					// コンテキストを取得
					auto* ctx = static_cast<InstantiateTaskContext<SkinnedMeshModelInstance>*>(ptr);
					// モデルがロードされているかチェック
					return ctx->owner->GetCollider().isInitialized;
				},
				options
			);
		}
		else if (instance.collisionType == ObjectCollisionType::COLLIDER_BOUNDING_BOX)
		{
			instance.collider.tag = m_skinnedConfig.colliderTag;
			instance.collider.owner = this;
			if (m_skinnedConfig.colliderTag != ColliderTag::PLAYER_ATTACK &&
				m_skinnedConfig.colliderTag != ColliderTag::ENEMY_ATTACK)
				instance.collider.enable = true;
			CollisionManager::get_instance().RegisterDynamicCollider(&instance.collider);
			instance.collisionBuilt = true; // 当たり判定のビルドが完了
		}
	}

	instance.use = true; // モデルを使用するフラグを立てる

	if (instance.pModel && instance.postInit && instance.pModel->AreAllTexturesLoaded())
	{
		instance.load = true; // モデルがロードされたらロードフラグを立てる
		Scene::get_instance().RegisterGameObject(this); // renderableObjectsに登録
	}
	else
	{
		instance.load = false; // モデルがロードできなかった場合はロードフラグを下げる
	}
}

template<>
void GameObject<SkinnedMeshModelInstance>::Update()
{
	if (!instance.load)
	{
		Instantiate(m_skinnedConfig);
		if (!instance.load) return; // モデルがロードされていない場合は何もしない
	}

	// ワールド行列の構築
	XMMATRIX mtxWorld = XMMatrixIdentity();
	mtxWorld = XMMatrixMultiply(mtxWorld, XMMatrixScaling(instance.transform.scl.x, instance.transform.scl.y, instance.transform.scl.z)); // スケーリング
	mtxWorld = XMMatrixMultiply(mtxWorld, XMMatrixRotationRollPitchYaw(instance.transform.rot.x, instance.transform.rot.y + XM_PI, instance.transform.rot.z)); // 回転
	mtxWorld = XMMatrixMultiply(mtxWorld, XMMatrixTranslation(instance.transform.pos.x, instance.transform.pos.y, instance.transform.pos.z)); // 平行移動
	instance.transform.mtxWorld = mtxWorld;

	// ボーン変形行列（スキニングの最終変換）
	XMMATRIX boneTransform = XMMatrixIdentity(); // ※実装に応じてルートボーンまたは全体ボーン補正

	// ローカル空間AABBの取得
	XMFLOAT3 localMin = instance.pModel->GetBoundingBox().minPoint;
	XMFLOAT3 localMax = instance.pModel->GetBoundingBox().maxPoint;

	// 8つのコーナー頂点を作成（ローカル空間）
	XMVECTOR localCorners[8] = {
		XMVectorSet(localMin.x, localMin.y, localMin.z, 1.0f),
		XMVectorSet(localMax.x, localMin.y, localMin.z, 1.0f),
		XMVectorSet(localMin.x, localMax.y, localMin.z, 1.0f),
		XMVectorSet(localMax.x, localMax.y, localMin.z, 1.0f),
		XMVectorSet(localMin.x, localMin.y, localMax.z, 1.0f),
		XMVectorSet(localMax.x, localMin.y, localMax.z, 1.0f),
		XMVectorSet(localMin.x, localMax.y, localMax.z, 1.0f),
		XMVectorSet(localMax.x, localMax.y, localMax.z, 1.0f),
	};

	// ワールド空間AABBの初期化
	XMVECTOR worldMin = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
	XMVECTOR worldMax = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);

	// 全ての角をスキン変換→ワールド変換→AABB更新
	for (int i = 0; i < 8; ++i)
	{
		XMVECTOR skinnedPt = XMVector3Transform(localCorners[i], boneTransform);    // ボーン変換
		XMVECTOR worldPt = XMVector3Transform(skinnedPt, mtxWorld);              // ワールド変換

		worldMin = XMVectorMin(worldMin, worldPt);
		worldMax = XMVectorMax(worldMax, worldPt);
	}

	// AABBをコライダーに格納（ワールド空間）
	instance.collider.UpdateBoundingBox(worldMin, worldMax);
}