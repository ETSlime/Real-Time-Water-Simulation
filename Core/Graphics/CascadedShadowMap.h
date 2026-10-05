#pragma once
//=============================================================================
//
// 複数カスケードによるシャドウマップ生成と分割管理 [CascadedShadowMap.h]
// Author : 
// カメラの視錐台を複数カスケードに分割し、各カスケードごとに
// ライトビュー投影・影生成・テクセルスナップ補正を行う
// 
//=============================================================================
#include "Core/Graphics/Renderer.h"
#include "Collision/AABBUtils.h"
#include "Scene/GameObject.h"
#include "Core/Camera.h"
#include "Utility/Debug/DebugBoundingBoxRenderer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
// シャドウマップ解像度（1カスケードごとのサイズ）
#define CSM_SHADOW_MAP_SIZE 2048
// カスケードの数（最大4）
#define MAX_CASCADES    4
#define SHADOW_CASTING_LIGHT_MAX 2
#define MIN_SHADOW_DISTANCE 15.0f
#define MAX_SHADOW_DISTANCE 40000.0f

// デバッグ用のカスケード境界可視化
#define DEBUG_CASCADE_BOUND     0

enum class ShadowRadiusStrategy
{
    FitBoundingSphere,
    FitBoundingBox,
    PaddingBuffer
};

//*********************************************************
// 構造体
//*********************************************************
struct CascadedShadowConfig
{
    int numCascades = MAX_CASCADES;
    int shadowMapSize = CSM_SHADOW_MAP_SIZE;
    float manualSplits[MAX_CASCADES] = { 0.05f, 0.15f, 0.3f, 1.0f };

    // 分割のタイプを制御する係数（0.0 = 線形、1.0 = 対数、0.95ならほぼ対数）
    float lambda = 0.95f;

    // スプリットの深度を手動で指定するかどうか
    bool useManualSplits = false;
    // テクセルスナップを有効化するかどうか
    bool enableTexelSnap = true;
    // スナップの精度を指定する
    ShadowRadiusStrategy radiusStrategy = ShadowRadiusStrategy::FitBoundingBox;
    // カスケードの間隔を調整するためのパディング
    float padding[MAX_CASCADES] = { 60.0f, 100.0f, 150.0f, 200.0f }; // レイヤーごとのpadding
    float maxShadowDistance = MAX_SHADOW_DISTANCE;
    float minShadowDistance = MIN_SHADOW_DISTANCE;
};

struct CBCascadeData
{
    XMMATRIX lightViewProj;
    XMMATRIX lightView;
    XMMATRIX lightProj;
    float splitDepth;
    float padding[3]; // 16バイトアライメント
};

struct CascadePrivateData
{
    XMMATRIX lightViewProjLocal;
    XMVECTOR viewFrustumCenter;
};

struct CBCascadeDataArray
{
    CBCascadeData cascadeArray[MAX_CASCADES];    // 最大4レイヤーのシャドウ
    XMFLOAT4 cascadeSplits;                    // ピクセルシェーダーでの深度比較用
};

class CascadedShadowMap 
{
public:
    CascadedShadowMap();
    ~CascadedShadowMap();

    void Initialize(int shadowMapSize = CSM_SHADOW_MAP_SIZE, int numCascades = MAX_CASCADES);
    void Shutdown(void);

    void UpdateCascades(const XMVECTOR& lightDir, float nearZ, float farZ);

    void UpdateCascadeCBufferArray(void);
    void UpdateCascadeCBuffer(int cascadeIndex);

    void UnbindShadowSRVs(void);
    void BeginCascadeRender(int lightIndex, int cascadeIndex);
    void EndCascadeRender(void);
    void SetViewport(void);

    void BindShadowSRVsToPixelShader(int lightIndex, int startSlot);
    ID3D11ShaderResourceView* GetSRV(int lightIndex, int cascadeIndex) const;

    // モデルの影を収集する
    void CollectFromScene_NoCulling(int cascadeIndex, const SimpleArray<IGameObject*>& objects);
    void CollectFromScene(int cascadeIndex, const SimpleArray<IGameObject*>& objects);

	// カスケードのコレクタを取得
    ShadowMeshCollector& GetCascadeCollector(int i) { return m_perCascadeCollector[i]; }
    int GetCascadeObjectCount(int i) const { return m_cascadeObjectCount[i]; }

    // 特定光源・カスケードのSRVを取得
    ID3D11ShaderResourceView* GetShadowSRV(int lightIndex, int cascadeIndex) const;
    // 特定光源・カスケードのDSVを取得
    ID3D11DepthStencilView* GetDSV(int lightIndex, int cascadeIndex) const;
    // ある光源の全カスケード分のSRV配列を取得
    ID3D11ShaderResourceView** GetAllShadowSRVs(int lightIndex);

