#pragma once
//=============================================================================
//
// 統合型レンダリングパイプラインの管理中枢 [Renderer.h]
// Author : 
// 各種モデル・スキンメッシュ・インスタンス・UI・VFX の描画と、
// デバイス管理・シェーダ制御・描画状態の切替を一元的に扱う
//
//=============================================================================
#include "main.h"
#include "Utility/SingletonBase.h"
#include "Core/Graphics/VertexStructs.h"
#include "Core/Graphics/ConstantBufferStructs.h"
#include "Core/Shader/ShaderManager.h"
#include "Core/Shader/ShaderResourceBinder.h"
#include "Collision/AABBUtils.h"

//*********************************************************
// マクロ定義
//*********************************************************
#define MAX_BONE_INDICES	(4)
#define SHADOWMAP_SIZE		(SCREEN_WIDTH * 3.5f)
#define DEPTHBIAS_LAYER_0	(-25)
#define DEPTHBIAS_LAYER_1	(-50)
#define DEPTHBIAS_LAYER_2	(-100)
#define DEPTHBIAS_LAYER_3	(-200)

enum BLEND_MODE
{
	BLEND_MODE_NONE,		//ブレンド無し
	BLEND_MODE_ALPHABLEND,	//αブレンド
	BLEND_MODE_ADD,			//加算ブレンド
	BLEND_MODE_SUBTRACT,	//減算ブレンド
	BLEND_MODE_SWORDTRAIL,

	BLEDD_MODE_NUM
};

enum CULL_MODE
{
	CULL_MODE_NONE,			//カリング無し
	CULL_MODE_FRONT,		//表のポリゴンを描画しない(CW)
	CULL_MODE_BACK,			//裏のポリゴンを描画しない(CCW)

	CULL_MODE_NUM
};

enum class DepthMode
{
	Enable,			// 深度テストON + 書き込みON（通常のモデル用）
	Effect,		// 深度テストON + 書き込みOFF（パーティクル用）
	Disable			// 深度テストOFF（UIなどに使用）
};

enum class RenderMode
{
	OBJ,
	SKINNED_MESH,
	INSTANCE,
	UI,
	VFX,
};

enum class RenderLayer
{
	DEFAULT,
	LAYER_0,
	LAYER_1,
	LAYER_2,
	LAYER_3,
};

//*********************************************************
// 構造体
//*********************************************************

// マテリアル構造体
struct MATERIAL
{
	XMFLOAT4	Ambient;
	XMFLOAT4	Diffuse;
	XMFLOAT4	Specular;
	XMFLOAT4	Emission;
	float		Shininess;
	int			noTexSampling;
	int			normalMapSampling;
	int			bumpMapSampling;
	int			opacityMapSampling;
	int			lightMapSampling;
	int			reflectMapSampling;
	int			translucencyMapSampling;
	BOOL		LoadMaterial;

	MATERIAL(void)
	{
		noTexSampling = 1;
		lightMapSampling = 0;
		normalMapSampling = 0;
		bumpMapSampling = 0;
		opacityMapSampling = 0;
		reflectMapSampling = 0;
		translucencyMapSampling = 0;
		LoadMaterial = FALSE;
		Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		Diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		Emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		Shininess = 0.0f;
	}
};

// ライト構造体
struct LightData
{
	XMFLOAT3	Direction;	// ライトの方向
	XMFLOAT3	Position;	// ライトの位置
	XMFLOAT4	Diffuse;	// 拡散光の色
	XMFLOAT4	Ambient;	// 環境光の色
	float		Attenuation;// 減衰率
	int			Type;		// ライト種別・有効フラグ
	int			Enable;		// ライト種別・有効フラグ

	XMFLOAT4X4	LightViewProj;
};

// フォグ構造体
struct FOG 
{
	float		FogStart;	// フォグの開始距離
	float		FogEnd;		// フォグの最大距離
	XMFLOAT4	FogColor;	// フォグの色
};

// 縁取り用バッファ
struct FUCHI
{
	int			fuchi;
	int			fill[3];
};

//*****************************************************************************
// プロトタイプ宣言
//*****************************************************************************
class Renderer : public SingletonBase<Renderer>
{
public:

	Renderer();

	HRESULT Init(HINSTANCE hInstance, HWND hWnd, BOOL bWindow);
	void Shutdown(void);

	void Clear(void);
	void Present(void);

	ID3D11Device* GetDevice(void);
	ID3D11DeviceContext* GetDeviceContext(void);

	void SetDepthEnable(BOOL Enable);
	void SetDepthForParticle(void);
	void SetDepthMode(DepthMode mode);
	void SetBlendState(BLEND_MODE bm);
	void SetCullingMode(CULL_MODE cm);
	void SetAlphaTestEnable(BOOL flag);

	void SetWorldViewProjection2D(void);
	void SetCurrentWorldMatrix(const XMMATRIX* WorldMatrix) const;
	void SetViewMatrix(const XMMATRIX* ViewMatrix) const;
	void SetProjectionMatrix(const XMMATRIX* ProjectionMatrix) const;

