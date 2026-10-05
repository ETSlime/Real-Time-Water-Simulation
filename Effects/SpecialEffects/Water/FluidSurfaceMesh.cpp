//=============================================================================
//
// [FluidSurfaceMesh.cpp]
// Author : 
//
//=============================================================================
#include "Effects/SpecialEffects/Water/FluidSurfaceMesh.h"


FluidSurfaceMesh::FluidSurfaceMesh()
    : m_device(Renderer::get_instance().GetDevice()), 
    m_context(Renderer::get_instance().GetDeviceContext()) 
{
	m_vertices.reserve(m_vertexCapacity);
	m_indices.reserve(m_indexCapacity);
}

void FluidSurfaceMesh::Initialize(void)
{
	if (!LoadShaders())
	{
		assert(false && "Failed to load shaders for FluidSurfaceMesh");
		return;
	}

	HRESULT hr = S_OK;

	// 頂点バッファとインデックスバッファの初期化
    CreateBuffers();

	// 定数バッファの初期化
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.ByteWidth = sizeof(CBFluidSurface);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_cbFluidSurface);
    assert(SUCCEEDED(hr));
}

void FluidSurfaceMesh::GenerateSurfaceMesh(const FluidHeightFieldBuilder& heightField)
{
    m_vertices.clear();
    m_indices.clear();

    const int gridX = heightField.GetGridX();
    const int gridZ = heightField.GetGridZ();
    const float minX = heightField.GetMinX();
    const float minZ = heightField.GetMinZ();
    const float dx = heightField.GetCellSizeX();
    const float dz = heightField.GetCellSizeZ();

    
    const int totalGrid = gridX * gridZ;

	// グリッドの頂点数とインデックス数を計算
    UINT maxVertexCount = totalGrid;
    UINT maxIndexCount = (gridX - 1) * (gridZ - 1) * 6;

    // バッファの容量を確認
    EnsureBufferCapacity(maxVertexCount, maxIndexCount);

    // グリッド構造が変化したかどうかをチェック
    bool gridStructureChanged = (m_prevGridX != gridX || m_prevGridZ != gridZ);
    int oldGridX = m_prevGridX;
    int oldGridZ = m_prevGridZ;

    // ダブルバッファリング: 前フレームと今フレームのバッファを切り替え
    m_activeBufferIndex = 1 - m_activeBufferIndex;
    SimpleArray<float>& prevHeightField = m_heightFieldBuffers[1 - m_activeBufferIndex];
    SimpleArray<float>& currHeightField = m_heightFieldBuffers[m_activeBufferIndex];

    // 必要に応じてバッファサイズを拡張
    if (currHeightField.getCapacity() < totalGrid)
        currHeightField.reserve(totalGrid);
    if (prevHeightField.getCapacity() < totalGrid)
        prevHeightField.reserve(totalGrid);

    // サイズだけ調整（内容は保持）
    currHeightField.setSize(totalGrid);
    if (prevHeightField.getSize() < totalGrid)
    {
        int oldSize = prevHeightField.getSize();
        prevHeightField.setSize(totalGrid);
		// 新しい部分はNaNで初期化
        for (int i = oldSize; i < totalGrid; ++i)
            prevHeightField[i] = NAN;
    }

    if (m_indexMap.getCapacity() < totalGrid)
        m_indexMap.reserve(totalGrid);
	m_indexMap.setSize(totalGrid);

    // インデックスマップと現在のHeightFieldの初期化
    for (int gz = 0; gz < gridZ; ++gz)
    {
        for (int gx = 0; gx < gridX; ++gx)
        {
            int newIdx = gz * gridX + gx;
            m_indexMap[newIdx] = -1;

            if (!gridStructureChanged && gx < oldGridX && gz < oldGridZ)
            {
                int oldIdx = gz * oldGridX + gx;
                currHeightField[newIdx] = prevHeightField[oldIdx];
            }
            else
            {
                currHeightField[newIdx] = NAN;
            }
        }
    }

    // 現在のグリッドサイズを記録
	m_prevGridX = gridX;
    m_prevGridZ = gridZ;

	// グリッドの各頂点を生成
    UINT currentVertexIndex = 0;
    for (int gz = 0; gz < gridZ; ++gz)
    {
        for (int gx = 0; gx < gridX; ++gx)
        {
            int idx = gz * gridX + gx;

            float height = heightField.GetHeight(gx, gz);
            float& prevH = prevHeightField[idx];
            float& currH = currHeightField[idx];
            float y = NAN;

            if (!isnan(height))
            {
                if (isnan(prevH))
                    y = currH = height;
                else
                    y = currH = Lerp(prevH, height, 0.2f); // 前フレームの高さと補間
            }

			// 高さがNaNの場合はスキップ
            if (isnan(y))
                continue;

            float x = minX + gx * dx;
            float z = minZ + gz * dz;

            // 頂点生成
            FluidSurfaceVertex v;
            v.position = XMFLOAT3(x, y, z);
            v.texcoord = XMFLOAT2(gx / float(gridX - 1), gz / float(gridZ - 1));
            v.normal = ComputeNormal(heightField, gx, gz);

			m_vertices.push_back(v);
            m_indexMap[idx] = currentVertexIndex++;
        }
    }

    // インデックス生成
    for (int gz = 0; gz < gridZ - 1; ++gz)
    {
        for (int gx = 0; gx < gridX - 1; ++gx)
        {
            int idx0 = gz * gridX + gx;
            int idx1 = gz * gridX + (gx + 1);
            int idx2 = (gz + 1) * gridX + gx;
            int idx3 = (gz + 1) * gridX + (gx + 1);

            int i0 = m_indexMap[idx0];
            int i1 = m_indexMap[idx1];
            int i2 = m_indexMap[idx2];
            int i3 = m_indexMap[idx3];

			// インデックスが有効な場合のみ追加
            if (i0 >= 0 && i1 >= 0 && i2 >= 0)
            {
                m_indices.push_back(i0);
                m_indices.push_back(i2);
                m_indices.push_back(i1);
            }
            if (i1 >= 0 && i2 >= 0 && i3 >= 0)
            {
                m_indices.push_back(i1);
                m_indices.push_back(i2);
                m_indices.push_back(i3);
            }
        }
    }

    // GPUにアップロード
    UpdateBuffers();
}

