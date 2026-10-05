//=============================================================================
//
// [LakeSurfaceRenderer.cpp]
// Author : 
// 
//=============================================================================
#include "LakeSurfaceRenderer.h"
#include "Core/TextureMgr.h"

//*****************************************************************************
// マクロ定義
//***************************************************************************** 
#define LAKE_SURFACE_NORMAL_MAP_1      "data/TEXTURE/Effects/water_normal1.jpeg"
#define LAKE_SURFACE_NORMAL_MAP_2      "data/TEXTURE/Effects/water_normal2.jpeg"

bool LakeSurfaceRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
#ifdef _DEBUG
    //DebugProc::get_instance().Register(this);
#endif // _DEBUG

    // ブレンドモードをアルファブレンドに設定
    m_blendMode = BLEND_MODE_ALPHABLEND;

    m_device = device;
    m_context = context;

    if (!m_device || !m_context)
        return false;

    // シェーダーの読み込み
    if (!LoadShaders())
        return false;

    // 水面パラメータの定数バッファを作成
	if (!CreateWaterParamsCB())
        return false;

    // 平面メッシュの生成
	if (!CreatePlaneMesh())
        return false;

    // ディフューズテクスチャの読み込み
    if (!CreateDiffuseTex())
        return false;

    return true;
}

void LakeSurfaceRenderer::Shutdown(void)
{
    SafeRelease(&m_vertexBuffer);
    SafeRelease(&m_indexBuffer);
    SafeRelease(&m_cbWater);
    m_isShutdown = true;
}

void LakeSurfaceRenderer::Update(void)
{
#ifdef _DEBUG
    LoadShaders();
#endif // DEBUG

    UpdateWaterParamsBuffer();
}

void LakeSurfaceRenderer::Draw(void)
{
    D3D11_BUFFER_DESC desc;
    m_cbWater->GetDesc(&desc);

    UINT stride = sizeof(XMFLOAT3);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    m_context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_renderer.SetBlendState(m_blendMode);

    m_context->DrawIndexed(m_indexCount, 0, 0);
}

void LakeSurfaceRenderer::SetupPipeline(void)
{
    m_context->IASetInputLayout(m_shaderSet.inputLayout);
    m_context->VSSetShader(m_shaderSet.vs, nullptr, 0);
    m_context->PSSetShader(m_shaderSet.ps, nullptr, 0);

    m_shaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_EFFECT_LAKE_SURFACE, m_cbWater);
	m_shaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_EFFECT_LAKE_SURFACE, m_cbWater);

    if (m_normalMap1)
    {
        m_shaderResourceBinder.BindShaderResource(ShaderStage::VS, SLOT_TEX_NORMAL, m_normalMap1);
        m_shaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_NORMAL, m_normalMap1);
    }

    if (m_normalMap2)
    {
        m_shaderResourceBinder.BindShaderResource(ShaderStage::VS, SLOT_TEX_NORMAL_2, m_normalMap2);
        m_shaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_NORMAL_2, m_normalMap2);
    }

    if (m_reflectionMap)
        m_shaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_REFLECT, m_reflectionMap);
}

