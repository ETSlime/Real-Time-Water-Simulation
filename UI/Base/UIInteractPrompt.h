#pragma once
//=============================================================================
//
// [UIInteractPrompt .h]
// Author : 
// 
//=============================================================================
#include "UI/Base/UIElement.h"

using InteractCallback = void(*)(void* userData);

class UIInteractPrompt : public UIElement, public ISpriteUI
{
public:

    UIInteractPrompt() {};
    ~UIInteractPrompt() {};

    void Initialize(const char* iconPath);
    void Update(void) override;
    void Draw(void) override;

    
    void Show(void) { m_icon->SetVisible(true); }
    void Hide(void) { m_icon->SetVisible(false); }
    //void SetFollowWorldPosition(const XMFLOAT3& worldPos); 

	// コールバック設定
    void SetCallback(InteractCallback cb, void* userData);
    void Trigger(void);

    // ISpriteUI インターフェースの実装
    DELEGATE_SPRITE_ALL_INTERFACE(m_icon);

private:
    UISprite* m_icon = nullptr;
    float m_alpha = 1.0f;
    bool m_followWorldPos = false;
    XMFLOAT3 m_worldPos;

    InteractCallback m_callback = nullptr;
    void* m_userData = nullptr;
};