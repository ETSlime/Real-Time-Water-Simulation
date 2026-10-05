#pragma once
//=============================================================================
//
// FBXファイルの読み込みクラス [FBXLoader.h]
// Author : 
// FBX形式のモデルファイルを解析し、アニメーション・マテリアル・ボーン情報などを読み込むシングルトンクラスです。
// モデルデータの構築や階層構造の構築、頂点データやUV情報の取得も行います
//
//=============================================================================
#include "main.h"
#include "Core/Graphics/Renderer.h"
#include "Utility/HashMap.h"
#include "Core/TextureMgr.h"
#include "Utility/SimpleArray.h"
#include "Utility/SingletonBase.h"
#include "AI/AnimStateMachine.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************

#define MAX_NODE_NUM			8192
#define TEXTURE_NAME_LENGTH		256
#define SMALL_NUM_THRESHOLD		1e-6
enum class FbxNodeType
{
	LimbNode,
	Mesh,
	Material,
	Skin,
	Cluster,
	BlendShape,
	BlendShapeChannel,
	BindPose,
	AnimationCurve,
	AnimationCurveNode,
	None,
};

enum class ObjectType
{
	Model,
	Material,
	Texture,
	AnimationStack,
	AnimationLayer,
	Geometry,
	Implementation,
	BindingTable,
	NONE
};

enum MappingInformationType
{
	ByPolygon,
	ByPolygonVertex,
	ByVertice,
	ByEdge,
	AllSame,
	NoMappingInformation
};

enum ShadingModelEnum
{
	Phong,
};

enum ReferenceInformationType
{
	Direct,
	IndexToDirect
};

enum class GeometryType
{
	Mesh,
	Shape,
};

enum class SkinnedModelType
{
	Default,
	Sigewinne,
	Klee,
	Lumine,
	Weapon,
	Hilichurl,
	Mitachurl,
	GBMonster,
	Boss,
	NPC,
	AbyssMage,
	Field,
	Skybox,
	Town_LOD0,
	Town_LOD1,
	Town_LOD2,
	Church,
};

//*****************************************************************************
// 構造体定義
//*****************************************************************************

struct ModelProperty
{
	int			modelIdx;
	BOOL		QuaternionInterpolate;
	BOOL		Visibility;
	XMFLOAT3	Translation;
	XMFLOAT3	Rotation;
	XMFLOAT3	Scaling;
	XMFLOAT3	RotationOffset;
	XMFLOAT3	RotationPivot;
	XMFLOAT3	ScalingOffset;
	XMFLOAT3	ScalingPivot;
	BOOL		TranslationActive;
	XMFLOAT3	TranslationMin;
	XMFLOAT3	TranslationMax;
	BOOL		TranslationMinX;
	BOOL		TranslationMinY;
	BOOL		TranslationMinZ;
	BOOL		TranslationMaxX;
	BOOL		TranslationMaxY;
	BOOL		TranslationMaxZ;
	UINT		RotationOrder;
	BOOL		RotationSpaceForLimitOnly;
	double		AxisLen;
	XMFLOAT3	PreRotation;
	XMFLOAT3	PostRotation;
	BOOL		RotationActive;
	XMFLOAT3	RotationMin;
	XMFLOAT3	RotationMax;
	BOOL		RotationMinX;
	BOOL		RotationMinY;
	BOOL		RotationMinZ;
	BOOL		RotationMaxX;
	BOOL		RotationMaxY;
	BOOL		RotationMaxZ;
	double		RotationStiffnessX;
	double		RotationStiffnessY;
	double		RotationStiffnessZ;
	double		MinDampRangeX;
	double		MinDampRangeY;
	double		MinDampRangeZ;
	double		MaxDampRangeX;
	double		MaxDampRangeY;
	double		MaxDampRangeZ;
	double		MinDampStrengthX;
	double		MinDampStrengthY;
	double		MinDampStrengthZ;
	double		MaxDampStrengthX;
	double		MaxDampStrengthY;
	double		MaxDampStrengthZ;
	double		PreferedAngleX;
	double		PreferedAngleY;
	double		PreferedAngleZ;
	UINT		InheritType;
	BOOL		ScalingActive;
	XMFLOAT3	ScalingMin;
	XMFLOAT3	ScalingMax;
	BOOL		ScalingMinX;
	BOOL		ScalingMinY;
	BOOL		ScalingMinZ;
	BOOL		ScalingMaxX;
	BOOL		ScalingMaxY;
	BOOL		ScalingMaxZ;
	XMFLOAT3	GeometricTranslation;
	XMFLOAT3	GeometricRotation;
	XMFLOAT3	GeometricScaling;
	void*		LookAtProperty;
	void*		UpVectorProperty;
	BOOL		Show;
	BOOL		NegativePercentShapeSupport;
	int			DefaultAttributeIndex;
	XMFLOAT3	Color;
	int			Camera_Index;
	double		Size;
	double		LimbLength;
	int			Look;
	BOOL		Freeze;
	BOOL		LODBox;
	BOOL		VisibilityInheritance;

