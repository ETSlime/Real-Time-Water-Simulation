//=============================================================================
//
//  [FluidThicknessRenderer.h]
// Author : 
//
//=============================================================================
#include "Effects/SpecialEffects/Water/FluidThicknessRenderer.h"
#include "Core/Camera.h"

FluidThicknessRenderer::FluidThicknessRenderer() {}

void FluidThicknessRenderer::Initialize(int width, int height)
{
    m_width = width;
    m_height = height;

    m_thicknessRT.Initialize(width, height, DXGI_FORMAT_R32_UINT, RenderTextureFlags::SRV | RenderTextureFlags::UAV);
    m_shaderSet = ShaderManager::get_instance().GetComputeShader(ParticleComputeGroup::WaterFluid, ComputePassType::Particle_Thickness);

	// 定数バッファの作成
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(CBFluidThickness);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = 0;

    HRESULT hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_cbFluidThickness);
    if (FAILED(hr))
    {
        OutputDebugStringA("[FluidThicknessRenderer] Constant Buffer 作成失敗にゃ…！\n");
    }
}

void FluidThicknessRenderer::Release(void)
{
    m_thicknessRT.Release();
	SafeRelease(&m_cbFluidThickness);
}

void FluidThicknessRenderer::Resize(int newWidth, int newHeight)
{
    if (m_width == newWidth && m_height == newHeight) return;
    Release();
    Initialize(newWidth, newHeight);
}

void FluidThicknessRenderer::UpdateCB(void)
{
    m_cbData.thicknessPerParticle = m_thicknessPerParticle;
    m_cbData.screenSize = XMFLOAT2(static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT));
	m_cbData.padding = 0.0f; // パディングを追加して構造体のサイズを16バイト境界に揃える
    m_context->UpdateSubresource(m_cbFluidThickness, 0, nullptr, &m_cbData, 0, 0);
}

void FluidThicknessRenderer::Draw(UINT count)
{
    if (count == 0) return;

	UpdateCB();

	ID3D11UnorderedAccessView* uav = m_thicknessRT.GetUAV();

    // UAV クリア（厚度初期化）
    UINT clearValue[4] = { 0, 0, 0, 0 };
    m_context->ClearUnorderedAccessViewUint(uav, clearValue);

    // ピクセルシェーダーの SRV をクリア
    ID3D11ShaderResourceView* nullSRV = nullptr;
	m_shaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_SRV_FLUID_THICKNESS, nullSRV); 

    // UAV をバインド
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_FLUID_THICKNESS, uav);

    // 定数バッファをバインド
    m_shaderResourceBinder.BindConstantBuffer(ShaderStage::CS, SLOT_CB_FLUID_THICKNESS, m_cbFluidThickness);

    // シェーダー設定
    m_context->CSSetShader(m_shaderSet.cs, nullptr, 0);

    // スレッド数に応じて Dispatch（1D 配置）
    UINT threadGroupCount = GET_THREAD_GROUP_COUNT(count, THREAD_COUNT);
    m_context->Dispatch(threadGroupCount, 1, 1);

    // リソースを解放（次のパスに影響を与えないように）
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    m_shaderResourceBinder.BindUnorderedAccessView(SLOT_UAV_FLUID_THICKNESS, nullUAV);
}

void FluidThicknessRenderer::ComputeParticleScreenRadius(float radius)
{
	XMFLOAT3 cameraPos = Camera::get_instance().GetPosition();
	XMFLOAT3 cameraForward = Camera::get_instance().GetForward();
    XMFLOAT3 cameraRight = Camera::get_instance().GetRight();
    XMFLOAT3 cameraUp = Camera::get_instance().GetUp();
	XMMATRIX viewProj = Camera::get_instance().GetViewProjMtx();

    // 視点から見て中距離にある粒子位置を仮定
    XMFLOAT3 worldCenter = {
        cameraPos.x + cameraForward.x * 5.0f,
        cameraPos.y + cameraForward.y * 5.0f,
        cameraPos.z + cameraForward.z * 5.0f
    };

    // billboard の右上方向へ radius 分ずらした corner
    XMFLOAT3 offset = {
        worldCenter.x + (cameraRight.x + cameraUp.x) * radius,
        worldCenter.y + (cameraRight.y + cameraUp.y) * radius,
        worldCenter.z + (cameraRight.z + cameraUp.z) * radius
    };

    // クリップ空間へ変換
    XMVECTOR clipA = XMVector4Transform(XMLoadFloat3(&worldCenter), viewProj);
    XMVECTOR clipB = XMVector4Transform(XMLoadFloat3(&offset), viewProj);

    // NDCへ
    XMFLOAT4 a, b;
    XMStoreFloat4(&a, clipA);
    XMStoreFloat4(&b, clipB);

    // NDC座標に変換
    XMFLOAT2 ndcA = XMFLOAT2(a.x / a.w, a.y / a.w);
    XMFLOAT2 ndcB = XMFLOAT2(b.x / b.w, b.y / b.w);

    // NDC [-1,1] を pixel 座標に変換するために float2 に変換
    XMVECTOR ndcAVec = XMLoadFloat2(&ndcA);
    XMVECTOR ndcBVec = XMLoadFloat2(&ndcB);

    // スクリーンサイズ
    XMVECTOR screenSize = XMVectorSet(static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f);
    XMVECTOR scale = XMVectorReplicate(0.5f);

    // NDC [-1,1] → 実ピクセル座標へ変換
    XMVECTOR ndcOffsetA = XMVectorAdd(ndcAVec, XMVectorReplicate(1.0f));
    XMVECTOR ndcOffsetB = XMVectorAdd(ndcBVec, XMVectorReplicate(1.0f));
    XMVECTOR half = XMVectorReplicate(0.5f);

	// NDC [-1,1] → [0,1] へ変換
    XMVECTOR pixelAVec = XMVectorMultiply(ndcOffsetA, half);
    XMVECTOR pixelBVec = XMVectorMultiply(ndcOffsetB, half);

	// [0,1] → [0, screen] へ変換
    pixelAVec = XMVectorMultiply(pixelAVec, screenSize);
    pixelBVec = XMVectorMultiply(pixelBVec, screenSize);

    // 差分を計算して距離を取得
    XMVECTOR deltaVec = XMVectorSubtract(pixelBVec, pixelAVec);
    XMVECTOR lengthVec = XMVector2Length(deltaVec);

}