    const CBCascadeData& GetCascadeData(int index) const { return m_cascadeData[index]; }
    const CBCascadeData& GetCurrentCascadeData(void) { return m_cascadeData[m_currentCascadeIndex]; }
    int GetCascadeCount() const { return m_config.numCascades; }

    void RenderImGui(void); // ImGuiでの設定表示
	void VisualizeCascadeBounds(void); // デバッグ用のカスケード境界可視化

private:
	// カスケードの分割を計算する関数
    void ComputeCascadeMatrices(int cascadeIndex, float splitNear, float splitFar, const XMVECTOR& lightDir);
    // カスケード用のCBufferを初期化
	void InitCascadeCBuffer(void); 

    // シャドウマップのテクセルグリッドにスナップする補正関数
    XMMATRIX SnapShadowMatrixToTexelGrid(const XMMATRIX& lightViewProjLocal);
    XMMATRIX SnapShadowMatrixToTexelGrid(const XMMATRIX& lightView, const XMMATRIX& lightProj, XMVECTOR center);

    // カスケードシャドウマップの半径を計算するユーティリティ関数
    float ComputeCascadeRadius(XMVECTOR* corners, XMVECTOR center, ShadowRadiusStrategy strategy, float padding);
    // 可視インスタンスをカリングする
    void CullVisibleInstancesForShadow(InstanceModelAttribute& attribute, int cascadeIndex);
	// AABBがライトの視野に入っているかどうかを確認する関数
    bool IsAABBInsideLightFrustum(const BOUNDING_BOX& worldAABB, int cascadeIndex);

    // CascadedShadowMapと相同のmaxDistanceを持つカメラの視野を再構築する関数
    XMMATRIX ComputeCSMViewProjMatrixWithMaxDistance(float maxDistance);

    // ライトビュープロジェクション行列を加算する関数
    XMMATRIX ComputeLightProjMatrixFromSliceCorners(const XMVECTOR sliceCorners[8], const XMMATRIX& lightView, float padding);

    void AdjustCascadePadding(void);

    int m_currentCascadeIndex = 0;

    CBCascadeData m_cascadeData[MAX_CASCADES]{}; // カスケードデータ
    CascadePrivateData m_cascadePrivateData[MAX_CASCADES]{}; // カスケードプライベートデータ

    ShadowMeshCollector m_perCascadeCollector[MAX_CASCADES]{}; // 各レイヤーのコレクタ
    int m_cascadeObjectCount[MAX_CASCADES]{}; // 各レイヤーのカウント

    float m_lastCascadeRadius = 0;
    XMVECTOR m_lastLightDir{};

	CascadedShadowConfig m_config{}; // シャドウマップの設定

	ID3D11Texture2D* m_shadowMaps[SHADOW_CASTING_LIGHT_MAX][MAX_CASCADES] = {}; // シャドウマップ  
	ID3D11DepthStencilView* m_dsvs[SHADOW_CASTING_LIGHT_MAX][MAX_CASCADES] = {}; // 深度ステンシルビュー
	ID3D11ShaderResourceView* m_srvs[SHADOW_CASTING_LIGHT_MAX][MAX_CASCADES] = {}; // シャドウSRV

	D3D11_VIEWPORT m_shadowViewport = {}; // ビューポート

	ID3D11Buffer* m_cascadeSingleCBuffer = nullptr; // 単一カスケード用のCBuffer
	ID3D11Buffer* m_cascadeArrayCBuffer = nullptr; // カスケード配列用のCBuffer

    // 保存されたRTV&DSV
	ID3D11RenderTargetView* m_savedRTV = nullptr; 
    ID3D11DepthStencilView* m_savedDSV = nullptr;

    ID3D11Texture2D* m_dummyRTTexture = nullptr;
    ID3D11RenderTargetView* m_dummyRTV = nullptr;

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    Camera& m_camera = Camera::get_instance();
    ShaderResourceBinder& m_ShaderResourceBinder = ShaderResourceBinder::get_instance();
    Renderer& m_renderer = Renderer::get_instance();

    // debug
    XMVECTOR m_tempFrustumCenter[MAX_CASCADES];
    XMMATRIX m_tempInvViewProj{};
    XMVECTOR m_tempSliceCorners[MAX_CASCADES][8];
    DebugBoundingBoxRenderer m_debugBoundingBoxRenderer[MAX_CASCADES];
    bool m_showCascadeBox[MAX_CASCADES] = { true, true, true, true };
};
