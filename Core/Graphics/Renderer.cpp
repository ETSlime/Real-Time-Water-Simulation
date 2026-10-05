//=============================================================================
//
// レンダリング処理 [Renderer.cpp]
// Author : 
//
//=============================================================================
#include "main.h"
#include "Core/Graphics/Renderer.h"
#include "Core/LightManager.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
//デバッグ用画面テキスト出力を有効にする
#define DEBUG_DISP_TEXTOUT
//シェーダーデバッグ設定を有効にする
//#define DEBUG_SHADER


Renderer::Renderer()
{
	m_RenderMode = RenderMode::OBJ;
}

ID3D11Device* Renderer::GetDevice( void )
{
	return m_D3DDevice;
}


ID3D11DeviceContext* Renderer::GetDeviceContext( void )
{
	return m_ImmediateContext;
}


void Renderer::SetDepthEnable( BOOL Enable )
{
	if (Enable)
	{
		m_ImmediateContext->OMSetDepthStencilState(m_DepthStateEnable, NULL);
		m_ImmediateContext->OMSetRenderTargets(1, &m_RenderTargetView, m_SceneDepthStencilView);
	}
	else
	{
		m_ImmediateContext->OMSetDepthStencilState(m_DepthStateDisable, NULL);
		m_ImmediateContext->OMSetRenderTargets(1, &m_RenderTargetView, nullptr);
	}
}

void Renderer::SetDepthForParticle(void)
{
	m_ImmediateContext->OMSetDepthStencilState(m_DepthStateParticle, 0);
	m_ImmediateContext->OMSetRenderTargets(1, &m_RenderTargetView, m_SceneDepthStencilView);
}

void Renderer::SetDepthMode(DepthMode mode)
{
	ID3D11DepthStencilState* depthState = nullptr;
	ID3D11DepthStencilView* depthView = nullptr;

	switch (mode)
	{
	case DepthMode::Enable:
		// 深度テスト・書き込みどちらも有効（3Dモデル描画用）
		depthState = m_DepthStateEnable;
		depthView = m_SceneDepthStencilView;
		break;

	case DepthMode::Effect:
		// 深度テストは有効、書き込みは無効（パーティクル描画用）
		depthState = m_DepthStateParticle;
		depthView = m_SceneDepthStencilView;
		break;

	case DepthMode::Disable:
		// 深度テスト無効、DSVもバインドしない（UI描画などに使用）
		depthState = m_DepthStateDisable;
		depthView = nullptr;
		break;
	}

	// 深度ステンシルステートを設定
	m_ImmediateContext->OMSetDepthStencilState(depthState, 0);

	// レンダーターゲットと深度ビューのバインド
	m_ImmediateContext->OMSetRenderTargets(1, &m_RenderTargetView, depthView);
}


void Renderer::SetBlendState(BLEND_MODE bm)
{
	m_BlendStateParam = bm;

	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	switch (m_BlendStateParam)
	{
	case BLEND_MODE_NONE:
		m_ImmediateContext->OMSetBlendState(m_BlendStateNone, blendFactor, 0xffffffff);
		break;
	case BLEND_MODE_ALPHABLEND:
		m_ImmediateContext->OMSetBlendState(m_BlendStateAlphaBlend, blendFactor, 0xffffffff);
		break;
	case BLEND_MODE_ADD:
		m_ImmediateContext->OMSetBlendState(m_BlendStateAdd, blendFactor, 0xffffffff);
		break;
	case BLEND_MODE_SUBTRACT:
		m_ImmediateContext->OMSetBlendState(m_BlendStateSubtract, blendFactor, 0xffffffff);
		break;
	case BLEND_MODE_SWORDTRAIL:
		m_ImmediateContext->OMSetBlendState(m_BlendStateSwordTrail, blendFactor, 0xffffffff);
		break;
	}
}

void Renderer::SetCullingMode(CULL_MODE cm)
{
	switch (cm)
	{
	case CULL_MODE_NONE:
		m_ImmediateContext->RSSetState(m_RasterStateCullOff);
		break;
	case CULL_MODE_FRONT:
		m_ImmediateContext->RSSetState(m_RasterStateCullCW);
		break;
	case CULL_MODE_BACK:
		m_ImmediateContext->RSSetState(m_RasterStateCullCCW);
		break;
	}
}

