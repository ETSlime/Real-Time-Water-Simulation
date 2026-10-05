//=============================================================================
//
// ライト処理 [LightManager.cpp]
// Author : 
//
//=============================================================================
#include "Core/LightManager.h"

//=============================================================================
// 初期化処理
//=============================================================================
LightManager::LightManager()
{
	memset(&m_CBuffer, 0, sizeof(m_CBuffer));
	m_enableLight = true;
}

//=============================================================================
// 更新処理
//=============================================================================
void LightManager::Update(void)
{
	memset(&m_CBuffer, 0, sizeof(m_CBuffer));
	m_CBuffer.Enable = m_enableLight;

	int index = 0;
	for (Light* light : m_LightList)
	{
		if (light == nullptr) continue;

		const LightData& data = light->GetLightData();
		if (data.Enable == FALSE)
			continue;

		light->Update();

		if (index >= LIGHT_MAX)
			continue;

		m_CBuffer.Position[index] = XMFLOAT4(data.Position.x, data.Position.y, data.Position.z, 1.0f);
		m_CBuffer.Direction[index] = XMFLOAT4(data.Direction.x, data.Direction.y, data.Direction.z, 0.0f);
		m_CBuffer.Diffuse[index] = data.Diffuse;
		m_CBuffer.Ambient[index] = data.Ambient;
		m_CBuffer.Attenuation[index] = XMFLOAT4(data.Attenuation, 0, 0, 0);
		m_CBuffer.Flags[index].Type = data.Type;
		m_CBuffer.Flags[index].OnOff = data.Enable;
		m_CBuffer.LightViewProj[index] = data.LightViewProj;

		index++;
	}

	m_renderer.SetLightBuffer(m_CBuffer);
}

void LightManager::AddLight(Light* light)
{
	if (light)
		m_LightList.push_back(light);
}

void LightManager::RemoveLight(Light* light)
{
	m_LightList.remove(&light);
}

void LightManager::SetLightEnable(BOOL flag)
{
	m_enableLight = flag;
	m_CBuffer.Enable = m_enableLight;
	m_renderer.SetLightBuffer(m_CBuffer);
}