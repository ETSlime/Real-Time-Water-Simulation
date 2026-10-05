//=============================================================================
//
// 当たり判定処理 [CollisionManager.cpp]
// Author : 
//
//=============================================================================
#include "Collision/CollisionManager.h"
#include "Collision/VoxelBufferUploader.h"
#include "Core/GameSystem.h"
#include "Scene/GameObject.h"
#include "Scene/Enemy.h"
#include "Scene/Player.h"
#include "Scene/Weapon/Projectile.h"
#include "Scene/Ground.h"
#include "Scene/Item/Interactable/Interactable.h"
#include "Utility/Debug/Debugproc.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define DRAW_BOUNDING_BOX               1
#define MAX_TRIGGER_EVENT_NUM           256
#define MAX_RAYCAST_DISTANCE            10000.0f

constexpr float HEIGHT_EPSILON = 5.0f;
constexpr float SMOOTHING_FACTOR = 0.5f;
// 最大許容斜面角度（ラジアン、例：45度）
const float MAX_SLOPE_ANGLE = 0.785398f; // 約45度
// 階段として許容する最大高さ差（ステップオフセット）
const float STEP_OFFSET = 45.0f;
const float FOOT_OFFSET = 0.0f;

void CollisionManager::Init(void)
{
#ifdef _DEBUG
    DebugProc::get_instance().Register(this);
    m_debugTriangles.reserve(INITIALIZE_DEBUG_TRIANGLE_COUNT);
#endif // DEBUG

#if DRAW_BOUNDING_BOX
    m_debugBoundingBoxRenderer.Initialize();
	m_debugTriangleRenderer.Initialize();
#endif

    m_framePairsA = HashMap<ColliderPair, bool, HashColliderPair, EqualColliderPair>(
        MAX_TRIGGER_EVENT_NUM, // 初期バケット数
        HashColliderPair(), // ハッシュ関数
        EqualColliderPair() // 等価比較関数
    );

    m_framePairsB = HashMap<ColliderPair, bool, HashColliderPair, EqualColliderPair>(
        MAX_TRIGGER_EVENT_NUM, // 初期バケット数
        HashColliderPair(), // ハッシュ関数
        EqualColliderPair() // 等価比較関数
    );

    m_currentFramePairs = &m_framePairsA;
    m_lastFramePairs = &m_framePairsB;

	m_initialized = true;
}

void CollisionManager::InitOctree(const BOUNDING_BOX& boundingBox)
{
	if (!m_initialized) return; // 初期化されていない場合は何もしない

    if (m_staticOctree == nullptr)
        m_staticOctree = new OctreeNode(boundingBox);
    else
    {
        SAFE_DELETE(m_staticOctree);
        m_staticOctree = new OctreeNode(boundingBox);
    }
}

void CollisionManager::InitVoxelGrid(const SimpleArray<Triangle*>& triangles, float voxelSize)
{
    if (!m_initialized) return; // 初期化されていない場合は何もしない

    if (m_voxelGrid == nullptr)
        m_voxelGrid = new VoxelGrid();
    else
    {
        SAFE_DELETE(m_voxelGrid);
        m_voxelGrid = new VoxelGrid();
    }

	// ボクセルグリッドの初期化
	m_voxelGrid->Initialize(voxelSize);
    m_voxelGrid->BuildTriangles(triangles);
	VoxelBufferUploader::get_instance().SetCachedGrid(m_voxelGrid); // ボクセルグリッドをキャッシュに設定
}

void CollisionManager::Update()
{
    if (!m_initialized) return; // 初期化されていない場合は何もしない
	if (!GameSystem::get_instance().GetFinishLoading()) return; // ロード中は何もしない

#ifdef _DEBUG
    if (m_voxelGrid)
    {
        if (InputManager::get_instance().GetKeyboardTrigger(DIK_V))
        {
            m_voxelGrid->print_debug_stats();
        }
    }
	m_debugTriangles.clear();
#endif // _DEBUG

    UINT numColliders = m_dynamicColliders.getSize();
    
    for (UINT i = 0; i < numColliders; i++)
    {
        const Collider* dynamicCol = m_dynamicColliders[i];

        if (dynamicCol->tag == ColliderTag::WALL ||
            dynamicCol->tag == ColliderTag::STATIC_OBJECT ||
            dynamicCol->tag == ColliderTag::AIR_WALL ||
            dynamicCol->tag == ColliderTag::TRANSPARENT_WALL ||
            dynamicCol->enable == false) continue;

        // 動的オブジェクトと静的オブジェクト（地面）の衝突検出
        HandleStaticCollision(dynamicCol);
      
        // 動的オブジェクト同士（例：プレイヤーと敵、その他）の衝突検出
        HandleDynamicCollision(dynamicCol, i);

		// トリガーイベント型の衝突検出
        HandleEventTriggerCollision(dynamicCol);

    }

    // 接触終了イベントを発行
	TriggerOnExit();
}

