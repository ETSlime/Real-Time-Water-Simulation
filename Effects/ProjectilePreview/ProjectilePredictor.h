#pragma once
//=============================================================================
//
// [ProjectilePredictor.h]
// Author : 
// 
//=============================================================================
#include "Collision/CollisionManager.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define     MAX_TRAJECTORY_POINTS 5000 // 軌跡点の最大数

//*****************************************************************************
// 構造体定義
//*****************************************************************************

// 軌跡点データ
struct TrajectoryPoint 
{
    XMFLOAT3 pos;  // 位置
    bool hit;      // 命中点かどうか
	XMFLOAT3 hitNormal; // 命中法線
};

class ProjectilePredictor 
{
public:
    ProjectilePredictor(float gravity = 1.0f);

    // 軌跡を計算して点リストを返す
    void ComputeTrajectory(
        const XMFLOAT3& start,
        const XMFLOAT3& dir,
        float speed,
        float maxTime,
        float stepTime,
        XMFLOAT3* outHitPos,
        XMFLOAT3* outHitNormal);

	// レーザーの進行を更新
    //bool AdvanceLaserLinear(const LaserParams& params,
    //    LaserState& state,
    //    float deltaTime,
    //    const BOUNDING_BOX* playerAABB,
    //    XMFLOAT3* outHitPos,
    //    XMFLOAT3* outHitNormal);

    void SetCollisionVoxelGrid(const VoxelGrid* grid) { m_voxelGrid = grid; }

    // 軌跡データを取得
    const SimpleArray<TrajectoryPoint>& GetTrajectoryPoints(void) const { return m_trajectoryPoints; }

private:
    // 線分とAABBの交差判定
  //  bool SegmentAABBIntersect(const XMFLOAT3& p0, const XMFLOAT3& p1,
  //      const XMFLOAT3& bmin, const XMFLOAT3& bmax,
		//float* outT, XMFLOAT3* outNormal);

    SimpleArray<TrajectoryPoint> m_trajectoryPoints;
    float m_gravity;
    const VoxelGrid* m_voxelGrid;

};