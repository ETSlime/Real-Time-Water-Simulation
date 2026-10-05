#include "Interactable.h"

Interactable::Interactable(ItemType itemType, Transform transform): Item(itemType, transform)
{
	/*GameObjectConfig config;
	config.modelPath = "data/MODEL/Item/Key/Key.obj";
	config.collisionType = ObjectCollisionType::COLLIDER_BOUNDING_BOX;
	config.colliderTag = ColliderTag::DEFAULT;
	config.scale = XMFLOAT3(5.0f, 5.0f, 5.0f);
	config.modelType = ModelType::Default;
	config.rotation = XMFLOAT3(90.0f, 0.0f, 0.0f);
	Instantiate(config);*/

	instance.collider.enable = false;

	m_triggerCollider.tag = ColliderTag::INTERACTABLE;
	m_triggerCollider.owner = this;
	m_triggerCollider.enable = true;

	XMFLOAT3 minPoint = m_triggerCollider.aabb.minPoint;
	XMFLOAT3 maxPoint = m_triggerCollider.aabb.maxPoint;

	minPoint.z -= 300;
	maxPoint.z -= 300;

	m_triggerCollider.aabb = BOUNDING_BOX(minPoint, maxPoint);

	XMVECTOR worldMin = XMVectorSet(minPoint.x, minPoint.y, minPoint.z, 1.0f);
	XMVECTOR worldMax = XMVectorSet(maxPoint.x, maxPoint.y, maxPoint.z, 1.0f);

	CollisionManager::get_instance().RegisterDynamicCollider(&m_triggerCollider);
}

Interactable::~Interactable()
{

}

void Interactable::Update(void)
{
	Item::Update();

	//モデルがロード待ち
	if (!instance.load) return;

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
		XMVectorSet(localMin.x, localMin.y, localMax.z+50, 1.0f),
		XMVectorSet(localMax.x, localMin.y, localMax.z+50, 1.0f),
		XMVectorSet(localMin.x, localMax.y, localMax.z+50, 1.0f),
		XMVectorSet(localMax.x, localMax.y, localMax.z+50, 1.0f),
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
	m_triggerCollider.UpdateBoundingBox(worldMin, worldMax);
}

void Interactable::Draw(void)
{
	Item::Draw();
}

void Interactable::SetActive(bool active)
{
	Item::SetActive(active);

	m_triggerCollider.enable = active;
}