bool CollisionManager::OctreeInsertTriangle(const Triangle* tri, bool insertWithHandle)
{
    if (!m_initialized) return false;

	bool success = false;
    if (m_staticOctree)
    {
        if (insertWithHandle)
			success = m_staticOctree->InsertWithHandle(tri);
        else
            success = m_staticOctree->Insert(tri);
    }

    return success;
}

void CollisionManager::VoxelGridInsertTriangle(const Triangle* tri)
{
    if (!m_initialized) return; // 初期化されていない場合は何もしない

    if (m_voxelGrid)
    {
        m_voxelGrid->InsertTriangle(tri);
    }
    else
    {
		// ボクセルグリッドが初期化されていない場合は新規に作成
		m_voxelGrid = new VoxelGrid();
		m_voxelGrid->Initialize();
		m_voxelGrid->InsertTriangle(tri);
		VoxelBufferUploader::get_instance().SetCachedGrid(m_voxelGrid); // ボクセルグリッドをキャッシュに設定
    }
}

void CollisionManager::VoxelGridInsertTriangles(const SimpleArray<Triangle*>& triangles)
{
    if (!m_initialized) return; // 初期化されていない場合は何もしない

    if (m_voxelGrid)
        m_voxelGrid->BuildTriangles(triangles);
    else
    {
		// ボクセルグリッドが初期化されていない場合は新規に作成
		m_voxelGrid = new VoxelGrid();
		m_voxelGrid->Initialize();
		m_voxelGrid->BuildTriangles(triangles);
		VoxelBufferUploader::get_instance().SetCachedGrid(m_voxelGrid); // ボクセルグリッドをキャッシュに設定
    }
}

void CollisionManager::UnregisterDynamicCollider(const Collider* collider)
{
    if (!m_initialized) return; // 初期化されていない場合は何もしない

    int index = m_dynamicColliders.find_index(collider);
    if (index >= 0)
    {
        m_dynamicColliders.erase(index);
    }
}

void CollisionManager::UnregisterEventCollider(Collider* collider)
{
    if (!m_initialized) return; // 初期化されていない場合は何もしない

    int index = m_triggerEventColliders.find_index(collider);
    if (index >= 0)
    {
        m_triggerEventColliders.erase(index);
    }

}

bool CollisionManager::RaycastForTargetPoint(const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDir, XMFLOAT3* targetHitPosOut) const
{
    float closestDistSq = MAX_RAYCAST_DISTANCE * MAX_RAYCAST_DISTANCE;
    bool hit = false;
    XMFLOAT3 closestHitPos;

    for (const auto& col : m_dynamicColliders)
    {
		// プレイヤー関連、トリガー、エアウォール、アイテムは無視
		if (col->tag == ColliderTag::PLAYER 
            || col->tag == ColliderTag::PLAYER_PROJECTILE
            || col->tag == ColliderTag::PLAYER_ATTACK
            || col->tag == ColliderTag::ENEMY_ATTACK
            || col->tag == ColliderTag::TRIGGER
            || col->tag == ColliderTag::AIR_WALL
            || col->tag == ColliderTag::ITEM)
            continue;

        float dist;
        XMFLOAT3 hitPos;
        if (col->aabb.IntersectRay(rayOrigin, rayDir, &dist, &hitPos))
        {
            if (dist < closestDistSq)
            {
                closestDistSq = dist;
                closestHitPos = hitPos;
                hit = true;
            }
        }
    }

	// 静的オブジェクト（地面）との衝突判定
    if (m_voxelGrid)
    {
        XMFLOAT3 voxelHitPos;
        float voxelHitDistSq;
		XMFLOAT3 endPoint = {
			rayOrigin.x + rayDir.x * MAX_RAYCAST_DISTANCE,
			rayOrigin.y + rayDir.y * MAX_RAYCAST_DISTANCE,
			rayOrigin.z + rayDir.z * MAX_RAYCAST_DISTANCE
		};

        bool voxelHit = m_voxelGrid->Raycast(rayOrigin, endPoint, &voxelHitPos, nullptr);
        voxelHitDistSq = (voxelHitPos.x - rayOrigin.x) * (voxelHitPos.x - rayOrigin.x) +
			(voxelHitPos.y - rayOrigin.y) * (voxelHitPos.y - rayOrigin.y) +
			(voxelHitPos.z - rayOrigin.z) * (voxelHitPos.z - rayOrigin.z);

        if (voxelHit && voxelHitDistSq < closestDistSq)
        {
            closestDistSq = voxelHitDistSq;
            closestHitPos = voxelHitPos;
            hit = true;
        }
    }

    if (hit && targetHitPosOut)
    {
        *targetHitPosOut = closestHitPos;
    }

    return hit;
}

