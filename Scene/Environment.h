#pragma once
//=============================================================================
//
// 自然環境をふわふわ描くクラスちゃん [Environment.h]
// Author : 
// 草や木、花などのインスタンスをランダム配置して、風にゆれる癒しの景色を描画します
// 八分木・風向き・ノイズ生成・シャドウ対応までしっかりしてる超がんばり屋さんっ
// 
//=============================================================================
#include "Core/Graphics/Renderer.h"
#include "Scene/GameObject.h"
#include "Collision/OctreeNode.h"
#include "Utility/Debug/DebugBoundingBoxRenderer.h"
#include "Core/Camera.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define GRASS_1_MODEL_PATH          "data/MODEL/Environment/Grass.obj"
#define GRASS_1_TEX_PATH            "data/MODEL/Environment/Grass.png"
#define GRASS_2_MODEL_PATH          "data/MODEL/Environment/Grass_2.obj"
#define GRASS_2_TEX_PATH            "data/MODEL/Environment/Grass_2.png"
#define CLOVER_1_MODEL_PATH         "data/MODEL/Environment/Clover_1.obj"
#define CLOVER_1_TEX_PATH           "data/MODEL/Environment/Clover_1.png"
#define BUSH_1_MODEL_PATH            "data/MODEL/Environment/Bush/Bush_1.obj"
#define BUSH_2_MODEL_PATH            "data/MODEL/Environment/Bush/Bush_2.obj"
#define FLOWER_1_MODEL_PATH         "data/MODEL/Environment/Flower_1.obj"
#define FLOWER_1_TEX_PATH           "data/MODEL/Environment/Flower_1.png"
#define SHRUBBERY_1_MODEL_PATH      "data/MODEL/Environment/Shrubbery_1.obj"
#define SHRUBBERY_1_TEX_PATH        "data/MODEL/Environment/Shrubbery_1.png"
#define SHRUBBERY_1_MODEL_PATH      "data/MODEL/Environment/Shrubbery_1.obj"
#define SHRUBBERY_1_TEX_PATH        "data/MODEL/Environment/Shrubbery_1.png"
#define TREE_1_MODEL_PATH           "data/MODEL/Environment/Tree.obj"
#define TREE_1_TEX_PATH             "data/MODEL/Environment/branch-01.png"

#define GRASS_MAX_SLOPE_ANGLE           (XM_PI * 0.33f)
#define CLOVER_MAX_SLOPE_ANGLE          (XM_PI * 0.33f)
#define BUSH_MAX_SLOPE_ANGLE            (XM_PI * 0.33f)
#define FLOWER_MAX_SLOPE_ANGLE          (XM_PI * 0.33f)
#define SHRUBBERY_MAX_SLOPE_ANGLE       (XM_PI * 0.33f)
#define TREE_MAX_SLOPE_ANGLE            (XM_PI * 0.33f)

enum class EnvironmentObjectType 
{
    Grass_1,
    Grass_2,
    Bush_1,
    Bush_2,
    Flower_1,
    Shrubbery_1,
    Clover_1,
    Tree_1,
    Rock,
    FallenTree,
    Other
};

enum RotationAxis
{
    AXIS_X,
    AXIS_Y,
    AXIS_Z
};


//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct InstanceVertex
{
    XMFLOAT3 Position;   // 頂点位置
    XMFLOAT3 Normal;     // 法線
    XMFLOAT2 TexCoord;   // テクスチャ座標
    float Weight;        // 風の影響度 (重み)
    XMFLOAT3 Tangent;    // 切線（法線マップ用）
};

struct alignas(16) PerFrameBuffer 
{
    float Time;                     // 経過時間 (4 bytes)
    XMFLOAT3 WindDirection;         // 風向き (12 bytes)
    float WindStrength;             // 風の強さ (4 bytes)
    float padding1;                 // パディング (4 bytes) - 16バイト整列用
    XMFLOAT2 NoiseTextureResolution; // ノイズテクスチャの解像度 (8 bytes)
    XMFLOAT2 padding2;              // パディング (8 bytes) - 16バイト整列用
};

struct EnvironmentObjAttributes
{
    bool use;
    bool load;
    bool affectedByWind;
    bool castShadow;
    bool collision;
    bool adaptedToTerrain;
    bool billboard;
	bool isDynamic;
    float maxSlopeAngle;
};

struct EnvironmentObject
{
    EnvironmentObjectType type;
    EnvironmentObjAttributes attributes;
	GameObject<ModelInstance>* modelGO = nullptr;
	Transform transform;
	BOUNDING_BOX boundingBoxLocal;

    char* modelPath = nullptr;
    char* extDiffuseTexPath = nullptr;
    UINT indexCount;
    UINT instanceCount;
    bool useExtDiffuseTex = false;

