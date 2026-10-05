//=============================================================================
//
// CursorManager処理 [CursorManager.h]
// Author : 
//
//=============================================================================
#include "Core/CursorManager.h"
#include "Utility/InputManager.h"

void CursorManager::Init(void)
{
    // デフォルトカーソル読み込み（システム）
    m_defaultCursor = LoadCursor(nullptr, IDC_ARROW);

    // オリジナルカーソル読み込み（.cur ファイル）
    m_customCursor = LoadCursorFromFileA("data/TEXTURE/cursor.cur");

    m_currentCursor = m_customCursor;
    m_cursorType = CursorType::Custom;

    m_inputManager.SetMousePosCenter(); // カメラ用：マウス位置を画面の中心に設定
    GetCursorPos(&m_lastCursorPos); // カーソル位置を保存
    ShowCursor(FALSE);  // カーソル非表示
}

void CursorManager::Show(void)
{
    // カーソルを表示する
    while (ShowCursor(TRUE) < 0);
    m_isCursorVisible = true;
}

void CursorManager::Hide(void)
{
    // カーソルを非表示にする
	while (ShowCursor(FALSE) >= 0);
	m_isCursorVisible = false;
}

void CursorManager::SetCursorType(CursorType type)
{
    m_cursorType = type;

    switch (type)
    {
    case CursorType::Default:
        m_currentCursor = m_defaultCursor;
        break;
    case CursorType::Custom:
        m_currentCursor = m_customCursor;
        break;
    }

    SetCursor(m_currentCursor); // すぐに反映（ただし WM_SETCURSOR も要対応）
}

HCURSOR CursorManager::GetCurrentCursor(void) const
{
    return m_currentCursor;
}

CursorType CursorManager::GetCurrentCursorType(void) const
{
    return m_cursorType;
}

void CursorManager::RememberCursorPosition(void)
{
    GetCursorPos(&m_lastCursorPos);
}

void CursorManager::RestoreLastCursorPosition(void)
{
    // マウスが自由に操作できる状態（UI操作中など）の場合は、
    // 無理にマウス位置を復元しない（ジャンプを防ぐ）
    if (IsMouseFreeMode())
        return;

    // カーソル位置を復元
    SetCursorPos(m_lastCursorPos.x, m_lastCursorPos.y);
}

bool CursorManager::IsMouseFreeMode() const
{
    // カーソルモードが UIExclusive または VisibleTemp の場合
    return m_mode == CursorMode::VisibleTemp || m_mode == CursorMode::UIExclusive;
}

void CursorManager::RestoreCursorAndEnterUIExclusive(void)
{
    RestoreLastCursorPosition(); // カーソル位置を復元
    EnterUIExclusive(); // UI専用モードに入る
}

void CursorManager::EnterUIExclusive(void)
{
    if (m_mode == CursorMode::UIExclusive) return;

    m_mode = CursorMode::UIExclusive; // カーソルモードを UIExclusive に変更
    Show(); // カーソルを表示
}

void CursorManager::EnterUIHidden(void)
{
    if (m_mode == CursorMode::Hidden) return;

	m_mode = CursorMode::Hidden; // カーソルモードを Hidden に変更
	Hide(); // カーソルを非表示
}

CursorMode CursorManager::GetCursorMode(void) const
{
    return m_mode;
}

void CursorManager::OnEnterVisibleTemp(void)
{
    if (m_mode == CursorMode::VisibleTemp) return;

    
    RestoreLastCursorPosition(); // カーソル位置を復元
    Show(); // カーソルを表示
    m_mode = CursorMode::VisibleTemp;
}

void CursorManager::OnExitVisibleTemp(void)
{
	if (m_mode != CursorMode::VisibleTemp) return;

    RememberCursorPosition(); // カーソル位置を保存
	Hide(); // カーソルを非表示
    m_inputManager.SetMouseRecentered(true); // カメラ用：次のフレームでマウス位置リセットを要求
	m_mode = CursorMode::Hidden;
}