void LakeSurfaceRenderer::RenderImGui(void)
{
    ImGui::SliderFloat("PosX: ", &m_lakeSurfaceParams.position.x, 0.0f, 8000.0f, "%.1f");
    ImGui::SliderFloat("PosY: ", &m_lakeSurfaceParams.position.y, 0.0f, 8000.0f, "%.1f");
    ImGui::SliderFloat("PosZ: ", &m_lakeSurfaceParams.position.z, 0.0f, 8000.0f, "%.1f");
    ImGui::SliderFloat("ScaleX: ", &m_lakeSurfaceParams.scale.x, 50.0f, 2000.0f, "%.1f");
    ImGui::SliderFloat("ScaleY: ", &m_lakeSurfaceParams.scale.y, 50.0f, 1500.0f, "%.1f");
    ImGui::SliderFloat("ScaleZ: ", &m_lakeSurfaceParams.scale.z, 50.0f, 2000.0f, "%.1f");

    if (ImGui::CollapsingHeader("Debug Water Surface"))
    {
        ImGui::SliderFloat("Units Per Grid: ", &m_lakeSurfaceParams.unitsPerGrid, 5.0f, 20.0f, "%.1f");
        ImGui::SliderFloat("Wave Strength: ", &m_lakeSurfaceParams.waveStrength, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Fresnel Bias: ", &m_lakeSurfaceParams.fresnelBias, 0.0f, 0.1f, "%.3f");
        ImGui::SliderFloat("Fresnel Power: ", &m_lakeSurfaceParams.fresnelPower, 0.0f, 5.0f, "%.2f");
        ImGui::SliderFloat("Water Color R: ", &m_lakeSurfaceParams.waterColor.x, 0.0f, 1.0f, "%.1f");
        ImGui::SliderFloat("Water Color G: ", &m_lakeSurfaceParams.waterColor.y, 0.0f, 1.0f, "%.1f");
        ImGui::SliderFloat("Water Color B: ", &m_lakeSurfaceParams.waterColor.z, 0.0f, 1.0f, "%.1f");
    }
}

bool LakeSurfaceRenderer::CreatePlaneMesh(void)
{
    SafeRelease(&m_vertexBuffer);
    SafeRelease(&m_indexBuffer);

    // 密度設定パラメータ（1点あたりの世界単位）
    const float unitsPerGrid = 20.0f;

    // 解像度はXZ軸のスケールに応じて設定（最大制限付き）
    UINT resX = static_cast<UINT>(m_lakeSurfaceParams.scale.x / unitsPerGrid);
    UINT resZ = static_cast<UINT>(m_lakeSurfaceParams.scale.z / unitsPerGrid);
    resX = max(1, min(resX, m_maxResolution));
    resZ = max(1, min(resZ, m_maxResolution));

    const int vertexCount = (resX + 1) * (resZ + 1);
    const int indexCount = resX * resZ * 6;

	m_vertices.clear();
	m_indices.clear();
	m_vertices.reserve(vertexCount);
	m_indices.reserve(indexCount);

    const float stepX = 2.0f / resX;
    const float stepZ = 2.0f / resZ;

	// 頂点座標を計算
    for (UINT z = 0; z <= resZ; ++z)
    {
        for (UINT x = 0; x <= resX; ++x)
        {
            // 座標はワールド空間ではなく、後でスケーリングや平行移動で調整。
            float posX = -1.0f + x * stepX;
            float posZ = -1.0f + z * stepZ;
            m_vertices.push_back({ posX, 0.0f, posZ }); // Yは頂点シェーダで揺らす
        }
    }

	// インデックスを計算
    for (UINT z = 0; z < resZ; ++z)
    {
        for (UINT x = 0; x < resX; ++x)
        {
            UINT i0 = (resX + 1) * z + x;
            UINT i1 = i0 + 1;
            UINT i2 = i0 + (resX + 1);
            UINT i3 = i2 + 1;

            m_indices.push_back(i0);
            m_indices.push_back(i1);
            m_indices.push_back(i2);

            m_indices.push_back(i1);
            m_indices.push_back(i3);
            m_indices.push_back(i2);
        }
    }

    m_indexCount = static_cast<UINT>(m_indices.getSize());

    HRESULT hr = S_OK;

    // 頂点バッファ生成
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(XMFLOAT3) * static_cast<UINT>(m_vertices.getSize());
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = m_vertices.data();
    hr = m_device->CreateBuffer(&vbDesc, &vbData, &m_vertexBuffer);
	if (FAILED(hr))
        return false;

    // インデックスバッファ生成
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = sizeof(UINT) * static_cast<UINT>(m_indices.getSize());
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = m_indices.data();
    hr = m_device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer);
    if (FAILED(hr))
        return false;

    return true;
}

