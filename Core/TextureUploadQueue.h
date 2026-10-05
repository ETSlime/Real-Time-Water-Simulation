#pragma once
//=============================================================================
//
// テクスチャ非同期アップロード管理 [TextureUploadQueue.h]
// Author :
// GPUリソースへの非同期アップロードを一括管理するテクスチャ配送キューちゃんっ
// 読み込みリクエストを受け付けて、専用のワーカースレッドがバッファ読み込み → SRV生成を担当！
// 登録されたタスクは Update() により逐次処理され、完了時にはコールバック通知されるよっ
// 
//=============================================================================
#include "main.h"
#include "Core/TextureMgr.h"
#include "Core/Graphics/Renderer.h"
#include "Core/Async/ThreadPool.h"
#include "Core/Async/SpinLock.h"
#include "Utility/HashMap.h"
#include "Utility/SingletonBase.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_TEXTURE_PATH            (256)
#define MAX_TEXTURE_UPLOAD_SIZE     (256)

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct TextureUploadTask 
{
    char* path = nullptr; // テクスチャパス
    BYTE* fileBuffer = nullptr; // 読み込まれたファイルバッファ
    unsigned int bufferSize = 0; // バッファサイズ

    ID3D11ShaderResourceView** outSRV = nullptr; // 出力先ポインタ（モデルが保持）

    void (*onFinish)(bool success, void* userData); // コールバック関数
    void* userData = nullptr; // コールバックに渡すデータ

    HANDLE finishEvent = nullptr; // 主スレッド通知用（主スレッドからWaitForSingleObject可能）
}; // アップロードタスク構造体

struct PendingCallback
{
    ID3D11ShaderResourceView** outSRV; // 読み込み後に代入するポインタ
    void(*onFinish)(bool success, void* userData); // 完了時に呼ばれる関数ポインタ
    void* userData; // 任意のユーザーデータ
}; // 非同期テクスチャ読み込みを待機しているコールバック情報構造体

class TextureUploadQueue : public SingletonBase<TextureUploadQueue>
{
public:
    void Init(void);
    void ShutDown(void);

    // 読み込み済みバッファがあればSRVを作成し、ペンディングコールバックも通知する
    void Update(void);
    // 新しい非同期アップロードを登録
    void Push(TextureUploadTask* task);

    bool HasTask(const char* path);
    TextureUploadTask* GetTask(const char* path);

    void RegisterPendingCallback(const char* filename, ID3D11ShaderResourceView** outSRV, 
        void(*onFinish)(bool, void*), void* userData);

private:
    // ファイル読み込みが終わったらSRV生成を担当
    void CreateAndUploadSRV(TextureUploadTask* task);
    // 同じファイルを待ってる子たちに、出来たてのSRVをお届け~
    void TextureUploadQueue::NotifyPendingCallbacks(const char* filename, ID3D11ShaderResourceView* srv, bool success);

    // 現在ロード中のテクスチャタスクを パス をキーに管理
    HashMap<const char*, TextureUploadTask*, CharPtrHash, CharPtrEquals> m_taskMap;
    // ファイル名ごとに待機中のコールバックを記録するマップ
    HashMap<const char*, SimpleArray<PendingCallback>, CharPtrHash, CharPtrEquals> m_pendingCallbackMap;
    SpinLock m_lock;
    Renderer& m_renderer = Renderer::get_instance();
    TextureMgr& m_textureMgr = TextureMgr::get_instance();
};