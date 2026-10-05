#pragma once
//=============================================================================
//
// ゲームオブジェクト共通基底クラス群 [GameObject.h]
// Author : 
// モデル・当たり判定・Transform・描画・アニメーション制御などをまとめて管理する基底クラスちゃんですっ！
// キャラの共通インターフェース（ISkinnedMeshModelChar）もここにいて、アニメーションのきっかけを提供するの
// 
//=============================================================================
#include "Core/Timer.h"
#include "Core/Camera.h"
#include "Core/Graphics/Renderer.h"
#include "Core/Graphics/ShadowMeshCollector.h"
#include "Core/Async/DeferredTaskAPI.h"
#include "Scene/Scene.h"
#include "Model/Model.h"
#include "Model/SkinnedMeshModel.h"
#include "Collision/CollisionManager.h"
#include "Utility/Debug/DebugBoundingBoxRenderer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
enum class ActionEnum
{
	NONE,
	ATTACK,
	AIM,
};

enum class ModelType
{
	Default,
	Static,
	SkinnedMesh,
	Instanced,
};

enum class ObjectCollisionType
{
	NONE,
	COLLIDER_BOUNDING_BOX,
	MESH_BOUNDING_BOX,
	OBJECT_BOUNDING_BOX,
};

#define DRAW_BOUNDING_BOX       1
#define MAX_MODEL_TYPE			32

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct Transform
{
	XMMATRIX			mtxWorld;			// ワールドマトリックス
	XMFLOAT3			pos;				// モデルの位置
	XMFLOAT3			rot;				// モデルの向き(回転)
	XMFLOAT3			scl;				// モデルの大きさ(スケール)

	Transform()
	{
		pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
		rot = XMFLOAT3(0.0f, 0.0f, 0.0f);
		scl = XMFLOAT3(1.0f, 1.0f, 1.0f);
		mtxWorld = XMMatrixIdentity();
	}

	// オブジェクトの前方向ベクトルを取得する関数
	XMFLOAT3 GetForward(void)
	{
		// Yaw (水平回転) を利用してXZ平面上の方向を求める
		float yaw = rot.y;

		// 前方向ベクトルを計算（右手座標系のZ前提）
		XMFLOAT3 forward;
		forward.x = sinf(yaw);
		forward.y = 0.0f;
		forward.z = cosf(yaw);

		// 正規化（必要なら）
		XMVECTOR f = XMVector3Normalize(XMLoadFloat3(&forward));
		XMStoreFloat3(&forward, f);

		return forward;
	}

	XMFLOAT3 GetRight(void)
	{
		// Forwardから右方向を計算
		XMFLOAT3 forward = GetForward();
		XMVECTOR f = XMLoadFloat3(&forward);
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);

		XMVECTOR r = XMVector3Normalize(XMVector3Cross(up, f));

		XMFLOAT3 right;
		XMStoreFloat3(&right, r);
		return right;
	}
};

struct Attributes
{
	XMFLOAT4X4		mtxWorld = XMFLOAT4X4();	// ワールドマトリックス

	float			spd = 0.0f;		// 移動スピード
	float			valueMove = 0.0f; // 移動量
	float			dir = 0.0f;		// 向き
	float			targetDir = 0.0f;
	bool			use = true;

	bool			stopRun = true;
	bool			isMoving = false;
	bool			isRotating = false;
	bool			isRunning = false;
	bool			isAttacking = false;
	bool			isAttacking2 = false;
	bool			isAttacking3 = false;
	bool			isDashing = false;
	bool			isJumping = false;
	bool			isGrounded = true;
	bool			isMoveBlocked = false;
	bool			isHit1 = false;
	bool			isHit2 = false;
	float			hitTimer = 0;
	float			glideDuration = 0.0f; // グライドの持続時間
	float			lastFireTimeCountDown = 0.0f; // 最後に弾を撃った時間（カウントダウン用）
	float			fallSpeed = 0.0f; // 落下速度
	bool			charSwitchEffect = false;

	bool			attackWindow2 = false;
	bool			attackWindow3 = false;
	float			attackWinwdowCnt = 0;
	bool			hasWeapon = false;
	bool			hasSeed = false;
	bool			hasGun = false;