void CollisionManager::PrepareVoxelMappingAfterSceneChange(void)
{
	// シーン切り替え後にボクセルマッピングを準備する
	VoxelBufferUploader::get_instance().RegisterAsyncMappingBuild();

    DeferredTaskOptions options{};
    options.async = true;
    options.debugName = "CollisionManager_PrepareVoxelMappingAfterSceneChange"; // デバッグ用の名前

    RawDeferredTask::Add(
        nullptr,
        [](void* ptr)
        {
            // ボクセルグリッドの三角形からボクセルマップを構築
			CollisionManager::get_instance().m_voxelGrid->BuildVoxelMapByTriangles();
        },
        [](void* ptr)
        {
            // 地形が準備完了かどうかを確認する
			return CollisionManager::get_instance().m_voxelGrid && Ground::get_instance().IsTerrainReady();
        },
        options
    );
}

void CollisionManager::BlockColliderMovement(const Collider* collider, const Collider* collider2)
{
    auto colliderOwner = static_cast<GameObject<SkinnedMeshModelInstance>*>(collider->owner);

    if (!colliderOwner) return;

    bool isObstacle = true;

    Transform transform = colliderOwner->GetTransform();

    // 障害物の AABB 情報
    const BOUNDING_BOX& obstacleAABB = collider2->aabb;  // 衝突した障害物の AABB

    // 各面との距離を計算
    float distLeft = fabs(transform.pos.x - obstacleAABB.minPoint.x);
    float distRight = fabs(transform.pos.x - obstacleAABB.maxPoint.x);
    float distFront = fabs(transform.pos.z - obstacleAABB.minPoint.z);
    float distBack = fabs(transform.pos.z - obstacleAABB.maxPoint.z);

    // 最も近い面を特定し、推定法線を決定
    XMFLOAT3 estimatedNormal = { 0, 0, 0 };

    float minDist = distLeft;
    estimatedNormal.x = 1.0f; // 法線向プレイヤー方向（プレイヤーが minX より左なら → 法線は右）

    // 左右の面の距離が近い場合は、X 軸方向の成分を優先
    if (distRight < minDist)
    {
        minDist = distRight;
        estimatedNormal.x = -1.0f; // プレイヤーが maxX より右 → 法線は左
        estimatedNormal.z = 0.0f;
    }

    // 後ろの面の距離が近い場合は、Z 軸方向の成分を優先
    if (distFront < minDist)
    {
        minDist = distFront;
        estimatedNormal.x = 0.0f;
        estimatedNormal.z = 1.0f; // プレイヤーが minZ より前 → 法線は奥
    }
    if (distBack < minDist)
    {
        minDist = distBack;
        estimatedNormal.x = 0.0f;
        estimatedNormal.z = -1.0f; // プレイヤーが maxZ より後ろ → 法線は手前
    }

    // 推定された法線を XMVECTOR に変換
    XMVECTOR estimatedObstacleNormal = XMVectorSet(estimatedNormal.x, 0, estimatedNormal.z, 0);
    estimatedObstacleNormal = XMVector3Normalize(estimatedObstacleNormal);

    // 意図する移動ベクトルを計算（スカラー速度と回転角を使用）
    Attributes attributes = colliderOwner->GetAttributesConst();
    float intendedMoveX = sinf(transform.rot.y) * attributes.spd;
    float intendedMoveZ = cosf(transform.rot.y) * attributes.spd;

    // XMVECTOR に変換（Y 成分は 0）
    XMVECTOR intendedMove = XMVectorSet(intendedMoveX, 0, intendedMoveZ, 0);
    intendedMove = XMVector3Normalize(intendedMove);

    // 内積を計算
    float dot = XMVectorGetX(XMVector3Dot(intendedMove, estimatedObstacleNormal));

    if (dot > 0.0f)
    {
        // 障害物方向の成分を除去
        XMVECTOR projection = XMVectorScale(estimatedObstacleNormal, dot);
        XMVECTOR allowedMove = XMVectorSubtract(intendedMove, projection);
        allowedMove = XMVector3Normalize(allowedMove);

        // 修正後の移動ベクトルに速度スカラーをかける
        float spd = attributes.spd;
        XMVECTOR finalMove = XMVectorScale(allowedMove, spd);

        // 最終的な移動量を取得してプレイヤーの位置を更新
        XMFLOAT3 moveDelta;
        XMStoreFloat3(&moveDelta, finalMove);
        transform.pos.x += moveDelta.x;
        transform.pos.z += moveDelta.z;

    }
    else
    {
        // 障害物の方向ではないので、そのまま移動
        transform.pos.x += intendedMoveX;
        transform.pos.z += intendedMoveZ;
    }

    colliderOwner->SetTransform(transform);

    // isMoveBlockedの更新を行う
    bool isMoveBlocked = colliderOwner->GetAttributesConst().isMoveBlocked;
    if (isObstacle && !isMoveBlocked)
    {
        colliderOwner->SetMoveBlock(true);
    }
    else if (!isObstacle && isMoveBlocked)
    {
        colliderOwner->SetMoveBlock(false);
    }
}

