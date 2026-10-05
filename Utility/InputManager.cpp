//=============================================================================
//
// 入力処理 [InputManager.cpp]
// Author : 
//
//=============================================================================
#include "main.h"
#include "Utility/InputManager.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
// game pad用設定値
#define DEADZONE		2500			// 各軸の25%を無効ゾーンとする
#define RANGE_MAX		1000			// 有効範囲の最大値
#define RANGE_MIN		-1000			// 有効範囲の最小値


//=============================================================================
// 入力処理の初期化
//=============================================================================
HRESULT InputManager::Init(HINSTANCE hInst, HWND hWnd)
{
	HRESULT hr;

	if(!m_pDInput)
	{
		// DirectInputオブジェクトの作成
		hr = DirectInput8Create(hInst, DIRECTINPUT_VERSION,
									IID_IDirectInput8, (void**)&m_pDInput, NULL);
	}

	// キーボードの初期化
	InitKeyboard(hInst, hWnd);

 	// マウスの初期化
	InitializeMouse(hInst, hWnd);
	
	// パッドの初期化
	InitializePad();

	return S_OK;
}

//=============================================================================
// 入力処理の終了処理
//=============================================================================
void InputManager::Shutdown(void)
{
	// キーボードの終了処理
	UninitKeyboard();

	// マウスの終了処理
	UninitMouse();

	// パッドの終了処理
	UninitPad();

	if(m_pDInput)
	{
		m_pDInput->Release();
		m_pDInput = NULL;
	}
}

//=============================================================================
// 入力処理の更新処理
//=============================================================================
void InputManager::Update(void)
{
	if (!GetWindowActive()) return; // 非アクティブなら入力処理をスキップ

	// キーボードの更新
	UpdateKeyboard();
	
	// マウスの更新
	UpdateMouse();
	
	// パッドの更新
	UpdatePad();

}

//=============================================================================
// キーボードの初期化
//=============================================================================
HRESULT InputManager::InitKeyboard(HINSTANCE hInst, HWND hWnd)
{
	HRESULT hr;

	// デバイスオブジェクトを作成
	hr = m_pDInput->CreateDevice(GUID_SysKeyboard, &m_pDIDevKeyboard, NULL);
	if(FAILED(hr) || m_pDIDevKeyboard == NULL)
	{
		MessageBox(hWnd, "キーボードがねぇ！", "警告！", MB_ICONWARNING);
		return hr;
	}

	// データフォーマットを設定
	hr = m_pDIDevKeyboard->SetDataFormat(&c_dfDIKeyboard);
	if(FAILED(hr))
	{
		MessageBox(hWnd, "キーボードのデータフォーマットを設定できませんでした。", "警告！", MB_ICONWARNING);
		return hr;
	}

	// 協調モードを設定（フォアグラウンド＆非排他モード）
	hr = m_pDIDevKeyboard->SetCooperativeLevel(hWnd, (DISCL_FOREGROUND | DISCL_NONEXCLUSIVE));
	if(FAILED(hr))
	{
		MessageBox(hWnd, "キーボードの協調モードを設定できませんでした。", "警告！", MB_ICONWARNING);
		return hr;
	}

	// キーボードへのアクセス権を獲得(入力制御開始)
	m_pDIDevKeyboard->Acquire();

	return S_OK;
}

//=============================================================================
// キーボードの終了処理
//=============================================================================
void InputManager::UninitKeyboard(void)
{
	if(m_pDIDevKeyboard)
	{
		m_pDIDevKeyboard->Release();
		m_pDIDevKeyboard = NULL;
	}
}

//=============================================================================
// キーボードの更新
//=============================================================================
HRESULT InputManager::UpdateKeyboard(void)
{
	HRESULT hr;
	BYTE keyStateOld[256];

	// 前回のデータを保存
	memcpy(keyStateOld, m_keyState, NUM_KEY_MAX);

	// デバイスからデータを取得
	hr = m_pDIDevKeyboard->GetDeviceState(sizeof(m_keyState), m_keyState);
	if(SUCCEEDED(hr))
	{
		for(int cnt = 0; cnt < NUM_KEY_MAX; cnt++)
		{
			m_keyStateTrigger[cnt] = (keyStateOld[cnt] ^ m_keyState[cnt]) & m_keyState[cnt];
			m_keyStateRelease[cnt] = (keyStateOld[cnt] ^ m_keyState[cnt]) & ~m_keyState[cnt];
			m_keyStateRepeat[cnt] = m_keyStateTrigger[cnt];

			if(m_keyState[cnt])
			{
				m_keyStateRepeatCnt[cnt]++;
				if(m_keyStateRepeatCnt[cnt] >= 20)
				{
					m_keyStateRepeat[cnt] = m_keyState[cnt];
				}
			}
			else
			{
				m_keyStateRepeatCnt[cnt] = 0;
				m_keyStateRepeat[cnt] = 0;
			}
		}
	}
	else
	{
		// キーボードへのアクセス権を取得
		m_pDIDevKeyboard->Acquire();
	}

	return S_OK;
}