	void Clear()
	{
		modelIdx = -1;
		Translation = XMFLOAT3(0.0f, 0.0f, 0.0f);
		Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
		Scaling = XMFLOAT3(1.0f, 1.0f, 1.0f);
	}

	ModelProperty()
	{
		Clear();
	}
};

struct Deformer
{
	SimpleArray<int> Index;
	SimpleArray<float> Weights;
	XMFLOAT4X4 Transform;
	XMFLOAT4X4 TransformLink;
	int	boneIdx;
};

struct AnimationCurve
{
	SimpleArray<uint64_t> KeyTime;
	SimpleArray<float> KeyValue;
	int KeyAttrRefCount;
	float DefaultValue;
};

struct AnimationCurveNode
{
	uint64_t dX;
	uint64_t dY;
	uint64_t dZ;
};

struct LimbNodeAnimation
{
	uint64_t LclTranslation;
	uint64_t LclRot;
	uint64_t LclScl;
};

struct BindPoseData
{
	uint64_t	ModelNodeID;
	XMFLOAT4X4	mtxBindPose;
};

struct BindPose
{
	int	numePoseNodes;
	SimpleArray<BindPoseData> bindPoseData;
	HashMap<uint64_t, XMFLOAT4X4, HashUInt64, EqualUInt64> mtxBindPoses = 
		HashMap<uint64_t, XMFLOAT4X4, HashUInt64, EqualUInt64>(
		MAX_NODE_NUM,
		HashUInt64(),
		EqualUInt64()
	);
};

struct IndexData
{
	int Index;
	XMFLOAT3 Normal;
	XMFLOAT3 Tangent;
	XMFLOAT3 Bitangent;
	XMFLOAT2 TexCoord;
};

struct SKINNED_VERTEX_3D_TEMP
{
	XMFLOAT3	Position;
	XMFLOAT3	Normal;
	XMFLOAT3	Tangent;
	XMFLOAT3	Bitangent;
	XMFLOAT4	Diffuse;
	XMFLOAT2	TexCoord;
	SimpleArray<float>	Weights;
	SimpleArray<int>	BoneIndices;

	//SimpleArray<SimpleArray<float>>	ModelWeights;
	//SimpleArray<int>	BoneIndices;
};

struct ModelMeshData
{
	SimpleArray<SKINNED_VERTEX_3D> Vertices;
	SimpleArray<SKINNED_VERTEX_3D_TEMP> VerticesTemp;
	SimpleArray<IndexData> IndicesData;
};

struct Subset
{
	Subset() :
		Id(-1),
		VertexStart(0), VertexCount(0),
		FaceStart(0), FaceCount(0)
	{
	}

	UINT Id;
	UINT VertexStart;
	UINT VertexCount;
	UINT FaceStart;
	UINT FaceCount;
};

struct FbxNode
{
	uint64_t	nodeID;
	char nodeName[128];
	FbxNodeType nodeType;
	SimpleArray<FbxNode*>	parentNodes;
	SimpleArray<FbxNode*>	childNodes;
	void*	nodeData;
	void*	limbNodeAnimation;

	FbxNode() : nodeID(MAXUINT64), nodeName(""), nodeType(FbxNodeType::None), parentNodes(SimpleArray<FbxNode*>()), nodeData(nullptr), limbNodeAnimation(nullptr) {};

	FbxNode(uint64_t nodeID) : nodeName("")
	{
		this->nodeID = nodeID;
		nodeData = nullptr;
		limbNodeAnimation = nullptr;
		nodeType = FbxNodeType::None;
		parentNodes = SimpleArray<FbxNode*>();
		childNodes = SimpleArray<FbxNode*>();
	}

	FbxNode(uint64_t nodeID, char* nodeName, FbxNodeType nodeType)
	{
		this->nodeID = nodeID;
		memset(nodeName, 0, sizeof(nodeName));
		strcpy_s(this->nodeName, nodeName);
		this->nodeType = nodeType;
		nodeData = nullptr;
		limbNodeAnimation = nullptr;
		parentNodes = SimpleArray<FbxNode*>();
		childNodes = SimpleArray<FbxNode*>();
	}

	~FbxNode()
	{
		//if (parentNode)
		//{
		//	delete parentNode;
		//	parentNode = nullptr;
		//}
		//	
		if (nodeData)
		{
			delete nodeData;
			nodeData = nullptr;
		}
		if (parentNodes.getSize() > 0)
			parentNodes.clear();
		if (childNodes.getSize() > 0)
			childNodes.clear();
	}
};

struct FbxMaterial
{
	ShadingModelEnum	ShadingModel;
	BOOL				MultiLayer;
	XMFLOAT3			EmissiveColor;
	float				EmissiveFactor;
	XMFLOAT3			AmbientColor;
	float				AmbientFactor;
	XMFLOAT3			DiffuseColor;
	float				DiffuseFactor;
	XMFLOAT3			Bump;
	XMFLOAT3			NormalMap;
	double				BumpFactor;
	XMFLOAT3			TransparentColor;
	double				TransparencyFactor;
	XMFLOAT3			DisplacementColor;
	double				DisplacementFactor;
	XMFLOAT3			VectorDisplacementColor;
	double				VectorDisplacementFactor;
	XMFLOAT3			SpecularColor;
	float				SpecularFactor;
	float				ShininessExponent;
	XMFLOAT3			ReflectionColor;
	float				ReflectionFactor;

