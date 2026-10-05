#pragma once
//=============================================================================
//
// 一時停止の魔法をかけるモーダルちゃん [PauseModal.h]
// Author : 
// ポーズ中に画面全体をふんわりオーバーレイで包んでくれる可愛いUIクラスですっ
// 背景をやさしく遮って、まるで時間が止まったみたいな演出をしてくれるの
// 
//=============================================================================
#include "UI/Base/UIElement.h"
#include "UI/Base/UIManager.h"

class PauseModal : public UIElement
{
public:
    PauseModal() 
    {
        //m_customOverlay = true; // オーバーレイを使用する
    }

    void Update(void) override {} // 無処理
    void Draw(void) override {}
    bool NeedsOverlay(void) const override { return true; } // オーバーレイが必要
    void DrawCustomOverlay(void) const override
    {
        UIManager::get_instance().DrawOverlay(SCREEN_CENTER_X, SCREEN_CENTER_Y, SCREEN_WIDTH, SCREEN_HEIGHT, { 1.0f, 0.7f, 0.9f, 0.4f });
    }
};

