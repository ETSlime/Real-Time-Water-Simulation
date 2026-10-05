#pragma once
//=============================================================================
//
// [UISprite.h]
// Author : 
// 
//=============================================================================
#include "UI/Base/ISpriteUI.h"
#include "UI/Base/UISpriteRenderer.h"
#include "Core/Shader/ShaderResourceBinder.h"

class UISprite : public ISpriteUI
{
public:
	UISprite(ID3D11Buffer* sharedVertexBuffer = nullptr, ID3D11ShaderResourceView* tex = nullptr) 
        : m_vertexBuffer(sharedVertexBuffer), m_texture(tex) {}

    // セッター（他のUI要素からは変更できないように）
    void SetPosition(const XMFLOAT2& pos) override { m_position = pos; }
    void SetPosition(float x, float y) override { m_position = { x, y }; }
    void SetScale(const XMFLOAT2& scale) override { m_scale = scale; }
    void SetScale(float x, float y) override { m_scale = { x, y }; }
    void SetRotation(float rot) override { m_rot = rot; }
    void SetSize(const XMFLOAT2& size) override { m_size = size; }
    void SetSize(float w, float h) override { m_size = { w, h }; }
    void SetColor(const XMFLOAT4& color) override { m_color = color; }
	void SetColor(float r, float g, float b, float a) override { m_color = { r, g, b, a }; }
	void SetUV(const XMFLOAT4& uv) override { m_uv = uv; }
    void SetUV(float u, float v, float w, float h) override { m_uv = { u, v, w, h }; }
    void SetTexture(ID3D11ShaderResourceView* tex) override { m_texture = tex; }
    void SetVisible(bool visible) override { m_visible = visible; }
    void SetDrawMode(SpriteDrawMode mode) { m_drawMode = mode; }

    // ゲッター（他のUI要素からも取得できるように）
    const XMFLOAT2& GetPosition(void) const override { return m_position; }
    const XMFLOAT2& GetScale(void) const override { return m_scale; }
    float GetRotation(void) const override { return m_rot; }
    const XMFLOAT2& GetSize(void) const override { return m_size; }
    const XMFLOAT4& GetColor(void) const override { return m_color; }
	const XMFLOAT4& GetUV(void) const override { return m_uv; }
	ID3D11ShaderResourceView* GetTexture(void) const override { return m_texture; }
	bool IsVisible(void) const override { return m_visible; }
    SpriteDrawMode GetDrawMode() const { return m_drawMode; }

	void SetVertexBuffer(ID3D11Buffer* vb) { m_vertexBuffer = vb; }

    // 基本スケールを初期化（Tween用）
	void InitBaseScale(void) { m_baseScale = m_scale; }
	const XMFLOAT2& GetBaseScale(void) const { return m_baseScale; }

    void Draw(void)
    {
        if (!m_visible || !m_texture || !m_vertexBuffer) return;

        XMFLOAT2 scaledSize = {
            m_size.x * m_scale.x,
            m_size.y * m_scale.y
        };

        switch (m_drawMode)
        {
        case SpriteDrawMode::Center:
            UISpriteRenderer::SetSpriteCenter(
                m_vertexBuffer,
                m_position.x, m_position.y,
                scaledSize.x, scaledSize.y,
                m_uv.x, m_uv.y, m_uv.z, m_uv.w);
            break;

        case SpriteDrawMode::LeftTop:
            UISpriteRenderer::SetSpriteLeftTop(
                m_vertexBuffer,
                m_position.x, m_position.y,
                scaledSize.x, scaledSize.y,
                m_uv.x, m_uv.y, m_uv.z, m_uv.w,
                m_color);
            break;

        case SpriteDrawMode::CenterWithColor:
            UISpriteRenderer::SetSpriteWithColor(
                m_vertexBuffer,
                m_position.x, m_position.y,
                scaledSize.x, scaledSize.y,
                m_uv.x, m_uv.y, m_uv.z, m_uv.w,
                m_color);
            break;

        case SpriteDrawMode::CenterWithRotation:
            UISpriteRenderer::SetSpriteWithRotation(
                m_vertexBuffer,
                m_position.x, m_position.y,
                scaledSize.x, scaledSize.y,
                m_uv.x, m_uv.y, m_uv.z, m_uv.w,
                m_color,
                m_rot);
            break;
        }


        m_resourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_DIFFUSE, m_texture);

        UINT stride = sizeof(UIVertex);
        UINT offset = 0;
		ID3D11DeviceContext* ctx = m_renderer.GetDeviceContext();
        ctx->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        ctx->Draw(4, 0);
    }

private:
    XMFLOAT2 m_position = { 0, 0 };
    XMFLOAT2 m_scale{ 1.0f, 1.0f };
    XMFLOAT2 m_baseScale = { 1.0f, 1.0f }; // 基本スケール（Tween用）
    float m_rot = 0.0f;
    XMFLOAT2 m_size = { 100, 100 };
    XMFLOAT4 m_color = { 1, 1, 1, 1 };
    XMFLOAT4 m_uv = { 0, 0, 1, 1 };
    bool m_visible = true;

	SpriteDrawMode m_drawMode = SpriteDrawMode::CenterWithColor; // デフォルトは中心基準で色付き
    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11ShaderResourceView* m_texture = nullptr;
	Renderer& m_renderer = Renderer::get_instance();
	ShaderResourceBinder& m_resourceBinder = ShaderResourceBinder::get_instance();
};