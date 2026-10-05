//=============================================================================
//
// GameSystem処理 [GameSystem.cpp]
// Author : 
//
//=============================================================================
#include "Core/GameSystem.h"
#include "Core/SaveSystem.h"
#include "Core/AudioManager.h"
#include "UI/PauseModal.h"
#include "UI/GameMenuModal.h"
#include "UI/UITitleScene.h"
#include "UI/UIMainHUD.h"
#include "Utility/Debug/Debugproc.h"
#include "Effects/EffectSystem.h"

void GameSystem::Init(void)
{
    m_mode = GameMode::TITLE;

    m_deferredTaskSystem.Init(); // DeferredTaskSystemの初期化 *必ず最初に

    m_effectSystem.Init(); // エフェクトシステムの初期化

    m_UIManager.Init(); // UIManagerの初期化

    m_textureUploadQueue.Init();

    m_cursorManager.Init(); // カーソルマネージャの初期化

    m_currLoadCtx.startInitialLoad = true;
    StartAsyncResourceLoading();

    // 初期モードに応じてUIなどを切り替えるよっ！
    ChangeMode(m_mode);
}

void GameSystem::Shutdown(void)
{
    SAFE_DELETE(m_player); // プレイヤーの解放

    m_UIManager.Shutdown(); // UIManagerの解放

    m_tweenManager.Shutdown(); // TweenManagerの解放

	m_effectSystem.Shutdown(); // エフェクトシステムの解放

    m_textureUploadQueue.ShutDown(); // テクスチャアップロードキューの解放

	m_deferredTaskSystem.ShutDown(); // DeferredTaskSystemの解放
}

void GameSystem::Update(void)
{
    m_asyncModelLoader.Update(); // 非同期読み込み状態を更新
    m_deferredTaskSystem.Update(); // 遅延タスクの更新
    m_textureUploadQueue.Update(); // テクスチャアップロードキューの更新

    HandleInput();// 入力処理

    bool isPaused = IsPaused(); // ゲームが一時停止中かどうかを確認

    // モードによって処理を分ける
    switch (m_mode)
    {
    case GameMode::TITLE:
        UpdateTitle(); // タイトル画面の更新処理
        break;
    case GameMode::GAME:
        UpdateGame(); // ゲームの更新処理
        break;
    case GameMode::LOADING:
        UpdateLoading();  // ローディング中は進行状況の更新と完了チェックを行う
        break;
    case GameMode::RESULT:
        UpdateResult(); // リザルト画面の更新処理
        break;
    }

    // UI 全体の更新を行う
    m_tweenManager.Update(); // TweenManagerの更新
    m_UIManager.Update();
}

void GameSystem::Draw(void)
{
    // メインパスのビューポートを設定
    m_renderer.SetMainPassViewport();

    // モードによって処理を分ける
    switch (m_mode)
    {
    case GameMode::TITLE:
        DrawTitle(); // タイトル画面の描画処理
        break;
    case GameMode::GAME:
        DrawGame(); // ゲームの描画処理
        break;
    case GameMode::RESULT:
        DrawResult(); // リザルト画面の描画処理
        break;
    default:
        break;
    }

    // UIの描画
    //m_lightManager.SetLightEnable(FALSE); // ライティングを無効
    m_renderer.SetCullingMode(CULL_MODE_NONE); // カリングモードを無効に
    m_renderer.SetUIInputLayout(); // モデルの入力レイアウトを設定
    m_renderer.SetRenderUI(); // UIの描画を設定
    // 深度テストを無効に
    m_renderer.SetDepthMode(DepthMode::Disable);
    m_UIManager.Draw();
    // ライティングを有効に
    //m_lightManager.SetLightEnable(TRUE);
}

void GameSystem::InitTitle(void)
{
    UITitleScene* titleScene = new UITitleScene();
    titleScene->Init(this);
    m_UIManager.AddElement(titleScene);
    m_cursorManager.RestoreCursorAndEnterUIExclusive();
}

