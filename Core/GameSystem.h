#pragma once
//=============================================================================
//
// ゲーム進行・描画・システム状態を統括管理する中枢クラス [GameSystem.h]
// Author : 
// タイトル／ゲーム／リザルト各モードの状態遷移・更新・描画制御を行い、
// プレイヤー／敵／UI／カメラ／エフェクトなど全主要サブシステムを統括する
//
//=============================================================================
#include "main.h"
#include "Core/Camera.h"
#include "Core/CursorManager.h"
#include "Core/TextureUploadQueue.h"
#include "Core/Async/AsyncResourceLoader.h"
#include "Core/Async/AsyncModelLoader.h"
#include "Core/Graphics/Renderer.h"
#include "Core/Graphics/ShadowMapRenderer.h"
#include "UI/UILoadingScene.h"
#include "UI/Base/UIManager.h"
#include "UI/Base/TweenManager.h"
#include "Scene/Player.h"
#include "Scene/EnemyManager.h"
#include "Scene/Item/ItemManager.h"
#include "Scene/Weapon/ProjectileManager.h"
#include "Scene/Ground.h"
#include "Effects/Skybox.h"
#include "Effects/EffectSystem.h"
#include "Effects/DecalRenderer.h"
#include "Utility/SingletonBase.h"
#include "Utility/InputManager.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
// ゲームモードの列挙
enum class GameMode : int		// ゲームモード
{
    TITLE = 0,			        // タイトル画面
    TUTORIAL,			        // ゲーム説明画面
    GAME,				        // ゲーム画面
    LOADING,                    // ゲームロード
    RESULT,				        // リザルト画面
    MAX
};

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct ResourceLoadContext
{
    GameMode nextMode;             // 次に入るモード（例：GAME）
    char* sceneName = nullptr;     // 読み込むマップ名やステージID
    bool startInitialLoad = false;
    bool finishInitialLoad = false;
    bool isFromLoad = false;       // セーブデータからの読込かどうか
};

struct ResourceLoadStatus
{
    int totalTasks = 0;         // 登録された全タスク数
    int doneTasks = 0;          // 完了済みタスク数

    void Clear(void)
    {
        totalTasks = 0;
        doneTasks = 0;
    }
};

// ゲーム全体の状態を管理するクラス
class GameSystem : public SingletonBase<GameSystem>
{
public:

    // 初期化と更新
    void Init(void);
    void Shutdown(void);
    void Update(void);
    void Draw(void);
	bool GetFinishLoading(void) const { return m_currLoadCtx.finishInitialLoad; }
    void RenderShadowPass(void);

    // 読み込みがすべて完了したかを判定する関数
    bool IsAllResourceReady(void) const;

    // ゲームの一時停止/再開
    void PauseGame(void);
    void ResumeGame(void);
    bool IsPaused(void) const { return m_isPaused; }
    bool IsAltDown(void) const { return m_isAltDown; } // Altキーが押されているかどうか

    bool AllowSaveAndSwitch(void) const;

    // 会話モード切替
    void EnterDialogue(void) { m_inDialogue = true; }
    void ExitDialogue(void) { m_inDialogue = false; }
    bool IsInDialogue(void) const { return m_inDialogue; }

    // ミニマップ表示切替
    void ToggleMiniMap(void) { m_showMiniMap = !m_showMiniMap; }
    bool IsMiniMapVisible(void) const { return m_showMiniMap; }

    void SetPauseFlag(void) { m_pauseFlag = true; }
    void ClearPauseFlag(void) { m_pauseFlag = false; }
    void SetAllowPause(bool allow) { m_allowPause = allow; }

	void SetLoadGame(bool load) { m_loadGame = load; }

	// ボタンのクリック状態を設定
    void SetResumeBtnClick(bool clicked) { m_resumeBtnClick = clicked; }

    void ChangeMode(GameMode mode, bool skipLoadingFadeIn = false); // モード切り替え
    GameMode GetMode(void) const { return m_mode; }