void Renderer::SetRenderLayer(RenderLayer layer)
{
	switch (layer)
	{
	case RenderLayer::LAYER_0:
		m_ImmediateContext->RSSetState(m_RasterizerLayer0);
		break;
	case RenderLayer::LAYER_1:
		m_ImmediateContext->RSSetState(m_RasterizerLayer1);
		break;
	case RenderLayer::LAYER_2:
		m_ImmediateContext->RSSetState(m_RasterizerLayer2);
		break;
	case RenderLayer::LAYER_3:
		m_ImmediateContext->RSSetState(m_RasterizerLayer3);
		break;
	default:
		m_ImmediateContext->RSSetState(m_RasterStateCullOff);
		break;
	}
}

void Renderer::SetFillMode(D3D11_FILL_MODE mode)
{
	ID3D11RasterizerState* pRasterizerState;
	GetDeviceContext()->RSGetState(&pRasterizerState);
	D3D11_RASTERIZER_DESC rasterDesc;
	pRasterizerState->GetDesc(&rasterDesc);
	rasterDesc.FillMode = mode;
	ID3D11RasterizerState* newRasterizerState;
	m_D3DDevice->CreateRasterizerState(&rasterDesc, &newRasterizerState);
	m_ImmediateContext->RSSetState(newRasterizerState);

}

void Renderer::SetBoneMatrix(const XMMATRIX matrices[BONE_MAX]) const
{
	m_ImmediateContext->UpdateSubresource(m_BoneMatrixBuffer, 0, NULL, matrices, 0, 0);
}

void Renderer::SetAlphaTestEnable(BOOL flag)
{
	D3D11_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(blendDesc));

	if (flag)
		blendDesc.AlphaToCoverageEnable = TRUE;
	else
		blendDesc.AlphaToCoverageEnable = FALSE;

	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;

	switch (m_BlendStateParam)
	{
	case BLEND_MODE_NONE:
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		break;
	case BLEND_MODE_ALPHABLEND:
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		break;
	case BLEND_MODE_ADD:
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		break;
	case BLEND_MODE_SUBTRACT:
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		break;
	}

	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	ID3D11BlendState* blendState = NULL;
	m_D3DDevice->CreateBlendState(&blendDesc, &blendState);
	m_ImmediateContext->OMSetBlendState(blendState, blendFactor, 0xffffffff);

	if (blendState != NULL)
		blendState->Release();
}


void Renderer::SetWorldViewProjection2D( void )
{
	XMMATRIX world;
	world = XMMatrixTranspose(XMMatrixIdentity());


	XMMATRIX invWorld = XMMatrixTranspose(XMMatrixIdentity());

	WorldMatrixCBuffer worldBuffer;
	worldBuffer.world = world;
	worldBuffer.invWorld = invWorld;

	m_ImmediateContext->UpdateSubresource(m_WorldBuffer, 0, NULL, &worldBuffer, 0, 0);

	XMMATRIX view;
	view = XMMatrixTranspose(XMMatrixIdentity());
	m_ImmediateContext->UpdateSubresource(m_ViewBuffer, 0, NULL, &view, 0, 0);

	XMMATRIX worldViewProjection;
	worldViewProjection = XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);
	worldViewProjection = XMMatrixTranspose(worldViewProjection);
	m_ImmediateContext->UpdateSubresource(m_ProjectionBuffer, 0, NULL, &worldViewProjection, 0, 0);
}