	void SetMaterial(MATERIAL material);

	void SetFogEnable(BOOL flag);
	void SetFog(FOG* fog);

	void SetRenderProgress(RenderProgressCBuffer renderProgress);

	void DebugTextOut(char* text, int x, int y);

	void SetFuchi(int flag);
	void SetShaderCamera(XMFLOAT3 pos);
	void SetFillMode(D3D11_FILL_MODE mode);
	void SetBoneMatrix(const XMMATRIX matrices[BONE_MAX]) const;
	void SetClearColor(float* color4);
	void SetRenderLayer(RenderLayer layer);
	void SetRenderObject(void);
	void SetRenderSkinnedMeshModel(void);
	void SetRenderInstance(void);
	void SetRenderVFX(void);
	void SetRenderUI(void);
	void SetStaticModelInputLayout(void);
	void SetUIInputLayout(void);
	void SetSkinnedMeshInputLayout(void);
	void SetVFXInputLayout(void);
	void ResetRenderTarget(void);
	void SetLightModeBuffer(int mode);
	void SetLightBuffer(const LIGHT_CBUFFER& lightBuffer);

	void SetMainPassViewport(void);
	void SetShadersets(void);

	RenderMode GetRenderMode(void);
	void SetRenderMode(RenderMode mode);

	ID3D11DepthStencilState* GetDepthWriteState(void) const { return m_DepthStateEnable; }
	ID3D11DepthStencilState* GetDepthDisabledState(void) const { return m_DepthStateDisable; }

	void BindViewBuffer(ShaderStage stage);


private:

	void SetFogBuffer(void);

	D3D_FEATURE_LEVEL       m_FeatureLevel = D3D_FEATURE_LEVEL_11_0;

	ID3D11Device* m_D3DDevice = NULL;
	ID3D11DeviceContext* m_ImmediateContext = NULL;
	IDXGISwapChain* m_SwapChain = NULL;

	ID3D11RenderTargetView* m_RenderTargetView = NULL;
	ID3D11DepthStencilView* m_SceneDepthStencilView = NULL;

	ID3D11Buffer* m_WorldBuffer = NULL;
	ID3D11Buffer* m_ViewBuffer = NULL;
	ID3D11Buffer* m_ProjectionBuffer = NULL;
	ID3D11Buffer* m_MaterialBuffer = NULL;
	ID3D11Buffer* m_LightBuffer = NULL;
	ID3D11Buffer* m_FogBuffer = NULL;
	ID3D11Buffer* m_FuchiBuffer = NULL;
	ID3D11Buffer* m_CameraPosBuffer = NULL;
	ID3D11Buffer* m_LightProjViewBuffer = NULL;
	ID3D11Buffer* m_BoneMatrixBuffer = NULL;
	ID3D11Buffer* m_LightModeBuffer = NULL;
	ID3D11Buffer* m_RenderProgressBuffer = NULL;


	ID3D11DepthStencilState* m_DepthStateEnable = NULL;
	ID3D11DepthStencilState* m_DepthStateDisable = NULL;
	ID3D11DepthStencilState* m_DepthStateParticle = NULL;

	ID3D11BlendState* m_BlendStateNone = NULL;
	ID3D11BlendState* m_BlendStateAlphaBlend = NULL;
	ID3D11BlendState* m_BlendStateAdd = NULL;
	ID3D11BlendState* m_BlendStateSubtract = NULL;
	ID3D11BlendState* m_BlendStateSwordTrail = NULL;
	BLEND_MODE				m_BlendStateParam;

	ID3D11SamplerState* m_SamplerState = NULL;
	ID3D11SamplerState* m_SamplerStateShadow = NULL;
	ID3D11SamplerState* m_SamplerStateOpacity = NULL;

	ID3D11RasterizerState* m_RasterStateCullOff = NULL;
	ID3D11RasterizerState* m_RasterStateCullCW = NULL;
	ID3D11RasterizerState* m_RasterStateCullCCW = NULL;
	ID3D11RasterizerState* m_RasterizerLayer0 = NULL;
	ID3D11RasterizerState* m_RasterizerLayer1 = NULL;
	ID3D11RasterizerState* m_RasterizerLayer2 = NULL;
	ID3D11RasterizerState* m_RasterizerLayer3 = NULL;

	ShaderManager& m_ShaderManager = ShaderManager::get_instance();
	ShaderResourceBinder& m_ShaderResourceBinder = ShaderResourceBinder::get_instance();

	ShaderSet m_StaticModelShaderSet;
	ShaderSet m_SkinnedModelShaderSet;
	ShaderSet m_InstanceModelShaderSet;
	ShaderSet m_VFXShaderSet;
	ShaderSet m_UIShaderSet;

	MATERIAL_CBUFFER	m_Material;
	LIGHT_CBUFFER	m_Light;
	FOG_CBUFFER		m_Fog;

	FUCHI			m_Fuchi;
	RenderMode		m_RenderMode;

	float m_ClearColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };	// 背景色
};