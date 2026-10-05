//=============================================================================
//
// モデルの処理 [Model.cpp]
// Author : 
//
//=============================================================================
#define _CRT_SECURE_NO_WARNINGS
#include "main.h"
#include "Model/Model.h"
#include "Core/Camera.h"
#include "Core/Async/AsyncModelLoader.h"
#include "Utility/InputManager.h"
#include "Utility/Debug/Debugproc.h"
#include "Model/ModelCacheLoader.h"
#include "Collision/CollisionManager.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_STATIC_NMODEL_NUM		(30)

//*****************************************************************************
// グローバル変数
//*****************************************************************************
HashMap<const char*, MODEL_POOL, CharPtrHash, CharPtrEquals> Model::m_modelHashMap(
	MAX_STATIC_NMODEL_NUM,
	CharPtrHash(),
	CharPtrEquals()
);

//=============================================================================
// 初期化処理
//=============================================================================
Model::Model(StaticModelData* modelData)
{
	// 所有権移譲：以降 Model が責任を持つ
	m_modelData = modelData;
}


//=============================================================================
// 終了処理
//=============================================================================
Model::~Model()
{
	SafeRelease(&m_vertexBuffer);
	SafeRelease(&m_indexBuffer);
	SafeRelease(&m_BBVertexBuffer);
	SAFE_DELETE(m_modelData);
}


//=============================================================================
// 描画処理
//=============================================================================
void Model::DrawModel()
{
	// 頂点バッファ設定
	UINT stride = sizeof( VERTEX_3D );
	UINT offset = 0;
	m_renderer.GetDeviceContext()->IASetVertexBuffers( 0, 1, &this->m_vertexBuffer, &stride, &offset );

	// インデックスバッファ設定
	m_renderer.GetDeviceContext()->IASetIndexBuffer( this->m_indexBuffer, DXGI_FORMAT_R32_UINT, 0 );

	// プリミティブトポロジ設定
	m_renderer.GetDeviceContext()->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	for( unsigned int i = 0; i < m_modelData->SubsetNum; i++ )
	{
		// サブセット設定
		if (m_modelData->SubsetArray[i].diffuseTexture == NULL)
			m_modelData->SubsetArray[i].Material.MaterialData.noTexSampling = 1;
		else
			m_modelData->SubsetArray[i].Material.MaterialData.noTexSampling = 0;

		if (m_modelData->SubsetArray[i].normalTexture == NULL)
			m_modelData->SubsetArray[i].Material.MaterialData.normalMapSampling = 0;
		else
			m_modelData->SubsetArray[i].Material.MaterialData.normalMapSampling = 1;

		if (m_modelData->SubsetArray[i].bumpTexture == NULL)
			m_modelData->SubsetArray[i].Material.MaterialData.bumpMapSampling = 0;
		else
			m_modelData->SubsetArray[i].Material.MaterialData.bumpMapSampling = 1;

		if (m_modelData->SubsetArray[i].opacityTexture == NULL)
			m_modelData->SubsetArray[i].Material.MaterialData.opacityMapSampling = 0;
		else
			m_modelData->SubsetArray[i].Material.MaterialData.opacityMapSampling = 1;

		if (m_modelData->SubsetArray[i].reflectTexture == NULL)
			m_modelData->SubsetArray[i].Material.MaterialData.reflectMapSampling = 0;
		else
			m_modelData->SubsetArray[i].Material.MaterialData.reflectMapSampling = 1;

		if (m_modelData->SubsetArray[i].translucencyTexture == NULL)
			m_modelData->SubsetArray[i].Material.MaterialData.translucencyMapSampling = 0;
		else
			m_modelData->SubsetArray[i].Material.MaterialData.translucencyMapSampling = 1;

		// マテリアル設定
		if (m_modelData->SubsetArray[i].Material.MaterialData.LoadMaterial)
			m_renderer.SetMaterial( m_modelData->SubsetArray[i].Material.MaterialData);

		// テクスチャ設定
		if (m_modelData->SubsetArray[i].Material.MaterialData.noTexSampling == 0)
		{
			m_ShaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_DIFFUSE, m_modelData->SubsetArray[i].diffuseTexture);
		}
		if (m_modelData->SubsetArray[i].Material.MaterialData.normalMapSampling == 1)
		{
			m_ShaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_NORMAL, m_modelData->SubsetArray[i].normalTexture);
		}
		if (m_modelData->SubsetArray[i].Material.MaterialData.bumpMapSampling == 1)
		{
			m_ShaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_BUMP, m_modelData->SubsetArray[i].bumpTexture);
		}
		if (m_modelData->SubsetArray[i].Material.MaterialData.opacityMapSampling == 1)
		{
			m_ShaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_OPACITY, m_modelData->SubsetArray[i].opacityTexture);
		}
		if (m_modelData->SubsetArray[i].Material.MaterialData.reflectMapSampling == 1)
		{
			m_ShaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_REFLECT, m_modelData->SubsetArray[i].reflectTexture);
		}
		if (m_modelData->SubsetArray[i].Material.MaterialData.translucencyMapSampling == 1)
		{
			m_ShaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_TRANSLUCENCY, m_modelData->SubsetArray[i].translucencyTexture);
		}

		// ポリゴン描画
		m_renderer.GetDeviceContext()->DrawIndexed( m_modelData->SubsetArray[i].IndexNum, m_modelData->SubsetArray[i].StartIndex, 0 );
	}

