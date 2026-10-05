//=============================================================================
//
// モデルデータのよみよみ＆管理 [Model.h]
// Author :
// OBJファイルからモデルを読み込み、マテリアルやサブセットを管理しますっ！
// バウンディングボックスの描画や三角形の構築、八分木の生成もできちゃう万能ちゃんですっ
// 
//=============================================================================
#pragma once

#include "main.h"
#include "Core/Graphics/Renderer.h"
#include "Utility/HashMap.h"
#include "Collision/OctreeNode.h"
//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MODEL_MAX_MATERIAL		(16)		// １モデルのMaxマテリアル数

// 反転フラグのビットマスク
enum class AxisFlip : uint8_t
{
	None = 0,
	FlipX = 1 << 0,
	FlipY = 1 << 1,
	FlipZ = 1 << 2,
};

// ビット演算を使えるようにする
inline AxisFlip operator|(AxisFlip a, AxisFlip b) {
	return static_cast<AxisFlip>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline AxisFlip operator&(AxisFlip a, AxisFlip b) {
	return static_cast<AxisFlip>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline bool HasFlip(AxisFlip flags, AxisFlip target) {
	return (flags & target) != AxisFlip::None;
}

enum TextureType
{
	Diffuse,
	Normal,
	Bump,
	Opacity,
	Reflect,
	Translucency
};

//*****************************************************************************
// 構造体定義
//*****************************************************************************

 // 前向宣言
class Model;
class AsyncModelLoader;

// マテリアル構造体
struct MODEL_MATERIAL
{
	char						Name[256] = {};
	MATERIAL					MaterialData;
	char						DiffuseTextureName[256] = {};
	char						NormalTextureName[256] = {};
	char						BumpTextureName[256] = {};
	char						OpacityTextureName[256] = {};
	char						ReflectTextureName[256] = {};
	char						TranslucencyTextureName[256] = {};
};

// 描画サブセット構造体
struct SUBSET
{
	unsigned int	StartIndex;
	unsigned int	IndexNum;
	MODEL_MATERIAL	Material;
	ID3D11ShaderResourceView* diffuseTexture;
	ID3D11ShaderResourceView* normalTexture;
	ID3D11ShaderResourceView* bumpTexture;
	ID3D11ShaderResourceView* opacityTexture;
	ID3D11ShaderResourceView* reflectTexture;
	ID3D11ShaderResourceView* translucencyTexture;

	SUBSET()
	{
		diffuseTexture = nullptr;
		normalTexture = nullptr;
		bumpTexture = nullptr;
		opacityTexture = nullptr;
		reflectTexture = nullptr;
		translucencyTexture = nullptr;
		Material = MODEL_MATERIAL();
		StartIndex = 0;
		IndexNum = 0;
	}

	~SUBSET(){}
};

// モデル構造体
struct StaticModelData
{
	VERTEX_3D*		VertexArray;
	unsigned int	VertexNum;
	unsigned int*	IndexArray;
	unsigned int	IndexNum;

	SUBSET*			SubsetArray;
	unsigned int	SubsetNum;
	BOUNDING_BOX	boundingBox;

	SimpleArray<Triangle*> triangles;

	StaticModelData()
	{
		VertexArray = nullptr;
		IndexArray = nullptr;
		SubsetArray = nullptr;
		VertexNum = 0;
		IndexNum = 0;
		SubsetNum = 0;
	}

	~StaticModelData()
	{
		SAFE_DELETE_ARRAY(VertexArray);
		SAFE_DELETE_ARRAY(IndexArray);
		SAFE_DELETE_ARRAY(SubsetArray);
	}
};

struct MODEL_POOL
{
	Model* pModel;
	unsigned int count;

	MODEL_POOL()
	{
		pModel = nullptr;
		count = 0;
	}

	void AddRef() { count++; }
};

struct StaticMeshPart
{
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
	ID3D11ShaderResourceView* OpacityTexture = nullptr;
	UINT IndexNum = 0;
	UINT StartIndex = 0;
};


class Model
{
	friend class AsyncModelLoader;

public:
	Model(StaticModelData* modelData);
	Model() = delete;

	~Model();
	void DrawModel();
	void DrawBoundingBox();

	// モデルのマテリアルのディフューズを取得する。Max16個分にしてある
	void GetModelDiffuse(XMFLOAT4* diffuse);

	// モデルの指定マテリアルのディフューズをセットする。
	void SetModelDiffuse(int mno, XMFLOAT4 diffuse);

	void BuildBoundingBoxTriangles(const BOUNDING_BOX& box);
	void BuildMeshTriangles(const XMMATRIX& worldMatrix, bool alwaysFaceUp = false);
	bool BuildOctree(bool trackTriangles = false);
	bool BuildVoxelGrid(bool batchInsert = false);

	const StaticModelData* GetModelData(void) { return m_modelData; }
	const SUBSET* GetSubset(void) { return m_modelData->SubsetArray; }
	unsigned int GetSubNum(void) { return m_modelData->SubsetNum; }
	BOUNDING_BOX GetBoundingBox(void) const { return m_modelData->boundingBox; }
	void SetDrawBoundingBox(bool draw) { m_drawBoundingBox = draw; }
	const SimpleArray<Triangle*>* GetTriangles(void) const;
	const SimpleArray<StaticMeshPart>& GetMeshParts(void) const;

	bool GetTrackTriangles(void) const { return m_trackTriangles; }

	bool Deferred_BuildBoundingBoxTriangles(const BOUNDING_BOX& box, bool drawBoundingBox = false, bool trackTriangles = false);
	bool Deferred_BuildMeshTriangles(const XMMATRIX& worldMatrix, bool alwaysFaceUp = false, bool drawBoundingBox = false);
	bool Deferred_BuildMeshVoxelGrid(const XMMATRIX& worldMatrix, bool alwaysFaceUp = false);
	bool Deferred_BuildBoundingBoxVoxelGrid(const BOUNDING_BOX& box);

	static Model* StoreModel(const char* modelPath, AxisFlip flip = AxisFlip::None);
	static MODEL_POOL* GetModel(const char* modelPath);
	static void RemoveModel(const char* modelPath);

private:

	void CreateGPUResources(void);
	void CreateBoundingBoxVertex(void);
	void BuildMeshParts(void);

	mutable SimpleArray<StaticMeshPart> m_meshParts;
	static HashMap<const char*, MODEL_POOL, CharPtrHash, CharPtrEquals> m_modelHashMap;

	ID3D11Buffer* m_vertexBuffer = nullptr;
	ID3D11Buffer* m_indexBuffer = nullptr;

	StaticModelData* m_modelData = nullptr;
	bool			m_drawBoundingBox = false;
	bool			m_GPUResource = false; // GPUリソースを生成したかどうか
	bool			m_buildTriangles = false; // 三角形を構築したかどうか

	ID3D11Buffer* m_BBVertexBuffer = nullptr;

	bool		m_trackTriangles = false; // 三角形の追跡を行うかどうか

	Renderer& m_renderer = Renderer::get_instance();
	ShaderResourceBinder& m_ShaderResourceBinder = ShaderResourceBinder::get_instance();
};