	void Initialize(void)
	{
		*this = {};
	}
};

struct InstanceModelAttribute
{
	bool			initialized = false;
	bool			isInstanced = false;
	bool			isCollision = false;
	UINT			instanceCount = 0;
	InstanceData*	instanceData = nullptr;
	ID3D11Buffer*	instanceBuffer = nullptr;
	ID3D11Buffer*	vertexBuffer = nullptr;
	ID3D11Buffer*	indexBuffer = nullptr;
	SimpleArray<InstanceData>* visibleInstanceDataArray = nullptr;
	SimpleArray<bool>* visibleInstanceDraw = nullptr;
	SimpleArray<Collider>* colliderArray = nullptr;
};

using PostInitCallback = void (*)(void*);

struct ModelInstance
{
	char*				modelPath = nullptr;
	char*				modelName = nullptr;
	bool				use = false;
	bool				load = false;
	bool				markedForDelete = false;
	bool				postInit = false;
	bool				collisionBuilt = false;	// 当たり判定のビルドが完了したかどうか
	bool				castShadow = true;
	bool				enableAlphaTest = false;

	PostInitCallback	onPostInit = nullptr; // モデルの読み込み完了後に呼び出される初期化処理関数
	void*				userData = nullptr;

	Model*				pModel = nullptr;				// モデル情報
	bool				modelBuildTaskRegistered = false;
	XMFLOAT4			diffuse[MODEL_MAX_MATERIAL]{};	// モデルの色
	Transform			transform;
	ObjectCollisionType collisionType = ObjectCollisionType::NONE;
	ColliderTag			colliderTag = ColliderTag::DEFAULT;
	Attributes			attributes;
	Collider			collider;

	BOOL				isSelected = false;
	BOOL				isCursorIn = false;
	int					editorIdx = -1;

	bool					drawWorldAABB = false;
	ModelType				modelType = ModelType::Static;
	RenderProgressCBuffer	renderProgress;

	// インスタンス化されたモデルかどうか
	InstanceModelAttribute instanceAttribute;

};

struct SkinnedMeshModelInstance
{
	char*				modelPath = nullptr;
	char*				modelName = nullptr;
	char*				modelFullPath = nullptr;
	bool				use = false;
	bool				load = false;
	bool				markedForDelete = false;
	bool				postInit = false;
	bool				collisionBuilt = false;	// 当たり判定のビルドが完了したかどうか
	bool				castShadow = true;
	bool				enableAlphaTest = false;

	PostInitCallback	onPostInit = nullptr; // モデルの読み込み完了後に呼び出される初期化処理関数
	void*				userData = nullptr;

	SkinnedMeshModel*	pModel = nullptr;
	bool				modelBuildTaskRegistered = false;
	Transform			transform;
	ObjectCollisionType collisionType = ObjectCollisionType::NONE;
	ColliderTag			colliderTag = ColliderTag::DEFAULT;
	Attributes			attributes;
	Collider			collider;

	BOOL			isSelected = false;
	BOOL			isCursorIn = false;
	int				editorIdx = -1;

	bool			drawWorldAABB = false;
	ModelType		modelType = ModelType::SkinnedMesh;
	RenderProgressCBuffer renderProgress;
};

struct GameObjectConfig 
{
	const char* modelPath = nullptr;
	XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
	XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
	bool castShadow = true;
	bool drawWorldAABB = false;
	ObjectCollisionType collisionType = ObjectCollisionType::NONE;
	ColliderTag			colliderTag = ColliderTag::DEFAULT;
	bool enableCollider = true;
	ModelType modelType = ModelType::Static;
	bool loadAsync = true;
	AxisFlip flip = AxisFlip::None;
	bool trackTriangles = false; // メッシュの頂点情報を追跡するかどうか
};

struct SkinnedGameObjectConfig
{
	const char* modelPath = nullptr;
	const char* modelName = nullptr;
	XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
	XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
	bool castShadow = true;
	bool drawWorldAABB = false;
	ObjectCollisionType collisionType = ObjectCollisionType::NONE;
	ColliderTag			colliderTag = ColliderTag::DEFAULT;
	SkinnedModelType modelType = SkinnedModelType::Default;
	AnimClipName animClipName = AnimClipName::ANIM_NONE;
	bool loadAsync = true;
};


