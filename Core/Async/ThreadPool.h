#pragma once
//=============================================================================
//
// スレッドとイベントを操る影の天才執事ちゃん [ThreadPool.h]
// Author : 
// タスク実行と完了通知を静かにさばく非同期処理のエキスパート！イベントの再利用で無駄なくスマートに！
// マルチスレッドな世界を陰から支えるクール系お姉さまなの~
//
//=============================================================================
#include "main.h"
#include "Utility/SimpleArray.h"
#include "Utility/SingletonBase.h"

// ==========================
// イベントハンドル再利用用プール
// ==========================
class FinishEventPool 
{
public:
    // イベントを取得（再利用または新規作成）
    HANDLE Acquire() 
    {
        if (!m_pool.empty()) 
        {
            HANDLE h = m_pool.back();
			m_pool.pop_back();
            ResetEvent(h); // イベントの状態を初期化
            return h;
        }
        return CreateEvent(nullptr, TRUE, FALSE, nullptr); // マニュアルリセット、初期状態非シグナル
    }

    // イベントを解放（再利用用に保存）
    void Release(HANDLE h) 
    {
        m_pool.push_back(h);
    }

    // 全イベントを解放（終了時呼び出し）
    void Clear() 
    {
        for (HANDLE h : m_pool) 
        {
            CloseHandle(h);
        }
        m_pool.clear();
    }

private:
    SimpleArray<HANDLE> m_pool;
};


// ==========================
// ThreadPool 再利用
// ==========================
class ThreadPool : public SingletonBase<ThreadPool>
{
public:
    // タスク完了通知用イベントの取得
    HANDLE AcquireFinishEvent() 
    {
        return m_eventPool.Acquire();
    }

    // 使用済みイベントの再登録
    void ReleaseFinishEvent(HANDLE h) 
    {
        m_eventPool.Release(h);
    }

    // シャットダウン時に全イベントを解放
    void ClearEvents() 
    {
        m_eventPool.Clear();
    }

    void RunAsync(void (*func)(void*), void* param)
    {
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)func, param, 0, nullptr);
    }
private:
    FinishEventPool m_eventPool;
};