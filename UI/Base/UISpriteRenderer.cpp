//=============================================================================
//
// UISpriteRenderer処理 [UISpriteRenderer.cpp]
// Author : 
//
//=============================================================================
#include "UI/Base/UISpriteRenderer.h"

void UISpriteRenderer::SetSpriteCenter(ID3D11Buffer* buf, float X, float Y, float Width, float Height,
    float U, float V, float UW, float VH)
{
    D3D11_MAPPED_SUBRESOURCE msr;
    Renderer::get_instance().GetDeviceContext()->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

    UIVertex* vertex = reinterpret_cast<UIVertex*>(msr.pData);

    float hw = Width * 0.5f;
    float hh = Height * 0.5f;

    // 中心を基準に頂点を設定
    vertex[0].Position = XMFLOAT2(X - hw, Y - hh); // 左上
    vertex[1].Position = XMFLOAT2(X + hw, Y - hh); // 右上
    vertex[2].Position = XMFLOAT2(X - hw, Y + hh); // 左下
    vertex[3].Position = XMFLOAT2(X + hw, Y + hh); // 右下

    vertex[0].TexCoord = XMFLOAT2(U, V);
    vertex[1].TexCoord = XMFLOAT2(U + UW, V);
    vertex[2].TexCoord = XMFLOAT2(U, V + VH);
    vertex[3].TexCoord = XMFLOAT2(U + UW, V + VH);

    Renderer::get_instance().GetDeviceContext()->Unmap(buf, 0);
}

void UISpriteRenderer::SetSpriteLeftTop(ID3D11Buffer* buf, float X, float Y, float Width, float Height,
    float U, float V, float UW, float VH, XMFLOAT4 color)
{
    HRESULT hr = S_OK;
    D3D11_MAPPED_SUBRESOURCE msr;
    hr = Renderer::get_instance().GetDeviceContext()->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
    assert(SUCCEEDED(hr));

    UIVertex* vertex = reinterpret_cast<UIVertex*>(msr.pData);

    // 左上を基準に頂点を設定
    vertex[0].Position = XMFLOAT2(X, Y);                   // 左上
    vertex[1].Position = XMFLOAT2(X + Width, Y);           // 右上
    vertex[2].Position = XMFLOAT2(X, Y + Height);          // 左下
    vertex[3].Position = XMFLOAT2(X + Width, Y + Height);  // 右下

    vertex[0].TexCoord = XMFLOAT2(U, V);
    vertex[1].TexCoord = XMFLOAT2(U + UW, V);
    vertex[2].TexCoord = XMFLOAT2(U, V + VH);
    vertex[3].TexCoord = XMFLOAT2(U + UW, V + VH);

    // カラー設定
    vertex[0].Color = color;
    vertex[1].Color = color;
    vertex[2].Color = color;
    vertex[3].Color = color;

    Renderer::get_instance().GetDeviceContext()->Unmap(buf, 0);
}

void UISpriteRenderer::SetSpriteWithColor(ID3D11Buffer* buf, float X, float Y, float Width, float Height,
    float U, float V, float UW, float VH, XMFLOAT4 color)
{
    D3D11_MAPPED_SUBRESOURCE msr;
    Renderer::get_instance().GetDeviceContext()->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

    UIVertex* vertex = reinterpret_cast<UIVertex*>(msr.pData);

    float hw = Width * 0.5f;
    float hh = Height * 0.5f;

    // 中心基準で頂点設定（左上、右上、左下、右下）
    vertex[0].Position = XMFLOAT2(X - hw, Y - hh);
    vertex[1].Position = XMFLOAT2(X + hw, Y - hh);
    vertex[2].Position = XMFLOAT2(X - hw, Y + hh);
    vertex[3].Position = XMFLOAT2(X + hw, Y + hh);

    vertex[0].TexCoord = XMFLOAT2(U, V);
    vertex[1].TexCoord = XMFLOAT2(U + UW, V);
    vertex[2].TexCoord = XMFLOAT2(U, V + VH);
    vertex[3].TexCoord = XMFLOAT2(U + UW, V + VH);

    // カラー設定
    vertex[0].Color = color;
    vertex[1].Color = color;
    vertex[2].Color = color;
    vertex[3].Color = color;

    Renderer::get_instance().GetDeviceContext()->Unmap(buf, 0);
}

void UISpriteRenderer::SetSpriteWithRotation(
    ID3D11Buffer* buf,
    float X, float Y,
    float Width, float Height,
    float U, float V, float UW, float VH,
    XMFLOAT4 color, float Rot)
{
    D3D11_MAPPED_SUBRESOURCE msr;
    Renderer::get_instance().GetDeviceContext()->Map(
        buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

    UIVertex* vertex = reinterpret_cast<UIVertex*>(msr.pData);

    float hw = Width * 0.5f;
    float hh = Height * 0.5f;

    float cosR = cosf(Rot);
    float sinR = sinf(Rot);

    XMFLOAT2 localPos[4] = {
        {-hw, -hh}, { hw, -hh},
        {-hw,  hh}, { hw,  hh}
    };

    for (int i = 0; i < 4; i++) {
        float x = localPos[i].x * cosR - localPos[i].y * sinR;
        float y = localPos[i].x * sinR + localPos[i].y * cosR;
        vertex[i].Position = XMFLOAT2(X + x, Y + y);

        // ★カラーを必ず設定する
        vertex[i].Color = color;
    }

    vertex[0].TexCoord = XMFLOAT2(U, V);
    vertex[1].TexCoord = XMFLOAT2(U + UW, V);
    vertex[2].TexCoord = XMFLOAT2(U, V + VH);
    vertex[3].TexCoord = XMFLOAT2(U + UW, V + VH);

    Renderer::get_instance().GetDeviceContext()->Unmap(buf, 0);
}


void UISpriteRenderer::SetSpriteParallelogram(
    ID3D11Buffer* buf,
    float x, float y,
    float width, float height,
    float skewX,
    float U, float V, float UW, float VH,
    XMFLOAT4 color)
{
    D3D11_MAPPED_SUBRESOURCE msr;
    Renderer::get_instance().GetDeviceContext()->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

    UIVertex* vertex = reinterpret_cast<UIVertex*>(msr.pData);

    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;

    vertex[0].Position = XMFLOAT2(x - halfWidth + skewX, y - halfHeight);
    vertex[1].Position = XMFLOAT2(x + halfWidth + skewX, y - halfHeight);
    vertex[2].Position = XMFLOAT2(x - halfWidth, y + halfHeight);
    vertex[3].Position = XMFLOAT2(x + halfWidth, y + halfHeight);

    // UV coordinates
    vertex[0].TexCoord = XMFLOAT2(U, V);
    vertex[1].TexCoord = XMFLOAT2(U + UW, V);
    vertex[2].TexCoord = XMFLOAT2(U, V + VH);
    vertex[3].TexCoord = XMFLOAT2(U + UW, V + VH);

    vertex[0].Color = color;
    vertex[1].Color = color;
    vertex[2].Color = color;
    vertex[3].Color = color;

    Renderer::get_instance().GetDeviceContext()->Unmap(buf, 0);
}