#ifdef _DEBUG
	if (m_drawBoundingBox)
		DrawBoundingBox();
#endif
}

void Model::DrawBoundingBox()
{
	m_renderer.SetFillMode(D3D11_FILL_WIREFRAME);
	// 頂点バッファ設定
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	m_renderer.GetDeviceContext()->IASetVertexBuffers(0, 1, &this->m_BBVertexBuffer, &stride, &offset);

	// プリミティブトポロジ設定
	m_renderer.GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	MATERIAL material;
	ZeroMemory(&material, sizeof(material));
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.noTexSampling = TRUE;
	m_renderer.SetMaterial(material);

	m_renderer.GetDeviceContext()->Draw(24, 0);
	m_renderer.SetFillMode(D3D11_FILL_SOLID);
}



void Model::CreateGPUResources(void)
{
	// 二重初期化チェック
	if (m_GPUResource) 
		return;

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX_3D) * m_modelData->VertexNum;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.pSysMem = m_modelData->VertexArray;

	m_renderer.GetDevice()->CreateBuffer(&bd, &sd, &this->m_vertexBuffer);


	bd.ByteWidth = sizeof(VERTEX_3D) * 24;
	m_renderer.GetDevice()->CreateBuffer(&bd, NULL, &this->m_BBVertexBuffer);

	CreateBoundingBoxVertex();

	// インデックスバッファ生成
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(unsigned int) * m_modelData->IndexNum;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;

	ZeroMemory(&sd, sizeof(sd));
	sd.pSysMem = m_modelData->IndexArray;

	m_renderer.GetDevice()->CreateBuffer(&bd, &sd, &this->m_indexBuffer);

	// サブセット設定
	for (unsigned int i = 0; i < m_modelData->SubsetNum; i++)
	{
		if (m_modelData->SubsetArray[i].diffuseTexture == NULL)
			m_modelData->SubsetArray[i].Material.MaterialData.noTexSampling = 1;

		if (m_modelData->SubsetArray[i].normalTexture == NULL)
			m_modelData->SubsetArray[i].Material.MaterialData.normalMapSampling = 0;

		if (m_modelData->SubsetArray[i].bumpTexture == NULL)
			m_modelData->SubsetArray[i].Material.MaterialData.bumpMapSampling = 0;

		if (m_modelData->SubsetArray[i].opacityTexture == NULL)
			m_modelData->SubsetArray[i].Material.MaterialData.opacityMapSampling = 0;

		if (m_modelData->SubsetArray[i].reflectTexture == NULL)
			m_modelData->SubsetArray[i].Material.MaterialData.reflectMapSampling = 0;

		if (m_modelData->SubsetArray[i].translucencyTexture == NULL)
			m_modelData->SubsetArray[i].Material.MaterialData.translucencyMapSampling = 0;
	}

	if (!m_GPUResource)
		m_GPUResource = true;
	
}