// 非テンプレート基底クラス
class IGameObject : public ISceneEntity
{
public:
	virtual ~IGameObject(void) = default;

	virtual bool GetUse(void) const = 0;
	virtual bool GetLoad(void) const = 0;
	virtual bool GetCastShadow(void) const = 0;
	virtual BOUNDING_BOX GetBoundingBoxWorld(void) const = 0;
	virtual const XMMATRIX& GetWorldMatrix(void) const = 0;
	virtual bool IsMarkedForDelete(void) const = 0;
	//virtual void OnCollisionEnter(const Collider& collider) = 0;
	//virtual void OnCollisionEnter(const Collider& collider) = 0;

	virtual void CollectShadowMesh(ShadowMeshCollector& collector) const = 0;
	virtual ModelType GetModelType(void) const = 0;
	virtual bool IsGameObject(void) const override { return true; }
};

template<typename TModel>
class GameObject : public IGameObject, public IDebugUI
{

public:
	GameObject();
	~GameObject();
	void Instantiate(const GameObjectConfig& config);
	void Instantiate(const SkinnedGameObjectConfig& config);
	virtual void Destroy() override // GameObject の破棄処理
	{ 
		instance.markedForDelete = true; 
		CollisionManager::get_instance().UnregisterDynamicCollider(&instance.collider);

		if (instance.pModel->GetTrackTriangles()
			&&  (instance.collisionType == ObjectCollisionType::OBJECT_BOUNDING_BOX 
				|| instance.collisionType == ObjectCollisionType::MESH_BOUNDING_BOX))
		{
			// メッシュの頂点情報を追跡している場合は、当たり判定システムから三角形情報を削除する
			auto triangles = instance.pModel->GetTriangles();
			for (UINT i = 0; i < triangles->getSize(); i++)
			{
				CollisionManager::get_instance().RemoveTriangleFromStaticOctree((*triangles)[i]); // 静的オクツリーから削除
				CollisionManager::get_instance().RemoveTriangleFromVoxelGrid((*triangles)[i]); // ボクセルグリッドから削除
			}
		}

	}
	virtual bool IsMarkedForDelete() const override { return instance.markedForDelete; }
	void SetPostInitCallback(PostInitCallback cb, void* userData) { instance.onPostInit = cb; instance.userData = userData;}

	// モデルの読み込みが完了した後に呼び出される初期化処理関数
	virtual void PostInit()
	{ 
		if (instance.onPostInit)
			instance.onPostInit(instance.userData);
		else
			instance.postInit = true; 
	}
	void SetPostInit(bool postInit) { instance.postInit = postInit; }

	virtual void Update();
	virtual void Draw();
	virtual void DrawEffect() {}
	virtual void RenderDebugInfo(void) override;
	void DrawModelEditor();
	void UpdateModelEditor();

	void CollectShadowMesh(ShadowMeshCollector& collector) const override
	{
		collector.Collect(instance);
	}

	inline const TModel* GetInstance() const { return &instance; }
	inline const TModel& GetInstanceRef() const { return instance; }
	inline InstanceModelAttribute& GetInstancedAttribute() { return instance.instanceAttribute; }
	inline Model* GetModel() { return instance.pModel; }
	inline SkinnedMeshModel* GetSkinnedMeshModel() { return instance.pModel; }
	inline const SkinnedMeshModel* GetSkinnedMeshModelConst() const { return instance.pModel; }
	inline void SetModelType(ModelType type) { instance.modelType = type; }
	inline ModelType GetModelType() const override { return instance.modelType; }
	inline void SetDrawWorldAABB(bool draw) { instance.drawWorldAABB = draw; }
	inline void SetPosition(XMFLOAT3 pos) { instance.transform.pos = pos; }
	inline void SetRotation(XMFLOAT3 rot) { instance.transform.rot = rot; }
	inline void SetScale(XMFLOAT3 scl) { instance.transform.scl = scl; }
	inline void SetTransform(Transform transform) { instance.transform = transform; }
	inline void SetWorldMatrix(XMMATRIX mtxWorld) { instance.transform.mtxWorld = mtxWorld; }
	inline Transform GetTransform() { return instance.transform; }
	inline const Transform* GetTransformP() { return &instance.transform; } // bind
	inline const XMMATRIX& GetWorldMatrix() const override { return instance.transform.mtxWorld; }


