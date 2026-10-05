//=============================================================================
//
// メイン処理 [main.cpp]
// Author : 
//
//=============================================================================
#include "main.h"
#include "Core/Timer.h"
#include "Core/LightManager.h"
#include "Core/GameSystem.h"
#include "Core/TextureMgr.h"
#include "Core/Camera.h"
#include "Core/CursorManager.h"
#include "Core/Shader/ShaderManager.h"
#include "Core/Shader/ShaderResourceBinder.h"
#include "Core/Graphics/Renderer.h"
#include "Core/Graphics/ShadowMapRenderer.h"
#include "Core/AudioManager.h"
#include "Collision/CollisionManager.h"
#include "Model/Model.h"
#include "Model/FBXLoader.h"
#include "Model/SkinnedMeshModel.h"
#include "Scene/EnemyManager.h"
#include "Scene/Ground.h"
#include "Scene/Scene.h"
#include "UI/Base/UIManager.h"
#include "UI/PauseModal.h"
#include "Utility/InputManager.h"
#include "Utility/Debug/Debugproc.h"
#include "Utility/Debug/imgui/imgui_impl_win32.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define CLASS_NAME		"AppClass"			// ウインドウのクラス名
#define WINDOW_NAME		"メッシュ表示"		// ウインドウのキャプション名
#define _SHOWFPS		1					// FPSを表示するかどうか

//*****************************************************************************
// プロトタイプ宣言
//*****************************************************************************
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
HRESULT Init(HINSTANCE hInstance, HWND hWnd, BOOL bWindow);
void Uninit(void);
void Update(void);
void Draw(void);
void EnableDPIAwareness(void);

// ImGui Win32 入力ハンドラーの前方宣言
// - ヘッダーで <windows.h> をインクルードしないための回避策
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


//*****************************************************************************
// グローバル変数:
//*****************************************************************************
long g_MouseX = 0;
long g_MouseY = 0;

bool g_IsWindowActive = true; // デフォルトでアクティブ
bool g_ExitApp = false; // アプリケーション終了フラグ

//MapEditor& mapEditor = MapEditor::get_instance();
InputManager& inputManager = InputManager::get_instance();
DebugProc& debugProc = DebugProc::get_instance();
GameSystem& gameSystem = GameSystem::get_instance();
CollisionManager& collisionManager = CollisionManager::get_instance();
Renderer& renderer = Renderer::get_instance();
Timer& timer = Timer::get_instance();
Camera& camera = Camera::get_instance();
LightManager& lightManager = LightManager::get_instance();
ShaderManager& shaderManager = ShaderManager::get_instance();
ShadowMapRenderer& shadowMapRenderer = ShadowMapRenderer::get_instance();
Scene& scene = Scene::get_instance();
UIManager& uiManager = UIManager::get_instance();
CursorManager& cursorManager = CursorManager::get_instance();
ShaderResourceBinder& shaderResourceBinder = ShaderResourceBinder::get_instance();
TextureMgr& textureMgr = TextureMgr::get_instance();
AudioManager& audioManager = AudioManager::get_instance();

#ifdef _SHOWFPS
int		g_CountFPS;							// FPSカウンタ
char	g_DebugStr[2048] = WINDOW_NAME;		// デバッグ文字表示用

#endif

#ifdef __has_include
#  if __has_include(<shellscalingapi.h>)
#    include <shellscalingapi.h>
#    pragma comment(lib, "Shcore.lib")
#    define HAS_SHCORE 1
#  endif
#endif