// モデルの全マテリアルのディフューズを取得する。Max16個分にしてある
void Model::GetModelDiffuse(XMFLOAT4 *diffuse)
{
	unsigned int max = (m_modelData->SubsetNum < MODEL_MAX_MATERIAL) ? m_modelData->SubsetNum : MODEL_MAX_MATERIAL;

	for (unsigned int i = 0; i < max; i++)
	{
		// ディフューズ設定
		diffuse[i] = m_modelData->SubsetArray[i].Material.MaterialData.Diffuse;
	}
}


// モデルの指定マテリアルのディフューズをセットする。
void Model::SetModelDiffuse(int mno, XMFLOAT4 diffuse)
{
	// ディフューズ設定
	m_modelData->SubsetArray[mno].Material.MaterialData.Diffuse = diffuse;
}

void Model::CreateBoundingBoxVertex()
{
	D3D11_MAPPED_SUBRESOURCE msr;
	m_renderer.GetDeviceContext()->Map(this->m_BBVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

	// 頂点座標の設定
	vertex[0].Position = XMFLOAT3(m_modelData->boundingBox.minPoint.x, m_modelData->boundingBox.minPoint.y, m_modelData->boundingBox.minPoint.z);
	vertex[1].Position = XMFLOAT3(m_modelData->boundingBox.maxPoint.x, m_modelData->boundingBox.minPoint.y, m_modelData->boundingBox.minPoint.z);
	vertex[2].Position = XMFLOAT3(m_modelData->boundingBox.minPoint.x, m_modelData->boundingBox.maxPoint.y, m_modelData->boundingBox.minPoint.z);

	vertex[3].Position = XMFLOAT3(m_modelData->boundingBox.maxPoint.x, m_modelData->boundingBox.minPoint.y, m_modelData->boundingBox.minPoint.z);
	vertex[4].Position = XMFLOAT3(m_modelData->boundingBox.maxPoint.x, m_modelData->boundingBox.maxPoint.y, m_modelData->boundingBox.minPoint.z);
	vertex[5].Position = XMFLOAT3(m_modelData->boundingBox.minPoint.x, m_modelData->boundingBox.maxPoint.y, m_modelData->boundingBox.minPoint.z);

	vertex[6].Position = XMFLOAT3(m_modelData->boundingBox.minPoint.x, m_modelData->boundingBox.minPoint.y, m_modelData->boundingBox.maxPoint.z);
	vertex[7].Position = XMFLOAT3(m_modelData->boundingBox.maxPoint.x, m_modelData->boundingBox.minPoint.y, m_modelData->boundingBox.maxPoint.z);
	vertex[8].Position = XMFLOAT3(m_modelData->boundingBox.minPoint.x, m_modelData->boundingBox.maxPoint.y, m_modelData->boundingBox.maxPoint.z);

	vertex[9].Position = XMFLOAT3(m_modelData->boundingBox.maxPoint.x, m_modelData->boundingBox.minPoint.y, m_modelData->boundingBox.maxPoint.z);
	vertex[10].Position = XMFLOAT3(m_modelData->boundingBox.maxPoint.x, m_modelData->boundingBox.maxPoint.y, m_modelData->boundingBox.maxPoint.z);
	vertex[11].Position = XMFLOAT3(m_modelData->boundingBox.minPoint.x, m_modelData->boundingBox.maxPoint.y, m_modelData->boundingBox.maxPoint.z);

	vertex[12].Position = XMFLOAT3(m_modelData->boundingBox.minPoint.x, m_modelData->boundingBox.maxPoint.y, m_modelData->boundingBox.minPoint.z);
	vertex[13].Position = XMFLOAT3(m_modelData->boundingBox.maxPoint.x, m_modelData->boundingBox.maxPoint.y, m_modelData->boundingBox.minPoint.z);
	vertex[14].Position = XMFLOAT3(m_modelData->boundingBox.minPoint.x, m_modelData->boundingBox.maxPoint.y, m_modelData->boundingBox.maxPoint.z);

	vertex[15].Position = XMFLOAT3(m_modelData->boundingBox.maxPoint.x, m_modelData->boundingBox.maxPoint.y, m_modelData->boundingBox.minPoint.z);
	vertex[16].Position = XMFLOAT3(m_modelData->boundingBox.maxPoint.x, m_modelData->boundingBox.maxPoint.y, m_modelData->boundingBox.maxPoint.z);
	vertex[17].Position = XMFLOAT3(m_modelData->boundingBox.minPoint.x, m_modelData->boundingBox.maxPoint.y, m_modelData->boundingBox.maxPoint.z);

	vertex[18].Position = XMFLOAT3(m_modelData->boundingBox.minPoint.x, m_modelData->boundingBox.minPoint.y, m_modelData->boundingBox.minPoint.z);
	vertex[19].Position = XMFLOAT3(m_modelData->boundingBox.maxPoint.x, m_modelData->boundingBox.minPoint.y, m_modelData->boundingBox.minPoint.z);
	vertex[20].Position = XMFLOAT3(m_modelData->boundingBox.minPoint.x, m_modelData->boundingBox.minPoint.y, m_modelData->boundingBox.maxPoint.z);

	vertex[21].Position = XMFLOAT3(m_modelData->boundingBox.maxPoint.x, m_modelData->boundingBox.minPoint.y, m_modelData->boundingBox.minPoint.z);
	vertex[22].Position = XMFLOAT3(m_modelData->boundingBox.maxPoint.x, m_modelData->boundingBox.minPoint.y, m_modelData->boundingBox.maxPoint.z);
	vertex[23].Position = XMFLOAT3(m_modelData->boundingBox.minPoint.x, m_modelData->boundingBox.minPoint.y, m_modelData->boundingBox.maxPoint.z);


	// 法線の設定
	vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[4].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[5].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[6].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[7].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[8].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[9].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[10].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[11].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[12].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[13].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[14].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[15].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[16].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[17].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[18].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[19].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[20].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[21].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[22].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[23].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);

	// 拡散光の設定
	vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[4].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[5].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[6].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[7].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[8].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[9].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[10].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[11].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[12].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[13].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[14].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[15].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[16].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[17].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[18].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[19].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[20].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[21].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[22].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[23].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	// テクスチャ座標の設定
	vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);
	vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);
	vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);
	vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);
	vertex[4].TexCoord = XMFLOAT2(0.0f, 0.0f);
	vertex[5].TexCoord = XMFLOAT2(1.0f, 0.0f);
	vertex[6].TexCoord = XMFLOAT2(0.0f, 1.0f);
	vertex[7].TexCoord = XMFLOAT2(1.0f, 1.0f);
	vertex[8].TexCoord = XMFLOAT2(0.0f, 0.0f);
	vertex[9].TexCoord = XMFLOAT2(1.0f, 0.0f);
	vertex[10].TexCoord = XMFLOAT2(0.0f, 1.0f);
	vertex[11].TexCoord = XMFLOAT2(1.0f, 1.0f);
	vertex[12].TexCoord = XMFLOAT2(0.0f, 0.0f);
	vertex[13].TexCoord = XMFLOAT2(1.0f, 0.0f);
	vertex[14].TexCoord = XMFLOAT2(0.0f, 1.0f);
	vertex[15].TexCoord = XMFLOAT2(1.0f, 1.0f);
	vertex[16].TexCoord = XMFLOAT2(0.0f, 0.0f);
	vertex[17].TexCoord = XMFLOAT2(1.0f, 0.0f);
	vertex[18].TexCoord = XMFLOAT2(0.0f, 1.0f);
	vertex[19].TexCoord = XMFLOAT2(1.0f, 1.0f);
	vertex[20].TexCoord = XMFLOAT2(0.0f, 0.0f);
	vertex[21].TexCoord = XMFLOAT2(1.0f, 0.0f);
	vertex[22].TexCoord = XMFLOAT2(0.0f, 1.0f);
	vertex[23].TexCoord = XMFLOAT2(1.0f, 1.0f);

	m_renderer.GetDeviceContext()->Unmap(this->m_BBVertexBuffer, 0);
}

