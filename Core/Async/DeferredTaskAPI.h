#pragma once
//=============================================================================
//
// 関数をそっと未来に繋げるバインダー秘書ちゃん [DeferredTaskAPI.h]
// Author : 
// メンバー関数でも非メンバー関数でも、やさしく丁寧にスケジューリングしてくれるタスク登録APIクラスですっ！
// 実行条件や遅延時間、非同期設定もぜ～んぶまとめて面倒見てくれるしっかり者バインドちゃん
//
//=============================================================================
#include "Core/Async/DeferredTaskSystem.h"

// ==========================
// クラスのメンバー関数を繋ぐバインダ
// ==========================
template<typename T>
class DeferredTaskBinder 
{
public:
    typedef void (T::* DeferredMethodExec)(void*);
    typedef bool (T::* DeferredMethodCond)(void*);

    static void DispatchExec(void* ptr) 
    {
		// タスクの実行関数を呼び出す
        TaskContext* ctx = static_cast<TaskContext*>(ptr);
        T* obj = static_cast<T*>(ctx->object);
        (obj->*ctx->execFunc)(ctx->param);
        SAFE_DELETE(ctx);
    }

    static bool DispatchCond(void* ptr) 
    {
		// タスクの条件関数を呼び出す
        TaskContext* ctx = static_cast<TaskContext*>(ptr);
        T* obj = static_cast<T*>(ctx->object);
        return ctx->condFunc ? (obj->*ctx->condFunc)(ctx->param) : true;
    }

    static void Add(T* object, void* param, DeferredMethodExec exec, DeferredMethodCond cond = nullptr,
        DeferredTaskOptions& options = DeferredTaskOptions{})
    {
		options.debugName = options.debugName ? options.debugName : "DeferredTaskBinder";

		// タスクのコンテキストを作成
        TaskContext* ctx = new TaskContext();
        ctx->object = object;
        ctx->execFunc = exec;
        ctx->condFunc = cond;
        ctx->param = param;

		// 遅延実行タスクシステムに登録
        DeferredTaskSystem::get_instance().AddTask(
            DispatchExec,
            ctx,
            cond ? DispatchCond : nullptr,
            options
        );
    }

private:
	// タスクのコンテキスト構造体
    struct TaskContext 
    {
        T* object;
        DeferredMethodExec execFunc;
        DeferredMethodCond condFunc;
        void* param;
    };
};

// ==========================
// 非メンバー関数を繋ぐラッパー型
// ==========================
class RawDeferredTask 
{
public:
    typedef void (*RawExecFunc)(void*);
    typedef bool (*RawCondFunc)(void*);

    static void DispatchExec(void* param) 
    {
		// タスクの実行関数を呼び出す
        RawDeferredTaskContext* task = (RawDeferredTaskContext*)param;
        task->execFunc(task->context);
        SAFE_DELETE(task);
    }

    static bool DispatchCond(void* param) 
    {
		// タスクの条件関数を呼び出す
        RawDeferredTaskContext* task = (RawDeferredTaskContext*)param;
        return task->condFunc ? task->condFunc(task->context) : true;
    }

    static void Add(void* context, RawExecFunc exec, RawCondFunc cond = nullptr, DeferredTaskOptions& options = DeferredTaskOptions{})
    {
		options.debugName = options.debugName ? options.debugName : "RawDeferredTask";

        // コンテキストを含むタスク構造体を作成
        RawDeferredTaskContext* ctx = new RawDeferredTaskContext{ context, exec, cond, options.timeout, 0.0f };
        // 遅延実行タスクシステムに登録
        DeferredTaskSystem::get_instance().AddTask(
            DispatchExec, 
            ctx,
            cond ? DispatchCond : nullptr, 
            options
        );
    }

private:
	// タスクのコンテキスト構造体
    struct RawDeferredTaskContext 
    {
        void* context;
        void (*execFunc)(void*);
        bool (*condFunc)(void*);
        float timeoutSec = -1.0f;   // タイムアウト秒数（-1 は無制限）
        float elapsedSec = 0.0f;    // 経過時間
    };
};