void Renderer::SetCurrentWorldMatrix(const XMMATRIX *WorldMatrix ) const
{
	XMMATRIX world, invWorld;
	world = *WorldMatrix;

	// 最適化のポイント
// 1  もし World が単位行列 (Identity Matrix) ならば 逆転置行列も単位行列になる
//     -  inverse(transpose(Identity)) = Identity なので、XMMatrixIdentity() をそのまま使える
//     - **XMMatrixInverse() のコストをゼロにできる
//     - **逆転置行列をそのまま返すだけで OK** なので、計算時間を大幅に削減できる
//
// 2  XMMatrixIsIdentity() の実行コストは非常に低い！**
//     - XMMatrixIsIdentity() は **4 回の SIMD 比較 (XMVector4Equal) だけでチェックできる**
//     - そのため、**逆行列 (XMMatrixInverse) のコストと比較すると、事前チェックの方が圧倒的に軽い**
//
// 3  非均一スケール (Non-uniform Scale) がある場合、逆転置行列は transpose(inverse(World3x3)) を計算する必要がある**
//     - World にスケールが含まれていない場合、transpose(World3x3) だけでよい。
//     - **スケールがある場合、逆行列計算が必要になるが、これはコストが高い処理なので、単位行列の事前チェックが有効**
	if (XMMatrixIsIdentity(world))
	{
		invWorld = XMMatrixIdentity();
	}
	else
	{
		XMMATRIX world3x3 = XMMatrixSet(
			WorldMatrix->r[0].m128_f32[0], WorldMatrix->r[0].m128_f32[1], WorldMatrix->r[0].m128_f32[2], 0.0f,
			WorldMatrix->r[1].m128_f32[0], WorldMatrix->r[1].m128_f32[1], WorldMatrix->r[1].m128_f32[2], 0.0f,
			WorldMatrix->r[2].m128_f32[0], WorldMatrix->r[2].m128_f32[1], WorldMatrix->r[2].m128_f32[2], 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);

		invWorld = XMMatrixTranspose(XMMatrixInverse(nullptr, world3x3));
	}

	world = XMMatrixTranspose(world);

	WorldMatrixCBuffer buffer;
	buffer.world = world;
	buffer.invWorld = invWorld;

	m_ImmediateContext->UpdateSubresource(m_WorldBuffer, 0, NULL, &buffer, 0, 0);
}

void Renderer::SetViewMatrix(const XMMATRIX *ViewMatrix ) const
{
	XMMATRIX view;
	view = *ViewMatrix;
	view = XMMatrixTranspose(view);

	m_ImmediateContext->UpdateSubresource(m_ViewBuffer, 0, NULL, &view, 0, 0);
}

void Renderer::SetProjectionMatrix(const XMMATRIX *ProjectionMatrix ) const
{
	XMMATRIX projection;
	projection = *ProjectionMatrix;
	projection = XMMatrixTranspose(projection);

	m_ImmediateContext->UpdateSubresource(m_ProjectionBuffer, 0, NULL, &projection, 0, 0);
}

void Renderer::SetMaterial( MATERIAL material )
{
	m_Material.Diffuse = material.Diffuse;
	m_Material.Ambient = material.Ambient;
	m_Material.Specular = material.Specular;
	m_Material.Emission = material.Emission;
	m_Material.Shininess = material.Shininess;
	m_Material.noTexSampling = material.noTexSampling;
	m_Material.lightMapSampling = material.lightMapSampling;
	m_Material.normalMapSampling = material.normalMapSampling;
	m_Material.bumpMapSampling = material.bumpMapSampling;
	m_Material.opacityMapSampling = material.opacityMapSampling;
	m_Material.reflectMapSampling = material.reflectMapSampling;
	m_Material.translucencyMapSampling = material.translucencyMapSampling;

	m_ImmediateContext->UpdateSubresource( m_MaterialBuffer, 0, NULL, &m_Material, 0, 0 );
}

void Renderer::SetLightBuffer(const LIGHT_CBUFFER& lightBuffer)
{
	m_ImmediateContext->UpdateSubresource(m_LightBuffer, 0, NULL, &lightBuffer, 0, 0);
}

void Renderer::SetFogBuffer(void)
{
	m_ImmediateContext->UpdateSubresource(m_FogBuffer, 0, NULL, &m_Fog, 0, 0);
}

void Renderer::SetFogEnable(BOOL flag)
{
	// フラグを更新する
	m_Fog.Enable = flag;

	SetFogBuffer();
}

void Renderer::SetFog(FOG* pFog)
{
	m_Fog.Fog.x = pFog->FogStart;
	m_Fog.Fog.y = pFog->FogEnd;
	m_Fog.FogColor = pFog->FogColor;

	SetFogBuffer();
}

