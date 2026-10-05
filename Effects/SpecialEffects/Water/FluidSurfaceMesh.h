#pragma once
//=============================================================================
//
// [FluidSurfaceMesh.h]
// Author : 
//
//=============================================================================
#include "Core/Graphics/VertexStructs.h"
#include "Effects/SpecialEffects/Water/FluidHeightFieldBuilder.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SHALLOW_COLOR_DEFAULT       XMFLOAT3(0.5f, 0.8f, 1.0f)	// 浅い水の色のデフォルト値
#define DEEP_COLOR_DEFAULT          XMFLOAT3(0.0f, 0.1f, 0.4f)	// 深い水の色のデフォルト値
#define FRESHENEL_POWER_DEFAULT     5.0f						// フレネル効果のデフォルト強度
#define ALPHA_SCALE_DEFAULT         0.5f						// アルファスケールのデフォルト値
#define THICKNESS_SCALE_DEFAULT     3.0f						// 流体の厚みスケールのデフォルト値
#define THICKNESS_BIAS_DEFAULT      -0.2f						// 流体の厚みバイアスのデフォルト値
#define INITIAL_VERTEX_CAPACITY		4096								// 初期頂点バッファの容量
#define INITIAL_INDEX_CAPACITY		INITIAL_VERTEX_CAPACITY * 6			// 初期インデックスバッファの容量

//*********************************************************
// 構造体
//*********************************************************
struct CBFluidSurface
{
	XMFLOAT3	shallowColor;	// 浅い水の色
    float		fresnelPower;	// フレネル効果の強さ
	XMFLOAT3	deepColor;		// 深い水の色
    float		alphaScale;		// アルファスケール
	float		thicknessScale;	// 流体の厚みスケール
	float		thicknessBias;	// 流体の厚みバイアス
	float		thicknessNormalizeFactor; // 厚度の正規化係数（テクスチャの最大値で割る）
	float		padding;     // パディング（16byteアライン）
	XMFLOAT2	screenSize;		// スクリーンサイズ（幅, 高さ）
	XMFLOAT2	invScreenSize;	// スクリーンサイズの逆数（幅, 高さ）
};

class FluidSurfaceMesh
{
public:
    FluidSurfaceMesh();
	~FluidSurfaceMesh() { ReleaseBuffers(); SafeRelease(&m_cbFluidSurface); }

	void Initialize(void);

    // 高さフィールドからメッシュを生成
	void GenerateSurfaceMesh(const FluidHeightFieldBuilder& heightField);
    void Draw(void);

	// 頂点バッファとインデックスバッファを取得
    ID3D11Buffer* GetVertexBuffer(void) const { return m_vertexBuffer; }
    ID3D11Buffer* GetIndexBuffer(void) const { return m_indexBuffer; }
    UINT GetIndexCount(void) const { return static_cast<UINT>(m_indices.getSize()); }

	void SetThicknessSRV(ID3D11ShaderResourceView* srv) { m_thicknessSRV = srv; }
	void SetShallowColor(const XMFLOAT3& color) { m_shallowColor = color; }
	void SetDeepColor(const XMFLOAT3& color) { m_deepColor = color; }
	void SetFresnelPower(float power) { m_fresnelPower = power; }
	void SetAlphaScale(float scale) { m_alphaScale = scale; }

private:
	bool LoadShaders(void); // シェーダーの読み込み
	void CreateBuffers(void); // 頂点バッファとインデックスバッファの作成
	XMFLOAT3 ComputeNormal(const FluidHeightFieldBuilder& heightField, int gx, int gz); // 法線計算
    void UpdateBuffers(void); // GPUにアップロード
	void EnsureBufferCapacity(UINT requiredVertexCount, UINT requiredIndexCount); // バッファの容量を確保
	bool HasNearbyValidHeight(const FluidHeightFieldBuilder& heightField, int gx, int gz) const;
	void UpdateCBFluidSurface(void); // 定数バッファの更新
	void ReleaseBuffers(void); // バッファを解放

	XMFLOAT3 m_shallowColor = SHALLOW_COLOR_DEFAULT; // 浅い水の色
	XMFLOAT3 m_deepColor = DEEP_COLOR_DEFAULT; // 深い水の色
	float m_fresnelPower = FRESHENEL_POWER_DEFAULT; // フレネル効果の強さ
	float m_alphaScale = ALPHA_SCALE_DEFAULT; // アルファスケール（透明度調整）
	float m_thicknessScale = THICKNESS_SCALE_DEFAULT; // 流体の厚みスケール
	float m_thicknessBias = THICKNESS_BIAS_DEFAULT; // 流体の厚みバイアス

	ShaderSet m_shaderSet; // シェーダーセット

    SimpleArray<FluidSurfaceVertex> m_vertices;
    SimpleArray<UINT> m_indices;
	SimpleArray<float> m_heightFieldBuffers[2]; // 0: 前のフレーム, 1: 現在のフレーム
	int m_activeBufferIndex = 0; // 現在のアクティブな高さフィールドバッファインデックス
	SimpleArray<UINT> m_indexMap; // インデックスマップ（頂点インデックスのマッピング）
	int m_prevGridX = 0; // 前フレームのグリッドXサイズ
	int m_prevGridZ = 0; // 前フレームのグリッドZサイズ
    UINT m_vertexCapacity = INITIAL_VERTEX_CAPACITY;
    UINT m_indexCapacity = INITIAL_INDEX_CAPACITY;

	ID3D11ShaderResourceView* m_thicknessSRV = nullptr; // 流体の厚みを表すテクスチャのSRV
    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11Buffer* m_indexBuffer = nullptr;
	ID3D11Buffer* m_cbFluidSurface = nullptr;
	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;

    ShaderResourceBinder& m_shaderResourceBinder = ShaderResourceBinder::get_instance();
};