//=============================================================================
// キーボードのプレス状態を取得
//=============================================================================
BOOL InputManager::GetKeyboardPress(int key)
{
	return (m_keyState[key] & 0x80) ? TRUE : FALSE;
}

//=============================================================================
// キーボードのトリガー状態を取得
//=============================================================================
BOOL InputManager::GetKeyboardTrigger(int key)
{
	return (m_keyStateTrigger[key] & 0x80) ? TRUE : FALSE;
}

//=============================================================================
// キーボードのリピート状態を取得
//=============================================================================
BOOL InputManager::GetKeyboardRepeat(int key)
{
	return (m_keyStateRepeat[key] & 0x80) ? TRUE : FALSE;
}

//=============================================================================
// キーボードのリリ－ス状態を取得
//=============================================================================
BOOL InputManager::GetKeyboardRelease(int key)
{
	return (m_keyStateRelease[key] & 0x80) ? TRUE : FALSE;
}


//=============================================================================
// マウス関係の処理
//=============================================================================
// マウスの初期化
HRESULT InputManager::InitializeMouse(HINSTANCE hInst,HWND hWindow)
{
	HRESULT result;
	// デバイス作成
	result = m_pDInput->CreateDevice(GUID_SysMouse,&m_pMouse,NULL);
	if(FAILED(result) || m_pMouse==NULL)
	{
		MessageBox(hWindow,"No mouse","Warning",MB_OK | MB_ICONWARNING);
		return result;
	}
	// データフォーマット設定
	result = m_pMouse->SetDataFormat(&c_dfDIMouse2);
	if(FAILED(result))
	{
		MessageBox(hWindow,"Can't setup mouse","Warning",MB_OK | MB_ICONWARNING);
		return result;
	}
	// 他のアプリと協調モードに設定
	result = m_pMouse->SetCooperativeLevel(hWindow, (DISCL_FOREGROUND | DISCL_NONEXCLUSIVE));
	if(FAILED(result))
	{
		MessageBox(hWindow,"Mouse mode error","Warning",MB_OK | MB_ICONWARNING);
		return result;
	}
	
	// デバイスの設定
	DIPROPDWORD prop;
	
	prop.diph.dwSize = sizeof(prop);
	prop.diph.dwHeaderSize = sizeof(prop.diph);
	prop.diph.dwObj = 0;
	prop.diph.dwHow = DIPH_DEVICE;
	prop.dwData = DIPROPAXISMODE_REL;		// マウスの移動値　相対値

	result = m_pMouse->SetProperty(DIPROP_AXISMODE,&prop.diph);
	if(FAILED(result))
	{
		MessageBox(hWindow,"Mouse property error","Warning",MB_OK | MB_ICONWARNING);
		return result;	
	}
	
	// アクセス権を得る
	m_pMouse->Acquire();
	return result;
}
//---------------------------------------------------------
void InputManager::UninitMouse()
{
	if(m_pMouse)
	{
		m_pMouse->Unacquire();
		m_pMouse->Release();
		m_pMouse = NULL;
	}

}
//-----------------------------------------------------------
HRESULT InputManager::UpdateMouse()
{
	if (!GetWindowActive()) return S_OK; // 非アクティブ時は更新しない

	HRESULT result;
	// 前回の値保存
	DIMOUSESTATE2 lastm_mouseState = m_mouseState;
	// データ取得
	result = m_pMouse->GetDeviceState(sizeof(m_mouseState),&m_mouseState);
	if(SUCCEEDED(result))
	{
		m_mouseTrigger.lX = m_mouseState.lX;
		m_mouseTrigger.lY = m_mouseState.lY;
		m_mouseTrigger.lZ = m_mouseState.lZ;
		// マウスのボタン状態
		for(int i=0;i<8;i++)
		{
			m_mouseTrigger.rgbButtons[i] = ((lastm_mouseState.rgbButtons[i] ^
				m_mouseState.rgbButtons[i]) & m_mouseState.rgbButtons[i]);
		}
	}
	else	// 取得失敗
	{
		// アクセス権を得てみる
		result = m_pMouse->Acquire();
	}
	return result;
	
}