void CollisionManager::HandleStaticCollision(const Collider* dynamicCol)
{
    if (m_staticOctree && dynamicCol->enableStaticCollision)
    {

        auto colliderOwner = static_cast<GameObject<SkinnedMeshModelInstance>*>(dynamicCol->owner);
        if (!colliderOwner) return;
        if (!colliderOwner->GetLoad()) return;

        Transform transform = colliderOwner->GetTransform();

        // 動的オブジェクトの AABB を元に、静的八分木から候補となる三角形をクエリする
        SimpleArray<const Triangle*> candidates;
        m_staticOctree->QueryRange(dynamicCol->aabb, candidates);

        bool hasCollision = false;
        bool hasValidSlope = false;
        bool isObstacle = false;
        float bestY = -FLT_MAX;
        float minHeightDiff = FLT_MAX;
        float minDistSq = FLT_MAX; // 最短距離
        float currentY = dynamicCol->aabb.minPoint.y;//transform.pos.y;
        XMVECTOR obstacleNormal = XMVectorZero();

        // 候補となる各三角形に対して、精密な衝突判定を実施する
        UINT numCandidates = candidates.getSize();
        for (UINT j = 0; j < numCandidates; j++)
        {
            const Triangle* tri = candidates[j];

            // AABB同士の粗判定（八叉木で取得しても再確認）
            if (dynamicCol->aabb.intersects(tri->aabb))
            {
                // 動的オブジェクトがプレイヤーと敵の場合のみ、斜面の高さ補正を計算する
                if (dynamicCol->tag == ColliderTag::PLAYER ||
                    dynamicCol->tag == ColliderTag::ENEMY)
                {
                    XMVECTOR p0 = XMLoadFloat3(&tri->v0);
                    XMVECTOR p1 = XMLoadFloat3(&tri->v1);
                    XMVECTOR p2 = XMLoadFloat3(&tri->v2);

                    XMFLOAT3 normal = tri->normal;
                    XMVECTOR normalVec = XMLoadFloat3(&normal); // 正規化済みの法線

                    // 法線がほぼ水平でない場合（地面系）
                    if (fabs(normal.y) > 0.001f) // y成分がある -> 地面/斜面
                    {
                        // 三角形の法線と平面方程式の d を計算
                        float d = -XMVectorGetX(XMVector3Dot(normalVec, p0));

                        // プレイヤーの水平座標を取得
                        float posX = transform.pos.x;
                        float posZ = transform.pos.z;

                        // プレイヤー位置の真下（XZ平面）での平面交点高さを求める
                        float candidateHitY = (-d - normal.x * posX - normal.z * posZ) / normal.y;

                        // 交点が三角形内部にあるか確認（外ならスキップ）
                        XMVECTOR hitPos = XMVectorSet(posX, candidateHitY, posZ, 1.0f);
                        if (!IsPointInTriangle(hitPos, p0, p1, p2))
                            continue;

                        // 斜面角度を計算
                        float slopeAngle = acosf(normal.y); // 単位法線を使用
                        // プレイヤー現在高さとの高さ差
                        float heightDiff = fabs(candidateHitY - currentY);

                        // 許容範囲の斜面または段差かどうか判定
                        if (slopeAngle <= MAX_SLOPE_ANGLE || heightDiff < STEP_OFFSET)
                        {
                            // この三角形は地面として有効
                            hasCollision = true;
                            hasValidSlope = true;

                            // プレイヤーの現在位置より上にある段差の場合、優先的にその高さを選択
                            if (candidateHitY > currentY && heightDiff < STEP_OFFSET)
                            {
                                // 台階の高さを優先（より高い面を選ぶ）
                                if (candidateHitY > bestY)
                                {
                                    bestY = candidateHitY;
                                    minHeightDiff = heightDiff;
                                }
                            }
                            else
                            {
                                // 通常の地面として、距離が最も近いものを選択
                                if (heightDiff < minHeightDiff)
                                {
                                    minHeightDiff = heightDiff;
                                    bestY = candidateHitY;
                                }
                            }
                        }
                        else
                        {
                            // 現在位置より上にある場合
                            if (candidateHitY > currentY)
                            {
                                // 許容角度を超える場合は障害物とみなす
                                isObstacle = true;
                                obstacleNormal = normalVec;
                            }
                        }
                    }
                    else
                    {
                        // n.y がほぼゼロ ->法線がほぼ水平な
                        XMFLOAT3 aabbCenter = dynamicCol->aabb.GetCenter();
                        float epsilon = ComputeWallCollisionEpsilon(dynamicCol->aabb);

                        // AABBの中心から三角形への最近距離を計算
                        XMFLOAT3 closest;
                        ClosestPointOnTriangle(aabbCenter, tri->v0, tri->v1, tri->v2, closest);

                        XMVECTOR c = XMLoadFloat3(&closest);
                        XMVECTOR aabbCenterVec = XMLoadFloat3(&aabbCenter);
                        float distSq = XMVectorGetX(XMVector3LengthSq(c - aabbCenterVec));

                        // 法線の向きに関係なく距離だけで判定（両面で壁判定）
                        if (distSq < epsilon * epsilon)
                        {
                            hasCollision = true;
                            hasValidSlope = false;
                            isObstacle = true; // 完全に壁として扱う
                            obstacleNormal = normalVec;
                        }
                    }

#ifdef _DEBUG
                    if (dynamicCol->tag == ColliderTag::PLAYER)
                    {
                        m_curCollisionBox = tri->aabb; // 現在の衝突ボックスを更新
                        m_debugTriangles.push_back(*tri); // デバッグ用に三角形を保存
                    }
#endif // _DEBUG

                }
                else if (dynamicCol->tag == ColliderTag::PLAYER_PROJECTILE
                    || dynamicCol->tag == ColliderTag::ENEMY_PROJECTILE)
                {
                    CollisionEvent event;
                    event.additionalData = &tri->normal;
					dynamicCol->CallCollisionCallbacks(event, CollisionCallbackID::ProjectileHit);
                }

                CollisionEvent collisionEvent;
                collisionEvent.colliderA = dynamicCol;
                // 静的オブジェクトは Triangle* で管理しているので、ここでは衝突対象の種類を WALL とする
                static Collider dummyStatic;
                dummyStatic.aabb = tri->aabb;
                dummyStatic.tag = ColliderTag::WALL;
                collisionEvent.colliderB = &dummyStatic;
                eventBus.publishCollision(collisionEvent);
            }
        }


        if (dynamicCol->tag == ColliderTag::PLAYER ||
            dynamicCol->tag == ColliderTag::ENEMY)
        {
            if (hasCollision)
            {
                if (hasValidSlope)
                {
                    if (fabs(currentY - bestY) > HEIGHT_EPSILON)
                    {
                        // 現在のcolliderの位置を XMVECTOR に変換
                        XMVECTOR currentPos = XMLoadFloat3(&transform.pos);

                        // 目標のcolliderの位置は、x, z はそのままで y を hitY に設定する
                        XMVECTOR targetPos = XMVectorSet(transform.pos.x, bestY - 5.0f, transform.pos.z, 0.0f);

                        // XMVectorLerp を用いて、現在位置と目標位置の間を補間する
                        XMVECTOR lerpedPos = XMVectorLerp(currentPos, targetPos, SMOOTHING_FACTOR);

                        // 補間結果をcolliderの位置に戻す
                        XMStoreFloat3(&transform.pos, lerpedPos);

                        colliderOwner->SetTransform(transform);
                    }
                }
                if (isObstacle)
                {
                    // 高い障害物の場合：移動ベクトルの調整（滑るように動く）

                    Attributes attributes = colliderOwner->GetAttributesConst();

                    // 意図する移動ベクトルの計算
                    float intendedMoveX = sinf(transform.rot.y) * attributes.spd;
                    float intendedMoveZ = cosf(transform.rot.y) * attributes.spd;

                    // 意図する移動ベクトルを DirectX の XMVECTOR に変換（Y成分は 0）
                    XMVECTOR intendedMove = XMVectorSet(intendedMoveX, 0, intendedMoveZ, 0);
                    intendedMove = XMVector3Normalize(intendedMove);  // 正規化

                    // 障害物との衝突がある場合、障害物法線（水平成分のみ）との内積を計算
                    float dot = XMVectorGetX(XMVector3Dot(intendedMove, obstacleNormal));

                    // dot > 0 なら、障害物方向に移動しようとしているので、その成分を除去
                    if (dot > 0.0f)
                    {
                        XMVECTOR projection = XMVectorScale(obstacleNormal, dot);
                        XMVECTOR allowedMove = XMVectorSubtract(intendedMove, projection);
                        allowedMove = XMVector3Normalize(allowedMove);

                        // 修正後の移動ベクトルに、元の移動スカラー（速度）をかける
                        float spd = attributes.spd; // 元の移動スカラー
                        XMVECTOR finalMove = XMVectorScale(allowedMove, spd);

                        // 最終的な移動量を取得して、transform.pos に加算する
                        XMFLOAT3 moveDelta;
                        XMStoreFloat3(&moveDelta, finalMove);
                        transform.pos.x += moveDelta.x;
                        transform.pos.z += moveDelta.z;
                    }
                    else
                    {
                        // dot が 0 以下の場合は、障害物方向には移動していないので、そのまま加算する
                        transform.pos.x += intendedMoveX;
                        transform.pos.z += intendedMoveZ;
                    }
                }

                colliderOwner->SetTransform(transform);
            }

            // isMoveBlockedの更新を行う
            bool isMoveBlocked = colliderOwner->GetAttributesConst().isMoveBlocked;
            if (isObstacle && !isMoveBlocked)
            {
                colliderOwner->SetMoveBlock(true);
            }
            else if (!isObstacle && isMoveBlocked)
            {
                colliderOwner->SetMoveBlock(false);
            }
            bool isGrounded = colliderOwner->GetAttributesConst().isGrounded;
            // 着地状態（isGrounded）の更新を行う
            if (hasCollision && !isGrounded)
            {
                colliderOwner->SetGrounded(true);
            }
            else if (!hasCollision && isGrounded)
            {
                colliderOwner->SetGrounded(false);
            }
        }
    }

}

