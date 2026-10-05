#pragma once
//=============================================================================
//
// [EventTrigger.h]
// Author : 
// 
//=============================================================================
#include "Collision/Collider.h"
#include "Scene/Scene.h"
#include "Core/Camera.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define DRAW_BOUNDING_BOX       1

class IEventBehavior
{
public:
    virtual ~IEventBehavior() = default;
    virtual void Initialize(void) {}
    // トリガーされた時の動作
    virtual void OnTrigger(const Collider* activator) = 0;
    virtual void OnExit(const Collider* activator) {}
    virtual void SetTriggerAABB(BOUNDING_BOX* aabb) {}
};


class EventTrigger : public ISceneEntity, IDebugUI
{
public:
    EventTrigger()
    {
        m_trigger.tag = ColliderTag::TRIGGER;
        m_trigger.owner = this;

        // トリガーコールバックを登録
        m_trigger.SetTriggerCallback(StaticTriggerEnter, StaticTriggerExit, this);
    }

    void Init(void)
    {
        m_trigger.enable = true;

#ifdef _DEBUG
        DebugProc::get_instance().Register(this);
#endif // DEBUG

#if DRAW_BOUNDING_BOX
        m_debugRenderer.Initialize();
#endif
    }

    virtual void Destroy(void) override
    {
        CollisionManager::get_instance().UnregisterEventCollider(&m_trigger);
    }
		

    void SetBoundingBox(const BOUNDING_BOX& box)
    {
        m_trigger.aabb = box;
    }

    void SetEventBehavior(IEventBehavior* behavior)
    {
        m_behavior = behavior;
		if (m_behavior)
		{
			m_behavior->Initialize();
            m_behavior->SetTriggerAABB(&m_trigger.aabb);
		}
    }

    TriggerEventCollider* GetCollider()
    {
        return &m_trigger;
    }


    void Update() override
    {
        // 通常は何もしない（移動等があれば可）
    }

    void Draw(void) override
    {
    }

    static void StaticTriggerEnter(const Collider* activator, void* context)
    {
        EventTrigger* self = static_cast<EventTrigger*>(context);
        if (self->m_behavior)
            self->m_behavior->OnTrigger(activator);
    }

    static void StaticTriggerExit(const Collider* activator, void* context)
    {
        EventTrigger* self = static_cast<EventTrigger*>(context);
        if (self->m_behavior)
            self->m_behavior->OnExit(activator);
    }

private:
    virtual void RenderDebugInfo(void) override
    {
#ifdef _DEBUG
        if (m_drawBoundingBox)
        {
            m_debugRenderer.DrawBox(m_trigger.aabb, Camera::get_instance().GetViewProjMtx(), { 1.0f, 0.0f, 1.0f, 1.0f }); // 紫色で表示
        }
#endif // DEBUG
    }

    TriggerEventCollider m_trigger;
    bool m_drawBoundingBox = true;
    DebugBoundingBoxRenderer m_debugRenderer; // 調整用
    IEventBehavior* m_behavior = nullptr;    // イベント本体（ポリモーフィズム）
};


