//=============================================================================
//
// ShadowMapRenderer処理 [ShadowMapRenderer.cpp]
// Author : 
//
//=============================================================================
#include "Core/Graphics/ShadowMapRenderer.h"
#include "Core/Shader/ShaderLoader.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define DEBUG_CASCASDE_SRV   1  // デバッグ用のカスケードシャドウマップ表示

bool ShadowMapRenderer::Init(int shadowMapSize, int numCascades)
{ 
#ifdef _DEBUG
    //DebugProc::get_instance().Register(this);
#endif // DEBUG

    m_csm.Initialize(shadowMapSize, numCascades);

    m_device = Renderer::get_instance().GetDevice();
    m_context = Renderer::get_instance().GetDeviceContext();

    // 空のピクセルシェーダーを読み込む
    ID3D11PixelShader* emptyPS = nullptr;
    if (ShaderLoader::LoadEmptyPixelShader(m_device, &emptyPS, "Shaders/DepthMap.hlsl", "DummyPS"))
        m_shaderManager.SetSharedShadowPixelShader(emptyPS);
    else
        return false;

    bool loadShaders = true;
	// シェーダーの読み込み
	if (!LoadShaders())
        return false;

	m_initialized = true;

    return true;
}

void ShadowMapRenderer::Shutdown() 
{
    m_csm.Shutdown();
}

void ShadowMapRenderer::BeginShadowPass(const DirectionalLight* light)
{
    XMFLOAT3 dir = light->GetDirection();
    m_lightDir = XMVectorSet(dir.x, dir.y, dir.z, 0.0f);

    m_csm.UpdateCascades(m_lightDir, m_camera.GetNearZ(), m_camera.GetFarZ());
    m_csm.UnbindShadowSRVs();

    if (m_shaderHotReload)
		LoadShaders();
}

void ShadowMapRenderer::RenderCSMForLight(DirectionalLight* light, int lightIndex, const SimpleArray<IGameObject*>& sceneObjects)
{
	if (!m_initialized || !light || lightIndex < 0 || lightIndex >= m_csm.GetCascadeCount())
		return;

    BeginShadowPass(light);

    for (int cascadeIndex = 0; cascadeIndex < m_csm.GetCascadeCount(); cascadeIndex++)
    {
        m_csm.BeginCascadeRender(lightIndex, cascadeIndex);
        m_csm.SetViewport();
        m_csm.UpdateCascadeCBuffer(cascadeIndex);

        m_csm.CollectFromScene(cascadeIndex, sceneObjects);
        RenderShadowPass(cascadeIndex);
    }

    EndShadowPass();

    m_csm.UpdateCascadeCBufferArray();
    m_csm.BindShadowSRVsToPixelShader(lightIndex, SLOT_TEX_CSM);
}

void ShadowMapRenderer::RenderShadowPass(int cascadeIndex)
{
    ShadowMeshCollector& collector = m_csm.GetCascadeCollector(cascadeIndex);

    UINT meshCount = 0;

    if (m_enableStaticShadow)
    {
        const auto& staticMeshes = collector.GetStaticMeshes();
		meshCount = staticMeshes.getSize();
        for (UINT i = 0; i < meshCount; ++i)
        {
            RenderStaticMesh(staticMeshes[i]);
        }
    }

    if (m_enableSkinnedShadow)
    {
        const auto& skinnedMeshes = collector.GetSkinnedMeshes();
		meshCount = skinnedMeshes.getSize();
        for (UINT i = 0; i < meshCount; ++i)
        {
            RenderSkinnedMesh(skinnedMeshes[i]);
        }
    }

    if (m_enableInstancedShadow)
    {
        auto& instancedMeshes = collector.GetInstancedMeshes();
		meshCount = instancedMeshes.getSize();
        for (UINT i = 0; i < meshCount; ++i)
        {
            RenderInstancedMesh(instancedMeshes[i]);
        }
    }
}

void ShadowMapRenderer::RenderStaticMesh(const StaticRenderData& mesh)
{
    m_context->IASetInputLayout(m_staticModelShaderSet.inputLayout);

    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &mesh.vertexBuffer, &mesh.stride, &offset);
    m_context->IASetIndexBuffer(mesh.indexBuffer, mesh.indexFormat, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_staticModelShaderSet.vs, nullptr, 0);

    m_renderer.SetCurrentWorldMatrix(&mesh.worldMatrix);

    if (mesh.enableAlphaTest && mesh.opacityMapSRV)
    {
        m_context->PSSetShader(m_staticModelShaderSet.alphaPs, nullptr, 0);
        m_context->PSSetShaderResources(0, 1, &mesh.opacityMapSRV);
    }
    else
    {
        m_context->PSSetShader(nullptr, nullptr, 0);
    }

    m_context->DrawIndexed(mesh.indexCount, mesh.startIndexLocation, 0);
}