//=============================================================================
// メイン関数
//=============================================================================
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	EnableDPIAwareness();

	UNREFERENCED_PARAMETER(hPrevInstance);	// 無くても良いけど、警告が出る（未使用宣言）
	UNREFERENCED_PARAMETER(lpCmdLine);		// 無くても良いけど、警告が出る（未使用宣言）

	// 時間計測用
	DWORD dwExecLastTime;
	DWORD dwFPSLastTime;
	DWORD dwCurrentTime;
	DWORD dwFrameCount;

	WNDCLASSEX	wcex = {
		sizeof(WNDCLASSEX),
		CS_CLASSDC,
		WndProc,
		0,
		0,
		hInstance,
		NULL,
		LoadCursor(NULL, IDC_ARROW),
		(HBRUSH)(COLOR_WINDOW + 1),
		NULL,
		CLASS_NAME,
		NULL
	};
	HWND		hWnd;
	MSG			msg;
	
	// ウィンドウクラスの登録
	RegisterClassEx(&wcex);

	RECT rc = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	// ウィンドウの作成
	hWnd = CreateWindow(CLASS_NAME,
		WINDOW_NAME,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,																		// ウィンドウの左座標
		CW_USEDEFAULT,																		// ウィンドウの上座標
		rc.right - rc.left,
		rc.bottom - rc.top,
		//SCREEN_WIDTH + GetSystemMetrics(SM_CXDLGFRAME) * 2,									// ウィンドウ横幅
		//SCREEN_HEIGHT + GetSystemMetrics(SM_CXDLGFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION),	// ウィンドウ縦幅
		NULL,
		NULL,
		hInstance,
		NULL);

	// 初期化処理(ウィンドウを作成してから行う)
	if(FAILED(Init(hInstance, hWnd, TRUE)))
	{
		return -1;
	}

	// フレームカウント初期化
	timeBeginPeriod(1);	// 分解能を設定
	dwExecLastTime = dwFPSLastTime = timeGetTime();	// システム時刻をミリ秒単位で取得
	dwCurrentTime = dwFrameCount = 0;

	// ウインドウの表示(初期化処理の後に呼ばないと駄目)
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	
	if (!(GetForegroundWindow() == hWnd) && gameSystem.GetMode() == GameMode::GAME)
	{
		gameSystem.PauseGame();
	}

	// メッセージループ
	while(true)
	{
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message == WM_QUIT)
			{// PostQuitMessage()が呼ばれたらループ終了
				break;
			}
			else
			{
				// メッセージの翻訳と送出
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
        }
		else
		{
			dwCurrentTime = timeGetTime();

			if ((dwCurrentTime - dwFPSLastTime) >= 1000)	// 1秒ごとに実行
			{
#ifdef _SHOWFPS
				g_CountFPS = dwFrameCount;
#endif
				dwFPSLastTime = dwCurrentTime;				// FPSを測定した時刻を保存
				dwFrameCount = 0;							// カウントをクリア
			}

			if ((dwCurrentTime - dwExecLastTime) >= (1000 / 60))	// 1/60秒ごとに実行
			{
				dwExecLastTime = dwCurrentTime;	// 処理した時刻を保存

#ifdef _SHOWFPS	// デバッグ版の時だけFPSを表示する
				wsprintf(g_DebugStr, WINDOW_NAME);
				wsprintf(&g_DebugStr[strlen(g_DebugStr)], " FPS:%d", g_CountFPS);
#endif

				Update();			// 更新処理
				Draw();				// 描画処理

#ifdef _SHOWFPS	// デバッグ版の時だけ表示する
				wsprintf(&g_DebugStr[strlen(g_DebugStr)], " MX:%d MY:%d", GetMousePosX(), GetMousePosY());
				SetWindowText(hWnd, g_DebugStr);
#endif

				dwFrameCount++;
			}
		}
	}

	timeEndPeriod(1);				// 分解能を戻す

	// ウィンドウクラスの登録を解除
	UnregisterClass(CLASS_NAME, wcex.hInstance);

	// 終了処理
	Uninit();

	return (int)msg.wParam;
}