	Player* GetPlayer(void) const { return m_player; }

protected:
    // プライベートコンストラクタ
    GameSystem()
        : SingletonBase()
        , m_renderer(Renderer::get_instance())
        , m_lightManager(LightManager::get_instance())
        , m_inputManager(InputManager::get_instance())
        , m_UIManager(UIManager::get_instance())
        , m_enemyManager(EnemyManager::get_instance())
        , m_itemManager(ItemManager::get_instance())
        , m_projectileManager(ProjectileManager::get_instance())
        , m_camera(Camera::get_instance())
        , m_scene(Scene::get_instance())
        , m_shadowMapRenderer(ShadowMapRenderer::get_instance())
        , m_cursorManager(CursorManager::get_instance())
        , m_effectSystem(EffectSystem::get_instance())
        , m_shaderManager(ShaderManager::get_instance())
        , m_asyncResourceLoader(AsyncResourceLoader::get_instance())
        , m_asyncModelLoader(AsyncModelLoader::get_instance())
        , m_deferredTaskSystem(DeferredTaskSystem::get_instance())
        , m_textureUploadQueue(TextureUploadQueue::get_instance())
        , m_ground(Ground::get_instance())
        , m_skybox(Skybox::get_instance())
        , m_tweenManager(TweenManager::get_instance())
        , m_decalRenderer(DecalRenderer::get_instance()) {}

    // この行が絶対に必要！！
    // SingletonBase<GameSystem> が GameSystem の protected コンストラクタにアクセスできるようにするための特別な許可
    // テンプレートクラスは「自動的にフレンド」にはならないため、明示的に friend 宣言が必要
    friend class SingletonBase<GameSystem>;


private:
    void InitTitle(void);
    void InitGame(void);
    void InitResult(void);
    void ProcessModeLoading(void);

    // モードごとの更新と描画関数
    void UpdateTitle(void);
    void UpdateGame(void);
    void UpdateLoading(void);
    void UpdateResult(void);

	// モードごとの描画関数
    void DrawTitle(void);
    void DrawGame(void);
    void DrawResult(void);

    // 入力処理
    void HandleInput(void);
    void HandlePauseInput(void);

    // 指定された文脈に基づいて必要なリソースを非同期で読み込む関数
    void StartAsyncResourceLoading(void);

    // 非同期読み込み進行度を0.0～1.0で返す関数
    float CalculateCurrentLoadingProgress(void);

    // 内部状態
    bool m_isPaused = false;
    bool m_resumeBtnClick = false;
    bool m_pauseFlag = false;
    bool m_allowPause = true;
    bool m_inDialogue = false;
    bool m_showMiniMap = true;
    bool m_isAltDown = false; // Altキーが押されているかどうか
    bool m_skipLoadingFadeIn = false;
	bool m_loadGame = false; // セーブデータからの読込かどうか

    GameMode m_mode = GameMode::TITLE;

    Player* m_player = nullptr;

    ResourceLoadStatus m_loadStatus;
    ResourceLoadContext m_currLoadCtx;
    UILoadingScene* m_loadingScene = nullptr;

    Ground& m_ground;
	Skybox& m_skybox;
    UIManager& m_UIManager;
    EnemyManager& m_enemyManager;
    ItemManager& m_itemManager;
    ProjectileManager& m_projectileManager;
    Renderer& m_renderer;
    LightManager& m_lightManager;
    Camera& m_camera;
    Scene& m_scene;
    ShadowMapRenderer& m_shadowMapRenderer;
    CursorManager& m_cursorManager;
    EffectSystem& m_effectSystem;
    ShaderManager& m_shaderManager;
    InputManager& m_inputManager;
    AsyncResourceLoader& m_asyncResourceLoader;
    AsyncModelLoader& m_asyncModelLoader;
	DeferredTaskSystem& m_deferredTaskSystem;
    TextureUploadQueue& m_textureUploadQueue;
    TweenManager& m_tweenManager;
    DecalRenderer& m_decalRenderer;
};