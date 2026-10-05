#pragma once
//=============================================================================
//
// [RenderTexture.h]
// Author : 
// 
//=============================================================================
#include "Core/Graphics/Renderer.h"

enum class RenderTextureFlags : UINT
{
    None = 0,
    RTV = 1 << 0,
    SRV = 1 << 1,
    UAV = 1 << 2,
};
inline RenderTextureFlags operator|(RenderTextureFlags a, RenderTextureFlags b)
{
    return static_cast<RenderTextureFlags>(static_cast<UINT>(a) | static_cast<UINT>(b));
}
inline bool HasFlag(RenderTextureFlags flags, RenderTextureFlags flag)
{
    return (static_cast<UINT>(flags) & static_cast<UINT>(flag)) != 0;
}


class RenderTexture
{
public:
    RenderTexture() = default;
    ~RenderTexture() = default;

    // RenderTexture の初期化処理（RTV + SRV 作成）
    void Initialize(UINT width, UINT height, DXGI_FORMAT format, RenderTextureFlags flags);
	// RenderTexture の解放処理（RTV + SRV + テクスチャの解放）
    void Release(void);

    ID3D11RenderTargetView* GetRTV(void) const { return m_rtv; }
    ID3D11ShaderResourceView* GetSRV(void) const { return m_srv; }
    ID3D11Texture2D* GetTexture(void) const { return m_texture; }
    ID3D11UnorderedAccessView* GetUAV(void) const { return m_uav; }

private:
    ID3D11Texture2D* m_texture = nullptr;               // 実際のテクスチャ
    ID3D11RenderTargetView* m_rtv = nullptr;            // 書き込み用ビュー
    ID3D11ShaderResourceView* m_srv = nullptr;          // 読み込み用ビュー
	ID3D11UnorderedAccessView* m_uav = nullptr;         // アンオーダードアクセスビュー（必要に応じて）
	Renderer& m_renderer = Renderer::get_instance(); // レンダラーインスタンス
};