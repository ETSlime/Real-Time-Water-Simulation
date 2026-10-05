//=============================================================================
//
// [UIInteractPrompt.cpp]
// Author : 
// 
//=============================================================================
#include "UI/Base/UIInteractPrompt.h"
#include "UI/Base/UIManager.h"
#include "Core/TextureMgr.h"

void UIInteractPrompt::Initialize(const char* iconPath)
{
	m_icon = new UISprite(
		UIManager::get_instance().GetSpriteVertexBuffer(),
		TextureMgr::get_instance().CreateTexture(const_cast<char*>(iconPath))
	);

	if (m_icon)
	{
		m_icon->SetDrawMode(SpriteDrawMode::CenterWithRotation); // ’†SŠî€{‰ñ“]
		
		m_icon->SetPosition({ SCREEN_CENTER_X + 120.0f, SCREEN_CENTER_Y });

		m_icon->SetScale({ 2.5f, 2.5f });

		m_icon->SetRotation(XM_PI / 12.0f);

		m_icon->SetColor({ 1, 1, 1, 1 });
	}

}

void UIInteractPrompt::Update(void)
{
}
void UIInteractPrompt::Draw(void)
{
	m_icon->Draw();
}

void UIInteractPrompt::SetCallback(InteractCallback cb, void* userData)
{
	m_callback = cb;
	m_userData = userData;
}

void UIInteractPrompt::Trigger(void)
{
	if (m_callback)
		m_callback(m_userData);
}
