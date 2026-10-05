//=============================================================================
//
// 方向光・点光源などライト情報の更新および管理 [Light.h]
// Author : 
// シーン内ライトの種類・有効状態・方向／位置を制御し、
// トランスフォームとの連携およびシェーダー連携用の LightData を構築する
//
//=============================================================================
#pragma once
#include "Scene/GameObject.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
enum class LIGHT_TYPE : int
{
	NONE = 0,			//ライト無し
	DIRECTIONAL,	//ディレクショナルライト
	POINT,			//ポイントライト

	LIGHT_TYPE_NUM
};

//*****************************************************************************
// プロトタイプ宣言
//*****************************************************************************
class Light
{
public:
	Light();
	virtual ~Light() = default;

	virtual void Update(void) = 0;
	virtual const LightData& GetLightData() const { return m_LightData; }
	virtual void BindToTransform(const Transform* pTransform);
	void SetEnable(bool flag) { m_LightData.Enable = flag; }
	LIGHT_TYPE GetType(void) const;

protected:
	LightData m_LightData;
	const Transform* ownerTransform;
};

class DirectionalLight : public Light
{
public:
	DirectionalLight();
	void SetTimeBasedRotation(bool enable);
	XMFLOAT3 GetDirection(void) const { return m_LightData.Direction; }
	virtual void Update(void) override;

private:
	bool m_TimeBased = false;
	float m_Time = 0.0f;
};


