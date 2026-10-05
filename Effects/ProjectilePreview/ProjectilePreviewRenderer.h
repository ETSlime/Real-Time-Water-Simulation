#pragma once
//=============================================================================
//
// [ProjectilePreviewRenderer.h]
// Author : 
//
//=============================================================================
#include "Core/Camera.h"
#include "Effects/ProjectilePreview/ProjectilePredictor.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_SPHERE_SLICES 16 // 半球メッシュの最大スライス数
#define MAX_SPHERE_STACKS 8 // 半球メッシュの最大スタック数

//*********************************************************
// 構造体
//*********************************************************
struct CBPreviewData
{
    XMFLOAT3 hitPos; // 半球ワールド位置
    float scale;  // 半球スケール係数

    XMFLOAT4 color;  // 軌跡・半球共通の色

    float time;   // 呼吸アニメ用時間
	float renderType; // 描画タイプ (0: 軌跡, 1: 半球)
    XMFLOAT2 padding; // アラインメント用（未使用）
};

class ProjectilePreviewRenderer 
{
public:
    ProjectilePreviewRenderer();
    ~ProjectilePreviewRenderer();

    void Initialize(void);
    void ShutDown(void);

    void Update(const SimpleArray<TrajectoryPoint>& trajectoryPoints, const XMFLOAT3& hitPos, const XMFLOAT3& hitNormal);

    void Draw(void);

private:
    // 毎フレーム呼ぶ：ヒット法線に揃えた上半球メッシュを生成してVB/IBに書き込む
    void UpdateSphereMesh(const XMFLOAT3& hitNormal, float radius, UINT sliceCount = MAX_SPHERE_SLICES, UINT stackCount = MAX_SPHERE_STACKS);
    // 初期化時に呼ぶ：動的に頂点/インデックスを書き込むためのバッファだけ確保する
    void CreateSphereBuffers(UINT maxSliceCount, UINT maxStackCount);
    // シェーダー読み込み
	bool LoadShaders(void);

    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;
    ID3D11Buffer* m_cbPreview = nullptr;

    // 軌跡線用の頂点バッファ
    ID3D11Buffer* m_trajectoryVB = nullptr;
	ID3D11Buffer* m_trajectoryIB = nullptr;
    UINT m_maxTrajectoryPoints = MAX_TRAJECTORY_POINTS;
    UINT m_currentTrajectoryPoints = 0;
	UINT m_trajectoryIndexCount = 0;

	SimpleArray<ProjectilePreviewVertex> m_trajectoryVertices; // 軌跡頂点データ
	SimpleArray<UINT> m_trajectoryIndices; // 軌跡インデックスデータ

    // 半球メッシュ用バッファ
	ID3D11Buffer* m_sphereVB = nullptr;
	ID3D11Buffer* m_sphereIB = nullptr;
	UINT m_sphereMaxSlice = MAX_SPHERE_SLICES; // 半球メッシュの最大スライス数
	UINT m_sphereMaxStack = MAX_SPHERE_STACKS; // 半球メッシュの最大スタック数
    UINT m_sphereIndexCount = 0;

    ShaderSet m_shaderSet;

    // 描画状態
    XMFLOAT3 m_hitPos = XMFLOAT3(0, 0, 0);
    bool m_hasHit = false;

	ShaderResourceBinder& m_shaderBinder = ShaderResourceBinder::get_instance(); // シェーダーリソースバインダー
	Camera& m_camera = Camera::get_instance(); // カメラインスタンス
	Renderer& m_renderer = Renderer::get_instance(); // レンダラーインスタンス
    Timer& m_timer = Timer::get_instance();
};
