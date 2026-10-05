//=============================================================================
//
// カーソルの種類・表示状態・位置を制御する入力補助クラス [CursorManager.h]
// Author : 
// ゲーム中のカーソル表示切替、種類設定、座標記録・復元を統括し、
// UI専用モードや一時表示など多様な操作状態に対応する
//
//=============================================================================
#pragma once
#include "main.h"
#include "Utility/SingletonBase.h"
#include "Utility/InputManager.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************

// 使用可能なカーソルタイプ
enum class CursorType
{
    Default,    // 標準の矢印カーソル
    Custom,     // きゃわカスタムカーソル
};

// カーソルのモード
enum class CursorMode
{
    Hidden,         // カーソル非表示
    VisibleTemp,    // 一時的にカーソル表示
    UIExclusive,	// UI専用カーソル表示
};

class CursorManager : public SingletonBase<CursorManager>

{
public:

    // カーソルの読み込み
    void Init(void);

    // カーソルの表示/非表示
    void Show(void);
    void Hide(void);

    // カーソルを設定（SetCursor）
    void SetCursorType(CursorType type);

    // 現在のカーソルを取得（WM_SETCURSOR 用）
    HCURSOR GetCurrentCursor(void) const;

    // 現在のカーソルタイプを取得
    CursorType GetCurrentCursorType(void) const;

    // ゲーム再開前に現在のカーソル位置を記録
    void RememberCursorPosition(void);

    // ゲーム一時停止時に前回位置へ戻す
    void RestoreLastCursorPosition(void);

    // 一時的にマウスが自由に操作できる状態かどうかを判定する
    bool IsMouseFreeMode() const;

    void RestoreCursorAndEnterUIExclusive(void);

    void EnterUIHidden(void);
    void EnterUIExclusive(void);

    CursorMode GetCursorMode() const;

    POINT GetLastCursorPos() const
	{
		return m_lastCursorPos;
	}

    void OnEnterVisibleTemp();
    void OnExitVisibleTemp();

private:

    HCURSOR m_defaultCursor = nullptr;
    HCURSOR m_customCursor = nullptr;

    HCURSOR m_currentCursor = nullptr;
    CursorType m_cursorType = CursorType::Default;

    // カーソルの表示状態
    bool m_isCursorVisible = true;

    CursorMode m_mode = CursorMode::Hidden;

    // マウスの位置
    POINT m_lastCursorPos;

	InputManager& m_inputManager = InputManager::get_instance();
};