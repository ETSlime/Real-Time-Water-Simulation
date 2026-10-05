//=============================================================================
//
// SwordTrail処理 [SwordTrail.cpp]
// Author : 
//
//=============================================================================

#include "Effects/SpecialEffects/SwordTrail.h"


SwordTrail::SwordTrail(const GameObject<SkinnedMeshModelInstance>* weapon) : m_weapon(weapon)
{
	m_vertexBuffer = nullptr;
	m_swordTrailTexture = nullptr;
	m_maxTrailFrames = 15;
	m_trailType = SwordTrailType::Default;

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = sizeof(VFXVertex) * m_maxTrailFrames * 6;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Renderer::get_instance().GetDevice()->CreateBuffer(&bufferDesc, nullptr, &m_vertexBuffer);

}

SwordTrail::~SwordTrail()
{
	SafeRelease(&m_vertexBuffer);
	SafeRelease(&m_swordTrailTexture);
}


void SwordTrail::GetSwordTrailPoints(XMVECTOR& hiltWorld, XMVECTOR& tipWorld)
{
	if (!m_weapon)
		return;

	BOUNDING_BOX swordAABB = m_weapon->GetSkinnedMeshModelConst()->GetBoundingBox();
	float hiltPosZ = swordAABB.minPoint.z;
	float tipPosZ = swordAABB.maxPoint.z;
	float swordPosY = (swordAABB.maxPoint.y + swordAABB.minPoint.y) * 0.5f;
	float sowrdPosX = (swordAABB.maxPoint.x + swordAABB.minPoint.x) * 0.5f;

	// ローカル座標系のヒルトと先端の座標を取得
	XMFLOAT3 hiltLocalPos = XMFLOAT3(sowrdPosX, swordPosY, hiltPosZ);
	XMFLOAT3 tipLocalPos = XMFLOAT3(sowrdPosX, swordPosY, tipPosZ);

	XMMATRIX swordWorldMatrix = m_weapon->GetWorldMatrix();

	hiltWorld = XMVector3Transform(XMLoadFloat3(&hiltLocalPos), swordWorldMatrix);
	tipWorld = XMVector3Transform(XMLoadFloat3(&tipLocalPos), swordWorldMatrix);
}

void SwordTrail::ClearTrail(void)
{
	m_hiltTrail.clear();
	m_tipTrail.clear();
	m_swordTrailVertices.clear();
}

void SwordTrail::Update(void)
{
	XMVECTOR hiltWorld, tipWorld;
	GetSwordTrailPoints(hiltWorld, tipWorld);

	m_hiltTrail.push_back(hiltWorld);
	m_tipTrail.push_back(tipWorld);

	// トレイルの最初のフレームを削除
	if (static_cast<int>(m_hiltTrail.getSize()) > m_maxTrailFrames)
		m_hiltTrail.erase(0);
	if (static_cast<int>(m_tipTrail.getSize()) > m_maxTrailFrames)
		m_tipTrail.erase(0);

	GenerateSwordTrailMesh();
}

void SwordTrail::GenerateSwordTrailMesh(void)
{
	m_swordTrailVertices.clear();

	if (m_hiltTrail.getSize() < 2) return;

	// トレイルのメッシュを生成
	for (UINT i = 1; i < m_hiltTrail.getSize(); i++)
	{
		XMVECTOR hilt1 = m_hiltTrail[i - 1];
		XMVECTOR tip1 = m_tipTrail[i - 1];
		XMVECTOR hilt2 = m_hiltTrail[i];
		XMVECTOR tip2 = m_tipTrail[i];

		VFXVertex v1, v2, v3, v4;

		XMStoreFloat3(&v1.position, hilt1);
		XMStoreFloat3(&v2.position, tip1);
		XMStoreFloat3(&v3.position, hilt2);
		XMStoreFloat3(&v4.position, tip2);

		// 透明度を設定（ヒルト側が不透明、先端側が透明）
		float alphaHilt = 1.0f;
		float alphaTip = 1.0f;

		v1.color = XMFLOAT4(1, 1, 1, alphaHilt); // 剣柄
		v3.color = XMFLOAT4(1, 1, 1, alphaHilt); // 剣柄
		v2.color = XMFLOAT4(1, 1, 1, alphaTip);  // 剣尖
		v4.color = XMFLOAT4(1, 1, 1, alphaTip);  // 剣尖


		v1.uv = XMFLOAT2(0.0f, 0.0f);
		v2.uv = XMFLOAT2(1.0f, 0.0f);
		v3.uv = XMFLOAT2(0.0f, 1.0f);
		v4.uv = XMFLOAT2(1.0f, 1.0f);

		m_swordTrailVertices.push_back(v1);
		m_swordTrailVertices.push_back(v2);
		m_swordTrailVertices.push_back(v3);

		m_swordTrailVertices.push_back(v3);
		m_swordTrailVertices.push_back(v2);
		m_swordTrailVertices.push_back(v4);
	}
}

bool SwordTrail::LoadTrailTexture(char* path)
{
	HRESULT hr = D3DX11CreateShaderResourceViewFromFile(m_renderer.GetDevice(),
		path,
		NULL,
		NULL,
		&m_swordTrailTexture,
		NULL);

	if (FAILED(hr))
		return false;

	return true;
}

bool SwordTrail::SetTrailType(SwordTrailType type)
{
	bool loadTexture = false;

	if (m_trailType == type)
		return true;
	else
	{
		// テクスチャを解放
		SafeRelease(&m_swordTrailTexture);
		// トレイルタイプを設定
		m_trailType = type;
		// テクスチャを読み込む
		switch (m_trailType)
		{
		case SwordTrailType::Normal:
			loadTexture = LoadTrailTexture(SWORD_TRAIL_TEXTURE_PATH);
			break;
		case SwordTrailType::Flame:
			loadTexture = LoadTrailTexture(SWORD_TRAIL_TEXTURE_PATH);
			break;
		case SwordTrailType::Ice:
			loadTexture = LoadTrailTexture(SWORD_TRAIL_TEXTURE_PATH);
			break;
		case SwordTrailType::Lightning:
			loadTexture = LoadTrailTexture(SWORD_TRAIL_TEXTURE_PATH);
			break;
		}
	}

	return loadTexture;
}

void SwordTrail::SetTrailFrames(int Frames)
{
	m_maxTrailFrames = Frames;
}

void SwordTrail::Draw(void)
{
	if (m_swordTrailVertices.getSize() == 0)
		return;

	VFXVertex* trailVerticesArray = new VFXVertex[m_swordTrailVertices.getSize()];
	for (UINT i = 0; i < m_swordTrailVertices.getSize(); i++)
	{
		trailVerticesArray[i] = m_swordTrailVertices[i];
	}

	// バーテックスバッファを作成
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	Renderer::get_instance().GetDeviceContext()->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, trailVerticesArray, sizeof(VFXVertex) * m_swordTrailVertices.getSize());
	m_renderer.GetDeviceContext()->Unmap(m_vertexBuffer, 0);

	m_renderer.GetDeviceContext()->PSSetShaderResources(0, 1, &m_swordTrailTexture);

	// 描画
	UINT stride = sizeof(VFXVertex);
	UINT offset = 0;
	m_renderer.GetDeviceContext()->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
	m_renderer.GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	m_renderer.GetDeviceContext()->Draw(m_swordTrailVertices.getSize(), 0);

	SAFE_DELETE_ARRAY(trailVerticesArray);
}