void Model::BuildMeshParts(void)
{
	m_meshParts.clear();

	ID3D11Buffer* sharedVB = m_vertexBuffer;
	ID3D11Buffer* sharedIB = m_indexBuffer;

	for (unsigned int i = 0; i < m_modelData->SubsetNum; i++)
	{
		StaticMeshPart part = {};
		part.VertexBuffer = sharedVB;
		part.IndexBuffer = sharedIB;
		part.IndexNum = m_modelData->SubsetArray[i].IndexNum;
		part.StartIndex = m_modelData->SubsetArray[i].StartIndex;
		part.OpacityTexture = m_modelData->SubsetArray[i].opacityTexture;
		m_meshParts.push_back(part);
	}
}

void Model::BuildBoundingBoxTriangles(const BOUNDING_BOX& box)
{
	if (m_buildTriangles) return;

	// AABB の 8 つの頂点を定義
	XMFLOAT3 v0 = XMFLOAT3(box.minPoint.x, box.minPoint.y, box.minPoint.z);
	XMFLOAT3 v1 = XMFLOAT3(box.maxPoint.x, box.minPoint.y, box.minPoint.z);
	XMFLOAT3 v2 = XMFLOAT3(box.minPoint.x, box.maxPoint.y, box.minPoint.z);
	XMFLOAT3 v3 = XMFLOAT3(box.maxPoint.x, box.maxPoint.y, box.minPoint.z);
	XMFLOAT3 v4 = XMFLOAT3(box.minPoint.x, box.minPoint.y, box.maxPoint.z);
	XMFLOAT3 v5 = XMFLOAT3(box.maxPoint.x, box.minPoint.y, box.maxPoint.z);
	XMFLOAT3 v6 = XMFLOAT3(box.minPoint.x, box.maxPoint.y, box.maxPoint.z);
	XMFLOAT3 v7 = XMFLOAT3(box.maxPoint.x, box.maxPoint.y, box.maxPoint.z);

	// 各面の法線
	XMFLOAT3 normalXPlus = XMFLOAT3(1, 0, 0);   // +X 面の法線
	XMFLOAT3 normalXMinus = XMFLOAT3(-1, 0, 0);  // -X 面の法線
	XMFLOAT3 normalYPlus = XMFLOAT3(0, 1, 0);   // +Y 面の法線
	XMFLOAT3 normalYMinus = XMFLOAT3(0, -1, 0);  // -Y 面の法線
	XMFLOAT3 normalZPlus = XMFLOAT3(0, 0, 1);   // +Z 面の法線
	XMFLOAT3 normalZMinus = XMFLOAT3(0, 0, -1);  // -Z 面の法線

	// 各面を 2 つの三角形で作成（法線を追加）

	// -X 面
	m_modelData->triangles.push_back(new Triangle(v3, v7, v5, normalXMinus)); // 三角形 1
	m_modelData->triangles.push_back(new Triangle(v3, v5, v1, normalXMinus)); // 三角形 2

	// +X 面
	m_modelData->triangles.push_back(new Triangle(v2, v0, v4, normalXPlus)); // 三角形 3
	m_modelData->triangles.push_back(new Triangle(v2, v4, v6, normalXPlus)); // 三角形 4

	// -Y 面
	m_modelData->triangles.push_back(new Triangle(v2, v6, v7, normalYPlus)); // 三角形 5
	m_modelData->triangles.push_back(new Triangle(v2, v7, v3, normalYPlus)); // 三角形 6

	// +Y 面
	m_modelData->triangles.push_back(new Triangle(v0, v1, v5, normalYMinus)); // 三角形 7
	m_modelData->triangles.push_back(new Triangle(v0, v5, v4, normalYMinus)); // 三角形 8

	// -Z 面
	m_modelData->triangles.push_back(new Triangle(v7, v6, v4, normalZMinus)); // 三角形 9
	m_modelData->triangles.push_back(new Triangle(v7, v4, v5, normalZMinus)); // 三角形 10

	// +Z 面
	m_modelData->triangles.push_back(new Triangle(v3, v2, v0, normalZPlus)); // 三角形 11
	m_modelData->triangles.push_back(new Triangle(v3, v0, v1, normalZPlus)); // 三角形 12

	m_buildTriangles = true;
}

