#pragma once
//=============================================================================
//
// 影生成パスとモデル描画の統合シャドウレンダラー [ShadowMapRenderer.h]
// Author : 
// カスケードシャドウマップに対応した影描画処理を統括し、
// 静的・スキンメッシュ・インスタンスモデルの影生成を制御する
// 
//=============================================================================
#include "Core/Camera.h"
#include "Core/Light.h"
#include "Core/Graphics/CascadedShadowMap.h"
#include "Core/Shader/ShaderManager.h"
#include "Core/Graphics/ShadowMeshCollector.h"
#include "Utility/Debug/DebugBoundingBoxRenderer.h"


class ShadowMapRenderer : public SingletonBase<ShadowMapRenderer>, public IDebugUI
{
public:

    bool Init(int shadowMapSize, int numCascades = 4);
    void Shutdown();

    // シャドウマップを描画する
    void RenderCSMForLight(DirectionalLight* light, int lightIndex, const SimpleArray<IGameObject*>& sceneObjects);

    ID3D11ShaderResourceView* GetShadowSRV(int lightIndex, int cascadeIndex) const;
    const XMMATRIX& GetLightViewProj(int cascadeIndex) const;

    inline void SetEnableStaticShadow(bool enable) { m_enableStaticShadow = enable; }
    inline void SetEnableSkinnedShadow(bool enable) { m_enableSkinnedShadow = enable; }
    inline void SetEnableInstancedShadow(bool enable) { m_enableInstancedShadow = enable; }

    inline bool IsStaticShadowEnabled() const { return m_enableStaticShadow; }
    inline bool IsSkinnedShadowEnabled() const { return m_enableSkinnedShadow; }
    inline bool IsInstancedShadowEnabled() const { return m_enableInstancedShadow; }

private:

    // シャドウマップの初期化
    void BeginShadowPass(const DirectionalLight* light);
    // シャドウマップの描画を開始
    void RenderShadowPass(int cascadeIndex);
    // シャドウマップの描画を終了
    void EndShadowPass(void);

    // モデルの影を描画する
    void RenderStaticMesh(const StaticRenderData& mesh);
    // ボーンアニメーションモデルの影を描画する
    void RenderSkinnedMesh(const SkinnedRenderData& mesh);
    // インスタンス化されたモデルの影を描画する
    void RenderInstancedMesh(const InstancedRenderData& mesh);

	bool LoadShaders(void);


    virtual void RenderImGui(void) override;
    virtual const char* GetPanelName(void) const override { return "Shadow Map Renderer"; };
    virtual void RenderDebugInfo(void) override;

    CascadedShadowMap m_csm;
    ShadowMeshCollector m_shadowCollector;

    XMVECTOR m_lightDir;

	bool m_initialized = false;
    float m_alphaCutoff = 0.5f;

    bool m_enableStaticShadow = true;
    bool m_enableSkinnedShadow = true;
    bool m_enableInstancedShadow = true;
	bool m_shaderHotReload = true;

    Camera& m_camera = Camera::get_instance();

    ShadowShaderSet m_staticModelShaderSet;
    ShadowShaderSet m_skinnedModelShaderSet;
    ShadowShaderSet m_instancedModelShaderSet;

    ID3D11Device* m_device = Renderer::get_instance().GetDevice();
    ID3D11DeviceContext* m_context = Renderer::get_instance().GetDeviceContext();
    Renderer& m_renderer = Renderer::get_instance();
    ShaderManager& m_shaderManager = ShaderManager::get_instance();
};