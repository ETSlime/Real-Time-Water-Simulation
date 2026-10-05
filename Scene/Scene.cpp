//=============================================================================
//
//  [Scene.h]
// Author :
// 
//=============================================================================
#include "Scene.h"
#include "Scene/GameObject.h"

void Scene::RegisterGameObject(IGameObject* obj)
{
    if (obj)
    {
        m_renderableObjects.push_back(obj);
        m_entities.push_back(obj); // IGameObject ÇÕ ISceneEntity Çåpè≥ÇµÇƒÇ¢ÇÈÇÕÇ∏
    }
}

void Scene::UnregisterGameObject(IGameObject* obj)
{
    if (obj)
    {
        int idx = m_renderableObjects.find_index(obj);
        if (idx != -1) m_renderableObjects.erase(idx);

        int idx2 = m_entities.find_index(obj);
        if (idx2 != -1) m_entities.erase(idx2);
    }
}

void Scene::RegisterEntity(ISceneEntity* entity)
{
    if (entity)
    {
        m_entities.push_back(entity);
    }
}

void Scene::UnregisterEntity(ISceneEntity* entity)
{
    if (entity)
    {
        int idx = m_entities.find_index(entity);
        if (idx != -1) m_entities.erase(idx);
    }
}

void Scene::DestroyAllMarkedGameObjects(void)
{
    for (UINT i = 0; i < m_renderableObjects.getSize(); )
    {
        IGameObject* obj = m_renderableObjects[i];

        if (obj->IsMarkedForDelete())
        {
            m_renderableObjects.erase(i);
            SAFE_DELETE(obj);
        }
        else
            ++i;
    }
}