//=============================================================================
// プロシージャ
//=============================================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// ImGui のマウス/キーボード入力を処理する関数
	// - TRUE を返した場合はアプリ側で処理を行わないようにする
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;

	// アプリケーション終了フラグが立っている場合は終了
	if (g_ExitApp)
	{
		Uninit();
		DestroyWindow(hWnd);
		return 0;
	}

	switch(message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_KEYDOWN:
		switch(wParam)
		{
		case VK_ESCAPE:
			DestroyWindow(hWnd);
			break;
		}
		break;

	case WM_MOUSEMOVE:
		g_MouseX = LOWORD(lParam);
		g_MouseY = HIWORD(lParam);
		break;

	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT)
		{
			// マウスカーソルを設定する
			SetCursor(cursorManager.GetCurrentCursor());
			return TRUE;
		}
		break;
	case WM_SYSKEYDOWN:
		if (wParam == VK_MENU) // VK_MENU = Altキー
		{
			// Altキーを押した時の標準動作をキャンセル
			return 0;
		}
		break;
	case WM_SYSCHAR:
		return 0;
	case WM_ACTIVATE:
		if (LOWORD(wParam) != WA_INACTIVE)
		{
			// ウィンドウがアクティブになった
			if (gameSystem.GetMode() == GameMode::GAME)
			{
				g_IsWindowActive = true;
				cursorManager.RememberCursorPosition(); // カーソル位置を保存
				inputManager.SetMouseRecentered(true); // カメラ回転防止用フラグ設定
				gameSystem.ClearPauseFlag(); // ポーズフラグをクリア
			}

		}
		else
		{
			if (gameSystem.GetMode() == GameMode::GAME && !gameSystem.IsPaused())
			{
				// ウィンドウが非アクティブになった
				g_IsWindowActive = false;

				// 自動ポーズ（手動入力と同じ処理）
				gameSystem.SetPauseFlag();
			}
		}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

//=============================================================================
// 初期化処理
//=============================================================================
HRESULT Init(HINSTANCE hInstance, HWND hWnd, BOOL bWindow)
{
	// レンダラーの初期化
	renderer.Init(hInstance, hWnd, bWindow);
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	renderer.SetClearColor(clearColor);

	debugProc.Init(hWnd);

	shaderManager.Init(renderer.GetDevice());
	renderer.SetShadersets();

	shadowMapRenderer.Init(CSM_SHADOW_MAP_SIZE, MAX_CASCADES);

	audioManager.Init(hWnd);

	// カメラの初期化
	camera.Init();

	// 衝突管理の初期化
	collisionManager.Init();

	// ゲームシステムの初期化
	gameSystem.Init();

	// 入力処理の初期化
	inputManager.Init(hInstance, hWnd);

	// ライトを有効化
	lightManager.SetLightEnable(TRUE);

	// 背面ポリゴンをカリング
	renderer.SetCullingMode(CULL_MODE_BACK);

	timer.Init();

	return S_OK;
}

//=============================================================================
// 終了処理
//=============================================================================
void Uninit(void)
{
	// カメラの終了処理
	camera.Shutdown();

	//入力の終了処理
	inputManager.Shutdown();

	gameSystem.Shutdown();

	textureMgr.Shutdown();

	// レンダラーの終了処理
	renderer.Shutdown();

	debugProc.Shutdown();

	//mapEditor.Uninit();
}

//=============================================================================
// 更新処理
//=============================================================================
void Update(void)
{
	shaderResourceBinder.Reset();

	timer.Update();

	// 入力の更新処理
	inputManager.Update();

	// カメラ更新
	camera.Update();

	// ゲームシステムの更新
	gameSystem.Update();

	//mapEditor.Update();

	collisionManager.Update();

	lightManager.Update();
}

//=============================================================================
// 描画処理
//=============================================================================
void Draw(void)
{
	// バックバッファクリア
	renderer.Clear();

	// シャドウマップの描画
	gameSystem.RenderShadowPass();

	// メインパス描画
	gameSystem.Draw();

#ifdef _DEBUG
	// 深度テストを無効に
	renderer.SetDepthEnable(FALSE);
	// デバッグ表示
	debugProc.BeginFrame();
	debugProc.Draw();
	// 深度テストを有効に
	renderer.SetDepthEnable(TRUE);
#endif

	// バックバッファ、フロントバッファ入れ替え
	renderer.Present();
}