	inline XMFLOAT4* GetDiffuse() { return instance.diffuse; }
	inline bool GetUse() const override { return instance.use; }
	inline bool GetLoad() const override { return instance.load; }
	inline void SetUse(bool use) { instance.use = use; }
	inline bool GetCastShadow() const override { return instance.castShadow; }
	inline void SetCastShadow(bool shadow) { instance.castShadow = shadow; }
	inline void GetEnableAlphaTest(void) { return instance.enableAlphaTest; }
	inline void SetEnableAlphaTest(bool alpha) { instance.enableAlphaTest = alpha; }
	inline void SetCollisionType(ObjectCollisionType type) { instance.collisionType = type; }
	inline ObjectCollisionType GetCollisionType(void) { return instance.collisionType; }
	// 当たり判定のビルドが完了しているかどうか
	inline bool GetCollisionBuilt() { return instance.collisionType == ObjectCollisionType::NONE ? true : instance.collisionBuilt; }

	// インスタンス化されたモデルかどうか
	inline void SetIsInstanced(bool instanced) { instance.instanceAttribute.isInstanced = instanced; }
	inline bool GetIsInstanced() { return instance.instanceAttribute.isInstanced; }
	inline void SetInstanceCount(UINT count) { instance.instanceAttribute.instanceCount = count; }
	inline UINT GetInstanceCount() { return instance.instanceAttribute.instanceCount; }
	inline void SetInstanceBuffer(ID3D11Buffer* buffer) { instance.instanceAttribute.instanceBuffer = buffer; }
	inline ID3D11Buffer* GetInstanceBuffer() { return instance.instanceAttribute.instanceBuffer; }
	inline void SetVertexBuffer(ID3D11Buffer* buffer) { instance.instanceAttribute.vertexBuffer = buffer; }
	inline ID3D11Buffer* GetVertexBuffer() { return instance.instanceAttribute.vertexBuffer; }
	inline void SetIndexBuffer(ID3D11Buffer* buffer) { instance.instanceAttribute.indexBuffer = buffer; }
	inline ID3D11Buffer* GetIndexBuffer() { return instance.instanceAttribute.indexBuffer; }
	inline void SetInstanceData(InstanceData* data) { instance.instanceAttribute.instanceData = data; }
	inline const InstanceData* GetInstanceData() { return instance.instanceAttribute.instanceData; }
	inline SimpleArray<Collider>* GetInstancedColliderArray() { return instance.instanceAttribute.colliderArray; }
	inline void SetInstanceCollision(bool collision) { instance.instanceAttribute.isCollision = collision; }
	inline void InitializeInstancedArray() 
	{ 
		auto& ptr = instance.instanceAttribute.visibleInstanceDataArray;
		const UINT totalCount = instance.instanceAttribute.instanceCount;

		if (ptr == nullptr)
		{
			ptr = new SimpleArray<InstanceData>(totalCount);
		}
		else
		{
			ptr->clear();                     // 前のフレームのデータをリセット
			ptr->reserve(totalCount);         // 必要に応じて拡張
		}

		instance.instanceAttribute.visibleInstanceDraw = new SimpleArray<bool>(totalCount, false);
		instance.instanceAttribute.colliderArray = new SimpleArray<Collider>(totalCount, Collider());

		if (instance.instanceAttribute.isCollision)
		{
			for (UINT i = 0; i < totalCount; i++)
			{
				Collider* collider = &(*instance.instanceAttribute.colliderArray)[i];
				collider->tag = ColliderTag::STATIC_OBJECT;
				collider->owner = this;
				collider->enable = true;
				CollisionManager::get_instance().RegisterDynamicCollider(collider);
			}
		}

		instance.instanceAttribute.initialized = true;
	}

