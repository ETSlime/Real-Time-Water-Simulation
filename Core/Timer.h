#pragma once
//=============================================================================
//
// フレーム経過時間・累積時間を計測するタイマークラス [Timer.h]
// Author : 
// フレーム間の経過時間・累積時間を精密に測定し、
// 固定タイムステップおよびスケーリング対応の時間制御を提供する
// 
//=============================================================================
#include "main.h"
#include "Utility/SingletonBase.h"
#include "Utility/Debug/Debugproc.h"

class Timer : public SingletonBase<Timer>
{
private:
    LARGE_INTEGER frequency;
    LARGE_INTEGER lastTime;
    float elapsedTime;
    float deltaTime;
    float scaledDeltaTime;
    const float targetFrameRate = 50.0f;
    const float timeScale = targetFrameRate;
    const float maxDeltaTime = 1.0f / 10.0f;
    DebugProc& debugProc = DebugProc::get_instance();

public:
    Timer() 
    {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&lastTime);

        Init();
    }

    void Init()
    {
        Update();
    }

    void Update() 
    {
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        deltaTime = static_cast<float>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
        if (deltaTime > maxDeltaTime)
        {
            deltaTime = maxDeltaTime; // 制限をかける
        }
        lastTime = currentTime;
        scaledDeltaTime = deltaTime * timeScale; // スケーリング
        elapsedTime += deltaTime;

#ifdef _DEBUG
        debugProc.PrintDebugProc("scaledDeltaTime: %f\n", scaledDeltaTime);
        debugProc.PrintDebugProc("deltaTime: %f\n", deltaTime);
#endif // _DEBUG

    }

    float GetDeltaTime(void) { return deltaTime; }
    float GetScaledDeltaTime(void) { return scaledDeltaTime; }
    //float GetScaledDeltaTime(void) { return 1; }}
    float GetElapsedTime(void) { return elapsedTime; }
};