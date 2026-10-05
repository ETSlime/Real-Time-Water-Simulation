#pragma once
//=============================================================================
//
// [ArcLengthPathFollower.h]
// Author : 
// 
// Arc-length ベースの軌跡追従（物理積分なし）
//  - 予測済みの TrajectoryPoint 列を折れ線として扱い
//  - 弧長テーブルを構築し、s(t)=speed*t で前進
//  - s の位置に対して線形補間で現在位置を取得
// 
//=============================================================================
#include "Effects/ProjectilePreview/ProjectilePredictor.h"

class ArcLengthPathFollower
{
public:
    ArcLengthPathFollower() = default;

    // --------------------------------------------
    //  予測軌跡から内部データをリセット＆構築
    //  - 命中点があればそこで打ち切り
    //  - 弧長テーブル（累積長）を作成
    // --------------------------------------------
    void ResetFromTrajectory(const SimpleArray<TrajectoryPoint>& srcPoints)
    {
        m_points.clear();
        m_cumLen.clear();
        m_totalLen = 0.0f;
        m_s = 0.0f;
        m_finished = false;

        // 軌跡コピー（命中点で打ち切り）
        m_points.reserve(srcPoints.getSize());
        for (UINT i = 0; i < srcPoints.getSize(); ++i)
        {
            m_points.push_back(srcPoints[i]);
            if (srcPoints[i].hit)
            {
				m_hitNormal = srcPoints[i].hitNormal;
                break;
            }
        }

        if (m_points.getSize() < 2)
        {
            m_finished = true;
            return;
        }

        // 累積弧長を構築
        m_cumLen.reserve(m_points.getSize());
        m_cumLen.setSize(m_points.getSize());
        m_cumLen[0] = 0.0f;
        for (UINT i = 1; i < m_points.getSize(); ++i)
        {
            m_cumLen[i] = m_cumLen[i - 1] + Length3(m_points[i - 1].pos, m_points[i].pos);
        }
        m_totalLen = m_cumLen.back();
        if (m_totalLen <= 1e-6f) 
            m_finished = true;
    }

    // --------------------------------------------
    //  前進更新：s += speed * dt
    //  - outPos: 現在位置（補間）
    //  - outDir: 進行方向（現在区間の接線）
    // --------------------------------------------
    void Advance(float speed, XMFLOAT3* outPos, XMFLOAT3* outDir)
    {
        if (m_finished || m_points.getSize() < 2)
        {
            if (outPos) *outPos = m_points.empty() ? XMFLOAT3(0, 0, 0) : m_points.back().pos;
            if (outDir) *outDir = XMFLOAT3(0, 1, 0);
            return;
        }

        // 弧長を前進
        m_s += max(0.0f, speed) * max(0.0f, Timer::get_instance().GetDeltaTime() * 30);
        if (m_s >= m_totalLen)
        {
            m_s = m_totalLen;
            m_finished = true;
        }

        // 現在 s が属する区間 [i, i+1] を二分探索で特定
        UINT lo = 0, hi = m_cumLen.getSize() - 1;
        while (lo + 1 < hi)
        {
            UINT mid = (lo + hi) / 2;
            if (m_cumLen[mid] <= m_s) lo = mid; else hi = mid;
        }
        const UINT i = lo;
        const float segLen = max(1e-6f, m_cumLen[i + 1] - m_cumLen[i]);
        const float alpha = (m_s - m_cumLen[i]) / segLen;

        // 位置は線形補間、方向は区間の接線
        const XMFLOAT3& A = m_points[i].pos;
        const XMFLOAT3& B = m_points[i + 1].pos;

        if (outPos) *outPos = Lerp3(A, B, alpha);
        if (outDir) *outDir = DirNorm(A, B);
    }

    //  状態取得
    bool   IsFinished(void) const { return m_finished; }
    float  GetTotalLength(void) const { return m_totalLen; }
    float  GetProgressS(void) const { return m_s; }
	XMFLOAT3 GetHitNormal(void) const { return m_hitNormal; }

private:
    SimpleArray<TrajectoryPoint> m_points; // 凍結した軌跡（命中で打ち切り）
    SimpleArray<float>           m_cumLen; // 累積弧長
    float                        m_totalLen = 0.0f; // 総弧長
    float                        m_s = 0.0f; // 現在の弧長
    bool                         m_finished = true; // 完了フラグ
	XMFLOAT3                     m_hitNormal = { 0,1,0 }; // 命中面の法線
};