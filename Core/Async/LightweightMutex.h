#pragma once
#include "main.h"

//*****************************************************************************
// 軽量ミューテックスロックの実装 (Windows 限定)
//*****************************************************************************
class LightweightMutex 
{
private:
    volatile LONG locked = 0; // ロック状態: 0 = 触可, 1 = ロック中

public:
    // ロック関数
    void lock() 
    {
        while (InterlockedCompareExchange(&locked, 1, 0) != 0) 
        {
            Sleep(0); // CPUリリースを避けるためのユーザーモードの調整
        }
    }

    // アンロック関数
    void unlock() 
    {
        InterlockedExchange(&locked, 0);
    }
};

//*****************************************************************************
// RAII のロックガード
//*****************************************************************************
class LightweightLockGuard 
{
private:
    LightweightMutex& m;
public:
    LightweightLockGuard(LightweightMutex& m_) : m(m_) { m.lock(); }
    ~LightweightLockGuard() { m.unlock(); }
};

//*****************************************************************************
// オプションロックガード（RAII + 有効制御）
//*****************************************************************************
class OptionalLockGuard 
{
    LightweightMutex* lockPtr = nullptr;
public:
    OptionalLockGuard(LightweightMutex* lock, bool enable) 
    {
        if (enable && lock)
        {
            lockPtr = lock;
            lockPtr->lock();
        }
    }
    ~OptionalLockGuard() 
    {
        if (lockPtr)
            lockPtr->unlock();
    }
};
