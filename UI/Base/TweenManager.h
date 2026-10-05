#pragma once
//=============================================================================
//
// [TweenManager.h]
// Author : 
// 
//=============================================================================
#include "Core/Timer.h"
#include "UI/Base/UISprite.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
enum class TweenTargetType 
{
    Position,
    Scale,
    Rotation,
};

enum class EaseType 
{
    Linear,
    InQuad,
    OutQuad,
    InOutQuad,
    InCubic,
	OutCubic,
	InOutCubic,
	InElastic,
	OutElastic,
	InOutElastic,
	InBounce,
	OutBounce,
	InOutBounce
};


class TweenTask 
{
public:
	TweenTask(void) = default;
    bool Update(float deltaTime, ISpriteTransformable* target);
    bool IsFinished(void) const { return elapsed >= duration; }
    TweenTargetType type = TweenTargetType::Position;
    float duration = 0.0f;
    float elapsed = 0.0f;
    EaseType ease = EaseType::Linear;

    XMFLOAT2 startVec2{}, endVec2{}; // 開始と終了の位置・スケール
	float startFloat = 0.0f, endFloat = 0.0f; // 開始と終了の回転

    bool finished = false;
	TweenTask* next = nullptr; // 次のタスクへのポインタ（連結リスト形式で管理）

    void (*onComplete)(ISpriteTransformable*) = nullptr; // コールバック関数

private:
    // チェーンされた次のタスクに進める関数
    void AdvanceToNextTask(void);
    float ApplyEase(float t, EaseType ease);

    XMFLOAT2 Lerp(const XMFLOAT2& a, const XMFLOAT2& b, float t);

    float LerpFloat(float a, float b, float t);
};

struct TweenEntry
{
	TweenEntry(ISpriteTransformable* target, TweenTask* tweenTask)
		: target(target), tweenTask(tweenTask), tailTask(tweenTask) {}

    ~TweenEntry(void)
    {
        TweenTask* current = tweenTask;
        while (current)
        {
            TweenTask* next = current->next;
            SAFE_DELETE(current);
            current = next;
        }
    }

    // 位置を移動するタスクを追加
    TweenEntry* ThenMove(const XMFLOAT2& to, float duration, EaseType ease)
    {
        TweenTask* newTask = new TweenTask();
        newTask->type = TweenTargetType::Position;

        // 連結リストの最後のタスクの終了位置を開始位置に設定
		newTask->startVec2 = tailTask ? tailTask->endVec2 : tweenTask->endVec2; 

        newTask->endVec2 = to;
        newTask->duration = duration;
        newTask->ease = ease;

        AppendTask(newTask);
        return this;
    }

	// スケールを変更するタスクを追加
    TweenEntry* ThenScale(const XMFLOAT2& to, float duration, EaseType ease)
    {
        TweenTask* newTask = new TweenTask();
        newTask->type = TweenTargetType::Scale;
        newTask->startVec2 = tweenTask->endVec2;
        newTask->endVec2 = to;
        newTask->duration = duration;
        newTask->ease = ease;

        AppendTask(newTask);
        return this;
    }

	// 回転を追加するタスクを追加
    TweenEntry* ThenRotate(float to, float duration, EaseType ease)
    {
        TweenTask* newTask = new TweenTask();
        newTask->type = TweenTargetType::Rotation;
        newTask->startFloat = tweenTask->endFloat;
        newTask->endFloat = to;
        newTask->duration = duration;
        newTask->ease = ease;

        AppendTask(newTask);
        return this;
    }

	// タスクを連結リストの末尾に追加
    void AppendTask(TweenTask* newTask)
    {
        if (!tailTask) 
        {
            tweenTask->next = newTask;
        }
        else 
        {
            tailTask->next = newTask;
        }

        tailTask = newTask;
    }

	// リストの最後のタスクを取得
    TweenTask* GetLastTask(TweenTask* current)
    {
        while (current->next) current = current->next;
        return current;
    }

    TweenEntry* OnComplete(void (*cb)(ISpriteTransformable*))
    {
        if (tailTask)
            tailTask->onComplete = cb;
        return this;
    }

    bool AllTasksFinished(void) const
    {
        TweenTask* current = tweenTask;
        while (current)
        {
            if (!current->finished)
                return false;
            current = current->next;
        }
        return true;
    }

    ISpriteTransformable* target = nullptr;
    TweenTask* tweenTask = nullptr;
    TweenTask* tailTask = nullptr;
};

class TweenManager : public SingletonBase<TweenManager>
{
public:
    void Update(void);
    void Shutdown(void);

	// タスクを追加する関数群
    TweenEntry* AddMove(ISpriteTransformable* target, XMFLOAT2 to, float duration, 
        EaseType ease = EaseType::Linear, void(*callback)(ISpriteTransformable*) = nullptr); // 位置を移動
    TweenEntry* AddScale(ISpriteTransformable* target, XMFLOAT2 to, float duration, 
        EaseType ease = EaseType::Linear, void(*callback)(ISpriteTransformable*) = nullptr); // スケールを変更
    TweenEntry* AddRotate(ISpriteTransformable* target, float to, float duration, 
        EaseType ease = EaseType::Linear, void(*callback)(ISpriteTransformable*) = nullptr); // 回転を変更

    void RemoveTweensForTarget(ISpriteTransformable* target);

private:


	SimpleArray<TweenEntry*> m_tasks; // 現在のタスク一覧
	SimpleArray<TweenEntry*> m_toRemove; // 削除予定のタスク

	Timer& m_timer = Timer::get_instance();
};