bool LakeSurfaceRenderer::LoadShaders(void)
{
    bool loadShaders = ShaderManager::get_instance().HasShaderSet(ShaderSetID::LakeSurface);
    if (loadShaders)
        m_shaderSet = ShaderManager::get_instance().GetShaderSet(ShaderSetID::LakeSurface);

    return loadShaders;
}

void LakeSurfaceRenderer::UpdateWaterParams(void)
{
    switch (m_ground.GetCurrentSceneID())
    {
	case SceneID::Spring_Day:
    case SceneID::Spring_Night:
	case SceneID::Summer_Day:
	case SceneID::Summer_Night:
	case SceneID::Autumn_Day:
	case SceneID::Autumn_Night:
        m_lakeSurfaceParams.waveStrength = WAVE_STRENGTH_NORMAL;
		m_lakeSurfaceParams.fresnelBias = FRESNEL_BIAS_NORMAL;
		m_lakeSurfaceParams.fresnelPower = FRESNEL_POWER_NORMAL;
		m_lakeSurfaceParams.waterColor = WATER_COLOR_NORMAL;
		break;
	case SceneID::Winter_Day:
	case SceneID::Winter_Night:
		m_lakeSurfaceParams.waveStrength = WAVE_STRENGTH_FREEZE;
		m_lakeSurfaceParams.fresnelBias = FRESNEL_BIAS_FREEZE;
		m_lakeSurfaceParams.fresnelPower = FRESNEL_POWER_FREEZE;
        m_lakeSurfaceParams.waterColor = WATER_COLOR_NORMAL;
		break;
	default:
		break;
    }
}

bool LakeSurfaceRenderer::CreateDiffuseTex(void)
{
    m_normalMap1 = TextureMgr::get_instance().CreateTexture(LAKE_SURFACE_NORMAL_MAP_1);
    if (!m_normalMap1)
        return false;

    m_normalMap2 = TextureMgr::get_instance().CreateTexture(LAKE_SURFACE_NORMAL_MAP_2);
    if (!m_normalMap2)
        return false;

    return true;
}

bool LakeSurfaceRenderer::CreateWaterParamsCB(void)
{
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(CBWaterParams);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags = 0;

    HRESULT hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_cbWater);
    if (FAILED(hr))
        return false;

    return true;
}

void LakeSurfaceRenderer::UpdateWaterParamsBuffer(void)
{
    CBWaterParams waterParams = {};
    waterParams.worldMtx = XMMatrixTranspose(m_worldMatrix);
	waterParams.cameraPos = m_camera.GetPosition();
    waterParams.time = m_timer.GetElapsedTime();

	UpdateWaterParams(); // シーンに応じた水面パラメータの更新
    waterParams.waveStrength = m_lakeSurfaceParams.waveStrength;
    waterParams.fresnelBias = m_lakeSurfaceParams.fresnelBias;
    waterParams.fresnelPower = m_lakeSurfaceParams.fresnelPower;
    waterParams.waterColor = m_lakeSurfaceParams.waterColor;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = m_context->Map(m_cbWater, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr))
    {
        memcpy(mapped.pData, &waterParams, sizeof(CBWaterParams));
        m_context->Unmap(m_cbWater, 0);
    }

}

void LakeSurfaceRenderer::UpdateWorldMatrix(void)
{
    XMMATRIX scaleMtx = XMMatrixScaling(
        m_lakeSurfaceParams.scale.x,
        m_lakeSurfaceParams.scale.y,
        m_lakeSurfaceParams.scale.z);
    XMMATRIX translationMtx = XMMatrixTranslation(
        m_lakeSurfaceParams.position.x, 
        m_lakeSurfaceParams.position.y,
        m_lakeSurfaceParams.position.z);

    m_worldMatrix = scaleMtx * translationMtx;
}
