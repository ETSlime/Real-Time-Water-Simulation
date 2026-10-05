#pragma once
#include "main.h"

//************************************************************
//  超軽量スピンロック（STLなし、atomicも不使用）
//************************************************************
class SpinLock
{
public:
    SpinLock() : m_flag(0) {}

    // ロック取得（ビジーウェイト）
    void Lock()
    {
        while (InterlockedExchange(&m_flag, 1) == 1)
        {
            // 忙しい待機（CPUを譲る）
            YieldProcessor(); // or Sleep(0);
        }
    }

    // ロック解除
    void Unlock()
    {
        InterlockedExchange(&m_flag, 0);
    }

private:
    volatile LONG m_flag; // 排他フラグ（0: free, 1: locked）
};


//************************************************************
// スコープロック（RAII形式）
//  - RAII によりスコープに入るとロック、スコープを出ると自動でアンロック。
//  - コピー/ムーブを禁止して誤用による二重アンロックを防止。
//************************************************************
class SpinLockGuard
{
public:
    SpinLockGuard(SpinLock& lock) noexcept : m_lock(lock)
    {
        m_lock.Lock();
    }

    ~SpinLockGuard() noexcept
    {
        m_lock.Unlock();
    }

    SpinLockGuard(const SpinLockGuard&) = delete;
    SpinLockGuard& operator=(const SpinLockGuard&) = delete;
    SpinLockGuard(SpinLockGuard&&) = delete;
    SpinLockGuard& operator=(SpinLockGuard&&) = delete;

private:
    SpinLock& m_lock;
};