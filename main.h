//=============================================================================
//
// メイン処理 [main.h]
// Author : 
//
//=============================================================================
#pragma once


#pragma warning(push)
#pragma warning(disable:4005)

#define _CRT_SECURE_NO_WARNINGS			// scanf のwarning防止
#include <stdio.h>
#include <random>

#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>

#include <DirectXMath.h>



// 本来はヘッダに書かない方が良い
using namespace DirectX;


#define DIRECTINPUT_VERSION 0x0800		// 警告対処
#include "dinput.h"
#include "mmsystem.h"

#pragma warning(pop)


//*****************************************************************************
// ライブラリのリンク
//*****************************************************************************
#pragma comment (lib, "d3d11.lib")		
#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "d3dx11.lib")	
#pragma comment (lib, "d3dx9.lib")	
#pragma comment (lib, "winmm.lib")
#pragma comment (lib, "dxerr.lib")
#pragma comment (lib, "dxguid.lib")
#pragma comment (lib, "dinput8.lib")


//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SCREEN_WIDTH	(1920)			// ウインドウの幅
#define SCREEN_HEIGHT	(1080)			// ウインドウの高さ
#define SCREEN_CENTER_X	(SCREEN_WIDTH / 2)	// ウインドウの中心Ｘ座標
#define SCREEN_CENTER_Y	(SCREEN_HEIGHT / 2)	// ウインドウの中心Ｙ座標

#define	MAP_W			(1280.0f)
#define	MAP_H			(1280.0f)
#define	MAP_TOP			(MAP_H/2)
#define	MAP_DOWN		(-MAP_H/2)
#define	MAP_LEFT		(-MAP_W/2)
#define	MAP_RIGHT		(MAP_W/2)

#define SAFE_DELETE(p) { if(p) { delete(p); (p) = nullptr; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[](p); (p) = nullptr; } }
#define ZERO(p) ZeroMemory(&p, sizeof(p))
#define TO_UINT64(entry)			static_cast<uint64_t>(entry)

// 補間用のデータ構造体を定義
struct INTERPOLATION_DATA
{
	XMFLOAT3	pos;		// 頂点座標
	XMFLOAT3	rot;		// 回転
	XMFLOAT3	scl;		// 拡大縮小
	float		frame;		// 実行フレーム数 ( dt = 1.0f/frame )
};


//*****************************************************************************
// プロトタイプ宣言
//*****************************************************************************
long GetMousePosX(void);
long GetMousePosY(void);
char* GetDebugStr(void);
bool GetWindowActive(void);
int GetRand(int min, int max);
float GetRandFloat(float min, float max);
float Lerp(float a, float b, float t);
XMFLOAT3 Lerp3(const XMFLOAT3& a, const XMFLOAT3& b, float t);
float Length3(const XMFLOAT3& a, const XMFLOAT3& b);
float SmoothAngleLerp(float current, float target, float t);
XMFLOAT3 DirNorm(const XMFLOAT3& a, const XMFLOAT3& b);
void ExitGame(void);
XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b);
XMFLOAT3 Sub(const XMFLOAT3& a, const XMFLOAT3& b);
XMFLOAT3 Scale(const XMFLOAT3& v, float s);
float ScalarSaturate(float x);
float DegToRad(float d);
XMFLOAT3 NormalizeSafe(const XMFLOAT3& v);
float Dot(const XMFLOAT3& a, const XMFLOAT3& b);
float Dist(const XMFLOAT3& a, const XMFLOAT3& b);
float DistSq(const XMFLOAT3& a, const XMFLOAT3& b);

// Declare additional functions for releasing interfaces, 
// and macros for error-handling and retrieving the module's base address.
template<class Interface>
inline void SafeRelease(
	Interface** ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != NULL)
	{
		(*ppInterfaceToRelease)->Release();
		(*ppInterfaceToRelease) = NULL;
	}
}

template<typename T>
T Clamp(const T& val, const T& low, const T& high)
{
	return val < low ? low : (val > high ? high : val);
}