void Renderer::SetRenderProgress(RenderProgressCBuffer renderProgress)
{
	m_ImmediateContext->UpdateSubresource(m_RenderProgressBuffer, 0, NULL, &renderProgress, 0, 0);
}

void Renderer::SetFuchi(int flag)
{
	m_Fuchi.fuchi = flag;
	m_ImmediateContext->UpdateSubresource(m_FuchiBuffer, 0, NULL, &m_Fuchi, 0, 0);
}


void Renderer::SetShaderCamera(XMFLOAT3 pos)
{
	XMFLOAT4 tmp = XMFLOAT4( pos.x, pos.y, pos.z, 0.0f );

	m_ImmediateContext->UpdateSubresource(m_CameraPosBuffer, 0, NULL, &tmp, 0, 0);
}

void Renderer::SetRenderSkinnedMeshModel(void)
{
	m_RenderMode = RenderMode::SKINNED_MESH;

	ResetRenderTarget();

	m_SkinnedModelShaderSet = m_ShaderManager.GetShaderSet(ShaderSetID::SkinnedModel);
	m_ImmediateContext->VSSetShader(m_SkinnedModelShaderSet.vs, nullptr, 0);
	m_ImmediateContext->PSSetShader(m_SkinnedModelShaderSet.ps, nullptr, 0);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_BONE_MATRIX_ARRAY, m_BoneMatrixBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_BONE_MATRIX_ARRAY, m_BoneMatrixBuffer);
}

void Renderer::SetRenderInstance(void)
{
	m_RenderMode = RenderMode::INSTANCE;
}

void Renderer::SetRenderVFX(void)
{
	m_RenderMode = RenderMode::VFX;

	XMMATRIX mtxWorld = XMMatrixIdentity();
	SetCurrentWorldMatrix(&mtxWorld);

	SetCullingMode(CULL_MODE_NONE);

	m_VFXShaderSet = m_ShaderManager.GetShaderSet(ShaderSetID::VFX);
	m_ImmediateContext->VSSetShader(m_VFXShaderSet.vs, NULL, 0);
	m_ImmediateContext->PSSetShader(m_VFXShaderSet.ps, NULL, 0);
}

void Renderer::SetRenderUI(void)
{
	m_RenderMode = RenderMode::UI;

	m_UIShaderSet = m_ShaderManager.GetShaderSet(ShaderSetID::UI);
	m_ImmediateContext->VSSetShader(m_UIShaderSet.vs, NULL, 0);
	m_ImmediateContext->PSSetShader(m_UIShaderSet.ps, NULL, 0);
}

void Renderer::SetStaticModelInputLayout(void)
{
	m_ImmediateContext->IASetInputLayout(m_StaticModelShaderSet.inputLayout);
}

void Renderer::SetUIInputLayout(void)
{
	m_ImmediateContext->IASetInputLayout(m_UIShaderSet.inputLayout);
}

void Renderer::SetSkinnedMeshInputLayout(void)
{
	m_ImmediateContext->IASetInputLayout(m_SkinnedModelShaderSet.inputLayout);
}

void Renderer::SetVFXInputLayout(void)
{
	m_ImmediateContext->IASetInputLayout(m_VFXShaderSet.inputLayout);
}

void Renderer::SetRenderObject(void)
{
	m_RenderMode = RenderMode::OBJ;

	ResetRenderTarget();

	m_StaticModelShaderSet = m_ShaderManager.GetShaderSet(ShaderSetID::StaticModel);
	m_ImmediateContext->VSSetShader(m_StaticModelShaderSet.vs, NULL, 0);
	m_ImmediateContext->PSSetShader(m_StaticModelShaderSet.ps, NULL, 0);
}

void Renderer::ResetRenderTarget(void)
{
	m_ImmediateContext->OMSetRenderTargets(1, &m_RenderTargetView, m_SceneDepthStencilView);
}

void Renderer::SetMainPassViewport(void)
{
	// ビューポート設定
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)SCREEN_WIDTH;
	vp.Height = (FLOAT)SCREEN_HEIGHT;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_ImmediateContext->RSSetViewports(1, &vp);
}

