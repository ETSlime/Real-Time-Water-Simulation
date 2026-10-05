//=============================================================================
//
// [ProjectilePredictor.cpp]
// Author : 
// 
//=============================================================================
#include "Effects/ProjectilePreview/ProjectilePredictor.h"



ProjectilePredictor::ProjectilePredictor(float gravity) 
	: m_gravity(gravity), m_voxelGrid(nullptr) 
{
    m_trajectoryPoints.reserve(MAX_TRAJECTORY_POINTS);
}

void ProjectilePredictor::ComputeTrajectory(const XMFLOAT3& start, const XMFLOAT3& dir, float speed, 
    float maxTime, float stepTime, XMFLOAT3* outHitPos, XMFLOAT3* outHitNormal)
{
    if (!m_voxelGrid)
		SetCollisionVoxelGrid(CollisionManager::get_instance().GetVoxelGrid());

    m_trajectoryPoints.clear();
    if (outHitPos) *outHitPos = XMFLOAT3(0, 0, 0);
	if (outHitNormal) *outHitNormal = XMFLOAT3(0, 0, 0);

    XMFLOAT3 prevPos = start;
    bool hitFound = false;

    for (float t = 0.0f; t <= maxTime; t += stepTime) 
    {
        XMFLOAT3 pos;
        pos.x = start.x + dir.x * speed * t;
        pos.y = start.y + dir.y * speed * t - 0.5f * m_gravity * t * t;
        pos.z = start.z + dir.z * speed * t;

        // DDAでの衝突判定
        XMFLOAT3 hitPos, hitNormal;
        if (m_voxelGrid && m_voxelGrid->Raycast(prevPos, pos, &hitPos, &hitNormal))
        {
            m_trajectoryPoints.push_back({ hitPos, true, hitNormal });
            if (outHitPos) *outHitPos = hitPos;
			if (outHitNormal) *outHitNormal = hitNormal;
            hitFound = true;
            break;
        }
        else 
        {
            m_trajectoryPoints.push_back({ pos, false });
        }

        prevPos = pos;
    }
}

