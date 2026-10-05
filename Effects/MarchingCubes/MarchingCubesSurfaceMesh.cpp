//=============================================================================
//
// [MarchingCubesSurfaceMesh.cpp]
// Author : 
//
//=============================================================================
#include "Effects/MarchingCubes/MarchingCubesSurfaceMesh.h"

MarchingCubesSurfaceMesh::MarchingCubesSurfaceMesh() {}

MarchingCubesSurfaceMesh::~MarchingCubesSurfaceMesh()
{
    ReleaseBuffers();
	SafeRelease(&m_cbMarchingCubes);
}

void MarchingCubesSurfaceMesh::Initialize(UINT maxParticles)
{
    m_maxParticles = maxParticles; // 最大粒子数を設定

    if (!LoadShaders())
    {
        assert(false && "Failed to load shaders for MarchingCubesSurfaceMesh");
        return;
    }

    if (!m_buildOnGPU)
    {
		// CPU上でのメッシュ生成の場合、頂点とインデックスの初期化
        m_vertices.reserve(m_vertexCapacity);
        m_indices.reserve(m_indexCapacity);

        // 頂点バッファとインデックスバッファの初期化
        CreateBuffers();
    }


    HRESULT hr = S_OK;

    // 定数バッファの初期化
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(CBMarchingCubes);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_cbMarchingCubes);
    assert(SUCCEEDED(hr));

    m_mesher.Initialize(maxParticles);
}

void MarchingCubesSurfaceMesh::UpdateBuffers(void)
{
    if (m_vertexBuffer && !m_vertices.empty())
    {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            memcpy(mapped.pData, m_vertices.data(), sizeof(FluidSurfaceVertex) * m_vertices.getSize());
            m_context->Unmap(m_vertexBuffer, 0);
        }
    }
    else
        return;

    if (m_indexBuffer && !m_indices.empty())
    {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(m_context->Map(m_indexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            memcpy(mapped.pData, m_indices.data(), sizeof(UINT) * m_indices.getSize());
            m_context->Unmap(m_indexBuffer, 0);
        }
    }
    else
        return;

	// 定数バッファの更新
    UpdateCBMarchingCubes();
}

void MarchingCubesSurfaceMesh::SetupPipeline(void)
{
    // InputLayout と Shader を切り替える（他の粒子と違うから！）
    m_context->VSSetShader(m_shaderSet.vs, nullptr, 0);
    m_context->GSSetShader(nullptr, nullptr, 0); // 必ず GS は切る
    m_context->PSSetShader(m_shaderSet.ps, nullptr, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    if (m_buildOnGPU)
    {
        // GPU 構築モード（DrawIndirect + SRV デコード）
        m_context->IASetInputLayout(nullptr); // InputLayout 不要

        m_context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
        m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    }
    else
    {
        // CPU 構築モード（IA 経由描画）
        m_context->IASetInputLayout(m_shaderSet.inputLayout);

        UINT stride = sizeof(MarchingCubesVertex);
        UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
        m_context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    }

    // シェーダーリソースのバインド
    m_shaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_SRV_FLUID_THICKNESS, m_thicknessSRV);
    m_shaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_FLUID_SURFACE, m_cbMarchingCubes);
    m_shaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_FLUID_SURFACE, m_cbMarchingCubes);
}

void MarchingCubesSurfaceMesh::UpdateCBMarchingCubes(void)
{
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(m_context->Map(m_cbMarchingCubes, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        CBMarchingCubes* cb = reinterpret_cast<CBMarchingCubes*>(mapped.pData);

        cb->isoLevel = m_lastParams.isoLevel;
        cb->shallowColor = m_shallowColor;
        cb->deepColor = m_deepColor;
        cb->fresnelPower = m_fresnelPower;
        cb->alphaScale = m_alphaScale;
        cb->thicknessScale = m_thicknessScale;
        cb->thicknessBias = m_thicknessBias;
        cb->thicknessNormalizeFactor = 1.0f / 255.0f; // テクスチャの最大値で割る
        cb->screenSize = XMFLOAT2(static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT));
        cb->invScreenSize = XMFLOAT2(1.0f / SCREEN_WIDTH, 1.0f / SCREEN_HEIGHT);
		if (m_buildOnGPU)
		{
            cb->numActiveVoxels = m_lastParams.activeVoxelCount; // GPU 構築モードではアクティブなボクセル数を設定
			//cb->edgeCacheDim = m_edgeCacheDim; // エッジキャッシュの次元を設定
		}
        else
        {
            cb->numActiveVoxels = 0; // CPU 構築モードではアクティブなボクセル数は 0
			//cb->edgeCacheDim = XMUINT3(0, 0, 0); // エッジキャッシュの次元は無効
        }
		cb->padding = XMFLOAT3(0.0f, 0.0f, 0.0f); // パディングを追加して構造体のサイズを16バイト境界に揃える
        m_context->Unmap(m_cbMarchingCubes, 0);
    }
}