void ShadowMapRenderer::RenderSkinnedMesh(const SkinnedRenderData& mesh)
{
    m_context->IASetInputLayout(m_skinnedModelShaderSet.inputLayout);

    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &mesh.vertexBuffer, &mesh.stride, &offset);
    m_context->IASetIndexBuffer(mesh.indexBuffer, mesh.indexFormat, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_skinnedModelShaderSet.vs, nullptr, 0);

    m_renderer.SetCurrentWorldMatrix(&mesh.worldMatrix);

    // ボーン行列をシェーダーにセット
    if (mesh.pBoneMatrices)
    {
        m_renderer.SetBoneMatrix(mesh.pBoneMatrices);
    }
    else
    {
        XMMATRIX	boneMatrices[BONE_MAX];
        boneMatrices[0] = XMMatrixTranspose(XMMatrixIdentity());
        m_renderer.SetBoneMatrix(boneMatrices);
    }

    if (mesh.enableAlphaTest && mesh.opacityMapSRV)
    {
        m_context->PSSetShader(m_skinnedModelShaderSet.alphaPs, nullptr, 0);
        m_context->PSSetShaderResources(0, 1, &mesh.opacityMapSRV);
    }
    else
    {
        m_context->PSSetShader(nullptr, nullptr, 0);
    }

    m_context->DrawIndexed(mesh.indexCount, 0, 0);
}

void ShadowMapRenderer::RenderInstancedMesh(const InstancedRenderData& mesh)
{
  //  for (UINT i = 0; i < mesh.instanceCount; ++i)
  //  {
  //      if (!mesh.visibleInstanceDraw) continue;

		//mesh.visibleInstanceDraw[i] = false; // 使用フラグをリセット
  //  }

    m_context->IASetInputLayout(m_instancedModelShaderSet.inputLayout);

    UINT strides[2] = { mesh.stride, mesh.instanceStride };
    UINT offsets[2] = { 0, 0 };
    ID3D11Buffer* buffers[2] = { mesh.vertexBuffer, mesh.instanceBuffer };

    m_context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
    m_context->IASetIndexBuffer(mesh.indexBuffer, mesh.indexFormat, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_instancedModelShaderSet.vs, nullptr, 0);

    m_renderer.SetCurrentWorldMatrix(&mesh.worldMatrix);

    if (mesh.enableAlphaTest && mesh.opacityMapSRV)
    {
        m_context->PSSetShader(m_instancedModelShaderSet.alphaPs, nullptr, 0);
        m_context->PSSetShaderResources(0, 1, &mesh.opacityMapSRV);
    }
    else
    {
        m_context->PSSetShader(nullptr, nullptr, 0);
    }
	// インスタンスバッファの可視インスタンス
    UINT visibleCount = mesh.visibleInstances->getSize();
    m_context->DrawIndexedInstanced(mesh.indexCount, visibleCount, mesh.startIndexLocation, 0, 0);
}

bool ShadowMapRenderer::LoadShaders(void)
{
	bool loadSuccessful = true;
    // 静的モデル用シャドウシェーダー
    loadSuccessful &= m_shaderManager.HasShadowShaderSet(ShaderSetID::StaticModel);
    if (loadSuccessful)
        m_staticModelShaderSet = m_shaderManager.GetShadowShaderSet(ShaderSetID::StaticModel);

    // スキニングメッシュ用シャドウシェーダー
    loadSuccessful &= m_shaderManager.HasShadowShaderSet(ShaderSetID::SkinnedModel);
    if (loadSuccessful)
        m_skinnedModelShaderSet = m_shaderManager.GetShadowShaderSet(ShaderSetID::SkinnedModel);

    // インスタンスモデル（草、木など）
    loadSuccessful &= m_shaderManager.HasShadowShaderSet(ShaderSetID::Instanced_Tree);
    if (loadSuccessful)
        m_instancedModelShaderSet = m_shaderManager.GetShadowShaderSet(ShaderSetID::Instanced_Tree);

    return loadSuccessful;
}

void ShadowMapRenderer::EndShadowPass()
{
    for (int i = 0; i < m_csm.GetCascadeCount(); ++i) 
    {
        m_csm.EndCascadeRender();
    }
}

ID3D11ShaderResourceView* ShadowMapRenderer::GetShadowSRV(int lightIndex, int cascadeIndex) const
{
    return m_csm.GetShadowSRV(lightIndex, cascadeIndex);
}

const XMMATRIX& ShadowMapRenderer::GetLightViewProj(int cascadeIndex) const
{
    return m_csm.GetCascadeData(cascadeIndex).lightViewProj;
}

void ShadowMapRenderer::RenderImGui(void)
{
    // シャドウのタイプごとの有効化設定（1行ずつチェックボックス表示）
    ImGui::Checkbox("Enable Static Shadow", &m_enableStaticShadow);
    ImGui::Checkbox("Enable Skinned Shadow", &m_enableSkinnedShadow);
    ImGui::Checkbox("Enable Instanced Shadow", &m_enableInstancedShadow);

#if DEBUG_CASCASDE_SRV
    for (int i = 0; i < m_csm.GetCascadeCount(); ++i)
    {
        ImGui::Text("Cascade %d", i);
        ImGui::Image((ImTextureID)m_csm.GetSRV(0, i), ImVec2(128, 128));
    }
#endif // DEBUG


    // CSM 設定のデバッグUIを表示
    ImGui::Separator();
    ImGui::Text("Cascaded Shadow Map");
    m_csm.RenderImGui();
}

void ShadowMapRenderer::RenderDebugInfo(void)
{
#if DEBUG_CASCADE_BOUND
    m_csm.VisualizeCascadeBounds();
#endif // DEBUG
}