	// モデルがロードされているかチェック
	inline bool DeferredCanRunModelReady() { return instance.pModel; }

	inline BOOL GetIsModelSelected() { return instance.isSelected; }
	inline BOOL GetIsCursorIn() { return instance.isCursorIn; }
	inline void SetIsCursorIn(BOOL cursorIn) { instance.isCursorIn = cursorIn; }
	inline int GetEditorIndex() { return instance.editorIdx; }
	inline void SetEditorIndex(int idx) { instance.editorIdx = idx; }
	inline const Attributes& GetAttributesConst() { return instance.attributes; }
	inline Attributes* GetAttributes() { return &instance.attributes; }
	inline void UpdateAttributes(Attributes attributes) { instance.attributes = attributes; }

	// 物理演算用の当たり判定
	inline void SetColliderTag(ColliderTag tag) { instance.collider.tag = tag; }
	inline void SetColliderOwner(void* owner) { instance.collider.owner = owner; }
	inline void SetColliderEnable(bool enable) { instance.collider.enable = enable; }
	inline void SetStaticCollisionEnable(bool enable) { instance.collider.enableStaticCollision = enable; }
	inline void SetDynamicCollisionEnable(bool enable) { instance.collider.enableDynamicCollision = enable; }
	inline void SetEventTriggerCollisionEnable(bool enable) { instance.collider.enableEventTriggerCollision = enable; }
	inline void SetColliderBoundingBox(BOUNDING_BOX boundingBox) { instance.collider.aabb = boundingBox; }
	inline const Collider& GetCollider(void) const { return instance.collider; }
	inline BOUNDING_BOX GetBoundingBoxWorld(void) const override { return instance.collider.aabb; }

	inline void SetGrounded(bool grounded) 
	{ 
		instance.attributes.isGrounded = grounded; 
		if (grounded)
		{
			instance.attributes.glideDuration = 0.0f; // 地面に着地したらグライドの持続時間をリセット
			instance.attributes.fallSpeed = 0.0f; // 落下速度をリセット
		}

	}

	inline void SetMoveBlock(bool blocked) { instance.attributes.isMoveBlocked = blocked; }
	inline void SetIsHit(bool isHit) { instance.attributes.isHit1 = isHit; }
	inline bool GetIsHit(void) { return instance.attributes.isHit1; }
	inline void SetIsHit2(bool isHit) { instance.attributes.isHit2 = isHit; }
	inline bool GetIsHit2(void) { return instance.attributes.isHit2; }
	inline int	GetHitTimer(void) { return instance.attributes.hitTimer; }
	inline void SetHitTimer(float hitTimer) { instance.attributes.hitTimer = hitTimer; }
	inline void EnableWeapon(bool enable) { instance.attributes.hasWeapon = enable; }
	inline void SetRenderProgress(float progress) { instance.renderProgress.progress = progress; instance.renderProgress.isRandomFade = true; }
	inline float GetRenderProgress() { return instance.renderProgress.progress; }
	inline void EnableSeed(bool enable) { instance.attributes.hasSeed = enable; }
	inline long GetID() { return m_id; }
	//inline void SetRenderProgress(float progress) { instance.attributes.renderProgress.progress = progress; }
	//inline void SetSwitchCharEffect(bool effectON) { instance.attributes.charSwitchEffect = effectOn; }

	inline void AddCollisionCallback(CollisionCallback callback, void* context, CollisionCallbackID callbackID)
	{
		instance.collider.AddCollisionCallback(callback, context, callbackID);
	}

	virtual AnimStateMachine* GetStateMachine() { return nullptr; }

	void SetBoundingboxSize(XMFLOAT3 size)
	{
		UINT numBones = instance.pModel.GetNumBones();
		for (int i = 0; i < numBones; i++)
		{
			instance.pModel.SetBoundingBoxSize(size, i);
		}
	}

	Renderer& renderer = Renderer::get_instance();
	Timer& m_timer = Timer::get_instance();

protected:
	TModel instance;
	GameObjectConfig m_config;
	SkinnedGameObjectConfig m_skinnedConfig;
	DebugBoundingBoxRenderer m_debugBoundingBoxRenderer;
	long m_id = 0;
};

