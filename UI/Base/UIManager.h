#pragma once
//=============================================================================
//
// すべてのUIちゃんを統括する管理お姉さま [UIManager.h]
// Author : 
// UI要素をまとめて管理・描画・更新し、モーダルの制御やオーバーレイ描画もしてくれる頼れるクラスですっ！
// UI界の女王さまみたいな存在で、みんながスムーズに動けるように支えてくれてるの~
//
//=============================================================================
#include "main.h"
#include "Utility/SimpleArray.h"
#include "Utility/SingletonBase.h"
#include "Utility/InputManager.h"
#include "UI/Base/UIElement.h"
#include "UI/Base/UIOverlayRenderer.h"
#include "UI/Base/ScreenFader.h"
#include "UI/Base/TweenManager.h"
#include "UI/Base/UIInteractPrompt.h"

class UIManager : public SingletonBase<UIManager>
{
public:
    UIManager() {}

    void Init(void);
    void Shutdown(void) {Clear();}
    void Update(void);
    void Draw(void);
    void Clear(void);
    void AddElement(UIElement* element);

    template<typename T, typename... Args>
    T* PushModal(Args&&... args)
    {
        auto* modal = new T(std::forward<Args>(args)...);
        m_modals.push_back(modal);
        return modal;
    }

    void PopModal(void);
	void PopAllModals(void);
    bool IsModalActive(void) const { return !m_modals.empty(); }
    UIElement* GetTopModal(void) const { return m_modals.empty() ? nullptr : m_modals.back(); }

    void DrawOverlay(float x, float y, float w, float h, XMFLOAT4 color);
    ID3D11Buffer* GetSpriteVertexBuffer(void) const { return m_spriteVB; }
    ScreenFader& GetFader(void) { return m_screenFader; }

	// アクティブなインタラクションプロンプトを設定
    void SetActivePrompt(UIInteractPrompt* prompt);
    void ClearActivePrompt(void);

    bool IsFading(void) { return m_screenFader.IsFading(); }

    UIOverlayRenderer& GetOverlayRenderer();
    Renderer& GetRenderer() { return m_renderer; }

private:
    SimpleArray<UIElement*> m_elements; // UI要素の配列
    SimpleArray<UIElement*> m_modals; // モーダルUIの保持
    UIOverlayRenderer m_overlay; // UIオーバーレイ描画
    ScreenFader m_screenFader;
    ID3D11Buffer* m_spriteVB = nullptr;
    UIInteractPrompt* m_activePrompt = nullptr;
	TweenManager& m_tweenManager = TweenManager::get_instance();
    Renderer& m_renderer = Renderer::get_instance();
	InputManager& m_inputManager = InputManager::get_instance();
};