	char				DiffuseColorTexName[TEXTURE_NAME_LENGTH];
	char				EmissiveColorTexName[TEXTURE_NAME_LENGTH];
};

class SkinnedMeshModel;
struct MeshData;
struct SkinnedMeshModelData;

class FBXLoader : public SingletonBase<FBXLoader>
{
public:
	FBXLoader() {};
	bool LoadModel(SkinnedMeshModelData* modelData, const char* modelPath, const char* modelName, const char* texturePath, 
		AnimClipName name = AnimClipName::ANIM_NONE, SkinnedModelType modelType = SkinnedModelType::Default);

	bool LoadAnimation(SkinnedMeshModel* model, SkinnedMeshModelData* modelData, const char* modelPath, const char* modelName, 
		AnimClipName name, AnimPlayMode animPlayMode = AnimPlayMode::LOOP);

private:
	bool ParseObjectDefinitions(FILE* file, SkinnedMeshModelData* model);
	bool ParseObjectProperties(FILE* file, SkinnedMeshModelData* model, bool loadAnimation = false);
	bool ParseGeometry(FILE* file, SkinnedMeshModelData* model, GeometryType geometryType, FbxNode* node);
	bool ParseModel(FILE* file, SkinnedMeshModelData* model, FbxNode* node);
	bool ParseMaterial(FILE* file, SkinnedMeshModelData* model, FbxNode* node);
	bool ParseDeformer(FILE* file, FbxNode* node);
	bool ParseMaterialProperty(FILE* file, FbxMaterial& globalMaterial);
	bool ParseModelProperty(FILE* file, ModelProperty& modelProperty, FbxNode* node);
	bool ParseVertexData(FILE* file, SkinnedMeshModelData* model, GeometryType geometryType, FbxNode* node, ModelMeshData* &modelMeshData);
	bool ParseIndexData(FILE* file, SkinnedMeshModelData* model, GeometryType geometryType, FbxNode* node, ModelMeshData* &modelMeshData);
	bool ParseNormal(FILE* file, SkinnedMeshModelData* model, GeometryType geometryType, FbxNode* node, ModelMeshData* &modelMeshData);
	bool ParseNormalData(FILE* file, SkinnedMeshModelData* model, GeometryType geometryType, 
		MappingInformationType mappingInformationType, ReferenceInformationType referenceInformationType, FbxNode* node, ModelMeshData* &modelMeshData, SimpleArray<float>& normals);
	bool ParseNormalIndexData(FILE* file, SkinnedMeshModelData* model, GeometryType geometryType, FbxNode* node, ModelMeshData* &modelMeshData, SimpleArray<float>& normals);
	bool ParseUV(FILE* file, SkinnedMeshModelData* model, GeometryType geometryType, FbxNode* node, ModelMeshData* &modelMeshData);
	bool ParseTexture(FILE* file, SkinnedMeshModelData* model, FbxNode* node);
	bool ParseObjectConnections(FILE* file, SkinnedMeshModelData* model);
	bool ParseBindPose(FILE* file, SkinnedMeshModelData* model, FbxNode* node);
	bool ParseAnimationStackCurve(FILE* file, SkinnedMeshModelData* model);
	bool ParseAnimationCurve(FILE* file, FbxNode* node);
	bool CreateFbxNode(char* buffer, FbxNode* fbxNode);
	void SkipNode(FILE* file);

	void HandleDeformer(FbxNode* node, MeshData* data, SkinnedMeshModelData* model, int boneCnt, int prevIdx);
	void HandleMeshNode(FbxNode* node, SkinnedMeshModelData* model);

	void BuildLimbHierarchy(FbxNode* armatureNode, MeshData* modelData, int curIdx, int prevIdx);

	FbxNode* GetGeometryNodeByLimbNode(FbxNode* limbNode);
	FbxNode* GetModelArmatureNodeByDeformer(FbxNode* deformerNode);
	FbxNode* GetModelArmatureNodeByModel(FbxNode* modelNode);

	char* GetTextureName(char* modelPath, char* relativeTexturePath);

	void CalculateTangentAndBitangent(
		const XMFLOAT3& p0, const XMFLOAT3& p1, const XMFLOAT3& p2,
		const XMFLOAT2& uv0, const XMFLOAT2& uv1, const XMFLOAT2& uv2,
		const XMFLOAT3& n0, const XMFLOAT3& n1, const XMFLOAT3& n2,
		XMFLOAT3& tangent0, XMFLOAT3& tangent1, XMFLOAT3& tangent2, 
		XMFLOAT3& bitangent0, XMFLOAT3& bitangent1, XMFLOAT3& bitangent2);

	Renderer& renderer = Renderer::get_instance();
};
