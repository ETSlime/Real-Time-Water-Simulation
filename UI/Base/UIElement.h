#pragma once
//=============================================================================
//
// ぜんぶのUIを優しく包む基底クラスちゃん [UIElement.h]
// Author : 
// 位置・サイズ・Z順・ホバー・オーバーレイなど、UIに必要な基本機能を提供する抽象クラスですっ
// 全てのUIちゃんたちの土台になって、しずかに見守る頼れるお姉さんタイプなの
//
//=============================================================================
#include "Core/Timer.h"
#include "Core/Graphics/Renderer.h"
#include "UI/Base/UISpriteRenderer.h"
#include "UI/Base/UISprite.h"

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct UVRect
{
    float left;
    float top;
    float right;
    float bottom;
}; // UV矩形構造体

class UIElement
{
public:
    virtual ~UIElement() { m_alive = false; }

    virtual void Update(void) = 0;
    virtual void Draw(void) = 0;

    virtual void OnHover(void) {}
    virtual void OnUnhover(void) {}

    void SetZOrder(int z) { m_zOrder = z; }
    int  GetZOrder(void) const { return m_zOrder; }

    bool IsMouseOver(float mouseX, float mouseY) const { return false; }
    bool IsValid() const { return m_alive; }

    virtual bool NeedsOverlay(void) const { return HasCustomOverlay(); }
    virtual bool HasCustomOverlay(void) const { return m_customOverlay; }
    virtual void DrawCustomOverlay(void) const {}

    // TweenManager用に、Tween対象のISpriteTransformableを集める
	virtual void CollectTweenTargets(SimpleArray<ISpriteTransformable*>& outTargets) const {}

protected:
    int m_zOrder = 0;
    bool m_alive = true;
    bool m_isHovering = false;
    bool m_customOverlay = false;
};