void CollisionManager::HandleDynamicCollision(const Collider* dynamicCol, UINT startIdx)
{
    if (dynamicCol->enableDynamicCollision)
    {
        UINT numColliders = m_dynamicColliders.getSize();

        for (UINT j = startIdx + 1; j < numColliders; j++)
        {
            const Collider* dynamicCol2 = m_dynamicColliders[j];

            if (!dynamicCol2->enable || !dynamicCol2->enableDynamicCollision)
                continue; // 動的衝突が無効な場合はスキップ
            else if (IsSelfCollision(dynamicCol, dynamicCol2)) continue;

            if ((dynamicCol->tag == ColliderTag::AIR_WALL && dynamicCol2->tag == ColliderTag::NPC)
                || (dynamicCol2->tag == ColliderTag::NPC && dynamicCol->tag == ColliderTag::AIR_WALL))
                continue;

            bool isObstacle = false;
            if (m_dynamicColliders[startIdx]->aabb.intersects(m_dynamicColliders[j]->aabb))
            {
                CollisionEvent event{ dynamicCol, dynamicCol2 };

                if ((dynamicCol->tag == ColliderTag::ENEMY && dynamicCol2->tag == ColliderTag::PLAYER_ATTACK)
                    || (dynamicCol2->tag == ColliderTag::ENEMY && dynamicCol->tag == ColliderTag::PLAYER_ATTACK)
                    || (dynamicCol->tag == ColliderTag::ENEMY && dynamicCol2->tag == ColliderTag::PLAYER_PROJECTILE)
                    || (dynamicCol2->tag == ColliderTag::ENEMY && dynamicCol->tag == ColliderTag::PLAYER_PROJECTILE)
                    )
                {
                    const Collider* enemyCol = dynamicCol->tag == ColliderTag::ENEMY ? dynamicCol : dynamicCol2;

                    enemyCol->CallCollisionCallbacks(event, CollisionCallbackID::PlayerAttackHit);

                }
                else if ((dynamicCol->tag == ColliderTag::PLAYER && dynamicCol2->tag == ColliderTag::ENEMY_ATTACK)
                    || (dynamicCol2->tag == ColliderTag::PLAYER && dynamicCol->tag == ColliderTag::ENEMY_ATTACK))
                {

                    // コールバックを持っていれば呼ぶ
                    dynamicCol->CallCollisionCallbacks(event, CollisionCallbackID::EnemyAttackHit);
                    dynamicCol2->CallCollisionCallbacks(event, CollisionCallbackID::EnemyAttackHit);
                }
                else if ((dynamicCol->tag == ColliderTag::PLAYER && dynamicCol2->tag == ColliderTag::ITEM)
                    || (dynamicCol2->tag == ColliderTag::PLAYER && dynamicCol->tag == ColliderTag::ITEM)
                    || (dynamicCol->tag == ColliderTag::ENEMY && dynamicCol2->tag == ColliderTag::ITEM)
                    || (dynamicCol2->tag == ColliderTag::ENEMY && dynamicCol->tag == ColliderTag::ITEM))
                {
                    // コールバックを持っていれば呼ぶ
                    dynamicCol->CallCollisionCallbacks(event, CollisionCallbackID::Item);
                    dynamicCol2->CallCollisionCallbacks(event, CollisionCallbackID::Item);

                }
                else if ((dynamicCol->tag == ColliderTag::PLAYER_PROJECTILE && dynamicCol2->tag == ColliderTag::BOSS_ICE)
                    || (dynamicCol2->tag == ColliderTag::PLAYER_PROJECTILE && dynamicCol->tag == ColliderTag::BOSS_ICE))
                {
                    const Collider* iceCol = dynamicCol->tag == ColliderTag::BOSS_ICE ? dynamicCol : dynamicCol2;

                    // コールバックを持っていれば呼ぶ
                    iceCol->CallCollisionCallbacks(event, CollisionCallbackID::PlayerAttackHit);

                }
                else if ((dynamicCol->tag == ColliderTag::PLAYER && dynamicCol2->tag == ColliderTag::ENEMY_PROJECTILE)
                    || (dynamicCol2->tag == ColliderTag::PLAYER && dynamicCol->tag == ColliderTag::ENEMY_PROJECTILE))
                {
                    const Collider* playerCol = dynamicCol->tag == ColliderTag::PLAYER ? dynamicCol : dynamicCol2;

                    playerCol->CallCollisionCallbacks(event, CollisionCallbackID::EnemyAttackHit);

                }
                //プレイヤーのHPとの関係性あり
                else if ((dynamicCol->tag == ColliderTag::PLAYER && dynamicCol2->tag == ColliderTag::INTERACTABLE)
                    || (dynamicCol2->tag == ColliderTag::PLAYER && dynamicCol->tag == ColliderTag::INTERACTABLE))
                {
                    const Collider* playerCol = dynamicCol->tag == ColliderTag::PLAYER ? dynamicCol : dynamicCol2;
                    Lumine* player = static_cast<Lumine*>(playerCol->owner);
                    Player* playerComp = static_cast<Player*>(player->GetPlayerComponent());

                    const Collider* itemCol = dynamicCol->tag == ColliderTag::INTERACTABLE ? dynamicCol : dynamicCol2;
                    auto itemColOwner = static_cast<Interactable*>(itemCol->owner);

                    playerComp->LinkInteractable(itemColOwner);

                }
                else
                {
					BlockColliderMovement(dynamicCol, dynamicCol2);
					BlockColliderMovement(dynamicCol2, dynamicCol);
                }

                CollisionEvent collisionEvent;
                collisionEvent.colliderA = m_dynamicColliders[startIdx];
                collisionEvent.colliderB = m_dynamicColliders[j];
                eventBus.publishCollision(collisionEvent);
            }
        }
    }

}

