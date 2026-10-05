#pragma once
//=============================================================================
//
// シーン全体をしっかり仕切る舞台監督ちゃん [Scene.h]
// Author :
// 描画対象のGameObjectを登録・管理して、影やエフェクトにも必要な情報を提供してくれる頼れる子ですっ！
// ゲーム内のすべての主役たちが、彼女のリストから登場するの
// 
//=============================================================================
#include "main.h"
#include "Utility/SingletonBase.h"
#include "Utility/SimpleArray.h"

class ISceneEntity
{
public:
    virtual ~ISceneEntity() = default;

    virtual void Update(void) = 0;
    virtual void Draw(void) = 0;

    virtual void Destroy(void) {};

    virtual bool IsGameObject(void) const { return false; }
    virtual bool IsEffectRenderer(void) const { return false; }
};

class IGameObject;

class Scene : public SingletonBase<Scene>
{
public:

    // 描画対象のGameObjectを登録（ISceneEntityにも登録する）
    void RegisterGameObject(IGameObject* obj);

    // 描画対象のGameObjectを削除（ISceneEntityからも削除する）
    void UnregisterGameObject(IGameObject* obj);

    // 汎用シーンエンティティの登録・削除
    // ISceneEntity の登録（GameObject以外も可）
    void RegisterEntity(ISceneEntity* entity);

    // ISceneEntity の削除（GameObject以外も対応）
    void UnregisterEntity(ISceneEntity* entity);

    // GameObject を安全に破棄するための関数
    void DestroyAllMarkedGameObjects(void);

    //描画対象すべてを取得（ShadowRenderer等から使用）
    const SimpleArray<IGameObject*>& GetAllRenderableObjects() const { return m_renderableObjects; }

    // 汎用的に使う SceneEntity 群
    const SimpleArray<ISceneEntity*>& GetAllEntities() const { return m_entities; }

private:
    SimpleArray<IGameObject*> m_renderableObjects; //描画、影用 GameObject 専用
    SimpleArray<ISceneEntity*> m_entities; // 全てのシーンエンティティ（汎用）
};