void Renderer::SetShadersets(void)
{
	m_StaticModelShaderSet = m_ShaderManager.GetShaderSet(ShaderSetID::StaticModel);
	m_SkinnedModelShaderSet = m_ShaderManager.GetShaderSet(ShaderSetID::SkinnedModel);
	m_InstanceModelShaderSet = m_ShaderManager.GetShaderSet(ShaderSetID::Instanced_Tree);
	m_VFXShaderSet = m_ShaderManager.GetShaderSet(ShaderSetID::VFX);
	m_UIShaderSet = m_ShaderManager.GetShaderSet(ShaderSetID::UI);
}

//=============================================================================
// 初期化処理
//=============================================================================
HRESULT Renderer::Init(HINSTANCE hInstance, HWND hWnd, BOOL bWindow)
{
	HRESULT hr = S_OK;

	// デバイス、スワップチェーン、コンテキスト生成
	DWORD deviceFlags = 0;
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory( &sd, sizeof( sd ) );
	sd.BufferCount = 1;
	sd.BufferDesc.Width = SCREEN_WIDTH;
	sd.BufferDesc.Height = SCREEN_HEIGHT;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = bWindow;

	//デバッグ文字出力用設定
#if defined(_DEBUG) && defined(DEBUG_DISP_TEXTOUT)
	sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE;
	deviceFlags = D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#endif

	hr = D3D11CreateDeviceAndSwapChain( NULL,
										D3D_DRIVER_TYPE_HARDWARE,
										NULL,
										deviceFlags,
										NULL,
										0,
										D3D11_SDK_VERSION,
										&sd,
										&m_SwapChain,
										&m_D3DDevice,
										&m_FeatureLevel,
										&m_ImmediateContext );
	if( FAILED( hr ) )
		return hr;

	m_ShaderResourceBinder.Initialize(m_ImmediateContext);

	//デバッグ文字出力用設定
#if defined(_DEBUG) && defined(DEBUG_DISP_TEXTOUT)
	hr = m_SwapChain->ResizeBuffers(0, SCREEN_WIDTH, SCREEN_HEIGHT, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE); // N.B. the GDI compatible flag
	if (FAILED(hr))
		return hr;
#endif

	// レンダーターゲットビュー生成、設定
	ID3D11Texture2D* pBackBuffer = NULL;
	m_SwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&pBackBuffer );
	m_D3DDevice->CreateRenderTargetView( pBackBuffer, NULL, &m_RenderTargetView );
	pBackBuffer->Release();


	D3D11_TEXTURE2D_DESC sceneDepthDesc = {};
	sceneDepthDesc.Width = sd.BufferDesc.Width;
	sceneDepthDesc.Height = sd.BufferDesc.Height;
	sceneDepthDesc.MipLevels = 1;
	sceneDepthDesc.ArraySize = 1;
	sceneDepthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	sceneDepthDesc.SampleDesc.Count = 1;
	sceneDepthDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneDepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	ID3D11Texture2D* sceneDepthTexture = nullptr;
	m_D3DDevice->CreateTexture2D(&sceneDepthDesc, nullptr, &sceneDepthTexture);


	D3D11_DEPTH_STENCIL_VIEW_DESC sceneDSVDesc = {};
	sceneDSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
	sceneDSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	m_D3DDevice->CreateDepthStencilView(sceneDepthTexture, &sceneDSVDesc, &m_SceneDepthStencilView);



	// ラスタライザステート作成
	D3D11_RASTERIZER_DESC rd; 
	ZeroMemory( &rd, sizeof( rd ) );
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE; 
	rd.DepthClipEnable = TRUE; 
	rd.MultisampleEnable = FALSE; 
	m_D3DDevice->CreateRasterizerState( &rd, &m_RasterStateCullOff);

	rd.CullMode = D3D11_CULL_FRONT;
	m_D3DDevice->CreateRasterizerState(&rd, &m_RasterStateCullCW);

	rd.CullMode = D3D11_CULL_BACK;
	m_D3DDevice->CreateRasterizerState(&rd, &m_RasterStateCullCCW);

	// カリングモード設定（CCW）
	SetCullingMode(CULL_MODE_BACK);

	ZeroMemory(&rd, sizeof(rd));

	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = TRUE;
	rd.MultisampleEnable = FALSE;

	// Layer 0
	rd.DepthBias = DEPTHBIAS_LAYER_0;
	m_D3DDevice->CreateRasterizerState(&rd, &m_RasterizerLayer0);

	// Layer 1
	rd.DepthBias = DEPTHBIAS_LAYER_1;
	m_D3DDevice->CreateRasterizerState(&rd, &m_RasterizerLayer1);

	// Layer 2
	rd.DepthBias = DEPTHBIAS_LAYER_2;
	m_D3DDevice->CreateRasterizerState(&rd, &m_RasterizerLayer2);

	// Layer 3
	rd.DepthBias = DEPTHBIAS_LAYER_3;
	m_D3DDevice->CreateRasterizerState(&rd, &m_RasterizerLayer3);


	// ブレンドステートの作成
	D3D11_BLEND_DESC blendDesc;
	ZeroMemory( &blendDesc, sizeof( blendDesc ) );
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	m_D3DDevice->CreateBlendState( &blendDesc, &m_BlendStateAlphaBlend );

	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	m_D3DDevice->CreateBlendState(&blendDesc, &m_BlendStateNone);

	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	m_D3DDevice->CreateBlendState(&blendDesc, &m_BlendStateAdd);

	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	m_D3DDevice->CreateBlendState(&blendDesc, &m_BlendStateSubtract);

	ZeroMemory(&blendDesc, sizeof(blendDesc));

	blendDesc.AlphaToCoverageEnable = TRUE;  // アルファ テスト カバレッジを有効にする
	blendDesc.IndependentBlendEnable = FALSE;

	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	m_D3DDevice->CreateBlendState(&blendDesc, &m_BlendStateSwordTrail);

	// アルファブレンド設定
	SetBlendState(BLEND_MODE_ALPHABLEND);


	// 深度ステンシルステート作成
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory( &depthStencilDesc, sizeof( depthStencilDesc ) );
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask	= D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthStencilDesc.StencilEnable = FALSE;

	//深度Test有効ステート
	m_D3DDevice->CreateDepthStencilState( &depthStencilDesc, &m_DepthStateEnable);

	//深度Write無効（パーティクル用）
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	m_D3DDevice->CreateDepthStencilState(&depthStencilDesc, &m_DepthStateParticle);

	//深度Test無効ステート
	depthStencilDesc.DepthEnable = FALSE;
	m_D3DDevice->CreateDepthStencilState( &depthStencilDesc, &m_DepthStateDisable);

	// 深度ステンシルステート設定
	SetDepthEnable(TRUE);

	// サンプラーステート設定
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory( &samplerDesc, sizeof( samplerDesc ) );

	// general sampler
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0;
	samplerDesc.MaxAnisotropy = 16;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	m_D3DDevice->CreateSamplerState( &samplerDesc, &m_SamplerState);
	m_ShaderResourceBinder.BindSampler(ShaderStage::VS, SLOT_SAMPLER_DEFAULT, m_SamplerState);
	m_ShaderResourceBinder.BindSampler(ShaderStage::PS, SLOT_SAMPLER_DEFAULT, m_SamplerState);

	ZeroMemory(&samplerDesc, sizeof(samplerDesc));

	// shadow map sampler
	samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.BorderColor[0] = 1.0f;                       
	samplerDesc.BorderColor[1] = 1.0f;
	samplerDesc.BorderColor[2] = 1.0f;
	samplerDesc.BorderColor[3] = 1.0f;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;      
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	m_D3DDevice->CreateSamplerState(&samplerDesc, &m_SamplerStateShadow);
	m_ShaderResourceBinder.BindSampler(ShaderStage::PS, SLOT_SAMPLER_SHADOW, m_SamplerStateShadow);


	ZeroMemory(&samplerDesc, sizeof(samplerDesc));

	// opacity map sampler
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MipLODBias = 0;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	m_D3DDevice->CreateSamplerState(&samplerDesc, &m_SamplerStateOpacity);
	m_ShaderResourceBinder.BindSampler(ShaderStage::PS, SLOT_SAMPLER_OPACITY, m_SamplerStateOpacity);

	// 定数バッファ生成
	D3D11_BUFFER_DESC hBufferDesc;
	hBufferDesc.ByteWidth = sizeof(WorldMatrixCBuffer);
	hBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	hBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hBufferDesc.CPUAccessFlags = 0;
	hBufferDesc.MiscFlags = 0;
	hBufferDesc.StructureByteStride = sizeof(float);

	//ワールドマトリクス
	m_D3DDevice->CreateBuffer(&hBufferDesc, NULL, &m_WorldBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_WORLD_MATRIX, m_WorldBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_WORLD_MATRIX, m_WorldBuffer);

	//ビューマトリクス
	m_D3DDevice->CreateBuffer(&hBufferDesc, NULL, &m_ViewBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_VIEW_MATRIX, m_ViewBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_VIEW_MATRIX, m_ViewBuffer);

	//プロジェクションマトリクス
	m_D3DDevice->CreateBuffer(&hBufferDesc, NULL, &m_ProjectionBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_PROJECTION_MATRIX, m_ProjectionBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_PROJECTION_MATRIX, m_ProjectionBuffer);

	//マテリアル情報
	hBufferDesc.ByteWidth = sizeof(MATERIAL_CBUFFER);
	m_D3DDevice->CreateBuffer(&hBufferDesc, NULL, &m_MaterialBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_MATERIAL, m_MaterialBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_MATERIAL, m_MaterialBuffer);

	//ライト情報
	hBufferDesc.ByteWidth = sizeof(LIGHT_CBUFFER);
	m_D3DDevice->CreateBuffer(&hBufferDesc, NULL, &m_LightBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_LIGHT, m_LightBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_LIGHT, m_LightBuffer);

	//フォグ情報
	hBufferDesc.ByteWidth = sizeof(FOG_CBUFFER);
	m_D3DDevice->CreateBuffer(&hBufferDesc, NULL, &m_FogBuffer);

	//縁取り
	hBufferDesc.ByteWidth = sizeof(FUCHI);
	m_D3DDevice->CreateBuffer(&hBufferDesc, NULL, &m_FuchiBuffer);

	//カメラ
	hBufferDesc.ByteWidth = sizeof(XMFLOAT4);
	m_D3DDevice->CreateBuffer(&hBufferDesc, NULL, &m_CameraPosBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_CAMERA_POS, m_CameraPosBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_CAMERA_POS, m_CameraPosBuffer);

	hBufferDesc.ByteWidth = sizeof(BoneMatricesCBuffer);
	m_D3DDevice->CreateBuffer(&hBufferDesc, NULL, &m_BoneMatrixBuffer);


	hBufferDesc.ByteWidth = sizeof(LIGHTMODE_CBUFFER);
	m_D3DDevice->CreateBuffer(&hBufferDesc, NULL, &m_LightModeBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_LIGHT_MODE, m_LightModeBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_LIGHT_MODE, m_LightModeBuffer);
	
	hBufferDesc.ByteWidth = sizeof(RenderProgressCBuffer);
	m_D3DDevice->CreateBuffer(&hBufferDesc, NULL, &m_RenderProgressBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_RENDER_PROGRESS, m_RenderProgressBuffer);
	m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_RENDER_PROGRESS, m_RenderProgressBuffer);

	// 入力レイアウト設定
	m_ImmediateContext->IASetInputLayout(m_ShaderManager.GetInputLayout(VertexLayoutID::Static));

	//マテリアル初期化
	MATERIAL material;
	ZeroMemory(&material, sizeof(material));
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	SetMaterial(material);

	return S_OK;
}


