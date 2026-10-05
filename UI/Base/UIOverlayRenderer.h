#pragma once
//=============================================================================
//
// ふんわりフェードで包み込む魔法の子 [UIOverlayRenderer.h]
// Author : 
// UIの背景に淡い光をかけたり、フェードイン・アウトで場面転換を演出するおしゃれなクラスですっ
// シンプルな矩形をふわっと描画して、世界に魔法をかけてくれるの
//
//=============================================================================
#include "Core/Graphics/Renderer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define UI_TEXTURE_PATH "data/TEXTURE/white_1x1.png"

// UIフェード・オーバーレイ描画クラス
class UIOverlayRenderer
{
public:
    ~UIOverlayRenderer();

    bool Initialize(ID3D11Buffer* vertexBuffer);

    // 瞬間表示（アルファ指定）
    void Draw(ID3D11DeviceContext* ctx, float x, float y, float width, float height, XMFLOAT4 color);

    // フェード効果（Alpha値0.0f~1.0f）
    void StartFadeIn(float duration, XMFLOAT4 color);
    void StartFadeOut(float duration, XMFLOAT4 color);

    void Update(float deltaTime);
    bool IsFading() const { return m_isFading; }

    // HP bar drawing (fixed right edge and shrunk from left)
    void DrawHPBar(ID3D11DeviceContext* ctx, float x, float y, float width, float height, float hpRatio, DirectX::XMFLOAT4 barColor, DirectX::XMFLOAT4 bgColor);

    void DrawOverlayTexture(
        ID3D11DeviceContext * ctx,
        float x, float y,
        float width, float height,
        ID3D11ShaderResourceView * textureSRV,
        const DirectX::XMFLOAT4 & color = { 1,1,1,1 });
    void Draw2DTexturedRect(ID3D11DeviceContext* ctx, float x, float y, float width, float height, const DirectX::XMFLOAT4& color);
private:
    // 現在のアルファ値を取得
    float ComputeAlpha() const;

    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11ShaderResourceView* m_whiteTextureSRV = nullptr;
    float m_fadeTime = 0.0f;
    float m_fadeDuration = 0.0f;
    bool m_fadeIn = true;
    bool m_isFading = false;
    XMFLOAT4 m_targetColor = { 0, 0, 0, 0 };

    Renderer& m_renderer = Renderer::get_instance();
    ShaderResourceBinder& m_shaderResourceBinder = ShaderResourceBinder::get_instance();

    ID3D11BlendState* m_alphaBlendState = nullptr;
};
