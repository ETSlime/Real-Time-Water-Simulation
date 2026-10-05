//=============================================================================
//
// ライト処理 [light.cpp]
// Author : 
//
//=============================================================================
#include "main.h"
#include "Core/Graphics/Renderer.h"
#include "Core/Light.h"
#include "Effects/Skybox.h"
//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define	LIGHT_VIEW_ANGLE		(XMConvertToRadians(45.0f))						// ビュー平面の視野角
#define	LIGHT_VIEW_ASPECT		((float)SCREEN_WIDTH / (float)SCREEN_HEIGHT)	// ビュー平面のアスペクト比	
#define	LIGHT_VIEW_NEAR_Z		(1.0f)											// ビュー平面のNearZ値
#define	LIGHT_VIEW_FAR_Z		(3000.0f)										// ビュー平面のFarZ値


//*****************************************************************************
// グローバル変数
//*****************************************************************************
constexpr XMFLOAT4 dayLightAmbient = XMFLOAT4(0.45f, 0.45f, 0.45f, 1.0f);
constexpr XMFLOAT4 nightLightAmbient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);

//=============================================================================
// 初期化処理
//=============================================================================
Light::Light()
{
	m_LightData.Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_LightData.Direction = XMFLOAT3( 0.0f, -1.0f, 0.0f );
	m_LightData.Diffuse   = XMFLOAT4( 1.0f, 1.0f, 1.0f, 1.0f );
	m_LightData.Ambient   = XMFLOAT4( 0.0f, 0.0f, 0.0f, 1.0f );
	m_LightData.Attenuation = 100.0f;	// 減衰距離
	m_LightData.Type = static_cast<int>(LIGHT_TYPE::NONE);	// ライトのタイプ
	m_LightData.Enable = FALSE;			// ON / OFF
	XMStoreFloat4x4(&m_LightData.LightViewProj, XMMatrixIdentity());

	ownerTransform = nullptr;
}

void Light::BindToTransform(const Transform* pTransform)
{
	ownerTransform = pTransform;
}


LIGHT_TYPE Light::GetType(void) const
{
	assert(m_LightData.Type > static_cast<int>(LIGHT_TYPE::NONE) &&
		m_LightData.Type <= static_cast<int>(LIGHT_TYPE::LIGHT_TYPE_NUM));
	return static_cast<LIGHT_TYPE>(m_LightData.Type);
}

//=============================================================================
// フォグの設定
//=============================================================================
//void Light::SetFogData(FOG *fog)
//{
//	renderer.SetFog(fog);
//}
//
//
//BOOL Light::GetFogEnable(void)
//{
//	return(g_FogEnable);
//}

DirectionalLight::DirectionalLight(void)
{
	m_LightData.Type = static_cast<int>(LIGHT_TYPE::DIRECTIONAL);
	m_LightData.Direction = { 0, -1, 0 };
}

void DirectionalLight::SetTimeBasedRotation(bool enable)
{
	m_TimeBased = enable;
}

void DirectionalLight::Update(void)
{
	m_LightData.Position = ownerTransform->pos;
	m_LightData.Position.y += 1111.0f;

	XMFLOAT3 targetPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 lightUp = { 0.0f, 1.0f, 0.0f };
	XMVECTOR pos = XMLoadFloat3(&m_LightData.Position);

	float currentTime = Skybox::GetCurrentDaytime();
	m_LightData.Ambient.x = dayLightAmbient.x * (1.0f - currentTime) + nightLightAmbient.x * currentTime;
	m_LightData.Ambient.y = dayLightAmbient.y * (1.0f - currentTime) + nightLightAmbient.y * currentTime;
	m_LightData.Ambient.z = dayLightAmbient.z * (1.0f - currentTime) + nightLightAmbient.z * currentTime;
	m_LightData.Ambient.w = 1.0f;

	XMVECTOR dayDir = XMVectorSet(0.1f, -0.9f, 0.3f, 0.0f);
	XMVECTOR nightDir = XMVectorSet(0.4f, -0.21f, 0.4f, 0.0f);
	XMVECTOR interpolatedDir = XMQuaternionSlerp(dayDir, nightDir, currentTime);
	interpolatedDir = XMVector3Normalize(interpolatedDir);

	XMFLOAT3 finalDir;
	XMStoreFloat3(&finalDir, interpolatedDir);
	m_LightData.Direction = finalDir;

	XMVECTOR lightDir = XMLoadFloat3(&m_LightData.Direction);
	XMMATRIX lightView = XMMatrixLookAtLH(
		XMLoadFloat3(&m_LightData.Position),
		pos + lightDir,
		XMLoadFloat3(&lightUp)
	);
	XMMATRIX lightProj = XMMatrixOrthographicLH(SHADOWMAP_SIZE, SHADOWMAP_SIZE, LIGHT_VIEW_NEAR_Z, LIGHT_VIEW_FAR_Z);
	XMStoreFloat4x4(&m_LightData.LightViewProj, XMMatrixTranspose(lightView * lightProj));

	//if (m_TimeBased)
	//{
	//	m_Time += deltaTime;
	//	float angle = m_Time * 0.5f;
	//	m_Data.Direction.x = sinf(angle);
	//	m_Data.Direction.y = -1.0f;
	//	m_Data.Direction.z = cosf(angle);
	//}
}