template<typename T>
struct InstantiateTaskContext
{
	GameObject<T>* owner;
	GameObjectConfig* config;
	SkinnedGameObjectConfig* skinnedConfig;
};

class ISkinnedMeshModelChar
{
public:
	virtual void InitializeAnimation() = 0;
	virtual void InitAnimInfo() {}

	virtual bool CanWalk() const = 0;
	virtual bool CanStopMoving() const = 0;
	virtual bool CanStopRunning() const { return false; }
	virtual bool CanAttack() const { return false; }
	virtual bool CanAttack2() const  { return false; }
	virtual bool CanStopAttacking2() const { return false; }
	virtual bool CanAttack3() const  { return false; }
	virtual bool CanStopAttacking3() const { return false; }
	virtual bool CanRun()	const { return false; }
	virtual bool CanHit()	const { return false; }
	virtual bool CanHit2()	const { return false; }
	virtual bool CanJump()	const { return false; }
	virtual bool CanSurprised() const { return false; }
	virtual bool CanDie()	const { return false; }
	virtual bool CanAim() const { return false; }
	virtual bool CanWalkAim() const { return false; }
	virtual bool CanWalkAimToWalk() const { return false; }
	virtual bool CanRunAim() const { return false; }
	virtual bool CanRunAimToRun() const { return false; }
	virtual bool CanStopAiming() const { return false; }
	virtual bool CanJumpAim() const { return false; }
	virtual bool CanRunJumpAim() const { return false; }
	virtual bool CanThrow() const { return false; }
	virtual bool CanStopThrowing() const { return false; }
	virtual bool CanClimb() const { return false; }
	virtual bool CanGlide() const { return false; }
	virtual bool CanStopGliding() const { return false; }
	virtual bool CanFallDown() const { return false; }
	virtual bool CanCastIce() const { return false; }
	virtual bool CanDash() const { return false; }
	virtual bool CanBackStep() const { return false; }
	virtual bool CanLaser() const { return false; }
	virtual void OnAttackAnimationEnd() {};
	virtual void OnHitAnimationEnd() {};
	virtual void OnDashAnimationEnd() {}
	virtual void OnJumpAnimationEnd() {}
	virtual void OnDieAnimationEnd() {}
	virtual void OnSurprisedEnd() {}
	virtual void OnPlantSeedAnimationEnd() {}
	virtual void OnClimbAnimationEnd() {}
	virtual void OnThrowAnimationEnd() {}
	virtual void OnBossfightReady() {}
	virtual void OnGetUpAnimationEnd() {}
	virtual void OnBossStandUp() {}
	virtual bool ExecuteAction(ActionEnum action) { return true; }
	bool AlwaysTrue() const { return true; }
	virtual void AddAnimation(char* animPath, char* animName, AnimClipName clipName, AnimPlayMode animPlayMode = AnimPlayMode::LOOP) = 0;
};


template <typename T>
GameObject<T>::GameObject()
{
#ifdef _DEBUG
	DebugProc::get_instance().Register(this);
#endif // DEBUG

#if DRAW_BOUNDING_BOX
	m_debugBoundingBoxRenderer.Initialize();
#endif
}

template <typename T>
GameObject<T>::~GameObject()
{
#ifdef _DEBUG
	DebugProc::get_instance().Unregister(this);
#endif // DEBUG

#if DRAW_BOUNDING_BOX
	m_debugBoundingBoxRenderer.Shutdown();
#endif

	if (instance.pModel)
	{
		instance.load = false;
		MODEL_POOL* modelPool = Model::GetModel(instance.modelPath);
		if (modelPool)
		{
			modelPool->count--;
			if (modelPool->count == 0)
			{
				SAFE_DELETE(modelPool->pModel);
				Model::RemoveModel(instance.modelPath);
			}
		}
	}
}