//bool ProjectilePredictor::AdvanceLaserLinear(const LaserParams& params, LaserState& state, float deltaTime, const BOUNDING_BOX* playerAABB, XMFLOAT3* outHitPos, XMFLOAT3* outHitNormal)
//{
//    if (!m_voxelGrid)
//        SetCollisionVoxelGrid(CollisionManager::get_instance().GetVoxelGrid());
//
//    if (outHitPos)    *outHitPos = { 0,0,0 };
//    if (outHitNormal) *outHitNormal = { 0,0,0 };
//
//    if (state.stopped) return;
//
//	// 1ステップの移動距離
//    const float stepDist = params.speed * deltaTime;
//
//	// 新しい線分の端点 p1 を計算
//    const float prevLen = state.traveled;
//    const float newLen = min(prevLen + stepDist, params.maxLength);
//
//	// 現在の線分 p0→p1 を計算
//    XMFLOAT3 p0 = Add(state.start, Scale(state.dir, prevLen));
//    XMFLOAT3 p1 = Add(state.start, Scale(state.dir, newLen));
//
//	// まずボクセル環境に対してレイキャスト（線分 vs ボクセル）
//    XMFLOAT3 hitPosEnv = { 0,0,0 }, hitNEnv = { 0,0,0 };
//    bool envHit = (m_voxelGrid && m_voxelGrid->Raycast(p0, p1, &hitPosEnv, &hitNEnv));
//
//	// 次にプレイヤー AABB に対してレイキャスト（線分 vs AABB）
//    bool playerHit = false;
//    XMFLOAT3 hitPosPl = { 0,0,0 }, hitNPl = { 0,0,0 };
//    float tAABB = 0.0f;
//    if (playerAABB)
//    {
//        if (SegmentAABBIntersect(p0, p1, playerAABB->minPoint, playerAABB->maxPoint, &tAABB, &hitNPl))
//        {
//            playerHit = true;
//            hitPosPl = Add(p0, Scale({ p1.x - p0.x, p1.y - p0.y, p1.z - p0.z }, tAABB));
//        }
//    }
//
//	// 環境とプレイヤーの両方で命中した場合、より近い方を採用
//    bool hit = false;
//    XMFLOAT3 finalHitPos = { 0,0,0 }, finalHitN = { 0,0,0 };
//    float hitDist = FLT_MAX;
//
//    if (envHit)
//    {
//        finalHitPos = hitPosEnv;
//        finalHitN = hitNEnv;
//		// p0 から命中点までの距離を計算
//        XMFLOAT3 v = { finalHitPos.x - p0.x, finalHitPos.y - p0.y, finalHitPos.z - p0.z };
//        hitDist = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
//        hit = true;
//    }
//
//    if (playerHit)
//    {
//        XMFLOAT3 v = { hitPosPl.x - p0.x, hitPosPl.y - p0.y, hitPosPl.z - p0.z };
//        float d = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
//        if (!hit || d < hitDist)
//        {
//            finalHitPos = hitPosPl;
//			finalHitN = hitNPl; // 法線はAABBの面法線
//            hitDist = d;
//            hit = true;
//        }
//    }
//
//	// 軌跡点を追加
//    auto pushPoint = [&](const XMFLOAT3& p, bool isHit, const XMFLOAT3& n = XMFLOAT3{ 0,0,0 }) {
//        TrajectoryPoint tp; tp.pos = p; tp.hit = isHit; tp.hitNormal = n; m_trajectoryPoints.push_back(tp);
//        };
//
//    // 初回は始点を追加
//    if (m_trajectoryPoints.empty())
//        pushPoint(p0, false);
//
//    const float segmentLen = (hit ? hitDist : (newLen - prevLen));
//    const float dirLen = (segmentLen > 1e-6f) ? segmentLen : 0.0f;
//
//    int numSteps = (dirLen > 1e-6f) ? (int)floorf(dirLen / params.pointSpacing) : 0;
//    XMFLOAT3 dseg = { state.dir.x * params.pointSpacing,
//                      state.dir.y * params.pointSpacing,
//                      state.dir.z * params.pointSpacing };
//
//    XMFLOAT3 cur = p0;
//    for (int i = 0; i < numSteps; ++i)
//    {
//        cur = Add(cur, dseg);
//        pushPoint(cur, false);
//    }
//
//    if (hit)
//    {
//		// 命中：補間して正確な命中点を追加
//        pushPoint(finalHitPos, true, finalHitN);
//        state.traveled += hitDist; 
//        state.stopped = true;     // 命中して停止
//        if (outHitPos)    *outHitPos = finalHitPos;
//        if (outHitNormal) *outHitNormal = finalHitN;
//    }
//    else
//    {
//		// 未命中：終点を追加
//        pushPoint(p1, false);
//        state.traveled = newLen;
//        if (state.traveled >= params.maxLength)
//			state.stopped = true; // 到達して停止
//    }
//
//    return playerHit;
//}
//
//bool ProjectilePredictor::SegmentAABBIntersect(const XMFLOAT3& p0, const XMFLOAT3& p1, const XMFLOAT3& bmin, const XMFLOAT3& bmax, float* outT, XMFLOAT3* outNormal)
//{
//    XMFLOAT3 d = { p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
//    float tmin = 0.0f, tmax = 1.0f;
//    XMFLOAT3 n = { 0,0,0 };
//
//    auto axis = [&](float p, float dp, float mn, float mx, XMFLOAT3 axN)->bool {
//		if (fabsf(dp) < 1e-6f) { // 平行
//            if (p < mn || p > mx) return false;
//            return true;
//        }
//        float ood = 1.0f / dp;
//        float t1 = (mn - p) * ood;
//        float t2 = (mx - p) * ood;
//        XMFLOAT3 n1 = axN, n2 = { -axN.x, -axN.y, -axN.z };
//        if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }
//        if (t1 > tmin) { tmin = t1; n = n1; }
//        if (t2 < tmax) { tmax = t2; }
//        if (tmin > tmax) return false;
//        return true;
//        };
//
//    if (!axis(p0.x, d.x, bmin.x, bmax.x, { -1,0,0 })) return false;
//    if (!axis(p0.y, d.y, bmin.y, bmax.y, { 0,-1,0 })) return false;
//    if (!axis(p0.z, d.z, bmin.z, bmax.z, { 0,0,-1 })) return false;
//
//    if (outT) *outT = tmin;
//    if (outNormal) *outNormal = n;
//    return true;
//}