//----------------------------------------------
BOOL InputManager::IsMouseLeftPressed(void)
{
	return (BOOL)(m_mouseState.rgbButtons[0] & 0x80);	// 押されたときに立つビットを検査
}
BOOL InputManager::IsMouseLeftTriggered(void)
{
	return (BOOL)(m_mouseTrigger.rgbButtons[0] & 0x80);
}
BOOL InputManager::IsMouseRightPressed(void)
{
	return (BOOL)(m_mouseState.rgbButtons[1] & 0x80);
}
BOOL InputManager::IsMouseRightTriggered(void)
{
	return (BOOL)(m_mouseTrigger.rgbButtons[1] & 0x80);
}
BOOL InputManager::IsMouseCenterPressed(void)
{
	return (BOOL)(m_mouseState.rgbButtons[2] & 0x80);
}
BOOL InputManager::IsMouseCenterTriggered(void)
{
	return (BOOL)(m_mouseTrigger.rgbButtons[2] & 0x80);
}
//------------------
long InputManager::GetMouseX(void)
{
	return m_mouseState.lX;
}
long InputManager::GetMouseY(void)
{
	return m_mouseState.lY;
}
long InputManager::GetMouseZ(void)
{
	return m_mouseState.lZ;
}
//================================================= game pad
//---------------------------------------- コールバック関数
BOOL CALLBACK InputManager::SearchGamePadCallback(const LPDIDEVICEINSTANCE lpddi, LPVOID pContext)
{
	InputManager* self = reinterpret_cast<InputManager*>(pContext);

	if (self->m_padCount >= 4) return DIENUM_STOP;

	HRESULT result = self->m_pDInput->CreateDevice(lpddi->guidInstance, &self->m_pGamePad[self->m_padCount++], NULL);
	return DIENUM_CONTINUE;	// 次のデバイスを列挙

}

//---------------------------------------- 初期化
HRESULT InputManager::InitializePad(void)			// パッド初期化
{
	HRESULT		result;
	int			i;

	m_padCount = 0;
	// ジョイパッドを探す
	m_pDInput->EnumDevices(DI8DEVCLASS_GAMECTRL, (LPDIENUMDEVICESCALLBACK)SearchGamePadCallback, this, DIEDFL_ATTACHEDONLY);
	// セットしたコールバック関数が、パッドを発見した数だけ呼ばれる。

	for ( i=0 ; i<m_padCount ; i++ ) {
		// ジョイスティック用のデータ・フォーマットを設定
		result = m_pGamePad[i]->SetDataFormat(&c_dfDIJoystick);
		if ( FAILED(result) )
			return FALSE; // データフォーマットの設定に失敗

		// モードを設定（フォアグラウンド＆非排他モード）
//		result = m_pGamePad[i]->SetCooperativeLevel(hWindow, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
//		if ( FAILED(result) )
//			return FALSE; // モードの設定に失敗

		// 軸の値の範囲を設定
		// X軸、Y軸のそれぞれについて、オブジェクトが報告可能な値の範囲をセットする。
		// (max-min)は、最大10,000(?)。(max-min)/2が中央値になる。
		// 差を大きくすれば、アナログ値の細かな動きを捕らえられる。(パッドの性能による)
		DIPROPRANGE				diprg;
		ZeroMemory(&diprg, sizeof(diprg));
		diprg.diph.dwSize		= sizeof(diprg); 
		diprg.diph.dwHeaderSize	= sizeof(diprg.diph); 
		diprg.diph.dwHow		= DIPH_BYOFFSET; 
		diprg.lMin				= RANGE_MIN;
		diprg.lMax				= RANGE_MAX;
		// X軸の範囲を設定
		diprg.diph.dwObj		= DIJOFS_X; 
		m_pGamePad[i]->SetProperty(DIPROP_RANGE, &diprg.diph);
		// Y軸の範囲を設定
		diprg.diph.dwObj		= DIJOFS_Y;
		m_pGamePad[i]->SetProperty(DIPROP_RANGE, &diprg.diph);

		// 各軸ごとに、無効のゾーン値を設定する。
		// 無効ゾーンとは、中央からの微少なジョイスティックの動きを無視する範囲のこと。
		// 指定する値は、10000に対する相対値(2000なら20パーセント)。
		DIPROPDWORD				dipdw;
		dipdw.diph.dwSize		= sizeof(DIPROPDWORD);
		dipdw.diph.dwHeaderSize	= sizeof(dipdw.diph);
		dipdw.diph.dwHow		= DIPH_BYOFFSET;
		dipdw.dwData			= DEADZONE;
		//X軸の無効ゾーンを設定
		dipdw.diph.dwObj		= DIJOFS_X;
		m_pGamePad[i]->SetProperty( DIPROP_DEADZONE, &dipdw.diph);
		//Y軸の無効ゾーンを設定
		dipdw.diph.dwObj		= DIJOFS_Y;
		m_pGamePad[i]->SetProperty(DIPROP_DEADZONE, &dipdw.diph);
			
		//ジョイスティック入力制御開始
		m_pGamePad[i]->Acquire();
	}
		
	return TRUE;

}
//------------------------------------------- 終了処理
void InputManager::UninitPad(void)
{
	for (int i=0 ; i<GAMEPADMAX ; i++) {
		if ( m_pGamePad[i] )
		{
			m_pGamePad[i]->Unacquire();
			m_pGamePad[i]->Release();
		}
	}

}