    ID3D11ShaderResourceView* externDiffuseTextureSRV = nullptr;
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    ID3D11Buffer* instanceBuffer = nullptr;
	ID3D11Buffer* instanceBufferShadow = nullptr;
    InstanceData* instanceData = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;

    ~EnvironmentObject()
	{
		SAFE_DELETE_ARRAY(modelPath);
		SAFE_DELETE_ARRAY(extDiffuseTexPath);
        SAFE_DELETE(modelGO);
        SAFE_DELETE(instanceData);

        SafeRelease(&vertexBuffer);
        SafeRelease(&indexBuffer);
        SafeRelease(&instanceBuffer);
		SafeRelease(&instanceBufferShadow);
        SafeRelease(&externDiffuseTextureSRV);
	}
};

struct EnvironmentConfig
{
    bool loadGrass;
    bool loadBush;
    bool loadFlower;
    bool loadTree;
    bool loadRock;

};

struct InstanceTransformInfo
{
    float posX;
    float posZ;
    float rotY;
    float scl;
    float offsetY;

    InstanceTransformInfo()
    {
        posX = 0.0f;
        posZ = 0.0f;
        rotY = 0.0f;
        scl = 1.0f;
        offsetY = 0.0f;
    }

    InstanceTransformInfo(float posX, float posZ, float rotY, float scl, float offsetY = 0.0f)
    {
        this->posX = posX;
        this->posZ = posZ;
        this->scl = scl;
        this->rotY = rotY;
        this->offsetY = offsetY;
    }
};

struct InstanceParams
{
    EnvironmentObjectType type;

    SimpleArray<InstanceTransformInfo> transformArray;

    bool randomScl = false;
    bool randomRotY = false;
};

class Environment : public IDebugUI
{
public:
    Environment();
    Environment(EnvironmentConfig config);
    ~Environment();

    void Update(void);
    void Draw(void);

    // フィールドモデルのインスタンスを生成
    bool GenerateRandomInstances(EnvironmentObjectType type, const Model* fieldModel, int clusterCount = 45);
    bool GenerateInstanceByParams(const InstanceParams& params, const Model* fieldModel, BOUNDING_BOX fieldBBox);

	void SetDrawBoundingBox(bool draw) { m_drawBoundingBox = draw; }

    static const char* GetModelPathByType(EnvironmentObjectType type);

private:

    bool Initialize(void);
    void RenderEnvironmentObj(EnvironmentObject* obj);
    EnvironmentObject* InitializeEnvironmentObj(EnvironmentObjectType type);
    EnvironmentObject* InitializeEnvironmentObjAttributes(EnvironmentObjectType type);
    bool GetInitRotation(EnvironmentObject* obj, XMFLOAT4& rotation, const Triangle* triangle);
    float GetRandomScale(EnvironmentObjectType type);
    bool CreateInstanceBuffer(EnvironmentObject* obj, const SimpleArray<InstanceData>& instanceDataArray);
    // 指定した軸を中心に角度（ラジアン）だけ回転を追加する
    XMVECTOR AddRotationToQuaternion(const XMVECTOR& baseRotation, RotationAxis axis, float angle);

    void UpdateBoundingBox(EnvironmentObject* obj);

    // 動的風向き計算
    XMFLOAT3 CalculateDynamicWindDirection(float time);

    bool GenerateNoiseTexture(ID3D11ShaderResourceView** noiseTextureSRV);
    float CalculateWeight(float vertexY, float minY, float maxY);
    float RayIntersectTriangle(const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDir, const Triangle* tri);
    bool LoadEnvironmentObj(EnvironmentObject* obj);
    void LoadShadersForEachObj(EnvironmentObject* obj);
	bool LoadAllShaders(void);

    OctreeNode* GenerateOctree(const SimpleArray<Triangle*>* triangles, BOUNDING_BOX boundingBox);

    virtual void RenderDebugInfo(void) override;

    SimpleArray<EnvironmentObject*> m_environmentObjects;

    ShaderSet m_grassShaderSet;
    ShaderSet m_treeShaderSet;

    ID3D11Buffer* m_perFrameBuffer;
    ID3D11ShaderResourceView* noiseTextureSRV;

    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;

	bool m_drawBoundingBox = true;

    DebugBoundingBoxRenderer m_debugBoundingBoxRenderer;
    Camera& m_camera = Camera::get_instance();
    ShaderResourceBinder& m_shaderResourceBinder = ShaderResourceBinder::get_instance();
	Timer& m_timer = Timer::get_instance();
	Renderer& m_renderer = Renderer::get_instance();
};


struct GenerateInstanceContext
{
    Environment* environment;
    InstanceParams instanceParams;
    IGameObject* fieldGO;
    const char* modelPath;
};