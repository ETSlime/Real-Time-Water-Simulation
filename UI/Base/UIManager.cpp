//=============================================================================
//
// UIElement処理 [UIElement.cpp]
// Author : 
//
//=============================================================================
#include "UI/Base/UIManager.h"
#include "UI/Base/UISpriteRenderer.h"
#include "UI/Base/ScreenFader.h"

void UIManager::Init(void)
{
    HRESULT hr = S_OK;

    // 頂点データを作成
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(UIVertex) * 4;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_renderer.GetDevice()->CreateBuffer(&bd, nullptr, &m_spriteVB);
    assert(SUCCEEDED(hr));

    m_overlay.Initialize(m_spriteVB);
    m_screenFader.Initialize();
}

void UIManager::Update(void)
{
	// UI要素の更新
    if (IsModalActive())
    {
		UIElement* topModal = GetTopModal();
		if (topModal)
		{
            // モーダルがアクティブな場合はモーダルだけ更新
            topModal->Update();
		}
    }
    else
    {
        for (auto& e : m_elements)
            e->Update();
    }

    m_screenFader.Update();

    if (m_activePrompt)
    {
		m_activePrompt->Update(); // アクティブなプロンプトがある場合は更新
        if (m_inputManager.GetKeyboardRelease(KEY_INTERACT))
        {
            m_activePrompt->Trigger();
            ClearActivePrompt(); // プロンプトをクリア
        }
    }
}

void UIManager::Draw(void)
{
    // Z順でソート（必要なら）
    UINT numUIElements = m_elements.getSize();
    for (UINT i = 1; i < numUIElements; ++i)
    {
        auto key = m_elements[i];
        int j = static_cast<int>(i) - 1;

        while (j >= 0 && m_elements[j]->GetZOrder() > key->GetZOrder())
        {
            m_elements[j + 1] = m_elements[j];
            --j;
        }

        m_elements[j + 1] = key;
    }

    // UI要素を描画
    for (auto& e : m_elements)
        e->Draw();

    if (m_activePrompt)
    {
		m_activePrompt->Draw(); // アクティブなプロンプトを描画
    }

	// モーダルが存在する場合はモーダルを描画
    for (UINT i = 0; i < m_modals.getSize(); ++i)
    {
        UIElement* modal = m_modals[i];

        if (modal->HasCustomOverlay())
        {
            // カスタムオーバーレイを描画
            modal->DrawCustomOverlay();
        }
        else if (modal->NeedsOverlay())
        {
            // 半透明の黒い背景
            DrawOverlay(
                SCREEN_CENTER_X, SCREEN_CENTER_Y,
                SCREEN_WIDTH, SCREEN_HEIGHT,
                { 0, 0, 0, 0.5f });
        }

        modal->Draw();
    }

    m_screenFader.Draw();
}

void UIManager::AddElement(UIElement* element)
{
    if (element == nullptr)
        return;

    for (auto* e : m_elements)
    {
        if (e == element)
            return;
    }

    m_elements.push_back(element);
}

void UIManager::PopModal(void)
{
    if (!m_modals.empty())
    {
		UIElement* modal = m_modals.back();

        SimpleArray<ISpriteTransformable*> targets;
        modal->CollectTweenTargets(targets);
		for (auto* target : targets)
            m_tweenManager.RemoveTweensForTarget(target); // TweenManager に通知して、対象の Tween を全部消す
		
        // モーダルの解放
        SAFE_DELETE(modal);
        m_modals.pop_back();
    }
}

void UIManager::PopAllModals(void)
{
    for (auto* modal : m_modals)
    {
        SimpleArray<ISpriteTransformable*> targets;
        modal->CollectTweenTargets(targets);
        for (auto* target : targets)
            m_tweenManager.RemoveTweensForTarget(target); // TweenManager に通知して、対象の Tween を全部消す
		
        // モーダルの解放
        SAFE_DELETE(modal);
    }

    m_modals.clear();
}

void UIManager::DrawOverlay(float x, float y, float w, float h, XMFLOAT4 color)
{
    m_overlay.Draw(m_renderer.GetDeviceContext(), x, y, w, h, color);
}

void UIManager::SetActivePrompt(UIInteractPrompt* prompt)
{
    m_activePrompt = prompt;
    if (prompt)
    {
        prompt->Show();
    }
}

void UIManager::ClearActivePrompt(void)
{
    if (m_activePrompt)
    {
        m_activePrompt->Hide();
        m_activePrompt = nullptr;
    }
}

void UIManager::Clear(void)
{
    for (UINT i = 0; i < m_elements.getSize(); ++i)
    {
        SAFE_DELETE(m_elements[i]); // 明示的解放
    }

    m_elements.clear();
}

UIOverlayRenderer& UIManager::GetOverlayRenderer()
{
    return m_overlay;
}