//------------------------------------------ 更新
void InputManager::UpdatePad(void)
{
	HRESULT			result;
	DIJOYSTATE2		dijs;
	int				i;

	for ( i=0 ; i<m_padCount ; i++ ) 
	{
		DWORD lastPadState;
		lastPadState = m_padState[i];
		m_padState[i] = 0x00000000l;	// 初期化

		result = m_pGamePad[i]->Poll();	// ジョイスティックにポールをかける
		if ( FAILED(result) ) {
			result = m_pGamePad[i]->Acquire();
			while ( result == DIERR_INPUTLOST )
				result = m_pGamePad[i]->Acquire();
		}

		result = m_pGamePad[i]->GetDeviceState(sizeof(DIJOYSTATE), &dijs);	// デバイス状態を読み取る
		if ( result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED ) {
			result = m_pGamePad[i]->Acquire();
			while ( result == DIERR_INPUTLOST )
				result = m_pGamePad[i]->Acquire();
		}

		// ３２の各ビットに意味を持たせ、ボタン押下に応じてビットをオンにする
		//* y-axis (forward)
		if ( dijs.lY < 0 )					m_padState[i] |= BUTTON_UP;
		//* y-axis (backward)
		if ( dijs.lY > 0 )					m_padState[i] |= BUTTON_DOWN;
		//* x-axis (left)
		if ( dijs.lX < 0 )					m_padState[i] |= BUTTON_LEFT;
		//* x-axis (right)
		if ( dijs.lX > 0 )					m_padState[i] |= BUTTON_RIGHT;
		//* Ａボタン
		if ( dijs.rgbButtons[0] & 0x80 )	m_padState[i] |= BUTTON_A;
		//* Ｂボタン
		if ( dijs.rgbButtons[1] & 0x80 )	m_padState[i] |= BUTTON_B;
		//* Ｃボタン
		if ( dijs.rgbButtons[2] & 0x80 )	m_padState[i] |= BUTTON_C;
		//* Ｘボタン
		if ( dijs.rgbButtons[3] & 0x80 )	m_padState[i] |= BUTTON_X;
		//* Ｙボタン
		if ( dijs.rgbButtons[4] & 0x80 )	m_padState[i] |= BUTTON_Y;
		//* Ｚボタン
		if ( dijs.rgbButtons[5] & 0x80 )	m_padState[i] |= BUTTON_Z;
		//* Ｌボタン
		if ( dijs.rgbButtons[6] & 0x80 )	m_padState[i] |= BUTTON_L;
		//* Ｒボタン
		if ( dijs.rgbButtons[7] & 0x80 )	m_padState[i] |= BUTTON_R;
		//* ＳＴＡＲＴボタン
		if ( dijs.rgbButtons[8] & 0x80 )	m_padState[i] |= BUTTON_START;
		//* Ｍボタン
		if ( dijs.rgbButtons[9] & 0x80 )	m_padState[i] |= BUTTON_M;

		// Trigger設定
		m_padTrigger[i] = ((lastPadState ^ m_padState[i])	// 前回と違っていて
						& m_padState[i]);					// しかも今ONのやつ
		
	}

}
//----------------------------------------------- 検査
BOOL InputManager::IsButtonPressed(int padNo,DWORD button)
{
	return (button & m_padState[padNo]);
}

BOOL InputManager::IsButtonTriggered(int padNo,DWORD button)
{
	return (button & m_padTrigger[padNo]);
}

void InputManager::SetMousePosCenter(void)
{
	HWND hwnd = GetForegroundWindow();

	RECT rect;
	GetClientRect(hwnd, &rect);  // クライアント領域取得

	POINT center;
	center.x = (rect.right - rect.left) / 2;
	center.y = (rect.bottom - rect.top) / 2;

	ClientToScreen(hwnd, &center); // スクリーン座標に変換
	SetCursorPos(center.x, center.y); // マウス位置を設定

	m_isMouseRecentered = true;            // センタリングフラグを有効化
}

void InputManager::SetMouseRecentered(BOOL recenter)
{
	m_isMouseRecentered = recenter;
}

BOOL InputManager::IsMouseRecentered(void)
{
	return m_isMouseRecentered;
}
	