void Model::BuildMeshTriangles(const XMMATRIX& worldMatrix, bool alwaysFaceUp)
{
	if (m_buildTriangles) return;

	XMFLOAT3 worldPos1, worldPos2;

	XMVECTOR localAABBMax = XMVectorSet(
		m_modelData->boundingBox.maxPoint.x,
		m_modelData->boundingBox.maxPoint.y,
		m_modelData->boundingBox.maxPoint.z,
		1.0f
	);

	XMVECTOR localAABBMin = XMVectorSet(
		m_modelData->boundingBox.minPoint.x,
		m_modelData->boundingBox.minPoint.y,
		m_modelData->boundingBox.minPoint.z,
		1.0f
	);


	XMVECTOR worldPosMax = XMVector3Transform(localAABBMax, worldMatrix);
	XMVECTOR worldPosMin = XMVector3Transform(localAABBMin, worldMatrix);


	XMStoreFloat3(&worldPos1, worldPosMax);
	XMStoreFloat3(&worldPos2, worldPosMin);


	int vertexNum = m_modelData->VertexNum;
	SimpleArray<XMFLOAT3> triangleVertices(3);
	m_modelData->triangles.reserve(ceil(vertexNum / 3.0f));
	for (int i = 0; i < vertexNum; i++)
	{
		XMFLOAT3 worldPos;

		XMVECTOR localPosVec = XMVectorSet(
			m_modelData->VertexArray[i].Position.x,
			m_modelData->VertexArray[i].Position.y,
			m_modelData->VertexArray[i].Position.z,
			1.0f
		);

		XMVECTOR worldPosVec = XMVector3Transform(localPosVec, worldMatrix);
		XMStoreFloat3(&worldPos, worldPosVec);

		triangleVertices.push_back(worldPos);

		if ((i + 1) % 3 == 0)
		{
			Triangle* triangle = new Triangle(
				triangleVertices[0],
				triangleVertices[1],
				triangleVertices[2],
				alwaysFaceUp
			);

			m_modelData->triangles.push_back(triangle);
			triangleVertices.clear();

		}
	}

	m_buildTriangles = true;
}

