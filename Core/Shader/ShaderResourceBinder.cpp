//=============================================================================
//
// ShaderResourceBinder処理 [ShaderResourceBinder.h]
// Author : 
//
//=============================================================================
#include "Core/Shader/ShaderResourceBinder.h"

void ShaderResourceBinder::Initialize(ID3D11DeviceContext* context)
{
	m_context = context;
}

// ===== 各種リセット（毎フレームリセット推奨）=====
void ShaderResourceBinder::Reset(void)
{
    memset(m_currentCB_VS, 0, sizeof(m_currentCB_VS));
    memset(m_currentCB_PS, 0, sizeof(m_currentCB_PS));
    memset(m_currentCB_GS, 0, sizeof(m_currentCB_GS));
    memset(m_currentCB_GS, 0, sizeof(m_currentCB_CS));

    memset(m_currentSRV_VS, 0, sizeof(m_currentSRV_VS));
    memset(m_currentSRV_PS, 0, sizeof(m_currentSRV_PS));
    memset(m_currentSRV_GS, 0, sizeof(m_currentSRV_GS));
    memset(m_currentSRV_GS, 0, sizeof(m_currentSRV_CS));

    memset(m_currentSampler_VS, 0, sizeof(m_currentSampler_VS));
    memset(m_currentSampler_PS, 0, sizeof(m_currentSampler_PS));
    memset(m_currentSampler_GS, 0, sizeof(m_currentSampler_GS));
    memset(m_currentSampler_GS, 0, sizeof(m_currentSampler_CS));

    memset(m_currentUAV_CS, 0, sizeof(m_currentUAV_CS));

    memset(m_dirtySRV_VS, true, sizeof(m_dirtySRV_VS));
    memset(m_dirtySRV_PS, true, sizeof(m_dirtySRV_PS));
    memset(m_dirtySRV_GS, true, sizeof(m_dirtySRV_GS));
    memset(m_dirtySRV_CS, true, sizeof(m_dirtySRV_CS));
    memset(m_dirtyUAV_CS, true, sizeof(m_dirtyUAV_CS));
}

// ===== 定数バッファのバインド =====
void ShaderResourceBinder::BindConstantBuffer(ShaderStage stage, UINT slot, ID3D11Buffer* buffer)
{
    ID3D11Buffer** cbArray = GetCBSlotArray(stage);
    if (cbArray[slot] != buffer)
    {
        cbArray[slot] = buffer;
        switch (stage)
        {
        case ShaderStage::VS: m_context->VSSetConstantBuffers(slot, 1, &buffer); break;
        case ShaderStage::PS: m_context->PSSetConstantBuffers(slot, 1, &buffer); break;
        case ShaderStage::GS: m_context->GSSetConstantBuffers(slot, 1, &buffer); break;
        case ShaderStage::CS: m_context->CSSetConstantBuffers(slot, 1, &buffer); break;
        }
    }
}

// ===== SRVのバインド =====
void ShaderResourceBinder::BindShaderResource(ShaderStage stage, UINT slot, ID3D11ShaderResourceView* srv)
{
    ID3D11ShaderResourceView** srvArray = GetSRVSlotArray(stage);
    bool* dirtyArray = GetSRVDirtyArray(stage);

    // GPU上のバインド状態が異なる、またはダーティフラグが立っている場合に再バインドを行う
    if (srvArray[slot] != srv || dirtyArray[slot])
    {
        srvArray[slot] = srv;
        dirtyArray[slot] = false; // ダーティフラグをクリア
        switch (stage)
        {
        case ShaderStage::VS: m_context->VSSetShaderResources(slot, 1, &srv); break;
        case ShaderStage::PS: m_context->PSSetShaderResources(slot, 1, &srv); break;
        case ShaderStage::GS: m_context->GSSetShaderResources(slot, 1, &srv); break;
        case ShaderStage::CS: m_context->CSSetShaderResources(slot, 1, &srv); break;
        }
    }
}

// ===== サンプラーのバインド =====
void ShaderResourceBinder::BindSampler(ShaderStage stage, UINT slot, ID3D11SamplerState* sampler)
{
    ID3D11SamplerState** samplerArray = GetSamplerSlotArray(stage);
    if (samplerArray[slot] != sampler)
    {
        samplerArray[slot] = sampler;
        switch (stage)
        {
        case ShaderStage::VS: m_context->VSSetSamplers(slot, 1, &sampler); break;
        case ShaderStage::PS: m_context->PSSetSamplers(slot, 1, &sampler); break;
        case ShaderStage::GS: m_context->GSSetSamplers(slot, 1, &sampler); break;
        case ShaderStage::CS: m_context->CSSetSamplers(slot, 1, &sampler); break;
        }
    }
}

// ===== 定数バッファのバッチバインド =====
void ShaderResourceBinder::BindConstantBuffers(ShaderStage stage, UINT startSlot, UINT numBuffers, ID3D11Buffer* const* buffers)
{
    ID3D11Buffer** cbArray = GetCBSlotArray(stage);
    bool changed = false;

    for (UINT i = 0; i < numBuffers; ++i)
    {
        if (cbArray[startSlot + i] != buffers[i])
        {
            cbArray[startSlot + i] = buffers[i];
            changed = true;
        }
    }

    if (changed)
    {
        switch (stage)
        {
        case ShaderStage::VS: m_context->VSSetConstantBuffers(startSlot, numBuffers, buffers); break;
        case ShaderStage::PS: m_context->PSSetConstantBuffers(startSlot, numBuffers, buffers); break;
        case ShaderStage::GS: m_context->GSSetConstantBuffers(startSlot, numBuffers, buffers); break;
        case ShaderStage::CS: m_context->CSSetConstantBuffers(startSlot, numBuffers, buffers); break;
        }
    }
}