void GameSystem::InitGame(void)
{
    ResumeGame();

    UIMainHUD* hud = new UIMainHUD();
    hud->Initialize();
    m_UIManager.AddElement(hud);

}

void GameSystem::InitResult(void)
{
}

void GameSystem::ProcessModeLoading(void)
{
    auto* loadingUI = new UILoadingScene();
    loadingUI->Init(
        TextureMgr::get_instance().CreateTexture(LOADING_ANIM_TEXTURE_PATH),
        m_skipLoadingFadeIn
    );
    m_UIManager.AddElement(loadingUI);
    m_loadingScene = loadingUI;
    StartAsyncResourceLoading(); // 非同期ロード開始
}

void GameSystem::RenderShadowPass(void)
{
    if (m_mode != GameMode::GAME)
        return;

    // シャドウマップのレンダリング
    const auto& lightList = m_lightManager.GetLightList();
    const auto& sceneObjects = m_scene.GetAllRenderableObjects();

    int lightIdx = 0;
    for (const auto& light : lightList)
    {
        // ライトの有効性を確認
        if (!light->GetLightData().Enable || lightIdx >= LIGHT_MAX) continue;

        // ディレクショナルライトの場合
        if (light->GetType() == LIGHT_TYPE::DIRECTIONAL)
        {
            m_shadowMapRenderer.RenderCSMForLight(static_cast<DirectionalLight*>(light), lightIdx, sceneObjects);
        }

        lightIdx++;
    }
}

void GameSystem::ChangeMode(GameMode mode, bool skipLoadingFadeIn)
{
    m_mode = mode;
    m_skipLoadingFadeIn = skipLoadingFadeIn;

    // 現在のUIを一旦クリアするよっ！
    m_UIManager.Clear();

    switch (mode)
    {
    case GameMode::TITLE:
        InitTitle();
        break;
    case GameMode::GAME:
        InitGame();
        break;
    case GameMode::RESULT:
        InitResult();
        break;
    case GameMode::LOADING:
        ProcessModeLoading();
        break;
    default:
        break;
    }
}

void GameSystem::PauseGame(void)
{
    if (m_UIManager.IsFading())
        return;

    m_isPaused = true;    
    // カーソル位置を復元し、その後UI専用状態に切り替える
    m_cursorManager.RestoreCursorAndEnterUIExclusive(); 
    m_UIManager.PushModal<PauseModal>(); // ポーズモーダルを表示
    m_UIManager.PushModal<GameMenuModal>(this, m_ground.GetCurrentSceneID()); // ゲームメニューを表示
    m_pauseFlag = false; // フラグをクリア
}

void GameSystem::ResumeGame(void)
{
    m_isPaused = false;
    m_cursorManager.EnterUIHidden(); // UI非表示モードに入る
    m_cursorManager.RememberCursorPosition(); // カーソル位置を保存
    m_inputManager.SetMouseRecentered(true); // カメラ用：次のフレームでマウス位置リセットを要求
}

bool GameSystem::AllowSaveAndSwitch(void) const
{
	Boss* boss = m_enemyManager.GetBoss();
	if (boss && boss->GetBossFightStarted())
		return false; // ボス戦中はセーブとシーン切り替えを禁止

    bool allow = true;

	allow &= !m_player->GetPlayerAttributes().climbMode; // 登攀中はセーブとシーン切り替えを禁止
	allow &= !(m_player->GetCurrentState() == PlayerState::PLANT_SEED); // 種まき中はセーブとシーン切り替えを禁止

    return allow;
}


