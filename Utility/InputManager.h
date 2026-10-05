#pragma once
//=============================================================================
//
// プレイヤーの操作をぜーんぶ受け取る司令塔ちゃん [InputManager.h]
// Author : 
// キーボード・マウス・ゲームパッドの入力状態をまとめて管理する入力受付クラスですっ
// 押された瞬間や押しっぱなし、マウス移動・ホイール操作まで、ゲームとの橋渡し役なの
// 
//=============================================================================
#include "main.h"
#include "Utility/SingletonBase.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************

// プログラム分けするときに使う
#define	USE_KEYBOARD										// 宣言するとキーボードで操作可能になる
#define	USE_MOUSE											// 宣言するとマウスで操作可能になる
#define	USE_PAD												// 宣言するとパッドで操作可能になる


/* game pad情報 */
#define BUTTON_UP		0x00000001l	// 方向キー上(.IY<0)
#define BUTTON_DOWN		0x00000002l	// 方向キー下(.IY>0)
#define BUTTON_LEFT		0x00000004l	// 方向キー左(.IX<0)
#define BUTTON_RIGHT	0x00000008l	// 方向キー右(.IX>0)
#define BUTTON_A		0x00000010l	// Ａボタン(.rgbButtons[0]&0x80)
#define BUTTON_B		0x00000020l	// Ｂボタン(.rgbButtons[1]&0x80)
#define BUTTON_C		0x00000040l	// Ｃボタン(.rgbButtons[2]&0x80)
#define BUTTON_X		0x00000080l	// Ｘボタン(.rgbButtons[3]&0x80)
#define BUTTON_Y		0x00000100l	// Ｙボタン(.rgbButtons[4]&0x80)
#define BUTTON_Z		0x00000200l	// Ｚボタン(.rgbButtons[5]&0x80)
#define BUTTON_L		0x00000400l	// Ｌボタン(.rgbButtons[6]&0x80)
#define BUTTON_R		0x00000800l	// Ｒボタン(.rgbButtons[7]&0x80)
#define BUTTON_START	0x00001000l	// ＳＴＡＲＴボタン(.rgbButtons[8]&0x80)
#define BUTTON_M		0x00002000l	// Ｍボタン(.rgbButtons[9]&0x80)
#define GAMEPADMAX		4			// 同時に接続するジョイパッドの最大数をセット

//===============================
// 入力キー定義（固定バインディング）
//===============================
#define	NUM_KEY_MAX			(256)

// 一時停止（ポーズ）キー
#define KEY_PAUSE       DIK_P

// プレイヤー移動キー
#define KEY_MOVE_FORWARD   DIK_W
#define KEY_MOVE_BACKWARD  DIK_S
#define KEY_MOVE_LEFT      DIK_A
#define KEY_MOVE_RIGHT     DIK_D

// ダッシュ（走る）キー
#define KEY_RUN         DIK_LSHIFT

// マップ／クエストキー
#define KEY_MAP         DIK_M
#define KEY_QUEST       DIK_Q

// メニュー操作
#define KEY_CONFIRM			DIK_RETURN
#define KEY_CANCEL			DIK_ESCAPE

#define KEY_INTERACT				DIK_F       // インタラクトキー（ドア開ける、アイテム拾うなど）
#define KEY_SWITCH_WEAPON			DIK_Q		// 武器切り替えキー
#define KEY_ENTER_THROW_MODE		DIK_G 		// 投擲モードに入るキー
#define KEY_EXIT_THROW_MODE			DIK_R		// 投擲モードを抜けるキー
#define KEY_SWITCH_BULLET			DIK_TAB

//*****************************************************************************
// プロトタイプ宣言
//*****************************************************************************

class InputManager : public SingletonBase<InputManager>
{
public:
	HRESULT Init(HINSTANCE hInst, HWND hWnd);
	void Shutdown(void);
	void Update(void);

	//---------------------------- keyboard
	BOOL GetKeyboardPress(int nKey);
	BOOL GetKeyboardTrigger(int nKey);
	BOOL GetKeyboardRepeat(int nKey);
	BOOL GetKeyboardRelease(int nKey);

	//---------------------------- mouse
	BOOL IsMouseLeftPressed(void);      // 左クリックした状態
	BOOL IsMouseLeftTriggered(void);    // 左クリックした瞬間
	BOOL IsMouseRightPressed(void);     // 右クリックした状態
	BOOL IsMouseRightTriggered(void);   // 右クリックした瞬間
	BOOL IsMouseCenterPressed(void);    // 中クリックした状態
	BOOL IsMouseCenterTriggered(void);  // 中クリックした瞬間
	long GetMouseX(void);               // マウスがX方向に動いた相対値
	long GetMouseY(void);               // マウスがY方向に動いた相対値
	long GetMouseZ(void);               // マウスホイールが動いた相対値

	//---------------------------- game pad
	BOOL IsButtonPressed(int padNo, DWORD button);
	BOOL IsButtonTriggered(int padNo, DWORD button);

	void SetMousePosCenter(void);
	void SetMouseRecentered(BOOL recenter);
	BOOL IsMouseRecentered(void);

private:
	HRESULT InitKeyboard(HINSTANCE hInst, HWND hWnd);
	void UninitKeyboard(void);
	HRESULT UpdateKeyboard(void);

	HRESULT InitializeMouse(HINSTANCE hInst, HWND hWindow); // マウスの初期化
	void UninitMouse();						// マウスの終了処理
	HRESULT UpdateMouse();					// マウスの更新処理

	static BOOL SearchGamePadCallback(const LPDIDEVICEINSTANCE lpddi, LPVOID pContext);

	HRESULT InitializePad(void);			// パッド初期化
	//BOOL CALLBACK SearchPadCallback(LPDIDEVICEINSTANCE lpddi, LPVOID);	// パッド検査コールバック
	void UpdatePad(void);
	void UninitPad(void);

	//------------------------------- keyboard
	LPDIRECTINPUT8			m_pDInput = NULL;					// IDirectInput8インターフェースへのポインタ
	LPDIRECTINPUTDEVICE8	m_pDIDevKeyboard = NULL;			// IDirectInputDevice8インターフェースへのポインタ(キーボード)
	BYTE					m_keyState[NUM_KEY_MAX]{};			// キーボードの状態を受け取るワーク
	BYTE					m_keyStateTrigger[NUM_KEY_MAX]{};		// キーボードの状態を受け取るワーク
	BYTE					m_keyStateRepeat[NUM_KEY_MAX]{};		// キーボードの状態を受け取るワーク
	BYTE					m_keyStateRelease[NUM_KEY_MAX]{};		// キーボードの状態を受け取るワーク
	int						m_keyStateRepeatCnt[NUM_KEY_MAX]{};	// キーボードのリピートカウンタ

	//--------------------------------- mouse
	LPDIRECTINPUTDEVICE8 m_pMouse = NULL; // mouse

	DIMOUSESTATE2   m_mouseState{};		// マウスのダイレクトな状態
	DIMOUSESTATE2   m_mouseTrigger{};	// 押された瞬間だけON
	BOOL			m_isMouseRecentered = FALSE;
	//--------------------------------- game pad

	LPDIRECTINPUTDEVICE8	m_pGamePad[GAMEPADMAX] = { NULL,NULL,NULL,NULL };// パッドデバイス

	DWORD	m_padState[GAMEPADMAX]{};	// パッド情報（複数対応）
	DWORD	m_padTrigger[GAMEPADMAX]{};
	int		m_padCount = 0;			// 検出したパッドの数
};