bool Model::BuildOctree(bool trackTriangles)
{
	m_trackTriangles = trackTriangles;

	if (!m_modelData->triangles.getSize())
		return false;

	if (!CollisionManager::get_instance().IsInitializedOctree())
		CollisionManager::get_instance().InitOctree(m_modelData->boundingBox);

	int numTriangles = m_modelData->triangles.getSize();
	for (int i = 0; i < numTriangles; i++)
	{
		if (!CollisionManager::get_instance().OctreeInsertTriangle(m_modelData->triangles[i], trackTriangles))
			return false;
	}

	return true;
}

bool Model::BuildVoxelGrid(bool batchInsert)
{
	if (!m_modelData->triangles.getSize())
		return false;

	if (batchInsert)
	{
		CollisionManager::get_instance().VoxelGridInsertTriangles(m_modelData->triangles);
	}
	else
	{
		int numTriangles = m_modelData->triangles.getSize();
		for (int i = 0; i < numTriangles; i++)
		{
			CollisionManager::get_instance().VoxelGridInsertTriangle(m_modelData->triangles[i]);
		}
	}

	return true;
}

const SimpleArray<Triangle*>* Model::GetTriangles(void) const
{
	return &m_modelData->triangles;
}

const SimpleArray<StaticMeshPart>& Model::GetMeshParts(void) const
{
	if (m_meshParts.empty())
	{
		// const_castを使っているのは、constメンバ関数内で非constメンバ関数を呼ぶため
		// ただし、const_castはあまり好ましくないので、注意して使うこと
		// ここでは、const_castを使わないとコンパイルエラーになるので、やむを得ず使用している
		const_cast<Model*>(this)->BuildMeshParts();
	}
	return m_meshParts;
}