void GameSystem::StartAsyncResourceLoading(void)
{
    m_loadStatus.Clear();

    if (m_currLoadCtx.startInitialLoad)
    {
        m_player = new Player(); // プレイヤーの初期化

        m_skybox.Initialize(0.0f); // Skyboxの初期化

        m_ground.Initialize(); // 地面の初期化

        m_enemyManager.Init(m_player); // エネミーの初期化

        m_itemManager.Init();

        m_projectileManager.Init();

		m_decalRenderer.Initialize(m_renderer.GetDevice(), m_renderer.GetDeviceContext());

        m_currLoadCtx.startInitialLoad = false;
    }
    else
    {
        m_player->InitializePlayerStatus();
        m_enemyManager.InitializeEnemyStatus();
        CollisionManager::get_instance().PrepareVoxelMappingAfterSceneChange(); // ボクセルマッピングの準備
        m_itemManager.InitializeItemStatus();
        m_projectileManager.InitializeObjectsStatus();

		if (m_loadGame)
		{
			SaveData saveData;
            // セーブデータから読み込み
            if (SaveSystem::get_instance().LoadGame(saveData))
            {
                m_ground.LoadScene(saveData.currentSceneID);
				m_player->LoadSaveData(saveData);
				m_itemManager.LoadSaveData(saveData);
            }
            else
            {
                m_ground.LoadScene(SceneID::Spring_Day);
            }

			m_loadGame = false;
		}
        else
        {
            m_ground.ReinitializeSceneObjects();
            m_ground.LoadScene(SceneID::Spring_Day);
        }

    }

    if (!m_currLoadCtx.finishInitialLoad)
    {
        m_loadStatus.totalTasks += m_enemyManager.GetTotalEnemyLoadCnt();
        m_loadStatus.totalTasks += m_itemManager.GetTotalItemLoadCnt();
        m_loadStatus.totalTasks += m_projectileManager.GetTotalProjectileLoadCnt();
        m_loadStatus.totalTasks += m_player->GetTotalModelLoadCnt();
        m_loadStatus.totalTasks += m_ground.GetTotalModelLoadCnt();
    }

}

void GameSystem::HandleInput(void)
{
    HandlePauseInput();

    //if (m_mode == GameMode::GAME)
    //{
    //    auto& fader = UIManager::get_instance().GetFader();

    //    if (InputManager::get_instance().GetKeyboardTrigger(DIK_RETURN))
    //    {
    //        if (!fader.IsFading())
    //        {
    //            struct ReturnToTitleContext
    //            {
    //                GameSystem* gameSystem;
    //            };

    //            fader.StartFadeOut(); // 画面を黒くフェードイン

    //            DeferredTaskOptions options{};
    //            options.async = false; // 同期で実行
    //            options.debugName = "Switch_Scene"; // デバッグ用の名前
    //            auto* ctx = new ReturnToTitleContext{ this }; // コンテキストを作成して渡す

    //            RawDeferredTask::Add(
    //                ctx, // コンテキストを渡す
    //                [](void* ptr)
    //                {
    //                    GameSystem* self = static_cast<ReturnToTitleContext*>(ptr)->gameSystem;
    //                    if (self->m_ground.GetCurrentScene() == SceneID::SCENE_1)
    //                        self->m_ground.LoadScene(SceneID::SCENE_2);
    //                    else
    //                        self->m_ground.LoadScene(SceneID::SCENE_1);
    //                    UIManager::get_instance().GetFader().StartFadeIn(); // タイトル画面をフェードアウト表示
    //                },
    //                [](void* ptr)
    //                {
    //                    return UIManager::get_instance().GetFader().IsFullyHidden(); // 完全に暗くなったら実行する♪
    //                },
    //                options
    //            );
    //        }
    //    }

    //    if (InputManager::get_instance().GetKeyboardTrigger(DIK_BACK))
    //    {
    //        if (!fader.IsFading())
    //        {
    //            struct ReturnToTitleContext
    //            {
    //                GameSystem* gameSystem;
    //            };

    //            fader.StartFadeOut(); // 画面を黒くフェードイン

    //            DeferredTaskOptions options{};
    //            options.async = false; // 同期で実行
    //            options.debugName = "Return_To_Title"; // デバッグ用の名前
    //            auto* ctx = new ReturnToTitleContext{ this }; // コンテキストを作成して渡す

    //            RawDeferredTask::Add(
    //                ctx, // コンテキストを渡す
    //                [](void* ptr)
    //                {
    //                    GameSystem* self = static_cast<ReturnToTitleContext*>(ptr)->gameSystem;
    //                    self->m_ground.UnloadCurrentScene();
    //                    self->Update();
    //                    self->ChangeMode(GameMode::TITLE);
    //                    UIManager::get_instance().GetFader().StartFadeIn(); // タイトル画面をフェードアウト表示
    //                },
    //                [](void* ptr)
    //                {
    //                    return UIManager::get_instance().GetFader().IsFullyHidden(); // 完全に暗くなったら実行する♪
    //                },
    //                options
    //            );
    //        }
    //    }

    //}

}