void CollisionManager::HandleEventTriggerCollision(const Collider* dynamicCol)
{
    if (dynamicCol->enableTriggerEventCollision)
    {
        UINT triggerEventCount = m_triggerEventColliders.getSize();
        for (UINT j = 0; j < triggerEventCount; j++)
        {

            Collider* triggerCol = m_triggerEventColliders[j];
            if (triggerCol->enable == false)
                continue; // トリガーイベントが無効な場合はスキップ

            // トリガーイベント型の衝突検出
            if (dynamicCol->aabb.intersects(triggerCol->aabb))
            {
                CheckEventCollision(dynamicCol, triggerCol);
            }
        }
    }
}

bool CollisionManager::IsSelfCollision(const Collider* col1, const Collider* col2)
{
    return (col1->tag == ColliderTag::PLAYER && col2->tag == ColliderTag::PLAYER_ATTACK) ||
        (col2->tag == ColliderTag::PLAYER && col1->tag == ColliderTag::PLAYER_ATTACK) ||
        (col1->tag == ColliderTag::PLAYER && col2->tag == ColliderTag::PLAYER_PROJECTILE) ||
        (col2->tag == ColliderTag::PLAYER && col1->tag == ColliderTag::PLAYER_PROJECTILE) ||
        (col1->tag == ColliderTag::ENEMY && col2->tag == ColliderTag::ENEMY_ATTACK) ||
        (col2->tag == ColliderTag::ENEMY && col1->tag == ColliderTag::ENEMY_ATTACK) ||
        (col1->tag == ColliderTag::AIR_WALL && col2->tag == ColliderTag::AIR_WALL) ||
        (col2->tag == ColliderTag::AIR_WALL && col1->tag == ColliderTag::AIR_WALL);
}

