//=============================================================================
//
// スカイボックス定義 [Skybox.h]
// Author : 
// - 昼夜の空を6面テクスチャで描くよ
// - blendFactor でスムーズな空の切り替えも実現
// - 専用のCBとSRVでGPUパイプラインにも優しい構成だよ
//
//=============================================================================
#pragma once
#include "Core/Graphics/Renderer.h"

#define DAY_TIME 0.0f
#define NIGHT_TIME 3000.0f

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct SkyBoxVertex 
{
    XMFLOAT3 position;
    XMFLOAT2 uv;
    int      faceIndex;
};

struct SkyBoxBuffer
{
    XMMATRIX view;
    XMMATRIX projection;
    float blendFactor;
};

enum class SeasonID : uint64_t
{
    SPRING,
    SUMMER,
    AUTUMN,
    WINTER,
};

class Skybox : public SingletonBase<Skybox>
{
public:

    bool Initialize(float timeSpeed);
    void ShutDown(void);
    void Update(void);
    void Draw(const XMMATRIX& viewMatrix, const XMMATRIX& projectionMatrix);
    void SetCurrentTime(float time);
    void SetSeason(SeasonID seasonID);

    static float GetCurrentDaytime(void) { return s_blendFactor; }
    

private:

    void CreateCube();
    void LoadShaders();
    float AdjustBlendFactor(float time);

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    float m_timeOfDay;
    bool m_dayToNight;
    static float s_blendFactor;

    ShaderSet m_shaderSet;

    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11Buffer* m_skyboxBuffer = nullptr;
    ID3D11DepthStencilState* m_depthStencilState = nullptr;

    ID3D11ShaderResourceView* m_skyboxDaySRVs[6]{};
    ID3D11ShaderResourceView* m_skyboxSpringNightSRVs[6]{};
    ID3D11ShaderResourceView* m_skyboxSummerNightSRVs[6]{};
    ID3D11ShaderResourceView* m_skyboxAutumnNightSRVs[6]{};
    ID3D11ShaderResourceView* m_skyboxWinterNightSRVs[6]{};

    ShaderResourceBinder& m_ShaderResourceBinder = ShaderResourceBinder::get_instance();
    Renderer& m_renderer = Renderer::get_instance();

    float m_timeSpeed = 1;

    SeasonID m_currentSeason = SeasonID::SPRING;
};