void MarchingCubesSurfaceMesh::SetThicknessSRV(ID3D11ShaderResourceView* srv)
{
    m_thicknessSRV = srv;
}

void MarchingCubesSurfaceMesh::Draw()
{
    if (!m_vertexBuffer || !m_indexBuffer || m_indices.empty())
        return;

	// パイプラインのセットアップ
	SetupPipeline();

    // 描画実行
    m_context->DrawIndexed(static_cast<UINT>(m_indices.getSize()), 0, 0);
}

bool MarchingCubesSurfaceMesh::LoadShaders(void)
{
    bool loadShaders = true;

    if (m_buildOnGPU)
    {
        loadShaders &= ShaderManager::get_instance().HasShaderSet(ShaderSetID::FluidMarchingCubeGPU);
        if (loadShaders)
            m_shaderSet = ShaderManager::get_instance().GetShaderSet(ShaderSetID::FluidMarchingCubeGPU);
    }
    else
    {
        loadShaders &= ShaderManager::get_instance().HasShaderSet(ShaderSetID::FluidMarchingCube);
        if (loadShaders)
            m_shaderSet = ShaderManager::get_instance().GetShaderSet(ShaderSetID::FluidMarchingCube);
    }


    return loadShaders;
}

void MarchingCubesSurfaceMesh::CreateBuffers(void)
{
    HRESULT hr = S_OK;

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(MarchingCubesVertex) * m_vertexCapacity);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer(&vbDesc, nullptr, &m_vertexBuffer);
    assert(SUCCEEDED(hr) && "Failed to create vertex buffer");

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DYNAMIC;
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(UINT) * m_indexCapacity);
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer(&ibDesc, nullptr, &m_indexBuffer);
    assert(SUCCEEDED(hr) && "Failed to create index buffer");
}

void MarchingCubesSurfaceMesh::ReleaseBuffers(void)
{
    SafeRelease(&m_vertexBuffer);
    SafeRelease(&m_indexBuffer);
}

void MarchingCubesSurfaceMesh::UpdateMesh(const SimpleArray<SimpleArray<SimpleArray<float>>>& volume, const VolumeReconstructionParams& params)
{
    // 必要な頂点数とインデックス数を計算
	UINT requiredVertexCount, requiredIndexCount;
    m_mesher.ComputeMeshRequirements(params, &requiredVertexCount, &requiredIndexCount);

	// バッファの容量を確認し、必要なら拡張
    EnsureBufferCapacity(requiredVertexCount, requiredIndexCount);

	// 頂点とインデックスの数をリセット
    m_mesher.SetTargetBuffer(m_vertices, m_indices);
	m_mesher.BuildMesh(volume, params);

	// 頂点とインデックスの数を更新
    UpdateBuffers();
}

void MarchingCubesSurfaceMesh::UpdateMesh(const HashMap<VoxelCoord, float, HashVoxelKey64, EqualVoxelKey64>& scalarFieldMap, const VolumeReconstructionParams& params)
{
    // 必要な頂点数とインデックス数を計算
    UINT requiredVertexCount, requiredIndexCount;
    m_mesher.ComputeMeshRequirements(params, &requiredVertexCount, &requiredIndexCount);

    // 頂点とインデックスの数をリセット
    m_mesher.SetTargetBuffer(m_vertices, m_indices);
    m_mesher.Clear();
    m_mesher.BuildMesh(scalarFieldMap, params);

    // 頂点とインデックスの数を更新
    UpdateBuffers();
}

void MarchingCubesSurfaceMesh::EnsureBufferCapacity(UINT requiredVertexCount, UINT requiredIndexCount)
{
    bool needResize = false;

    if (requiredVertexCount > m_vertexCapacity)
    {
        m_vertexCapacity = max(requiredVertexCount, m_vertexCapacity * 2);
        m_vertices.reserve(static_cast<UINT>(m_vertexCapacity)); // 頂点バッファの容量を確保
        needResize = true;
    }

    if (requiredIndexCount > m_indexCapacity)
    {
        m_indexCapacity = max(requiredIndexCount, m_indexCapacity * 2);
        m_indices.reserve(static_cast<UINT>(m_indexCapacity)); // インデックスバッファの容量を確保
        needResize = true;
    }

    if (needResize)
    {
        ReleaseBuffers(); // GPUバッファを解放
        CreateBuffers();  // 新しい容量で再生成
    }
}