//=============================================================================
// 終了処理
//=============================================================================
void Renderer::Shutdown(void)
{
	// オブジェクト解放
	SafeRelease(&m_DepthStateEnable);
	SafeRelease(&m_DepthStateDisable);
	SafeRelease(&m_DepthStateParticle);

	SafeRelease(&m_BlendStateNone);
	SafeRelease(&m_BlendStateAlphaBlend);
	SafeRelease(&m_BlendStateAdd);
	SafeRelease(&m_BlendStateSubtract);

	SafeRelease(&m_RasterStateCullOff);
	SafeRelease(&m_RasterStateCullCW);
	SafeRelease(&m_RasterStateCullCCW);
	SafeRelease(&m_RasterizerLayer0);
	SafeRelease(&m_RasterizerLayer1);
	SafeRelease(&m_RasterizerLayer2);
	SafeRelease(&m_RasterizerLayer3);


	SafeRelease(&m_WorldBuffer);
	SafeRelease(&m_ViewBuffer);
	SafeRelease(&m_ProjectionBuffer);
	SafeRelease(&m_MaterialBuffer);
	SafeRelease(&m_LightBuffer);
	SafeRelease(&m_FogBuffer);
	SafeRelease(&m_FuchiBuffer);
	SafeRelease(&m_CameraPosBuffer);
	SafeRelease(&m_LightProjViewBuffer);
	SafeRelease(&m_BoneMatrixBuffer);
	SafeRelease(&m_LightModeBuffer);
	SafeRelease(&m_RenderProgressBuffer);

	SafeRelease(&m_SamplerState);
	SafeRelease(&m_SamplerStateShadow);
	SafeRelease(&m_SamplerStateOpacity);

	if (m_ImmediateContext)		m_ImmediateContext->ClearState();
	SafeRelease(&m_RenderTargetView);
	SafeRelease(&m_SwapChain);
	SafeRelease(&m_ImmediateContext);
	SafeRelease(&m_D3DDevice);
}