template <typename T>
void GameObject<T>::Instantiate(const GameObjectConfig& config)
{
	if (instance.load) return; // 既にロード済みなら何もしない

	// モデルパスが空なら設定
	if (!instance.modelPath)
	{
		instance.modelPath = const_cast<char*>(config.modelPath);
		instance.modelName = const_cast<char*>(config.modelPath);
	}

	if (!instance.pModel)
		instance.pModel = Model::StoreModel(config.modelPath, config.flip);

	// モデルのビルドタスクを登録
	if (!instance.modelBuildTaskRegistered)
	{
		m_config = config; // コンフィグを保存
		SetCollisionType(m_config.collisionType);
		SetScale(m_config.scale);
		SetPosition(m_config.position);
		SetRotation(m_config.rotation);
		SetCastShadow(m_config.castShadow);
		SetDrawWorldAABB(m_config.drawWorldAABB);
		SetModelType(m_config.modelType);

		DeferredTaskOptions options{};
		options.async = false; // postInit は同期で実行
		options.debugName = "GameObject_InstantiateGameObject_PostInit"; // デバッグ用の名前
		auto* ctx = new InstantiateTaskContext<T>{ this, &m_config }; // コンテキストを作成して渡す

		if (instance.modelType == ModelType::Instanced)
			PostInit();
		else
		{
			RawDeferredTask::Add(
				ctx, // コンテキストを渡す
				[](void* ptr) // タスクの実行関数
				{
					// コンテキストを取得
					auto* ctx = static_cast<InstantiateTaskContext<T>*>(ptr);
					ctx->owner->PostInit();
				},
				[](void* ptr) // タスクの条件関数
				{
					// コンテキストを取得
					auto* ctx = static_cast<InstantiateTaskContext<T>*>(ptr);
					// モデルがロードされているかチェック
					return ctx->owner->DeferredCanRunModelReady();
				},
				options
			);
		}

		// 非同期で実行するかどうかを決定
		if (config.loadAsync)
			options.async = instance.collisionType == ObjectCollisionType::MESH_BOUNDING_BOX ? true : false;
		else
			options.async = false;
		options.debugName = "GameObject_InstantiateGameObject_Collision";
		options.callbackAsync = false; // コールバックは同期で実行
		options.onComplete = [](void* ptr) // タスク完了後のコールバック関数
			{
				// コンテキストを取得
				auto* ctx = static_cast<InstantiateTaskContext<T>*>(ptr);
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
					auto* ctx = static_cast<InstantiateTaskContext<T>*>(ptr);
					const XMMATRIX& worldMatrix = ctx->owner->GetWorldMatrix();
					Model* model = ctx->owner->instance.pModel;
					// モデルの三角形をビルド
					model->Deferred_BuildMeshTriangles(worldMatrix, true, ctx->owner->instance.drawWorldAABB);
					// モデルのボクセルグリッドをビルド
					model->Deferred_BuildMeshVoxelGrid(worldMatrix, true);
				},
				[](void* ptr) // タスクの条件関数
				{
					// コンテキストを取得
					auto* ctx = static_cast<InstantiateTaskContext<T>*>(ptr);
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
					auto* ctx = static_cast<InstantiateTaskContext<T>*>(ptr);
					const BOUNDING_BOX& boundingBox = ctx->owner->GetCollider().aabb;
					Model* model = ctx->owner->instance.pModel;
					// モデルの三角形をビルド
					model->Deferred_BuildBoundingBoxTriangles(boundingBox, ctx->owner->instance.drawWorldAABB, ctx->config->trackTriangles);
					// モデルのボクセルグリッドをビルド
					model->Deferred_BuildBoundingBoxVoxelGrid(boundingBox);
				},
				[](void* ptr) // タスクの条件関数
				{
					// コンテキストを取得
					auto* ctx = static_cast<InstantiateTaskContext<T>*>(ptr);
					// モデルがロードされているかチェック
					return ctx->owner->GetCollider().isInitialized;
				},
				options
			);
		}
		else if (instance.collisionType == ObjectCollisionType::COLLIDER_BOUNDING_BOX)
		{
			if (m_skinnedConfig.colliderTag != ColliderTag::PLAYER_ATTACK &&
				m_skinnedConfig.colliderTag != ColliderTag::ENEMY_ATTACK)
				instance.collider.enable = config.enableCollider;

			CollisionManager::get_instance().RegisterDynamicCollider(&instance.collider);
			instance.collisionBuilt = true; // 当たり判定のビルドが完了
		}

		instance.collider.tag = m_config.colliderTag;
		instance.collider.owner = this;

		instance.modelBuildTaskRegistered = true;
	}

	instance.use = true; // モデルを使用するフラグを立てる

	if (instance.pModel && instance.postInit)
	{
		instance.load = true; // モデルがロードされたらロードフラグを立てる
		Scene::get_instance().RegisterGameObject(this); // renderableObjectsに登録
	}
	else
	{
		instance.load = false; // モデルがロードできなかった場合はロードフラグを下げる
	}
}

