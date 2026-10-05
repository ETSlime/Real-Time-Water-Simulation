//=============================================================================
//
// [TweenManager.cpp]
// Author : 
// 
//=============================================================================
#include "UI/Base/TweenManager.h"
#include "UI/Base/UIElement.h"

bool TweenTask::Update(float deltaTime, ISpriteTransformable* target)
{
    if (!target) return true;

    elapsed += deltaTime;
    float t = min(elapsed / duration, 1.0f);
    t = ApplyEase(t, ease);

    switch (type) 
    {
    case TweenTargetType::Position:
        target->SetPosition(Lerp(startVec2, endVec2, t));
        break;
    case TweenTargetType::Scale:
        target->SetScale(Lerp(startVec2, endVec2, t));
        break;
    case TweenTargetType::Rotation:
        target->SetRotation(LerpFloat(startFloat, endFloat, t));
        break;
    }

    if (elapsed >= duration) 
    {
        finished = true;

		if (onComplete) onComplete(target); // コールバックが設定されていれば呼び出す

		AdvanceToNextTask(); // 次のタスクに進む
    }

	return finished && !next; // 全てのタスクが完了したかどうかを返す
}

void TweenTask::AdvanceToNextTask(void)
{
    if (!next) return; // 次のタスクが存在しない場合は何もしない

    TweenTask* oldNext = next;

    // 次のタスクのデータを現在のタスクに移行する
    type = oldNext->type;
    duration = oldNext->duration;
    elapsed = 0.0f;              // 経過時間をリセット
    ease = oldNext->ease;
    startVec2 = oldNext->startVec2;
    endVec2 = oldNext->endVec2;
    startFloat = oldNext->startFloat;
    endFloat = oldNext->endFloat;
    onComplete = oldNext->onComplete;
    finished = false;            // 新しいタスクはまだ未完了

    // oldNext の残りのチェーンを引き継ぐ
    next = oldNext->next;

    // oldNext を解放する（チェーンは保持したまま）
    SAFE_DELETE(oldNext);
}

float TweenTask::ApplyEase(float t, EaseType ease)
{
	auto EaseInQuad = [](float t) -> float {
		return t * t; // t^2
		};

	auto EaseOutQuad = [](float t) -> float {
		return t * (2 - t); // 2t - t^2
		};

	auto EaseInOutQuad = [](float t) -> float {
		return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
		};

    auto EaseInCubic = [](float t) -> float {
        return t * t * t; // t^3
        };

	auto EaseOutCubic = [](float t) -> float {
		float p = t - 1.0f;
		return p * p * p + 1.0f; // t^3 - 3t^2 + 3t + 1
		};

	auto EaseInOutCubic = [](float t) -> float {
        return t < 0.5f
            ? 4 * t * t * t
            : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
		};

    auto EaseOutBounce = [](float t) -> float {
        if (t < 1 / 2.75f) {
            return 7.5625f * t * t;
        }
        else if (t < 2 / 2.75f) {
            t -= 1.5f / 2.75f;
            return 7.5625f * t * t + 0.75f;
        }
        else if (t < 2.5f / 2.75f) {
            t -= 2.25f / 2.75f;
            return 7.5625f * t * t + 0.9375f;
        }
        else {
            t -= 2.625f / 2.75f;
            return 7.5625f * t * t + 0.984375f;
        }
        };

    auto EaseInBounce = [&](float t) -> float {
        return 1.0f - EaseOutBounce(1.0f - t);
        };

    auto EaseInOutBounce = [&](float t) -> float {
        return t < 0.5f
            ? EaseInBounce(t * 2.0f) * 0.5f
            : EaseOutBounce(t * 2.0f - 1.0f) * 0.5f + 0.5f;
        };

    auto EaseInElastic = [](float t) -> float {
        if (t == 0 || t == 1) return t;
        float c4 = (2 * XM_PI) / 0.3f;
        return -powf(2.0f, 10.0f * (t - 1.0f)) * sinf((t - 1.075f) * c4);
        };

    auto EaseOutElastic = [](float t) -> float {
        if (t == 0 || t == 1) return t;
        float c4 = (2 * XM_PI) / 0.3f;
        return powf(2.0f, -10.0f * t) * sinf((t - 0.075f) * c4) + 1.0f;
        };

    auto EaseInOutElastic = [](float t) -> float {
        if (t == 0 || t == 1) return t;
        float c5 = (2 * XM_PI) / 0.45f;
        if (t < 0.5f)
            return -(powf(2.0f, 20.0f * t - 10.0f) * sinf((20.0f * t - 11.125f) * c5)) * 0.5f;
        else
            return powf(2.0f, -20.0f * t + 10.0f) * sinf((20.0f * t - 11.125f) * c5) * 0.5f + 1.0f;
        };

    switch (ease) 
    {
	case EaseType::Linear:          return t; // 線形補間
	case EaseType::InQuad:          return EaseInQuad(t); // t^2
	case EaseType::OutQuad:         return EaseOutQuad(t); // 2t - t^2
	case EaseType::InOutQuad:       return EaseInOutQuad(t); // 2t^2 (t < 0.5) or -1 + (4 - 2t)t (t >= 0.5)
	case EaseType::InCubic:         return EaseInCubic(t); // t^3
	case EaseType::OutCubic:        return EaseOutCubic(t); // t^3 - 3t^2 + 3t + 1
	case EaseType::InOutCubic:      return EaseInOutCubic(t); // 4t^3 - 6t^2 + 4t

    case EaseType::InElastic:       return EaseInElastic(t);
    case EaseType::OutElastic:      return EaseOutElastic(t);
    case EaseType::InOutElastic:    return EaseInOutElastic(t);
    case EaseType::InBounce:        return EaseInBounce(t);
    case EaseType::OutBounce:       return EaseOutBounce(t);
    case EaseType::InOutBounce:     return EaseInOutBounce(t);

    default: return t;
    }
}

