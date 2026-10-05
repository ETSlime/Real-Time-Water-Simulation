#pragma once
//=============================================================================
//
// [MarchingCubesSurfaceMesh.h]
// Author : 
// 
// SPH流体シミュレーションから生成されたスカラーフィールドをもとに、
// Marching Cubesアルゴリズムを用いてメッシュを構築・描画するクラス。
// 
// - CPU / GPU 両対応のメッシュ生成方式に対応（m_buildOnGPU）
// - スカラーフィールドは三次元配列またはVoxelCoord->floatのマップ形式を入力可能
// - 描画にはDirectX 11のバッファとパイプラインを使用
// - フレネル効果・アルファ調整・厚みスケールなど視覚パラメータを定数バッファで制御
// - 内部でOptimizedMarchingCubesMesherを使用し、法線やインデックスを生成
//
//=============================================================================
#include "Core/Graphics/Renderer.h"
#include "Effects/MarchingCubes/OptimizedMarchingCubesMesher.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define ISO_LEVEL_DEFAULT           0.5f						// 等値面のしきい値のデフォルト値
#define SHALLOW_COLOR_DEFAULT       XMFLOAT3(0.5f, 0.8f, 1.0f)	// 浅い水の色のデフォルト値
#define DEEP_COLOR_DEFAULT          XMFLOAT3(0.0f, 0.1f, 0.4f)	// 深い水の色のデフォルト値
#define FRESHENEL_POWER_DEFAULT     5.0f						// フレネル効果のデフォルト強度
#define ALPHA_SCALE_DEFAULT         0.5f						// アルファスケールのデフォルト値
#define THICKNESS_SCALE_DEFAULT     3.0f						// 流体の厚みスケールのデフォルト値
#define THICKNESS_BIAS_DEFAULT      -0.2f						// 流体の厚みバイアスのデフォルト値
#define INITIAL_VERTEX_CAPACITY		200000								// 初期頂点バッファの容量
#define INITIAL_INDEX_CAPACITY		INITIAL_VERTEX_CAPACITY * 6			// 初期インデックスバッファの容量

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct CBMarchingCubes
{
    XMFLOAT3	shallowColor;	// 浅い水の色
    float		fresnelPower;	// フレネル効果の強さ
    XMFLOAT3	deepColor;		// 深い水の色
    float		alphaScale;		// アルファスケール
    float		thicknessScale;	// 流体の厚みスケール
    float		thicknessBias;	// 流体の厚みバイアス
    float		thicknessNormalizeFactor; // 厚度の正規化係数（テクスチャの最大値で割る）
    float       isoLevel;                // 等値面のしきい値
    XMFLOAT2	screenSize;		// スクリーンサイズ（幅, 高さ）
    XMFLOAT2	invScreenSize;	// スクリーンサイズの逆数（幅, 高さ）
	UINT        numActiveVoxels; // アクティブなボクセルの数
	XMFLOAT3    padding;		// 16バイトアライメントのためのパディング
};

class MarchingCubesSurfaceMesh
{
public:
	MarchingCubesSurfaceMesh();
    ~MarchingCubesSurfaceMesh();

    void Initialize(UINT maxParticles);
    void Draw(void);

	// Marching Cubes メッシュを生成
    void UpdateMesh(const SimpleArray<SimpleArray<SimpleArray<float>>>& volume, const VolumeReconstructionParams& params);
	void UpdateMesh(const HashMap<VoxelCoord, float, HashVoxelKey64, EqualVoxelKey64>& scalarFieldMap, const VolumeReconstructionParams& params);

    void SetThicknessSRV(ID3D11ShaderResourceView* srv);


private:
    bool LoadShaders(void); // シェーダーの読み込み
    void CreateBuffers(void); // 頂点バッファとインデックスバッファの作成
    void UpdateBuffers(void); // GPUにアップロード
    void ReleaseBuffers(void); // バッファを解放
    void EnsureBufferCapacity(UINT requiredVertexCount, UINT requiredIndexCount); // バッファの容量を確保

    OptimizedMarchingCubesMesher m_mesher; // Marching Cubes メッシャー
    SimpleArray<MarchingCubesVertex> m_vertices;
    SimpleArray<UINT> m_indices;

    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11Buffer* m_indexBuffer = nullptr;

protected:
	void SetupPipeline(void); // パイプラインのセットアップ
    void UpdateCBMarchingCubes(void); // 定数バッファの更新

    ShaderSet m_shaderSet;
    UINT m_vertexCapacity = INITIAL_VERTEX_CAPACITY;
    UINT m_indexCapacity = INITIAL_INDEX_CAPACITY;
    XMUINT3 m_edgeCacheDim = { 0, 0, 0 }; // エッジキャッシュの次元
    ID3D11Buffer* m_cbMarchingCubes = nullptr;
    UINT m_maxParticles; // 最大粒子数（グローバルメッシュ容量の計算に使用）

	bool m_buildOnGPU = false; // GPU上でメッシュを生成するかどうか
    //float m_isoLevel = ISO_LEVEL_DEFAULT; // 等値面のしきい値
    XMFLOAT3 m_shallowColor = SHALLOW_COLOR_DEFAULT; // 浅い水の色
    XMFLOAT3 m_deepColor = DEEP_COLOR_DEFAULT; // 深い水の色
    float m_fresnelPower = FRESHENEL_POWER_DEFAULT; // フレネル効果の強さ
    float m_alphaScale = ALPHA_SCALE_DEFAULT; // アルファスケール（透明度調整）
    float m_thicknessScale = THICKNESS_SCALE_DEFAULT; // 流体の厚みスケール
    float m_thicknessBias = THICKNESS_BIAS_DEFAULT; // 流体の厚みバイアス

    VolumeReconstructionParams m_lastParams;

    ID3D11ShaderResourceView* m_thicknessSRV = nullptr; // 流体の厚みを表すテクスチャのSRV
    ID3D11Device* m_device = Renderer::get_instance().GetDevice();
    ID3D11DeviceContext* m_context = Renderer::get_instance().GetDeviceContext();

    ShaderResourceBinder& m_shaderResourceBinder = ShaderResourceBinder::get_instance();
};