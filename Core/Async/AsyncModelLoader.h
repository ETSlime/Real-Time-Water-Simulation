#pragma once
//=============================================================================
//
// モデルちゃんの順番管理もバッチリ～非同期読み込み受付嬢　[AsyncModelLoader.h]
// Author : 
// .fbx や .obj モデルを非同期で読み込み、リクエストを優先度つきで並べて処理してくれる超有能ローダーちゃんっ
// ロード完了チェックや失敗検知、GPUリソースへの反映もすべてお任せ！！モデル界の受付嬢なの～っ！
//
//=============================================================================
#include "main.h"
#include "Utility/SingletonBase.h"
#include "Utility/HashMap.h"
#include "Model/Model.h"
#include "Model/SkinnedMeshModel.h"
#include "Scene/GameObject.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_MODEL_LOAD_TASK (128) // 最大モデル数

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct IModelLoadTask
{
    char* modelPath = nullptr;                      // モデルパス（読み取り専用）
    bool isDone = false;                            // パース完了フラグ
    bool hasFailed = false;                         // 失敗フラグ
    ModelType modelType = ModelType::Default;

    virtual void* GetParsedDataRaw() const = 0; // 型非依存でパースデータ取得
    virtual ~IModelLoadTask() = default;             // RTTI、dynamic_cast のため
}; // モデル非同期読み込みタスク（基底クラス）

template<typename ParsedDataType>
struct ModelLoadTask : public IModelLoadTask
{
    ParsedDataType* parsedData = nullptr;           // パース済みデータ（CreateGPUResources に渡す）

    // 型安全な取得関数（parsedData を返す）
    ParsedDataType* GetParsedData() const 
    {
        return parsedData;
    }
    void* GetParsedDataRaw() const override 
    {
        return parsedData;
    }

    virtual ~ModelLoadTask() 
    {
        SAFE_DELETE(parsedData);
    }
}; // テンプレートを用いた型安全なモデル読み込みタスク

struct StaticModelLoadTask : public ModelLoadTask<StaticModelData>
{
    unsigned int pendingTextureCount = 0;       // 要求テクスチャ数
    unsigned int completedTextureCount = 0;     // 完了テクスチャ数
    bool texturesReady = false;                 // テクスチャ読み込み完了 
    AxisFlip flip = AxisFlip::None;
}; // 静的モデル用の読み込みタスク


struct SkinnedMeshModelLoadTask : public ModelLoadTask<SkinnedMeshModelData>
{
    char* modelName = nullptr;                           // モデル名
    AnimClipName animClipName = AnimClipName::ANIM_NONE; // 初期再生アニメーション名
    SkinnedModelType modelType = SkinnedModelType::Default; // スキンモデルの種別
};

struct DeferredRequestInfo
{
    int priority = 0;               // 優先度（数値が大きいほど優先）
    char* requestedBy = nullptr;    // 要求元
    float requestTime = 0.0f;       // 登録された時間（表示やデバッグ用）

    char* modelPath = nullptr;
    char* modelName = nullptr;
    AxisFlip flip = AxisFlip::None;
    SkinnedModelType modelType = SkinnedModelType::Default;
};

struct DeferredEntry 
{
    const char* modelPath = nullptr;
    const char* modelName = nullptr;
    const char* modelFullPath = nullptr;
    SkinnedModelType modelType = SkinnedModelType::Default;
    AxisFlip flip = AxisFlip::None;
    DeferredRequestInfo info;
};

class AsyncModelLoader : public SingletonBase<AsyncModelLoader>
{
public:
    AsyncModelLoader() {};
    ~AsyncModelLoader();

    // リクエストが未登録なら登録
    void RequestLoadIfNotQueued(const char* modelPath, AxisFlip flip = AxisFlip::None, int priority = 0, const char* source = nullptr);
    void RequestLoadIfNotQueued(const char* modelPath, const char* modelName, const char* modelFullPath, 
        SkinnedModelType type, int priority = 0, const char* source = nullptr);

    // 非同期ロード要求を追加
    void EnqueueLoad(const char* modelPath, AxisFlip flip = AxisFlip::None);
    void EnqueueLoad(const char* modelPath, const char* modelName, const char* modelFullPath, SkinnedModelType type);

    // 毎フレーム完了チェック
    void Update(void); 

    // ロード完了か判定
    template<typename TaskType>
    bool IsLoaded(const char* modelPath) const;
    // ロード失敗か判定
    bool HasFailed(const char* modelPath) const;

    // GPUリソース作成（主スレッド）
    Model* CreateModelIfReady(const char* modelPath);     
    SkinnedMeshModel* CreateSkinnedMeshModelIfReady(const char* modelPath);

private:
    void AsyncModelLoader_LoadModel(void* param);
    bool AsyncModelLoader_CanStart(void* ptr) { return true; }
    void AsyncFbxModelLoader_LoadModel(void* param);
    void RequestModelTexture(const char* path, ID3D11ShaderResourceView** out, StaticModelLoadTask* task);

    SimpleArray<DeferredEntry> m_cachedSortArray;
    HashMap<const char*, IModelLoadTask*, CharPtrHash, CharPtrEquals> m_taskMap =
		HashMap<const char*, IModelLoadTask*, CharPtrHash, CharPtrEquals>(MAX_MODEL_LOAD_TASK, CharPtrHash(), CharPtrEquals());
    HashMap<const char*, DeferredRequestInfo, CharPtrHash, CharPtrEquals> m_deferredRequests =
        HashMap<const char*, DeferredRequestInfo, CharPtrHash, CharPtrEquals>(MAX_MODEL_LOAD_TASK, CharPtrHash(), CharPtrEquals());

};

template<typename TaskType>
bool AsyncModelLoader::IsLoaded(const char* modelPath) const
{
    if (!m_taskMap.contains(modelPath)) return false;

    IModelLoadTask* base = m_taskMap.at(modelPath);
    TaskType* task = static_cast<TaskType*>(base);

    return task->isDone && task->GetParsedDataRaw();
}
