#pragma once
//=============================================================================
//
// [DecalRenderer.h]
// Author : 
// 
//=============================================================================
#include "Core/Graphics/Renderer.h"
#include "Core/Timer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_DECAL_INSTANCES 512

//*****************************************************************************
// 構造体定義
//*****************************************************************************

struct DecalInstance
{
    XMFLOAT3 position;  // デカール中心位置（ワールド空間）
    float size;         // デカールの大きさ（正方形）

    XMFLOAT3 normal;    // デカール面の法線（貼り付け方向）
    float lifetime;     // 表示時間（秒）

	XMFLOAT2 uvOffset;  // UVオフセット（ランダム回転用）
    float elapsed;      // 経過時間（秒）
    float rotation;     // 回転（地面に対して回転させる用）

	float alpha; 	    // 透明度（フェードアウト用）
    XMFLOAT3 padding;

};

class DecalRenderer : public SingletonBase<DecalRenderer>
{
public:
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
    void Update(void);
    void Draw(void);
    void AddDecal(const XMFLOAT3& pos, const XMFLOAT3& normal, float size = 25.0f, float lifetime = 20.0f, float alpha = 1.0f);

private:
    bool LoadShaders(void);

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11Buffer* m_instanceBuffer = nullptr;

	ShaderSet m_shaderSet;

    ID3D11ShaderResourceView* m_decalTexture = nullptr;

    SimpleArray<DecalInstance> m_instances;
    UINT m_maxDecals = MAX_DECAL_INSTANCES;

	Timer& m_timer = Timer::get_instance();
	ShaderResourceBinder& m_resourceBinder = ShaderResourceBinder::get_instance();
};