void CollisionManager::CheckEventCollision(const Collider* dynamicCol, Collider* triggerCol)
{
    // triggerColがTriggerEventColliderであることを保証
    if (triggerCol->tag != ColliderTag::TRIGGER)
        return; // ありえないが、安全のためチェック

    TriggerEventCollider* trigger = static_cast<TriggerEventCollider*>(triggerCol);

    ColliderPair pair(dynamicCol, triggerCol);
    m_currentFramePairs->insert(pair, true); // 接触ペアを記録

    // 前のフレームには存在しない → 新規接触
    if (!m_lastFramePairs->contains(pair))
    {
        trigger->TriggerEnter(dynamicCol);
    }
}

void CollisionManager::TriggerOnExit(void)
{
    for (auto it = m_lastFramePairs->begin(); it != m_lastFramePairs->end(); ++it)
    {
        const ColliderPair& pair = it->key;

        if (!m_currentFramePairs->contains(pair))
        {
            const Collider* collider1 = pair.collider1;
            const Collider* collider2 = pair.collider2;

            TriggerEventCollider* trigger = nullptr;
            const Collider* activator = nullptr;

            if (collider1->tag == ColliderTag::TRIGGER)
            {
                trigger = static_cast<TriggerEventCollider*>(const_cast<Collider*>(collider1));
                activator = collider2;
            }
            else if (collider2->tag == ColliderTag::TRIGGER)
            {
                trigger = static_cast<TriggerEventCollider*>(const_cast<Collider*>(collider2));
                activator = collider1;
            }

            if (trigger)
            {
                trigger->TriggerExit(activator); // 接触終了イベントを発行
            }
        }
    }

    // ポインタをスワップ（ping-pong）
    std::swap(m_currentFramePairs, m_lastFramePairs);
    // フレーム終わりにペアを更新
    m_currentFramePairs->clear();

}

