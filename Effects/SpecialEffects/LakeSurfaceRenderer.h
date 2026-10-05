#pragma once
//=============================================================================
//
// [LakeSurfaceRenderer.h]
// Author : 
// 
//=============================================================================
#include "Effects/IEffectRenderer.h"
#include "Core/Camera.h"
#include "Scene/Ground.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define WAVE_STRENGTH_NORMAL			0.34f
#define WAVE_STRENGTH_FREEZE			0.0f
#define FRESNEL_BIAS_NORMAL				0.02f
#define FRESNEL_BIAS_FREEZE				0.1f
#define FRESNEL_POWER_NORMAL			1.2f
#define FRESNEL_POWER_FREEZE			1.7f
#define WATER_COLOR_NORMAL				XMFLOAT3(0.0f, 0.5f, 0.8f)

//*********************************************************
// 構造体
//*********************************************************
struct CBWaterParams
{
	XMMATRIX worldMtx;
	XMFLOAT3 cameraPos;
	float time;

	float waveStrength;
	float fresnelBias;
	float fresnelPower;
	float padding1;

	XMFLOAT3 waterColor;
	float padding2;
};

struct LakeSurfaceParams
{
	float unitsPerGrid = 10.0f;
	float waveStrength = 0.34f;
	float fresnelBias = 0.02f;
	float fresnelPower = 1.2f;
	XMFLOAT3 waterColor = { 0.0f, 0.5f, 0.8f };
	XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
};

class LakeSurfaceRenderer : public IEffectRenderer, public IDebugUI
{
public:
	LakeSurfaceRenderer(const XMFLOAT3& scale = { 1.0f, 1.0f, 1.0f }) { m_lakeSurfaceParams.scale = scale; }
	~LakeSurfaceRenderer() { Shutdown(); };

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
	void Shutdown(void) override;
	bool IsShutdown(void) const override { return m_isShutdown; }
	void Update(void) override;
	void Draw(void) override;
	void SetupPipeline(void) override;

	virtual void RenderImGui(void) override;
	virtual const char* GetPanelName(void) const override { return "Lake Surface Renderer"; }


	const XMFLOAT3& GetPosition(void) const override { return m_lakeSurfaceParams.position; }
	void SetPosition(const XMFLOAT3& pos) override { m_lakeSurfaceParams.position = pos; UpdateWorldMatrix(); }
	void SetScale(const XMFLOAT3& scale) { m_lakeSurfaceParams.scale = scale; UpdateWorldMatrix(); }
	EffectType GetEffectType(void) const override { return EffectType::LakeSurface; }


private:
	bool CreatePlaneMesh(void);
	bool CreateDiffuseTex(void);
	bool CreateWaterParamsCB(void);
	bool LoadShaders(void);
	void UpdateWaterParams(void);
	void UpdateWaterParamsBuffer(void);
	void UpdateWorldMatrix(void);

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;

	ShaderSet m_shaderSet;

	UINT m_indexCount = 0;
	ID3D11Buffer* m_vertexBuffer = nullptr;
	ID3D11Buffer* m_indexBuffer = nullptr;
	ID3D11Buffer* m_cbWater = nullptr;


	ID3D11ShaderResourceView* m_reflectionMap = nullptr;
	ID3D11ShaderResourceView* m_normalMap1 = nullptr;
	ID3D11ShaderResourceView* m_normalMap2 = nullptr;

	BLEND_MODE m_blendMode = BLEND_MODE_NONE;
	XMMATRIX m_worldMatrix{};
	LakeSurfaceParams m_lakeSurfaceParams;
	bool m_isShutdown = false;

	// メッシュ生成時に使用する最大頂点数（拡張対応用）
	UINT m_maxResolution = 2560; // 仮定：1軸最大256分割

	SimpleArray<XMFLOAT3> m_vertices; // 頂点データ
	SimpleArray<UINT> m_indices; // インデックスデータ

	ShaderResourceBinder& m_shaderResourceBinder = ShaderResourceBinder::get_instance();
	Renderer& m_renderer = Renderer::get_instance();
	Camera& m_camera = Camera::get_instance();
	Timer& m_timer = Timer::get_instance();
	Ground& m_ground = Ground::get_instance();
};