#pragma once
//=============================================================================
//
// [ProjectileFollowerManager.h]
// Author : 
// 
//  複数投擲物フォロワ管理
// 
//=============================================================================
#include "Effects/ProjectilePreview/ArcLengthPathFollower.h"
#include "Scene/GameObject.h"
#include "Scene/Item/Item.h"
#include "Core/AudioManager.h"

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct ProjectileSlot 
{
    bool active = false;
    ArcLengthPathFollower follower;
    float speed = 0.0f;                  // m/s
    Item* entity = nullptr; // 外部所有
}; // スロット（1発の投擲物インスタンスを表す）

class ProjectileFollowerManager : public SingletonBase<ProjectileFollowerManager>
{
public:
    // 最大同時発射数を想定してプール確保
    void Initialize(UINT capacity = 8) 
    { 
        m_slots.reserve(capacity);
        for (UINT i = 0; i < capacity; ++i) 
        {
            ProjectileSlot slot;        // デフォルトコンストラクタで active=false
            m_slots.push_back(slot);    // 実際に要素を構築
        }
        m_initialized = true; 
    }

    // --------------------------------------------
    //  新規スポーン：空きスロットに割り当て
    //  - predicted: 予測軌跡（命中で打ち切る）
    //  - speed: 追従速度
    //  - entity: 表示/当たり判定を持つゲームオブジェクト
    //  戻り値: 成功/失敗
    // --------------------------------------------
    bool Spawn(const SimpleArray<TrajectoryPoint>& predicted,
        float speed,
        Item* entity)
    {
        if (!m_initialized)
            Initialize();

        ProjectileSlot* slot = FindFreeSlot();
        if (!slot) return false;

        slot->follower.ResetFromTrajectory(predicted);
        if (slot->follower.IsFinished()) return false; // 無効軌跡

        slot->speed = speed;
        slot->entity = entity;
        slot->active = true;
        return true;
    }

    // --------------------------------------------
    //  毎フレーム更新：全スロットを前進
    // --------------------------------------------
    void UpdateAll(void)
    {
        for (auto& s : m_slots) 
        {
            if (!s.active || !s.entity) continue;

            // 既に破棄済み
            if (s.entity->GetDestroy())
            {
				// スロットを無効化
				s.active = false;
                s.entity = nullptr;
                continue;
            }

            XMFLOAT3 pos, dir;
            s.follower.Advance(s.speed, &pos, &dir);

            // 位置適用
            s.entity->SetPosition(pos);

            // 向き（前方=dir）からオイラー角を算出して適用
            ApplyEulerFromDir(dir, s.entity);

            // 終了判定
            if (s.follower.IsFinished()) 
            {
                // 爆発/消滅など
                s.entity->PlayEffect(HitColliderType::Environment, s.follower.GetHitNormal());
                s.entity->SetDestroy(true);
                s.active = false;
                s.entity = nullptr;
                AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_boom_long);
            }
        }
    }

private:
    SimpleArray<ProjectileSlot> m_slots;

    ProjectileSlot* FindFreeSlot(void) 
    {
        for (auto& s : m_slots) if (!s.active) return &s;
        return nullptr; // 枠不足（必要なら自動拡張）
    }

    // 進行方向ベクトル→オイラー角(Yaw,Pitch,Roll=0) の簡易適用
    static void ApplyEulerFromDir(const XMFLOAT3& dir, Item* entity)
    {
        // forward = dir, up = (0,1,0) 基準
        XMVECTOR f = XMVector3Normalize(XMLoadFloat3(&dir));
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);

        // 直交基底
        XMVECTOR r = XMVector3Normalize(XMVector3Cross(up, f));
        XMVECTOR u = XMVector3Cross(f, r);

        // 回転行列（行ベクトル）
        XMMATRIX R(r, u, f, XMVectorSet(0, 0, 0, 1));

        // オイラー角抽出（Yaw-Pitch-Roll 想定）
        XMFLOAT4X4 m; XMStoreFloat4x4(&m, R);
        XMFLOAT3 euler;
        euler.y = atan2f(m._13, m._33);  // Yaw
        euler.x = asinf(-m._23);         // Pitch
        euler.z = 0.0f;                  // Roll は任意（必要なら算出）

        entity->SetRotation(euler);
    }

    bool m_initialized = false;
};