bool CollisionManager::IsPointInTriangle(XMVECTOR p, XMVECTOR a, XMVECTOR b, XMVECTOR c)
{
    XMVECTOR v0 = c - a;
    XMVECTOR v1 = b - a;
    XMVECTOR v2 = p - a;

    float dot00 = XMVectorGetX(XMVector3Dot(v0, v0));
    float dot01 = XMVectorGetX(XMVector3Dot(v0, v1));
    float dot02 = XMVectorGetX(XMVector3Dot(v0, v2));
    float dot11 = XMVectorGetX(XMVector3Dot(v1, v1));
    float dot12 = XMVectorGetX(XMVector3Dot(v1, v2));

    float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

    return (u >= 0) && (v >= 0) && (u + v <= 1);
}

void CollisionManager::ClosestPointOnTriangle(const XMFLOAT3& p, const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, XMFLOAT3& out)
{
    XMVECTOR P = XMLoadFloat3(&p);
    XMVECTOR A = XMLoadFloat3(&a);
    XMVECTOR B = XMLoadFloat3(&b);
    XMVECTOR C = XMLoadFloat3(&c);

    // 各辺をベクトルで表す
    XMVECTOR AB = B - A;
    XMVECTOR AC = C - A;
    XMVECTOR AP = P - A;

    // バリセンター座標で射影
    float d1 = XMVectorGetX(XMVector3Dot(AB, AP));
    float d2 = XMVectorGetX(XMVector3Dot(AC, AP));
    if (d1 <= 0.0f && d2 <= 0.0f) { XMStoreFloat3(&out, A); return; }

    // B頂点近傍
    XMVECTOR BP = P - B;
    float d3 = XMVectorGetX(XMVector3Dot(AB, BP));
    float d4 = XMVectorGetX(XMVector3Dot(AC, BP));
    if (d3 >= 0.0f && d4 <= d3) { XMStoreFloat3(&out, B); return; }

    // AB辺上での射影
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        float v = d1 / (d1 - d3);
        XMStoreFloat3(&out, A + v * AB);
        return;
    }

    // C頂点近傍
    XMVECTOR CP = P - C;
    float d5 = XMVectorGetX(XMVector3Dot(AB, CP));
    float d6 = XMVectorGetX(XMVector3Dot(AC, CP));
    if (d6 >= 0.0f && d5 <= d6) { XMStoreFloat3(&out, C); return; }

    // AC辺上での射影
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        float w = d2 / (d2 - d6);
        XMStoreFloat3(&out, A + w * AC);
        return;
    }

    // BC辺上での射影
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        XMStoreFloat3(&out, B + w * (C - B));
        return;
    }

    // 三角形内部（面上）
    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    XMStoreFloat3(&out, A + AB * v + AC * w);
}

float CollisionManager::ComputeWallCollisionEpsilon(const BOUNDING_BOX& box)
{
    float halfX = (box.maxPoint.x - box.minPoint.x) * 0.5f;
    float halfZ = (box.maxPoint.z - box.minPoint.z) * 0.5f;

    // 横方向の半径（XZ平面）を計算
    float horizontalRadius = sqrtf(halfX * halfX + halfZ * halfZ);

    // 浮動小数点誤差対策で少し余裕を追加
    return horizontalRadius + 0.1f;
}

void CollisionManager::RenderDebugInfo(void)
{
#if DRAW_BOUNDING_BOX
    if (m_drawBoundingBox)
    {
        XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        const BOUNDING_BOX& box = m_curCollisionBox;
        m_debugBoundingBoxRenderer.DrawBox(box, Camera::get_instance().GetViewProjMtx(), color);
        color.x = 0.0f;
        m_debugTriangleRenderer.DrawTriangles(m_debugTriangles, Camera::get_instance().GetViewProjMtx(), color);
    }
#endif
}