bool Model::Deferred_BuildBoundingBoxTriangles(const BOUNDING_BOX& box, bool drawBoundingBox, bool trackTriangles)
{
	SetDrawBoundingBox(drawBoundingBox);

	bool success = false;
	BuildBoundingBoxTriangles(box);
	success = BuildOctree(trackTriangles);

	return success;
}

bool Model::Deferred_BuildMeshTriangles(const XMMATRIX& worldMatrix, bool alwaysFaceUp, bool drawBoundingBox)
{
	SetDrawBoundingBox(drawBoundingBox);

	bool success = false;
	BuildMeshTriangles(worldMatrix, alwaysFaceUp);
	success = BuildOctree();

	return success;
}

bool Model::Deferred_BuildMeshVoxelGrid(const XMMATRIX& worldMatrix, bool alwaysFaceUp)
{
	bool success = false;
	BuildMeshTriangles(worldMatrix, alwaysFaceUp);
	success = BuildVoxelGrid(true);

	return success;
}

bool Model::Deferred_BuildBoundingBoxVoxelGrid(const BOUNDING_BOX& box)
{
	bool success = false;
	BuildBoundingBoxTriangles(box);
	success = BuildVoxelGrid(false);

	return success;
}

Model* Model::StoreModel(const char* modelPath, AxisFlip flip)
{
	MODEL_POOL* modelPool = GetModel(modelPath);

	// 既にロードされている場合は、カウントを増やしてポインタを返す
	if (modelPool)
	{
		modelPool->count++;
		return modelPool->pModel;
	}


	// モデルが AsyncModelLoader によってロード済みか？
	AsyncModelLoader& loader = AsyncModelLoader::get_instance();

	// モデルがロード済みかチェック
	if (loader.IsLoaded<StaticModelLoadTask>(modelPath))
	{
		// CreateModelIfReady() で GPU リソースを作成
		Model* model = loader.CreateModelIfReady(modelPath);

		// 安全チェック：まれにロードフラグありでもモデル構築失敗するケース
		if (!model) 
			return nullptr; // ロード済みと見せかけて失敗していたケース

		// モデルプールへ登録
		modelPool = new MODEL_POOL;
		modelPool->pModel = model;
		modelPool->count = 1;
		m_modelHashMap.insert(modelPath, *modelPool);
		return model;
	}

	// 未ロード → ロード要求を保留リストに登録（次のフレームで処理）
	loader.RequestLoadIfNotQueued(modelPath, flip);


	// 今はリソースがないので null
	return nullptr;
}

MODEL_POOL* Model::GetModel(const char* modelPath)
{
	return  m_modelHashMap.search(modelPath);
}

void Model::RemoveModel(const char* modelPath)
{
	m_modelHashMap.remove(modelPath);
}
