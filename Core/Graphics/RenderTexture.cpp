//=============================================================================
//
// [RenderTexture.cpp]
// Author : 
// 
//=============================================================================
#include "Core/Graphics/RenderTexture.h"

void RenderTexture::Initialize(UINT width, UINT height, DXGI_FORMAT format, RenderTextureFlags flags)
{
	Release(); // 既存リソースがあれば解放する

	// 必要なBindFlagsを設定
	UINT bindFlags = 0;
	if (HasFlag(flags, RenderTextureFlags::RTV)) bindFlags |= D3D11_BIND_RENDER_TARGET;
	if (HasFlag(flags, RenderTextureFlags::SRV)) bindFlags |= D3D11_BIND_SHADER_RESOURCE;
	if (HasFlag(flags, RenderTextureFlags::UAV)) bindFlags |= D3D11_BIND_UNORDERED_ACCESS;

    // テクスチャ作成
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
	texDesc.BindFlags = bindFlags;

	D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
	UINT cpuAccessFlags = 0;
	if (HasFlag(flags, RenderTextureFlags::UAV)) 
	{
		usage = D3D11_USAGE_DEFAULT;
	}
	else if (HasFlag(flags, RenderTextureFlags::SRV)) 
	{
		usage = D3D11_USAGE_DYNAMIC;
		cpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}
	else 
	{
		usage = D3D11_USAGE_DEFAULT;
	}
	texDesc.Usage = usage;
	texDesc.CPUAccessFlags = cpuAccessFlags;

    HRESULT hr = m_renderer.GetDevice()->CreateTexture2D(&texDesc, nullptr, &m_texture);
    if (FAILED(hr)) 
    {
        OutputDebugStringA("[RenderTexture] Texture2D作成に失敗しました！\n");
        return;
    }

	// Render Target View 作成
	if (HasFlag(flags, RenderTextureFlags::RTV))
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = format;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;

		hr = m_renderer.GetDevice()->CreateRenderTargetView(m_texture, &rtvDesc, &m_rtv);
		if (FAILED(hr))
		{
			OutputDebugStringA("[RenderTexture] RTV作成に失敗しました！\n");
			return;
		}
	}

	// Shader Resource View 作成
	if (HasFlag(flags, RenderTextureFlags::SRV))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		hr = m_renderer.GetDevice()->CreateShaderResourceView(m_texture, &srvDesc, &m_srv);
		if (FAILED(hr))
		{
			OutputDebugStringA("[RenderTexture] SRV作成に失敗しました！\n");
			return;
		}
	}

	// UAV (Unordered Access View) が必要な場合はここで作成
	if (HasFlag(flags, RenderTextureFlags::UAV))
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = texDesc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		hr = m_renderer.GetDevice()->CreateUnorderedAccessView(m_texture, &uavDesc, &m_uav);
		if (FAILED(hr))
		{
			OutputDebugStringA("[RenderTexture] UAV作成に失敗しました！\n");
			return;
		}
	}
}

void RenderTexture::Release(void)
{
	SafeRelease(&m_srv);
	SafeRelease(&m_rtv);
	SafeRelease(&m_texture);
	SafeRelease(&m_uav);
}