//=============================================================================
// バックバッファクリア
//=============================================================================
void Renderer::Clear(void)
{
	// バックバッファクリア
	m_ImmediateContext->ClearRenderTargetView( m_RenderTargetView, m_ClearColor );
	m_ImmediateContext->ClearDepthStencilView( m_SceneDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}


void Renderer::SetClearColor(float* color4)
{
	m_ClearColor[0] = color4[0];
	m_ClearColor[1] = color4[1];
	m_ClearColor[2] = color4[2];
	m_ClearColor[3] = color4[3];
}


//=============================================================================
// プレゼント
//=============================================================================
void Renderer::Present(void)
{
	m_SwapChain->Present( 0, 0 );
}


//=============================================================================
// デバッグ用テキスト出力
//=============================================================================
void Renderer::DebugTextOut(char* text, int x, int y)
{
#if defined(_DEBUG) && defined(DEBUG_DISP_TEXTOUT)
	HRESULT hr;

	//バックバッファからサーフェスを取得する
	IDXGISurface1* pBackSurface = NULL;
	hr = m_SwapChain->GetBuffer(0, __uuidof(IDXGISurface1), (void**)&pBackSurface);

	if (SUCCEEDED(hr))
	{
		//取得したサーフェスからデバイスコンテキストを取得する
		HDC hdc;
		hr = pBackSurface->GetDC(FALSE, &hdc);

		if (SUCCEEDED(hr))
		{
			//文字色を白に変更
			SetTextColor(hdc, RGB(255, 255, 255));
			//背景を透明に変更
			SetBkMode(hdc, TRANSPARENT);

			RECT rect;
			rect.left = 0;
			rect.top = 0;
			rect.right = SCREEN_WIDTH;
			rect.bottom = SCREEN_HEIGHT;

			//テキスト出力
			DrawText(hdc, text, (int)strlen(text), &rect, DT_LEFT);

			//デバイスコンテキストを解放する
			pBackSurface->ReleaseDC(NULL);
		}
		//サーフェスを解放する
		pBackSurface->Release();

		//レンダリングターゲットがリセットされるのでセットしなおす
		m_ImmediateContext->OMSetRenderTargets(1, &m_RenderTargetView, m_SceneDepthStencilView);
	}
#endif
}

void Renderer::SetLightModeBuffer(int mode)
{
	LIGHTMODE_CBUFFER md;
	md.mode = mode;
	m_ImmediateContext->UpdateSubresource(m_LightModeBuffer, 0, NULL, &md, 0, 0);
}

RenderMode Renderer::GetRenderMode(void)
{
	return m_RenderMode;
}

void Renderer::SetRenderMode(RenderMode mode)
{
	m_RenderMode = mode;
}

void Renderer::BindViewBuffer(ShaderStage stage)
{
	m_ShaderResourceBinder.BindConstantBuffer(stage, SLOT_CB_VIEW_MATRIX, m_ViewBuffer);
}
