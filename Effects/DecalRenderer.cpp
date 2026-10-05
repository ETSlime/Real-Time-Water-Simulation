//=============================================================================
//
// [DecalRenderer.cpp]
// Author : 
// 
//=============================================================================
#include "Effects/DecalRenderer.h"
#include "Core/TextureMgr.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define DECAL_TEXTURE_PATH		"data/TEXTURE/Effects/bullet_mark.png"

bool DecalRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    m_device = device;
    m_context = context;

    if (!m_device || !m_context)
        return false;

    // シェーダーの読み込み
    if (!LoadShaders())
        return false;

	m_instances.reserve(m_maxDecals);

    // 頂点バッファ（正方形デカール）
    XMFLOAT3 quadVerts[] = {
        { -0.5f, 0, -0.5f },
        {  0.5f, 0, -0.5f },
        { -0.5f, 0,  0.5f },
        {  0.5f, 0,  0.5f },
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.ByteWidth = sizeof(quadVerts);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = quadVerts;

    m_device->CreateBuffer(&vbDesc, &vbData, &m_vertexBuffer);

    // インスタンスバッファ（動的）
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DYNAMIC;
    ibDesc.ByteWidth = sizeof(DecalInstance) * m_maxDecals;
    ibDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_device->CreateBuffer(&ibDesc, nullptr, &m_instanceBuffer);

    m_decalTexture = TextureMgr::get_instance().CreateTexture(DECAL_TEXTURE_PATH);

    return true;
}

void DecalRenderer::Update(void)
{
#ifdef _DEBUG
    LoadShaders();
#endif // _DEBUG


	float deltaTime = m_timer.GetDeltaTime();
	// デカールの寿命を更新し、寿命が尽きたデカールを削除
	for (UINT i = 0; i < m_instances.getSize(); i++)
	{
		m_instances[i].elapsed += deltaTime;
        if (m_instances[i].elapsed >= m_instances[i].lifetime)
        {
            m_instances.erase(i);
            --i;
        }
	}
}

void DecalRenderer::Draw(void)
{
    if (m_instances.empty()) return;

    // インスタンスバッファ更新
    D3D11_MAPPED_SUBRESOURCE mapped{};
    m_context->Map(m_instanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, m_instances.data(), sizeof(DecalInstance) * m_instances.getSize());
    m_context->Unmap(m_instanceBuffer, 0);

    UINT strides[] = { sizeof(XMFLOAT3), sizeof(DecalInstance) };
    UINT offsets[] = { 0, 0 };
    ID3D11Buffer* bufs[] = { m_vertexBuffer, m_instanceBuffer };
    m_context->IASetVertexBuffers(0, 2, bufs, strides, offsets);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_context->IASetInputLayout(m_shaderSet.inputLayout);

    // シェーダーとテクスチャ設定
    m_context->VSSetShader(m_shaderSet.vs, nullptr, 0);
    m_context->PSSetShader(m_shaderSet.ps, nullptr, 0);
	m_resourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_DIFFUSE, m_decalTexture);

    // デカール描画（インスタンス数分）
    m_context->DrawInstanced(4, m_instances.getSize(), 0, 0);
}

void DecalRenderer::AddDecal(const XMFLOAT3& pos, const XMFLOAT3& normal, float size, float lifetime, float alpha)
{
    if (m_instances.getSize() >= m_maxDecals)
        m_instances.erase(0); // 最古を消去して空きを作る

    DecalInstance inst{};
    inst.position = pos;
    inst.normal = normal;
    inst.size = size;
    inst.lifetime = lifetime;
	inst.alpha = alpha;
	inst.padding = XMFLOAT3(0.0f, 0.0f, 0.0f);

    float deg = static_cast<float>(GetRand(0, 0) % 360);
    inst.rotation = XMConvertToRadians(deg);

    int idx = GetRand(0, 7); // ランダムに0～7を取得
    int col = idx % 4;
    int row = idx / 4;
	inst.uvOffset = { col / 4.0f, row / 2.0f };

    m_instances.push_back(inst);
}

bool DecalRenderer::LoadShaders(void)
{
    bool loadShaders = ShaderManager::get_instance().HasShaderSet(ShaderSetID::Decal);
    if (loadShaders)
        m_shaderSet = ShaderManager::get_instance().GetShaderSet(ShaderSetID::Decal);

    return loadShaders;
}
