//=============================================================================
//
// ShadowMeshCollector処理 [ShadowMeshCollector.hpp]
// Author :
//
//=============================================================================
#pragma once
#include "Core/Graphics/ShadowMeshCollector.h"
#include "Scene/GameObject.h"
#include "Scene/Environment.h"
//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define DEFINE_SHADOW_COLLECT_EMPTY(MODEL, RENDERDATA) \
template<> \
inline void CollectShadowMeshFromModel<MODEL>(const MODEL&, SimpleArray<RENDERDATA>&) {}

// ★ テンプレート本体
template<typename TModel, typename RenderData>
inline void CollectShadowMeshFromModel(const TModel&, SimpleArray<RenderData>&)
{
    static_assert(sizeof(TModel) == 0, "Unsupported model instance");
}

// ================================
// 有効な特化（それぞれ対応する組合せ）
// ================================

// StaticModel → StaticRenderData
template<>
inline void CollectShadowMeshFromModel<ModelInstance>(
    const ModelInstance& modelInstance,
    SimpleArray<StaticRenderData>& out)
{
    if (!modelInstance.pModel || modelInstance.instanceAttribute.isInstanced) return;

    const auto& parts = modelInstance.pModel->GetMeshParts();
    for (const auto& part : parts)
    {
        StaticRenderData data = {};
        data.vertexBuffer = part.VertexBuffer;
        data.indexBuffer = part.IndexBuffer;
        data.stride = sizeof(VERTEX_3D);
        data.indexCount = part.IndexNum;
        data.startIndexLocation = part.StartIndex;
        data.indexFormat = DXGI_FORMAT_R32_UINT;
        data.worldMatrix = modelInstance.transform.mtxWorld;
        data.enableAlphaTest = modelInstance.enableAlphaTest;
        out.push_back(std::move(data));
    }
}

// StaticModel（インスタンシング）→ InstancedRenderData
template<>
inline void CollectShadowMeshFromModel<ModelInstance>(
    const ModelInstance& modelInstance,
    SimpleArray<InstancedRenderData>& out)
{
    if (!modelInstance.pModel || !modelInstance.instanceAttribute.isInstanced) return;

    const auto& parts = modelInstance.pModel->GetMeshParts();
    for (const auto& part : parts)
    {
        InstancedRenderData data = {};
        data.vertexBuffer = modelInstance.instanceAttribute.vertexBuffer;
        data.indexBuffer = modelInstance.instanceAttribute.indexBuffer;
        data.stride = sizeof(InstanceVertex);
        data.instanceStride = sizeof(InstanceData);
        data.indexCount = part.IndexNum;
        data.startIndexLocation = part.StartIndex;
        data.indexFormat = DXGI_FORMAT_R32_UINT;
        data.worldMatrix = modelInstance.transform.mtxWorld;
        data.instanceBuffer = modelInstance.instanceAttribute.instanceBuffer;
        data.instanceCount = modelInstance.instanceAttribute.instanceCount;
        data.enableAlphaTest = modelInstance.enableAlphaTest;
        data.opacityMapSRV = part.OpacityTexture;
		data.allInstanceData = modelInstance.instanceAttribute.instanceData;
		data.visibleInstances = modelInstance.instanceAttribute.visibleInstanceDataArray;
		data.visibleInstanceDraw = modelInstance.instanceAttribute.visibleInstanceDraw;
		data.colliderArray = modelInstance.instanceAttribute.colliderArray;
        out.push_back(std::move(data));
    }
}

// SkinnedModel → SkinnedRenderData
template<>
inline void CollectShadowMeshFromModel<SkinnedMeshModelInstance>(
    const SkinnedMeshModelInstance& modelInstance,
    SimpleArray<SkinnedRenderData>& out)
{
    if (!modelInstance.pModel) return;

    const auto& parts = modelInstance.pModel->GetMeshParts();
    const auto* boneMatrices = modelInstance.pModel->GetFinalBoneMatrices();

    for (const auto& part : parts)
    {
        SkinnedRenderData data = {};
        data.vertexBuffer = part.VertexBuffer;
        data.indexBuffer = part.IndexBuffer;
        data.stride = sizeof(SKINNED_VERTEX_3D);
        data.indexCount = part.IndexNum;
        data.indexFormat = DXGI_FORMAT_R32_UINT;
        data.worldMatrix = modelInstance.transform.mtxWorld;
        data.pBoneMatrices = boneMatrices;
        data.enableAlphaTest = modelInstance.enableAlphaTest;
        data.opacityMapSRV = part.OpacityTexture;
        out.push_back(std::move(data));
    }
}

// ================================
// 無効な組み合わせ → 空実装
// ================================
// StaticModel → SkinnedRenderData（無効）
DEFINE_SHADOW_COLLECT_EMPTY(ModelInstance, SkinnedRenderData);

// SkinnedModel → StaticRenderData（無効）
DEFINE_SHADOW_COLLECT_EMPTY(SkinnedMeshModelInstance, StaticRenderData);

// SkinnedModel → InstancedRenderData（無効）
DEFINE_SHADOW_COLLECT_EMPTY(SkinnedMeshModelInstance, InstancedRenderData);