bool GetWindowActive(void)
{
	return g_IsWindowActive;
}

long GetMousePosX(void)
{
	return g_MouseX;
}


long GetMousePosY(void)
{
	return g_MouseY;
}


#ifdef _SHOWFPS
char* GetDebugStr(void)
{
	return g_DebugStr;
}
#endif

int GetRand(int min, int max)
{
	static int flagRand = 0;
	static std::mt19937 g_mt;

	if (flagRand == 0)
	{
		// ランダム生成準備
		std::random_device rnd;	// 非決定的な乱数生成器
		g_mt.seed(rnd());		// メルセンヌ・ツイスタ版　引数は初期SEED
		flagRand = 1;
	}

	std::uniform_int_distribution<> random(min, max);	// 生成ランダムは0～100の範囲
	int answer = random(g_mt);
	return answer;
}


float GetRandFloat(float min, float max) 
{
	static std::mt19937 g_mt(std::random_device{}());
	std::uniform_real_distribution<float> dist(min, max);
	return dist(g_mt);
}

// 線形補間関数
// a: 開始値, b: 終了値, t: 補間係数 (0.0f <= t <= 1.0f)
float Lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

XMFLOAT3 Lerp3(const XMFLOAT3& a, const XMFLOAT3& b, float t)
{
	return XMFLOAT3(
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t
	);
}

float Length3(const XMFLOAT3& a, const XMFLOAT3& b)
{
	XMVECTOR A = XMLoadFloat3(&a);
	XMVECTOR B = XMLoadFloat3(&b);
	return XMVectorGetX(XMVector3Length(B - A));
}

float SmoothAngleLerp(float current, float target, float t)
{
	// 差分を[-π, π]に正規化
	float delta = target - current;
	while (delta > XM_PI)  delta -= XM_2PI;
	while (delta < -XM_PI) delta += XM_2PI;

	// 最短経路で補間
	current += delta * t;

	// 結果を[-π, π]に再正規化
	if (current > XM_PI)  current -= XM_2PI;
	if (current < -XM_PI) current += XM_2PI;

	return current;
}

XMFLOAT3 DirNorm(const XMFLOAT3& a, const XMFLOAT3& b)
{
	XMVECTOR A = XMLoadFloat3(&a);
	XMVECTOR B = XMLoadFloat3(&b);
	XMVECTOR D = XMVector3Normalize(B - A);
	XMFLOAT3 out; XMStoreFloat3(&out, D);
	return out;
}

void ExitGame(void)
{
	g_ExitApp = true; // アプリケーション終了フラグを立てる
}

XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b)
{
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

XMFLOAT3 Sub(const XMFLOAT3& a, const XMFLOAT3& b)
{
	return { a.x - b.x,a.y - b.y,a.z - b.z };
}

XMFLOAT3 Scale(const XMFLOAT3& v, float s)
{
	return { v.x * s, v.y * s, v.z * s };
}

float ScalarSaturate(float x)
{
	return (x < 0.0f) ? 0.0f : (x > 1.0f ? 1.0f : x);
}

float DegToRad(float d)
{
	return d * (XM_PI / 180.0f);
}

XMFLOAT3 NormalizeSafe(const XMFLOAT3& v)
{
	float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	if (len < 1e-6f) return XMFLOAT3(0, 1, 0);
	float inv = 1.0f / len;
	return XMFLOAT3(v.x * inv, v.y * inv, v.z * inv);
}

float Dot(const XMFLOAT3& a, const XMFLOAT3& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Dist(const XMFLOAT3& a, const XMFLOAT3& b)
{
	const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
	return sqrtf(dx * dx + dy * dy + dz * dz);
}

float DistSq(const XMFLOAT3& a, const XMFLOAT3& b)
{
	const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
	return (dx * dx + dy * dy + dz * dz);
}


void EnableDPIAwareness(void)
{
#if defined(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

#elif defined(HAS_SHCORE) && defined(PROCESS_PER_MONITOR_DPI_AWARE)
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

#else
	SetProcessDPIAware();
#endif
}