void GameSystem::HandlePauseInput(void)
{
    if ((m_inputManager.GetKeyboardTrigger(KEY_PAUSE) && m_mode == GameMode::GAME && !m_inDialogue)
        || m_resumeBtnClick == true)
    {
        // ゲームが一時停止中かどうかを確認
        if (IsPaused())
        {
			if (m_UIManager.IsFading())
				return; // フェード中は何もしない

            // ゲームを再開
            ResumeGame();
            m_UIManager.PopAllModals();
        }
        else
        {
            // ゲームを一時停止
            PauseGame();
        }

        m_resumeBtnClick = false;
    }
}

float GameSystem::CalculateCurrentLoadingProgress(void)
{
    int doneTasks = 0;
    if (!m_currLoadCtx.finishInitialLoad)
    {
        doneTasks += m_enemyManager.GetEnemyLoadedCnt();
        doneTasks += m_itemManager.GetItemLoadedCnt();
        doneTasks += m_projectileManager.GetProjectileLoadedCnt();
        doneTasks += m_player->GetCurrentModelLoadCnt();
        doneTasks += m_ground.GetCurrentModelLoadCnt();
    }

    m_loadStatus.doneTasks = doneTasks;
    int totalTasks = m_loadStatus.totalTasks;

    return totalTasks > 0 ? static_cast<float>(doneTasks) / static_cast<float>(totalTasks) : 1.0f;
}

bool GameSystem::IsAllResourceReady(void) const
{
    return m_loadStatus.totalTasks == m_loadStatus.doneTasks;
}

// 各モードの更新
void GameSystem::UpdateTitle(void)
{
    // タイトル画面のロジック
}

void GameSystem::UpdateGame(void)
{
    m_ground.Update(); // 地面の更新処理

    // ポーズフラグが立っている場合はゲームを一時停止
	if (m_pauseFlag)
        PauseGame();

    // ゲームが一時停止中
    if (m_isPaused)
        return;

    m_isAltDown = m_inputManager.GetKeyboardPress(DIK_LALT) || m_inputManager.GetKeyboardPress(DIK_RALT);

    if (m_isAltDown)
        m_cursorManager.OnEnterVisibleTemp();
    else
        m_cursorManager.OnExitVisibleTemp();

    m_player->Update(); // プレイヤーの更新処理
    m_skybox.Update(); // Skyboxの更新処理
    m_enemyManager.Update(); // エネミーの更新処理
	m_itemManager.Update(); // アイテムの更新処理
	m_projectileManager.Update(); // 弾の更新処理
	m_decalRenderer.Update(); // デカールの更新処理
    m_effectSystem.Update(); // エフェクトの更新処理

#ifdef _DEBUG
    m_shaderManager.CollectAndReloadShaders(m_renderer.GetDevice());
#endif // !_ DEBUG
}

