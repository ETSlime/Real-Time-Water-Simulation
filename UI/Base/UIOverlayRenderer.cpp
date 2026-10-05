//=============================================================================
//
// UIOverlayRenderer処理 [UIOverlayRenderer.cpp]
// Author : 
//
//=============================================================================
#include "UI/Base/UIOverlayRenderer.h"
#include "UI/Base/UISpriteRenderer.h"
#include "Core/TextureMgr.h"
#include <algorithm>

bool UIOverlayRenderer::Initialize(ID3D11Buffer* vertexBuffer)
{
    if (!vertexBuffer) return false;
    m_vertexBuffer = vertexBuffer;

    m_whiteTextureSRV = TextureMgr::get_instance().CreateTexture(UI_TEXTURE_PATH);
    if (m_whiteTextureSRV == nullptr)
        return false;

    ID3D11Device* device = m_renderer.GetDevice();

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = device->CreateBlendState(&blendDesc, &m_alphaBlendState);
    if (FAILED(hr)) {
        return  false;
    }

    return true;
}

UIOverlayRenderer::~UIOverlayRenderer()
{
    if (m_alphaBlendState)
    {
        m_alphaBlendState->Release();
        m_alphaBlendState = nullptr;
    }
}

void UIOverlayRenderer::Draw(ID3D11DeviceContext* ctx, float x, float y, float width, float height, XMFLOAT4 color)
{
    // アニメーション中であれば、アルファ値を補間する
    if (m_isFading)
    {
        color.w = ComputeAlpha();
    }

    // テクスチャをバインド
    m_shaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_DIFFUSE, m_whiteTextureSRV);
    // 頂点バッファを更新
    UISpriteRenderer::SetSpriteWithColor(m_vertexBuffer, x, y, width, height, 0, 0, 1, 1, color);

    UINT stride = sizeof(UIVertex);
    UINT offset = 0;
    m_renderer.GetDeviceContext()->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    m_renderer.GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // 描画
    m_renderer.GetDeviceContext()->Draw(4, 0);
}

void UIOverlayRenderer::StartFadeIn(float duration, XMFLOAT4 color)
{
    m_fadeIn = true;
    m_fadeDuration = duration;
    m_fadeTime = 0.0f;
    m_isFading = true;
    m_targetColor = color;
}

void UIOverlayRenderer::StartFadeOut(float duration, XMFLOAT4 color)
{
    m_fadeIn = false;
    m_fadeDuration = duration;
    m_fadeTime = 0.0f;
    m_isFading = true;
    m_targetColor = color;
}

void UIOverlayRenderer::Update(float deltaTime)
{
    if (m_isFading)
    {
        m_fadeTime += deltaTime;
        if (m_fadeTime >= m_fadeDuration)
        {
            m_fadeTime = m_fadeDuration;
            m_isFading = false;
        }
    }
}

float UIOverlayRenderer::ComputeAlpha() const
{
    float t = m_fadeTime / m_fadeDuration;
    t = (t > 1.0f) ? 1.0f : t;
    return m_fadeIn ? (t * m_targetColor.w) : ((1.0f - t) * m_targetColor.w);
}

void UIOverlayRenderer::DrawHPBar(
    ID3D11DeviceContext* ctx,
    float x, float y,
    float width, float height,
    float hpRatio,
    DirectX::XMFLOAT4 barColor,
    DirectX::XMFLOAT4 bgColor)
{

    hpRatio = (hpRatio < 0.0f) ? 0.0f : ((hpRatio > 1.0f) ? 1.0f : hpRatio);

    
    bool originalIsFading = m_isFading;
    float originalAlpha = 1.0f;
    if (originalIsFading)
    {
        originalAlpha = ComputeAlpha();
        m_isFading = false;
    }

    Draw(ctx, x + width * 0.5f, y + height * 0.5f, width, height, bgColor);

    // HP bar (changed to trapezoidal)
    float hpWidth = width * hpRatio;
    if (hpWidth > 0.0f)
    {
        float hpWidth = width * hpRatio;
        float centerX = x + hpWidth * 0.5f;
        float centerY = y + height * 0.5f;
        float skewX = -55.0f;

        UISpriteRenderer::SetSpriteParallelogram(
            m_vertexBuffer,
            centerX, centerY,
            hpWidth, height,
            skewX,
            0, 0, 1, 1,
            barColor
        );

        UINT stride = sizeof(UIVertex);
        UINT offset = 0;
        ctx->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        ctx->Draw(4, 0);
    }
}


void UIOverlayRenderer::Draw2DTexturedRect(
    ID3D11DeviceContext* ctx,
    float x, float y,
    float width, float height,
    const DirectX::XMFLOAT4& color)
{
    // Rectangular drawing using the entire UV (0-1)
    UISpriteRenderer::SetSpriteWithColor(m_vertexBuffer, x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f, color);

    UINT stride = sizeof(UIVertex);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->Draw(4, 0);
}

void UIOverlayRenderer::DrawOverlayTexture(
    ID3D11DeviceContext* ctx,
    float x, float y,
    float width, float height,
    ID3D11ShaderResourceView* textureSRV,
    const DirectX::XMFLOAT4& color)
{
    if (!ctx || !textureSRV) return;

    // ★ 前のブレンドステートを取得
    ID3D11BlendState* prevBlendState = nullptr;
    FLOAT blendFactor[4] = { 0 };
    UINT sampleMask = 0xffffffff;
    ctx->OMGetBlendState(&prevBlendState, blendFactor, &sampleMask);

    // 1. テクスチャセット
    ctx->PSSetShaderResources(0, 1, &textureSRV);

    // 2. アルファブレンドステートに設定
    ctx->OMSetBlendState(m_alphaBlendState, nullptr, 0xffffffff);

    // 3. 描画（装飾テクスチャなど）
    Draw2DTexturedRect(ctx, x, y, width, height, color);

    // 4. テクスチャ解放
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    ctx->PSSetShaderResources(0, 1, nullSRV);

    // 5. ★ ブレンドステートを元に戻す
    ctx->OMSetBlendState(prevBlendState, blendFactor, sampleMask);
    if (prevBlendState) prevBlendState->Release();
}