template<typename T>
void GameObject<T>::Update()
{
	if (!instance.load)
	{
		Instantiate(m_config);
		if (!instance.load) return; // モデルがロードされていない場合は何もしない
	}

	XMMATRIX mtxScl, mtxRot, mtxTranslate, mtxWorld;

	// ワールドマトリックスの初期化
	mtxWorld = XMMatrixIdentity();

	// スケールを反映
	mtxScl = XMMatrixScaling(instance.transform.scl.x, instance.transform.scl.y, instance.transform.scl.z);
	mtxWorld = XMMatrixMultiply(mtxWorld, mtxScl);

	// 回転を反映
	mtxRot = XMMatrixRotationRollPitchYaw(instance.transform.rot.x, instance.transform.rot.y + XM_PI, instance.transform.rot.z);
	mtxWorld = XMMatrixMultiply(mtxWorld, mtxRot);

	// 移動を反映
	mtxTranslate = XMMatrixTranslation(instance.transform.pos.x, instance.transform.pos.y, instance.transform.pos.z);
	mtxWorld = XMMatrixMultiply(mtxWorld, mtxTranslate);

	instance.transform.mtxWorld = mtxWorld;

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
		XMVECTOR worldPt = XMVector3Transform(localCorners[i], instance.transform.mtxWorld);
		worldMin = XMVectorMin(worldMin, worldPt);
		worldMax = XMVectorMax(worldMax, worldPt);
	}

	// AABBをコライダーに格納（ワールド空間）
	instance.collider.UpdateBoundingBox(worldMin, worldMax);
}

template <typename T>
void GameObject<T>::Draw()
{
	if (instance.pModel == nullptr || !instance.load || !instance.use)
	{
		return; // モデルがロードされていない場合は描画しない
	}

	// ワールドマトリックスの設定
	renderer.SetCurrentWorldMatrix(&instance.transform.mtxWorld);

	instance.pModel->DrawModel();
}


template<typename T>
inline void GameObject<T>::RenderDebugInfo(void)
{
#if DRAW_BOUNDING_BOX
	if (instance.drawWorldAABB)
	{
		const BOUNDING_BOX& box = instance.collider.aabb;

		XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

		m_debugBoundingBoxRenderer.DrawBox(box, Camera::get_instance().GetViewProjMtx(), color);
	}
#endif
}

template<typename T>
inline void GameObject<T>::DrawModelEditor()
{
	if (instance.isCursorIn == TRUE)
	{
		renderer.SetFillMode(D3D11_FILL_WIREFRAME);
		instance.pModel->DrawModel();
		renderer.SetFillMode(D3D11_FILL_SOLID);
	}
	else
	{
		instance.pModel->DrawModel();
	}
}

template<typename T>
inline void GameObject<T>::UpdateModelEditor()
{
	if (instance.isCursorIn == FALSE) return;

	if (IsMouseLeftTriggered())
	{
		instance.isSelected = instance.isSelected == TRUE ? FALSE : TRUE;
		if (instance.isSelected == TRUE)
			MapEditor::get_instance().SetCurSelectedModelIdx(instance.editorIdx);
		else
			MapEditor::get_instance().ResetCurSelectedModelIdx();
	}
}

template <>
GameObject<SkinnedMeshModelInstance>::~GameObject();

template <>
void GameObject<SkinnedMeshModelInstance>::Instantiate(const SkinnedGameObjectConfig& config);

template<>
void GameObject< SkinnedMeshModelInstance>::Update();