void GameSystem::UpdateLoading(void)
{
    if (m_loadingScene)
    {
        if (!m_currLoadCtx.finishInitialLoad)
        {
            m_enemyManager.Update();
            m_itemManager.Update();
            m_projectileManager.Update();
            m_player->Update();
            m_ground.Update();
        }

        // 現在の読み込み進行を設定（0.0～1.0）
        float progress = CalculateCurrentLoadingProgress();
        m_loadingScene->SetProgress(progress);

        // 全てのリソースが読み込まれたら通知する
        if (IsAllResourceReady())
        {
            if (!m_currLoadCtx.finishInitialLoad)
            {
                m_currLoadCtx.finishInitialLoad = true;
				//m_player->ResetPlayerPosition(); // プレイヤーの位置をリセット
            }

            m_loadingScene->NotifyResourceLoadComplete();
        }

        // ローディングが完全に終了したら、ゲームに切り替える
        if (m_loadingScene->IsFinished())
        {
            // 画面が黒い状態からゲーム画面へフェードインを開始
            m_UIManager.GetFader().StartFadeIn();
			AudioManager::get_instance().StopSound();
   //         switch (m_ground.GetCurrentSceneID())
   //         {
			//case SceneID::Spring_Day:
   //         case SceneID::Spring_Night:
   //             AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_BGM_spring);
			//	break;
			//case SceneID::Summer_Day:
			//case SceneID::Summer_Night:
			//	AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_BGM_summer);
			//	break;
			//case SceneID::Autumn_Day:
			//case SceneID::Autumn_Night:
			//	AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_BGM_autumn);
			//	break;
			//case SceneID::Winter_Day:
			//case SceneID::Winter_Night:
			//	AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_BGM_winter);
			//	break;
   //         default:
   //             break;
   //         }
            AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_BGM_spring);
            ChangeMode(GameMode::GAME);
        }
    }
}

void GameSystem::UpdateResult(void)
{
    // リザルト画面のロジック
}

// 各モードの描画
void GameSystem::DrawTitle(void)
{
    // タイトル画面の描画

}

void GameSystem::DrawGame(void)
{
    // スカイボックスの描画
    m_skybox.Draw(XMLoadFloat4x4(&m_camera.GetViewMatrix()), XMLoadFloat4x4(&m_camera.GetProjMatrix()));

    m_renderer.SetDepthMode(DepthMode::Enable); // 深度テストを有効に
    m_renderer.SetCullingMode(CULL_MODE_BACK); // カリングモードを有効に

    m_renderer.SetStaticModelInputLayout(); // モデルの入力レイアウトを設定
    m_renderer.SetRenderObject(); // モデルの描画を設定

    // ライティングを無効
    m_lightManager.SetLightEnable(FALSE);
    m_renderer.SetRenderLayer(RenderLayer::LAYER_1); // UIの描画レイヤーを設定
    m_enemyManager.DrawUI(EnemyUIType::HPGauge); // HPゲージの描画
    m_renderer.SetRenderLayer(RenderLayer::DEFAULT);
    m_enemyManager.DrawUI(EnemyUIType::HPGaugeCover); // HPゲージカバーの描画
    // ライティングを有効に
    m_lightManager.SetLightEnable(TRUE);

    m_ground.Draw(); // 地面の描画

    m_itemManager.Draw();
    m_projectileManager.Draw();
    
    m_renderer.SetSkinnedMeshInputLayout(); // スキニングメッシュの入力レイアウトを設定
    m_renderer.SetRenderSkinnedMeshModel(); // スキニングメッシュモデルの描画を設定
    m_player->Draw(); // プレイヤーの描画
    m_enemyManager.Draw(); // エネミーの描画
    m_ground.Draw(); // 地面の描画

    m_renderer.SetRenderInstance(); // インスタンスの描画を設定
    m_ground.Draw(); // 地面の描画


    m_renderer.SetDepthMode(DepthMode::Effect); // パーティクルの深度設定
    m_renderer.SetVFXInputLayout(); // VFXの入力レイアウトを設定
    m_renderer.SetRenderVFX(); // VFXの描画を設定
    m_effectSystem.Draw(); // エフェクトの描画

    m_renderer.SetVFXInputLayout(); // VFXの入力レイアウトを設定
    m_renderer.SetRenderVFX(); // VFXの描画を設定
    m_player->DrawEffect(); // プレイヤーのエフェクト描画

    m_decalRenderer.Draw(); // デカールの描画

	if (m_enemyManager.GetBoss())
        m_enemyManager.GetBoss()->DrawEffect(); // ボスのエフェクト描画
    m_renderer.SetBlendState(BLEND_MODE_ALPHABLEND); // ブレンドステートを設定
}

void GameSystem::DrawResult(void)
{
    // リザルト画面の描画
}