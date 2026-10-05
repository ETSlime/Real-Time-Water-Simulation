//=============================================================================
//
// ライト一覧の登録・有効管理および定数バッファ生成 [light.h]
// Author : 
// 全ライトの追加／削除／更新を統括し、有効状態の制御とともに、
// シェーダー向けの LIGHT_CBUFFER を構築・提供するマネージャクラス
//
//=============================================================================
#pragma once
#include "Core/Light.h"
#include "Utility/DoubleLinkedList.h"

class LightManager : public SingletonBase<LightManager>
{
public:
	LightManager();

	void AddLight(Light* light);
	void RemoveLight(Light* light);
	void SetLightEnable(BOOL flag);
	void Update(void);

	const LIGHT_CBUFFER& GetCBuffer() const { return m_CBuffer; }

	const DoubleLinkedList<Light*>& GetLightList(void) { return m_LightList; }

private:

	bool m_enableLight;
	DoubleLinkedList<Light*> m_LightList;
	LIGHT_CBUFFER m_CBuffer;
	Renderer& m_renderer = Renderer::get_instance();
};