XMFLOAT2 TweenTask::Lerp(const XMFLOAT2& a, const XMFLOAT2& b, float t)
{
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
}

float TweenTask::LerpFloat(float a, float b, float t)
{
    return a + (b - a) * t;
}

void TweenManager::Update(void)
{
    for (auto& task : m_tasks)
    {
        if (!task->target) continue;

		// UIElementの有効性をチェック
        if (auto* element = dynamic_cast<UIElement*>(task->target))
        {
            if (!element->IsValid())
            {
                m_toRemove.push_back(task);
                continue;
            }
        }

        // TweenTask::Update が true を返したら、チェーンが完全終了
        if (task->tweenTask->Update(m_timer.GetDeltaTime(), task->target))
            m_toRemove.push_back(task);

		//// タスクの更新
  //      if (!task->tweenTask->finished)
  //          task->tweenTask->Update(m_timer.GetDeltaTime(), task->target);

		//// タスクが完了したかどうかをチェック
  //      if (task->AllTasksFinished())
  //          m_toRemove.push_back(task); // 完了したタスクを削除予定リストに追加
    }

    // 完了したタスクを削除
    for (auto& task : m_toRemove)
    {
        int index = m_tasks.find_index(task);
        if (index >= 0)
        {
			SAFE_DELETE(task); // メモリ解放
            m_tasks.erase(index);
        }
    }
    m_toRemove.clear();
}

void TweenManager::Shutdown(void)
{
	for (auto& task : m_tasks)
	{
		SAFE_DELETE(task); // メモリ解放
	}

	m_tasks.clear();
	m_toRemove.clear();
}

TweenEntry* TweenManager::AddMove(ISpriteTransformable* target, XMFLOAT2 to, float duration, EaseType ease, void(*callback)(ISpriteTransformable*))
{
    TweenTask* t = new TweenTask();
    t->type = TweenTargetType::Position;
    t->duration = duration;
    t->ease = ease;
    t->startVec2 = target->GetPosition();
    t->endVec2 = to;
    t->onComplete = callback;

    TweenEntry* entry = new TweenEntry(target, t);
    m_tasks.push_back(entry);

    return entry;
}

TweenEntry* TweenManager::AddScale(ISpriteTransformable* target, XMFLOAT2 to, float duration, EaseType ease, void(*callback)(ISpriteTransformable*))
{
    TweenTask* t = new TweenTask();
    t->type = TweenTargetType::Scale;
    t->duration = duration;
    t->ease = ease;
    t->startVec2 = target->GetScale();
    t->endVec2 = to;
    t->onComplete = callback;

    TweenEntry* entry = new TweenEntry(target, t);
    m_tasks.push_back(entry);

    return entry;
}

TweenEntry* TweenManager::AddRotate(ISpriteTransformable* target, float to, float duration, EaseType ease, void(*callback)(ISpriteTransformable*))
{
    TweenTask* t = new TweenTask();
    t->type = TweenTargetType::Rotation;
    t->duration = duration;
    t->ease = ease;
    t->startFloat = target->GetRotation();
    t->endFloat = to;
    t->onComplete = callback;

    TweenEntry* entry = new TweenEntry(target, t);
    m_tasks.push_back(entry);

    return entry;
}

void TweenManager::RemoveTweensForTarget(ISpriteTransformable* target)
{
	if (!target) return;

	// 対象のタスクを削除
    for (int i = m_tasks.getSize() - 1; i >= 0; --i)
    {
        if (m_tasks[i]->target == target)
        {
            SAFE_DELETE(m_tasks[i]);
            m_tasks.erase(i);
        }
    }
}
