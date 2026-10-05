#pragma once
//=============================================================================
//
// 先送りでも忘れない！完璧スケジュール係ちゃん [DeferredTaskSystem.h]
// Author : 
// 遅延タスク・非同期実行・タイムアウト管理をこなす、頼もしすぎる万能タスク調整クラスですっ！
// タスクを「今じゃないけどあとでね~」ってちゃんと覚えててくれる、几帳面お姉さんなの~
//
//=============================================================================
#include "main.h"
#include "Utility/SimpleArray.h"
#include "Utility/SingletonBase.h"
#include "Core/Timer.h"
#include "Core/Async/ThreadPool.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_DEFERRED_TASK_NUM		(256)

// ==========================
// ジェネリック延期タスク系統（任意のクラスで使用可）
// ==========================
typedef void (*DeferredFunc)(void*); // タスクを実行する関数の型
typedef bool (*DeferredCanRunFunc)(void*); // タスクが実行可能かどうかを判定する関数（nullptr の場合は常に実行可能）
typedef void (*CallbackFunc)(void*); // コールバック関数の型

struct DeferredTask
{
	DeferredFunc executeFunc;         // 実行する関数
	DeferredCanRunFunc canRunFunc;    // 実行可否判定
	void* param;                      // 関数に渡すパラメータ
	float delayTime;                  // 遅延時間（秒）
	bool pauseDelay = false;		  // 遅延実行時に一時停止中でも実行するかどうか
	float elapsedTime;                // 経過時間（秒）
	float timeoutSec;				  // タイムアウト秒数（-1 は無制限）
	float timeoutElapsed;			  // タイムアウト経過時間（秒）
	float runningTime;				  // 実行開始時間（秒）
	bool isRunning;					  // タスクが実行中かどうか
	bool runAsync;                    // 非同期実行
	HANDLE finishEvent;				  // タスク完了イベント（非同期実行時に使用）
	char* debugName;			      // デバッグ用の名前

	// コールバック関連
	CallbackFunc onComplete = nullptr;
	void* callbackParam = nullptr;
	bool callbackAsync = false;

}; // タスクの情報を保持する構造体


struct DeferredTaskOptions 
{
	bool async = false; // 非同期実行かどうか
	float delay = 0.0f; // 遅延実行時間（秒）
	bool pauseDelay = false; // 遅延実行時に一時停止中でも実行するかどうか
	float timeout = -1.0f; // タイムアウト秒数（-1 は無制限）
	const char* debugName = nullptr; // デバッグ用の名前

	CallbackFunc onComplete = nullptr; // タスク完了時のコールバック関数
	void* callbackParam = nullptr; // コールバック関数に渡すパラメータ
	bool callbackAsync = false; // コールバックを非同期で実行するかどうか
}; // タスクのオプション設定

class DeferredTaskSystem : public SingletonBase<DeferredTaskSystem>, public IDebugUI
{
public:

	void Init(void);
	void ShutDown(bool wait = true);

	// タスクを追加
	void AddTask(DeferredFunc execFunc, void* param, DeferredCanRunFunc canRunFunc = nullptr, const DeferredTaskOptions& options = DeferredTaskOptions{});

	// タスクを更新（経過時間を進めて実行可能なタスクを実行）
	void Update(void);

	virtual void RenderImGui(void);
	virtual const char* GetPanelName(void) const { return "Deferred Task Monitor"; };

private:

	void WaitAll(void); // すべての非同期タスクが完了するまで待機
	void CancelAll(void); // すべてのタスクをキャンセル（実行中のタスクは終了するまで待つ）

	bool m_shutdown = false;
	SimpleArray<DeferredTask> m_tasks;
	Timer& m_timer = Timer::get_instance(); // タイマーインスタンス
};