// ===== SRVのバッチバインド =====
void ShaderResourceBinder::BindShaderResources(ShaderStage stage, UINT startSlot, UINT numSRVs, ID3D11ShaderResourceView* const* srvs)
{
    ID3D11ShaderResourceView** srvArray = GetSRVSlotArray(stage);
    bool* dirtyArray = GetSRVDirtyArray(stage);

    bool changed = false;

    for (UINT i = 0; i < numSRVs; ++i)
    {
        if (dirtyArray[startSlot + i] || srvArray[startSlot + i] != srvs[i])
        {
            srvArray[startSlot + i] = srvs[i];
            dirtyArray[startSlot + i] = false; // ダーティフラグをクリア
            changed = true;
        }
    }

    if (changed)
    {
        switch (stage)
        {
        case ShaderStage::VS: m_context->VSSetShaderResources(startSlot, numSRVs, srvs); break;
        case ShaderStage::PS: m_context->PSSetShaderResources(startSlot, numSRVs, srvs); break;
        case ShaderStage::GS: m_context->GSSetShaderResources(startSlot, numSRVs, srvs); break;
        case ShaderStage::CS: m_context->CSSetShaderResources(startSlot, numSRVs, srvs); break;
        }
    }
}

// ===== サンプラーのバッチバインド =====
void ShaderResourceBinder::BindSamplers(ShaderStage stage, UINT startSlot, UINT numSamplers, ID3D11SamplerState* const* samplers)
{
    ID3D11SamplerState** samplerArray = GetSamplerSlotArray(stage);
    bool changed = false;

    for (UINT i = 0; i < numSamplers; ++i)
    {
        if (samplerArray[startSlot + i] != samplers[i])
        {
            samplerArray[startSlot + i] = samplers[i];
            changed = true;
        }
    }

    if (changed)
    {
        switch (stage)
        {
        case ShaderStage::VS: m_context->VSSetSamplers(startSlot, numSamplers, samplers); break;
        case ShaderStage::PS: m_context->PSSetSamplers(startSlot, numSamplers, samplers); break;
        case ShaderStage::GS: m_context->GSSetSamplers(startSlot, numSamplers, samplers); break;
        case ShaderStage::CS: m_context->CSSetSamplers(startSlot, numSamplers, samplers); break;
        }
    }
}

void ShaderResourceBinder::BindUnorderedAccessView(UINT slot, ID3D11UnorderedAccessView* uav)
{
    if (m_currentUAV_CS[slot] != uav || m_dirtyUAV_CS[slot])
    {
        m_currentUAV_CS[slot] = uav;
        m_dirtyUAV_CS[slot] = false; // ダーティフラグをクリア

        UINT dummyInitialCount = 0;
        m_context->CSSetUnorderedAccessViews(slot, 1, &uav, &dummyInitialCount);
    }
}

void ShaderResourceBinder::BindUnorderedAccessViews(UINT startSlot, UINT numUAVs, ID3D11UnorderedAccessView* const* uavs)
{
    bool changed = false;
    for (UINT i = 0; i < numUAVs; ++i)
    {
        UINT slot = startSlot + i;

        if (m_currentUAV_CS[slot] != uavs[i] || m_dirtyUAV_CS[slot])
        {
            m_currentUAV_CS[slot] = uavs[i];
            m_dirtyUAV_CS[slot] = false; // ダーティフラグをクリア
            changed = true;
        }
    }

    if (changed)
    {
        UINT dummyInitialCount = 0;
        m_context->CSSetUnorderedAccessViews(startSlot, numUAVs, uavs, &dummyInitialCount);
    }
}

void ShaderResourceBinder::BindCounterUnorderedAccessView(UINT slot, ID3D11UnorderedAccessView* uav, UINT initialCount)
{
    if (m_currentUAV_CS[slot] != uav || m_dirtyUAV_CS[slot])
    {
        m_currentUAV_CS[slot] = uav;
        m_dirtyUAV_CS[slot] = false;
        m_context->CSSetUnorderedAccessViews(slot, 1, &uav, &initialCount);
    }
}


ID3D11Buffer** ShaderResourceBinder::GetCBSlotArray(ShaderStage stage)
{
    switch (stage)
    {
    case ShaderStage::VS: return m_currentCB_VS;
    case ShaderStage::PS: return m_currentCB_PS;
    case ShaderStage::GS: return m_currentCB_GS;
    case ShaderStage::CS: return m_currentCB_CS;
    default:              return nullptr;
    }
}

ID3D11ShaderResourceView** ShaderResourceBinder::GetSRVSlotArray(ShaderStage stage)
{
    switch (stage)
    {
    case ShaderStage::VS: return m_currentSRV_VS;
    case ShaderStage::PS: return m_currentSRV_PS;
    case ShaderStage::GS: return m_currentSRV_GS;
    case ShaderStage::CS: return m_currentSRV_CS;
    default:              return nullptr;
    }
}

ID3D11SamplerState** ShaderResourceBinder::GetSamplerSlotArray(ShaderStage stage)
{
    switch (stage)
    {
    case ShaderStage::VS: return m_currentSampler_VS;
    case ShaderStage::PS: return m_currentSampler_PS;
    case ShaderStage::GS: return m_currentSampler_GS;
    case ShaderStage::CS: return m_currentSampler_CS;
    default:              return nullptr;
    }
}

bool* ShaderResourceBinder::GetSRVDirtyArray(ShaderStage stage)
{
    switch (stage)
    {
    case ShaderStage::VS: return m_dirtySRV_VS;
    case ShaderStage::PS: return m_dirtySRV_PS;
    case ShaderStage::GS: return m_dirtySRV_GS;
    case ShaderStage::CS: return m_dirtySRV_CS;
    default:              return nullptr;
    }
}
