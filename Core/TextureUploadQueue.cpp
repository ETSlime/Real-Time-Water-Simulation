//=============================================================================
//
//  [TextureUploadQueue.cpp]
// Author :
// 
//=============================================================================
#include "Core/TextureUploadQueue.h"

void TextureUploadQueue::Init(void)
{
    m_taskMap = HashMap<const char*, TextureUploadTask*, CharPtrHash, CharPtrEquals>(
        MAX_TEXTURE_UPLOAD_SIZE,
        CharPtrHash(),
        CharPtrEquals());

    m_pendingCallbackMap = HashMap<const char*, SimpleArray<PendingCallback>, CharPtrHash, CharPtrEquals>(
        MAX_TEXTURE_UPLOAD_SIZE,
        CharPtrHash(),
        CharPtrEquals());
}

bool TextureUploadQueue::TextureUploadQueue::HasTask(const char* path)
{
    m_lock.Lock();
    bool exists = m_taskMap.search(path) != nullptr;
    m_lock.Unlock();
    return exists;
}

void TextureUploadQueue::Push(TextureUploadTask* task)
{
    m_lock.Lock();

    if (!m_taskMap.search(task->path))
    {
        m_taskMap.insert(task->path, task);
    }

    m_lock.Unlock();
}

TextureUploadTask* TextureUploadQueue::GetTask(const char* path)
{
    m_lock.Lock();
    TextureUploadTask* result = m_taskMap[path];
    m_lock.Unlock();
    return result;
}

void TextureUploadQueue::ShutDown(void)
{
    for (auto& it : m_taskMap)
    {
        TextureUploadTask* task = it.value;
        if (!task) continue;

        // バッファ解放
        SAFE_DELETE_ARRAY(task->fileBuffer);

        // イベント解放
        if (task->finishEvent)
        {
            ThreadPool::get_instance().ReleaseFinishEvent(task->finishEvent);
            task->finishEvent = nullptr;
        }

        // 失敗通知
        if (task->onFinish)
        {
            task->onFinish(false, task->userData);
        }

        SAFE_DELETE(task);
    }

    m_taskMap.clear();
}

void TextureUploadQueue::CreateAndUploadSRV(TextureUploadTask* task)
{
    if (!task->fileBuffer || task->bufferSize == 0)
    {
        // 読み込み失敗（ファイルが無い、空など）
        if (task->onFinish)
            task->onFinish(false, task->userData);

        // 待機中のリクエスタに通知
        NotifyPendingCallbacks(task->path, nullptr, false);

        // イベントを完了状態に設定
        if (task->finishEvent)
            ThreadPool::get_instance().ReleaseFinishEvent(task->finishEvent);

        return; // 失敗なのでSRV生成など行わずに終了
    }

    ID3D11ShaderResourceView* createdSRV = nullptr;
    HRESULT hr = D3DX11CreateShaderResourceViewFromMemory(
        m_renderer.GetDevice(),
        task->fileBuffer,
        task->bufferSize,
        nullptr,
        nullptr,
        &createdSRV,
        nullptr);

    // メインの outSRV へ代入
    if (createdSRV)
        *(task->outSRV) = createdSRV;
    // 登録された onFinish を呼び出し
    if (task->onFinish)
        task->onFinish(SUCCEEDED(hr), task->userData);

    // 待機中のリクエスタに通知
    NotifyPendingCallbacks(task->path, createdSRV, SUCCEEDED(hr));

    if (SUCCEEDED(hr))
        m_textureMgr.RegisterSRV(task->path, createdSRV);

    SAFE_DELETE_ARRAY(task->fileBuffer);

    // イベントを完了状態に設定
    if (task->finishEvent)
        ThreadPool::get_instance().ReleaseFinishEvent(task->finishEvent);
}

void TextureUploadQueue::Update()
{
    m_lock.Lock();

    for (auto& it = m_taskMap.begin(); it != m_taskMap.end();)
    {
        TextureUploadTask* task = (*it).value;
        if (!task)
        {
            it = m_taskMap.erase(it);
            continue;
        }
            
        if (task->finishEvent && WaitForSingleObject(task->finishEvent, 0) != WAIT_OBJECT_0)
        {
            ++it;
            continue; // スキップ（次フレーム再確認）
        }

        // GPU アップロード処理
        CreateAndUploadSRV(task);

        // タスク削除
        it = m_taskMap.erase(it);
    }

    m_lock.Unlock();
}

void TextureUploadQueue::RegisterPendingCallback(
    const char* filename,
    ID3D11ShaderResourceView** outSRV,
    void(*onFinish)(bool, void*),
    void* userData)
{
    m_lock.Lock();

    m_pendingCallbackMap[filename].push_back({ outSRV, onFinish, userData });

    m_lock.Unlock();
}

void TextureUploadQueue::NotifyPendingCallbacks(
    const char* filename,
    ID3D11ShaderResourceView* srv,
    bool success)
{
    auto it = m_pendingCallbackMap.find(filename);
    if (it != m_pendingCallbackMap.end())
    {
        for (const auto& cb : it->value)
        {
            if (cb.outSRV)
                *(cb.outSRV) = srv;

            if (cb.onFinish)
                cb.onFinish(success, cb.userData);
        }

        // コールバックが全部通知されたので削除
        m_pendingCallbackMap.erase(it);
    }
}