void FluidSurfaceMesh::UpdateBuffers(void)
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

	UpdateCBFluidSurface();
}

void FluidSurfaceMesh::EnsureBufferCapacity(UINT requiredVertexCount, UINT requiredIndexCount)
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

bool FluidSurfaceMesh::HasNearbyValidHeight(const FluidHeightFieldBuilder& heightField, int gx, int gz) const
{
	const int r = 2; // 近傍探索半径

    for (int dz = -r; dz <= r; ++dz)
    {
        for (int dx = -r; dx <= r; ++dx)
        {
            int nx = gx + dx;
            int nz = gz + dz;

            if (nx >= 0 && nx < heightField.GetGridX() &&
                nz >= 0 && nz < heightField.GetGridZ())
            {
                float h = heightField.GetHeight(nx, nz);
                if (!isnan(h))
					return true; // 有効な高さが見つかった
            }
        }
    }
    return false;
}

void FluidSurfaceMesh::UpdateCBFluidSurface(void)
{
    CBFluidSurface cbData = {};
	cbData.shallowColor = m_shallowColor;
	cbData.deepColor = m_deepColor;
    cbData.fresnelPower = m_fresnelPower;
    cbData.alphaScale = m_alphaScale;
    cbData.thicknessScale = m_thicknessScale;
	cbData.thicknessBias = m_thicknessBias;
    cbData.thicknessNormalizeFactor = 1.0f / 255.0f; // テクスチャの最大値で割る
	cbData.screenSize = XMFLOAT2(static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT));
	cbData.invScreenSize = XMFLOAT2(1.0f / SCREEN_WIDTH, 1.0f / SCREEN_HEIGHT);
	cbData.padding = 0.0f; // パディング（16byteアライン）

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    m_context->Map(m_cbFluidSurface, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &cbData, sizeof(CBFluidSurface));
    m_context->Unmap(m_cbFluidSurface, 0);
}

void FluidSurfaceMesh::Draw(void)
{
    if (!m_vertexBuffer || !m_indexBuffer || m_indices.empty()) return;

    // InputLayout と Shader を切り替える（他の粒子と違うから！）
    m_context->IASetInputLayout(m_shaderSet.inputLayout);
    m_context->VSSetShader(m_shaderSet.vs, nullptr, 0);
    m_context->GSSetShader(nullptr, nullptr, 0); // 必ず GS は切る
    m_context->PSSetShader(m_shaderSet.ps, nullptr, 0);

	// シェーダーリソースのバインド
    m_shaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_FLUID_SURFACE, m_cbFluidSurface);
    m_shaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_FLUID_SURFACE, m_cbFluidSurface);

	// 流体の厚みをSRVとしてバインド
    ID3D11ShaderResourceView* thicknessSRV = m_thicknessSRV;
    m_shaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_SRV_FLUID_THICKNESS, thicknessSRV);

    // 頂点バッファとインデックスバッファの設定
    UINT stride = sizeof(FluidSurfaceVertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    m_context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 描画実行
    m_context->DrawIndexed(static_cast<UINT>(m_indices.getSize()), 0, 0);
}

bool FluidSurfaceMesh::LoadShaders(void)
{
    bool loadShaders = true;

    loadShaders &= ShaderManager::get_instance().HasShaderSet(ShaderSetID::FluidSurface);
    if (loadShaders)
        m_shaderSet = ShaderManager::get_instance().GetShaderSet(ShaderSetID::FluidSurface);

    return loadShaders;
}

void FluidSurfaceMesh::CreateBuffers(void)
{
    HRESULT hr = S_OK;

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(FluidSurfaceVertex) * m_vertexCapacity);
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

XMFLOAT3 FluidSurfaceMesh::ComputeNormal(const FluidHeightFieldBuilder& heightField, int gx, int gz)
{
    float dx = 0.0f, dz = 0.0f;

    // dx = right - left
    if (gx > 0 && gx < heightField.GetGridX() - 1)
    {
        float hl = heightField.GetHeight(gx - 1, gz);
        float hr = heightField.GetHeight(gx + 1, gz);
        if (!isnan(hl) && !isnan(hr))
            dx = hr - hl;
    }

    // dz = up - down
    if (gz > 0 && gz < heightField.GetGridZ() - 1)
    {
        float hd = heightField.GetHeight(gx, gz - 1);
        float hu = heightField.GetHeight(gx, gz + 1);
        if (!isnan(hd) && !isnan(hu))
            dz = hu - hd;
    }

    XMVECTOR n = XMVector3Normalize(XMVectorSet(-dx, 2.0f, -dz, 0.0f));
    XMFLOAT3 normal;
    XMStoreFloat3(&normal, n);
    return normal;
}

void FluidSurfaceMesh::ReleaseBuffers(void)
{
	SafeRelease(&m_vertexBuffer);
	SafeRelease(&m_indexBuffer);
}
