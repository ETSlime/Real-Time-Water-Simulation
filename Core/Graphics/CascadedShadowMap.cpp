//=============================================================================
//
// CascadedShadowMap処理 [CascadedShadowMap.cpp]
// Author : 
//
//=============================================================================
#include "Core/Graphics/CascadedShadowMap.h"
#include "Utility/Debug/Debugproc.h"

CascadedShadowMap::CascadedShadowMap() {}
CascadedShadowMap::~CascadedShadowMap() { Shutdown(); }

void CascadedShadowMap::Initialize(int shadowMapSize, int numCascades) 
{
    m_device = Renderer::get_instance().GetDevice();
    m_context = Renderer::get_instance().GetDeviceContext();

    assert(numCascades <= MAX_CASCADES);

    m_config.shadowMapSize = shadowMapSize;
    m_config.numCascades = max(1, numCascades); // 1以上に制限

    m_shadowViewport = {};
    m_shadowViewport.TopLeftX = 0.0f;
    m_shadowViewport.TopLeftY = 0.0f;
    m_shadowViewport.Width = static_cast<float>(shadowMapSize);
    m_shadowViewport.Height = static_cast<float>(shadowMapSize);
    m_shadowViewport.MinDepth = 0.0f;
    m_shadowViewport.MaxDepth = 1.0f;

    for (int lightIdx = 0; lightIdx < SHADOW_CASTING_LIGHT_MAX; ++lightIdx)
    {
        for (int cascadeIdx = 0; cascadeIdx < m_config.numCascades; ++cascadeIdx)
        {
            // テクスチャーの設定
            D3D11_TEXTURE2D_DESC texDesc = {};
            texDesc.Width = shadowMapSize;
            texDesc.Height = shadowMapSize;
            texDesc.MipLevels = 1;
            texDesc.ArraySize = 1;
            texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
            texDesc.SampleDesc.Count = 1;
            texDesc.Usage = D3D11_USAGE_DEFAULT;
            texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

            HRESULT hr = m_device->CreateTexture2D(&texDesc, nullptr, &m_shadowMaps[lightIdx][cascadeIdx]);
            if (FAILED(hr)) continue; // エラー処理

            // DSV 設定
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;

            hr = m_device->CreateDepthStencilView(m_shadowMaps[lightIdx][cascadeIdx], &dsvDesc, &m_dsvs[lightIdx][cascadeIdx]);
            if (FAILED(hr)) continue;

            // SRV 設定
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;

            hr = m_device->CreateShaderResourceView(m_shadowMaps[lightIdx][cascadeIdx], &srvDesc, &m_srvs[lightIdx][cascadeIdx]);
            if (FAILED(hr)) continue;
        }
    }

    InitCascadeCBuffer();

#ifdef _DEBUG
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = static_cast<UINT>(shadowMapSize);
    texDesc.Height = static_cast<UINT>(shadowMapSize);
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

    HRESULT hr = m_device->CreateTexture2D(&texDesc, nullptr, &m_dummyRTTexture);
    if (SUCCEEDED(hr)) {
        hr = m_device->CreateRenderTargetView(m_dummyRTTexture, nullptr, &m_dummyRTV);
        m_dummyRTTexture->Release();
    }
#endif // _DEBUG


#if DEBUG_CASCADE_BOUND
    for (int cascadeIndex = 0; cascadeIndex < m_config.numCascades; ++cascadeIndex)
    {
        char name[DEBUG_BOUNDING_BOX_NAME_LENGTH] = "Cascade BoundingBox: ";
        char idx[8] = {};
        sprintf(idx, "%d", cascadeIndex);
        strcat(name, idx);
        m_debugBoundingBoxRenderer[cascadeIndex].Initialize(name);
    }
#endif // _DEBUG

}

void CascadedShadowMap::Shutdown() 
{
    for (int lightIdx = 0; lightIdx < SHADOW_CASTING_LIGHT_MAX; ++lightIdx)
    {
        for (int cascadeIdx = 0; cascadeIdx < m_config.numCascades; ++cascadeIdx)
        {
            SafeRelease(&m_srvs[lightIdx][cascadeIdx]);
            SafeRelease(&m_dsvs[lightIdx][cascadeIdx]);
            SafeRelease(&m_shadowMaps[lightIdx][cascadeIdx]);
        }
    }

    SafeRelease(&m_dummyRTV);
}

void CascadedShadowMap::SetViewport(void) 
{
    m_context->RSSetViewports(1, &m_shadowViewport);
}

void CascadedShadowMap::BindShadowSRVsToPixelShader(int lightIndex, int startSlot)
{
    m_ShaderResourceBinder.BindShaderResources(ShaderStage::PS, startSlot, m_config.numCascades, m_srvs[lightIndex]);
}

ID3D11ShaderResourceView* CascadedShadowMap::GetSRV(int lightIndex, int cascadeIndex) const
{
    assert(lightIndex >= 0 && lightIndex < SHADOW_CASTING_LIGHT_MAX);
    assert(cascadeIndex >= 0 && cascadeIndex < MAX_CASCADES);
    return m_srvs[lightIndex][cascadeIndex];
}

void CascadedShadowMap::RenderImGui(void)
{
    if (ImGui::CollapsingHeader("Cascaded Shadow Settings"))
    {
        ImGui::SliderInt("Cascade Count", &m_config.numCascades, 1, MAX_CASCADES);
        ImGui::SliderFloat("Max Shadow Distance", &m_config.maxShadowDistance, MIN_SHADOW_DISTANCE, MAX_SHADOW_DISTANCE, "%.1f");
        ImGui::Checkbox("Use Manual Splits", &m_config.useManualSplits);

        if (m_config.useManualSplits)
        {
            ImGui::Text("Manual Cascade Splits:");
            for (int i = 0; i < m_config.numCascades; ++i)
            {
                char label[32];
                sprintf_s(label, "Split %d", i);
                ImGui::SliderFloat(label, &m_config.manualSplits[i], 0.0f, 1.0f);
            }

            for (int i = 1; i < m_config.numCascades; ++i)
            {
                if (m_config.manualSplits[i] < m_config.manualSplits[i - 1])
                    m_config.manualSplits[i] = m_config.manualSplits[i - 1];
            }
        }
        else
        {
            ImGui::SliderFloat("Lambda (auto split)", &m_config.lambda, 0.0f, 1.0f);
        }

        ImGui::Checkbox("Enable Texel Snap", &m_config.enableTexelSnap);

        const char* strategyNames[] = {
            "FitBoundingSphere",
            "FitBoundingBox",
            "PaddingBuffer"
        };

        int strategyIndex = static_cast<int>(m_config.radiusStrategy);
        if (ImGui::Combo("Radius Strategy", &strategyIndex, strategyNames, IM_ARRAYSIZE(strategyNames)))
        {
            m_config.radiusStrategy = static_cast<ShadowRadiusStrategy>(strategyIndex);
        }

        if (m_config.radiusStrategy == ShadowRadiusStrategy::PaddingBuffer)
        {
            for (int i = 0; i < m_config.numCascades; ++i)
            {
                char label[32];
                sprintf_s(label, "Padding Layer %d", i);
                ImGui::SliderFloat("Padding", &m_config.padding[i], 0.0f, 100.0f, "%.1f");
            }
        }

        if (ImGui::CollapsingHeader("Cascade Debug Draw"))
        {
            ImGui::Checkbox("Show Cascade 0", &m_showCascadeBox[0]);
            ImGui::Checkbox("Show Cascade 1", &m_showCascadeBox[1]);
            ImGui::Checkbox("Show Cascade 2", &m_showCascadeBox[2]);
            ImGui::Checkbox("Show Cascade 3", &m_showCascadeBox[3]);
        }

        ImGui::Separator();

        ImGui::Text("Cascade Statistics:");
        for (int i = 0; i < m_config.numCascades; ++i)
        {
            const auto& cascade = m_cascadeData[i];
            float depth = cascade.splitDepth;

            char label[64];
            sprintf_s(label, "Cascade %d: Depth %.2f", i, depth);
            ImGui::Text("%s", label);

            ImGui::Text("Objects: %d", m_cascadeObjectCount[i]);

            XMFLOAT3 center;
            XMStoreFloat3(&center, m_tempFrustumCenter[i]);
            ImGui::Text("Cascade[%d] Center: (%.2f, %.2f, %.2f)", i, center.x, center.y, center.z);
        }




        //if (m_config.useManualSplits)
        //{
        //    for (int i = 0; i < m_config.numCascades; ++i)
        //    {
        //        char* label = "Split[" + std::to_string(i) + "]";
        //        ImGui::SliderFloat(label.c_str(), &m_config.manualSplits[i], 0.0f, 1.0f);
        //    }
        //}
    }
}

void CascadedShadowMap::CollectFromScene_NoCulling(int cascadeIndex, const SimpleArray<IGameObject*>& objects)
{
    m_perCascadeCollector[cascadeIndex].Clear();
    m_cascadeObjectCount[cascadeIndex] = 0;

    for (auto* obj : objects)
    {
        if (!obj->GetUse() || !obj->GetCastShadow())
            continue;

        obj->CollectShadowMesh(m_perCascadeCollector[cascadeIndex]);
        m_cascadeObjectCount[cascadeIndex]++;
    }
}

void CascadedShadowMap::CollectFromScene(int cascadeIndex, const SimpleArray<IGameObject*>& objects)
{
    m_perCascadeCollector[cascadeIndex].Clear();
    m_cascadeObjectCount[cascadeIndex] = 0;

    for (auto* obj : objects)
    {
        if (!obj->GetLoad() || !obj->GetUse() || !obj->GetCastShadow()) continue;

		if (obj->GetModelType() == ModelType::Instanced)
		{
			// Instancedモデルの場合、インスタンスの可視性を確認
			GameObject<ModelInstance>* instancedObj = dynamic_cast<GameObject<ModelInstance>*>(obj);
			if (!instancedObj) continue; // dynamic_cast失敗時はスキップ

            InstanceModelAttribute& attribute = instancedObj->GetInstancedAttribute();
			if (!attribute.initialized) continue; // 初期化されていない場合はスキップ

			// インスタンスの可視性を確認
			CullVisibleInstancesForShadow(attribute, cascadeIndex);
			// 可視インスタンスが存在する場合、カウントを更新
            m_cascadeObjectCount[cascadeIndex] += attribute.visibleInstanceDataArray->getSize();
		}
        else
        {
            // StaticまたはSkinnedモデルの場合、AABBの可視性を確認
            const BOUNDING_BOX& worldAABB = obj->GetBoundingBoxWorld();
            if (!IsAABBInsideLightFrustum(worldAABB, cascadeIndex))
                continue;

			m_cascadeObjectCount[cascadeIndex]++; // 可視オブジェクトのカウントを更新
        }

        obj->CollectShadowMesh(m_perCascadeCollector[cascadeIndex]);
    }
}

void CascadedShadowMap::CullVisibleInstancesForShadow(InstanceModelAttribute& attribute, int cascadeIndex)
{

    // 削除前に配列をリセット（size=0, capacity保持）
    if (attribute.visibleInstanceDataArray)
        attribute.visibleInstanceDataArray->clear();
    else
        return; // visibleInstancesがnullptrの場合、何もしない

    const XMMATRIX& lightViewProjLocal = m_cascadePrivateData[cascadeIndex].lightViewProjLocal;
    const XMVECTOR& frustumCenter = m_cascadePrivateData[cascadeIndex].viewFrustumCenter;

    for (UINT i = 0; i < attribute.instanceCount; ++i)
    {
        //if (attribute.visibleInstanceDraw && (*attribute.visibleInstanceDraw)[i]) continue;

        const InstanceData& inst = attribute.instanceData[i];
        XMVECTOR worldPos = XMLoadFloat3(&inst.OffsetPosition);

		if (IsAABBInsideLightFrustum((*attribute.colliderArray)[i].aabb, cascadeIndex)) // AABBがライトの視野に入っている場合
        {
            attribute.visibleInstanceDataArray->push_back(inst);
            (*attribute.visibleInstanceDraw)[i] = true; // 記録
        }
    }

    // 可視インスタンスが存在する場合、GPUにアップロード
    const UINT visibleCount = attribute.visibleInstanceDataArray->getSize();
    if (visibleCount > 0)
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(m_context->Map(attribute.instanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            memcpy(mapped.pData, attribute.visibleInstanceDataArray->data(), sizeof(InstanceData) * visibleCount);
            m_context->Unmap(attribute.instanceBuffer, 0);
        }
    }
}

ID3D11ShaderResourceView* CascadedShadowMap::GetShadowSRV(int lightIndex, int cascadeIndex) const
{
    return m_srvs[lightIndex][cascadeIndex];
}

ID3D11DepthStencilView* CascadedShadowMap::GetDSV(int lightIndex, int cascadeIndex) const
{
    return m_dsvs[lightIndex][cascadeIndex];
}

ID3D11ShaderResourceView** CascadedShadowMap::GetAllShadowSRVs(int lightIndex)
{
    return m_srvs[lightIndex];
}

void CascadedShadowMap::UnbindShadowSRVs(void)
{
    ID3D11ShaderResourceView* nullSRVs[MAX_CASCADES] = { nullptr, nullptr, nullptr, nullptr };
    m_ShaderResourceBinder.BindShaderResources(ShaderStage::PS, SLOT_TEX_CSM, m_config.numCascades, nullSRVs);
}

void CascadedShadowMap::BeginCascadeRender(int lightIndex, int cascadeIndex)
{
    // 現在のRTV/DSVを保存
    m_context->OMGetRenderTargets(1, &m_savedRTV, &m_savedDSV);

#ifdef _DEBUG
    // dummy RTVをバインドして、警告を回避する
    ID3D11RenderTargetView* rtvs[] = { m_dummyRTV };
    m_context->OMSetRenderTargets(1, rtvs, m_dsvs[lightIndex][cascadeIndex]);
#else
    m_context->OMSetRenderTargets(0, nullptr, m_dsvs[lightIndex][cascadeIndex]);
#endif // _DEBUG

    // ★注意★ 
    // OMSetRenderTargets を呼び出すと、以前に設定された DepthStencilState が無効化される。
    // そのため、RenderTarget / DepthStencilView を再設定した直後には、
    // 必ず OMSetDepthStencilState を使って明示的に深度ステンシルステートを再設定する必要がある。
    // これを行わないと、Releaseビルドでは深度書き込みが行われず、シャドウマップに何も描画されなくなる可能性がある。
    m_context->OMSetDepthStencilState(m_renderer.GetDepthWriteState(), 0);

    m_currentCascadeIndex = cascadeIndex;

    // DSVをクリア
    m_context->ClearDepthStencilView(m_dsvs[lightIndex][cascadeIndex], D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void CascadedShadowMap::EndCascadeRender(void) 
{
    m_context->OMSetRenderTargets(1, &m_savedRTV, m_savedDSV);
    SafeRelease(&m_savedRTV);
    SafeRelease(&m_savedDSV);
}

void CascadedShadowMap::UpdateCascades(const XMVECTOR& lightDir, float nearZ, float farZ) 
{
    // 各カスケードの分割割合（0.0~1.0）を格納する配列
    float cascadeSplits[MAX_CASCADES] = {};


    float lambda = m_config.lambda;
    float range = m_config.maxShadowDistance; // 視錐台の奥行きの範囲
    float ratio = (nearZ + range) / nearZ; // 対数スケールの計算に使用

    // カスケードごとにスプリット位置を計算
    for (int i = 0; i < m_config.numCascades; ++i)
    {
        if (m_config.useManualSplits)
        {
            float split = nearZ + m_config.manualSplits[i] * range;
            cascadeSplits[i] = (split - nearZ) / range;
        }
        else
        {

            float p = (i + 1) / static_cast<float>(m_config.numCascades);   // 分割の位置（正規化）
            float logSplit = nearZ * powf(ratio, p);                        // 対数スプリット
            float uniSplit = nearZ + range * p;                             // 線形スプリット
            float split = lambda * logSplit + (1.0f - lambda) * uniSplit;   // ハイブリッド分割
            cascadeSplits[i] = (split - nearZ) / range;                     // 0～1 に正規化して保存
        }
    }


    float lastSplitDist = 0.0f;

    // 各カスケードに対応するフラスタム範囲を決定
    for (int i = 0; i < m_config.numCascades; ++i)
    {
        float splitDist = cascadeSplits[i];                     // 現在のスプリットの割合（0~1）
        float splitNearW = nearZ + lastSplitDist * range;        // このカスケードの近クリップ
        float splitFarW = nearZ + splitDist * range;             // このカスケードの遠クリップ

        // NDCへの正規化(0~1)
        float splitNearNDC = (splitNearW - nearZ) / range;
        float splitFarNDC = (splitFarW - nearZ) / range;

        // 現在のカスケードに対応するシャドウ行列を計算
        ComputeCascadeMatrices(i, splitNearNDC, splitFarNDC, lightDir);

        // シャドウ比較用のスプリット深度
        m_cascadeData[i].splitDepth = splitFarW;

        // 次の nearZ 用に更新
        lastSplitDist = splitDist;
    }

#ifdef _DEBUG
    AdjustCascadePadding();
#endif
}

void CascadedShadowMap::UpdateCascadeCBufferArray(void)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_cascadeArrayCBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return;

    CBCascadeDataArray* buffer = reinterpret_cast<CBCascadeDataArray*>(mapped.pData);

    float splits[4] = { 1e6f, 1e6f, 1e6f, 1e6f };
    for (int i = 0; i < m_config.numCascades; ++i)
    {
        buffer->cascadeArray[i] = m_cascadeData[i];

        splits[i] = m_cascadeData[i].splitDepth;
    }
    buffer->cascadeSplits = XMFLOAT4(splits[0], splits[1], splits[2], splits[3]);

    m_context->Unmap(m_cascadeArrayCBuffer, 0);

    m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_CASCADE_DATA_ARRAY, m_cascadeArrayCBuffer);
    m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_CASCADE_DATA_ARRAY, m_cascadeArrayCBuffer);
}

void CascadedShadowMap::UpdateCascadeCBuffer(int cascadeIndex)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_cascadeSingleCBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return;

    memcpy(mapped.pData, &m_cascadeData[cascadeIndex], sizeof(CBCascadeData));
    m_context->Unmap(m_cascadeSingleCBuffer, 0);

    // VS用の場合
    m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_CASCADE_DATA, m_cascadeSingleCBuffer);

    // PSでも使いたい場合はこれも追加
    //m_ShaderResourceBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_CASCADE_DATA, m_cascadeSingleCBuffer);
}

void CascadedShadowMap::VisualizeCascadeBounds(void)
{
    BOUNDING_BOX box = {};
    XMFLOAT3 minPt = { FLT_MAX, FLT_MAX, FLT_MAX };
    XMFLOAT3 maxPt = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    for (int cascadeIndex = 0; cascadeIndex < m_config.numCascades; ++cascadeIndex)
    {
        for (int i = 0; i < 8; ++i)
        {
            XMFLOAT3 pt;
            XMStoreFloat3(&pt, m_tempSliceCorners[cascadeIndex][i]);

            minPt.x = min(minPt.x, pt.x);
            minPt.y = min(minPt.y, pt.y);
            minPt.z = min(minPt.z, pt.z);

            maxPt.x = max(maxPt.x, pt.x);
            maxPt.y = max(maxPt.y, pt.y);
            maxPt.z = max(maxPt.z, pt.z);
        }

        box.minPoint = minPt;
        box.maxPoint = maxPt;

        XMFLOAT4 color = { 0.0f, 0.0f, 0.0f, 1.0f };

        if (cascadeIndex == 1) color = { 1.0f, 0.0f, 0.0f, 1.0f };
        else if (cascadeIndex == 2) color = { 0.0f, 1.0f, 0.0f, 1.0f };
        else if (cascadeIndex == 3) color = { 0.0f, 0.0f, 1.0f, 1.0f };

        if (m_showCascadeBox[cascadeIndex])
        {
            m_debugBoundingBoxRenderer[cascadeIndex].DrawBox(box, m_camera.GetViewProjMtx(), color);
        }
    }
}

void CascadedShadowMap::ComputeCascadeMatrices(int cascadeIndex, float splitNear, float splitFar,
                                               const XMVECTOR& lightDir) 
{

    // カメラのビュー投影行列の逆行列を計算（NDC -> ワールド座標変換用）
    XMMATRIX CSMViewProjMtx = ComputeCSMViewProjMatrixWithMaxDistance(m_config.maxShadowDistance);
    XMMATRIX invViewProj = XMMatrixInverse(nullptr, CSMViewProjMtx);

    // 視錐台全体(NDC空間の8つのコーナー(0~1の深度)を定義
    XMVECTOR ndcCorners[8] =
    {
        XMVectorSet(-1,  1, 0, 1),  // near 左上
        XMVectorSet( 1,  1, 0, 1),  // near 右上
        XMVectorSet( 1, -1, 0, 1),  // near 右下
        XMVectorSet(-1, -1, 0, 1),  // near 左下
        XMVectorSet(-1,  1, 1, 1),  // far 左上
        XMVectorSet( 1,  1, 1, 1),  // far 右上
        XMVectorSet( 1, -1, 1, 1),  // far 右下
        XMVectorSet(-1, -1, 1, 1),  // far 左下
    };

    // ワールド空間に変換
    XMVECTOR frustumCornersWS[8];
    for (int i = 0; i < 8; ++i) 
    {
        frustumCornersWS[i] = XMVector3TransformCoord(ndcCorners[i], invViewProj);
    }


    // 分割された視錐台のスライスを構築(splitNear/splitFar の範囲)
    XMVECTOR sliceCorners[8];
    for (int i = 0; i < 4; ++i) 
    {
        // near 面(0~3) splitNear 平面
        sliceCorners[i] = XMVectorLerp(frustumCornersWS[i], frustumCornersWS[i + 4], splitNear);
        // far 面(4~7) splitFar 平面
        sliceCorners[i + 4] = XMVectorLerp(frustumCornersWS[i], frustumCornersWS[i + 4], splitFar);
    }

    // 視錐スライスの中心を計算
    XMVECTOR frustumCenter = XMVectorZero();
    for (int i = 0; i < 8; ++i) 
    {
        frustumCenter += sliceCorners[i];
    }
    frustumCenter /= 8.0f;

    // ローカルスペースに中心化
    XMVECTOR localCorners[8];
    for (int i = 0; i < 8; ++i)
        localCorners[i] = sliceCorners[i] - frustumCenter;

    // カスケードスライスの中心点とコーナー配列が計算済みであることを前提とする
    float radius = ComputeCascadeRadius(sliceCorners, frustumCenter, m_config.radiusStrategy, m_config.padding[cascadeIndex]);
    float radiusLocal = ComputeCascadeRadius(localCorners, XMVectorZero(), m_config.radiusStrategy, m_config.padding[cascadeIndex]);

    // ライトビュー行列(光源方向からスライス中心を見下ろす)
    XMVECTOR lightPos = frustumCenter - lightDir * radius * 2.0f;
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, frustumCenter, XMVectorSet(0, 1, 0, 0));
    XMVECTOR lightPosLocal = XMVectorZero() - lightDir * radiusLocal * 2.0f;
    XMMATRIX lightViewLocal = XMMatrixLookAtLH(lightPosLocal, XMVectorZero(), XMVectorSet(0, 1, 0, 0));

    // 正射影行列(スライス全体をカバー)
    XMMATRIX lightProj = ComputeLightProjMatrixFromSliceCorners(sliceCorners, lightView, m_config.padding[cascadeIndex]);
    XMMATRIX lightProjLocal = ComputeLightProjMatrixFromSliceCorners(localCorners, lightViewLocal, m_config.padding[cascadeIndex]);
    
    XMMATRIX lightViewProjLocal = lightViewLocal * lightProjLocal;
    XMMATRIX lightViewProj = lightView * lightProj;
    m_cascadePrivateData[cascadeIndex].lightViewProjLocal = lightViewProjLocal;
    m_cascadePrivateData[cascadeIndex].viewFrustumCenter = frustumCenter;
    m_lastCascadeRadius = radiusLocal;
    m_lastLightDir = lightDir;

    // 最終ビュー行列を保存する前に、必要に応じてスナップ
    if (m_config.enableTexelSnap)
    {
        lightViewProj = SnapShadowMatrixToTexelGrid(lightView, lightProj, frustumCenter);
    }

    // 最終シャドウ行列を保存
	m_cascadeData[cascadeIndex].lightViewProj = XMMatrixTranspose(lightViewProj);
    m_cascadeData[cascadeIndex].lightView = XMMatrixTranspose(lightView);
    m_cascadeData[cascadeIndex].lightProj = XMMatrixTranspose(lightProj);

    // splitDepth(カメラのview空間などで使用する深度閾値)
    m_cascadeData[cascadeIndex].splitDepth = splitFar; // 通常はsplitFarを使う


#if DEBUG_CASCADE_BOUND
    m_tempFrustumCenter[cascadeIndex] = frustumCenter;
    m_tempInvViewProj = invViewProj;
    for (int i = 0; i < 8; ++i)
    {
        m_tempSliceCorners[cascadeIndex][i] = sliceCorners[i];
    }
#endif // DEBUG

}

void CascadedShadowMap::InitCascadeCBuffer(void)
{
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(CBCascadeDataArray);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbd.MiscFlags = 0;

    m_device->CreateBuffer(&cbd, nullptr, &m_cascadeArrayCBuffer);

    cbd.ByteWidth = sizeof(CBCascadeData);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbd.MiscFlags = 0;

    m_device->CreateBuffer(&cbd, nullptr, &m_cascadeSingleCBuffer);
}

XMMATRIX CascadedShadowMap::SnapShadowMatrixToTexelGrid(const XMMATRIX& lightViewProjLocal)
{
    // シャドウマップのサイズを取得
    float shadowMapSize = static_cast<float>(m_config.shadowMapSize);

    // テクセルサイズを計算
    float texelSize = (2.0f * m_lastCascadeRadius) / shadowMapSize;

    // XY の位置を抽象
    float offsetX = lightViewProjLocal.r[3].m128_f32[0];
    float offsetY = lightViewProjLocal.r[3].m128_f32[1];

    float snappedX = roundf(offsetX / texelSize) * texelSize - offsetX;
    float snappedY = roundf(offsetY / texelSize) * texelSize - offsetY;

    XMMATRIX snapOffset = XMMatrixTranslation(snappedX, snappedY, 0.0f);
    return snapOffset * lightViewProjLocal;
}

XMMATRIX CascadedShadowMap::SnapShadowMatrixToTexelGrid(const XMMATRIX& lightView, const XMMATRIX& lightProj, XMVECTOR center)
{
    // ビュー射影行列の合成
    XMMATRIX lightViewProj = lightView * lightProj;

    // 中心点をライトビュー空間に変換
    XMVECTOR lightSpaceCenter = XMVector3TransformCoord(center, lightView);

    // シャドウマップのサイズを取得
    float shadowMapSize = static_cast<float>(m_config.shadowMapSize);

    // テクセルサイズを計算
    float texelSize = (2.0f * m_lastCascadeRadius) / shadowMapSize;

    // テクセル単位にスナップ（格子に合わせる）
    XMVECTOR snapped = XMVectorFloor(lightSpaceCenter / texelSize) * texelSize;

    // スナップ後の中心点をワールド空間に戻す
    XMMATRIX invLightView = XMMatrixInverse(nullptr, lightView);
    XMVECTOR newCenter = XMVector3TransformCoord(snapped, invLightView);

    // 新しいビュー行列を再計算
    XMVECTOR lightPos = newCenter - m_lastLightDir * m_lastCascadeRadius * 2.0f;
    return XMMatrixLookAtLH(lightPos, newCenter, XMVectorSet(0, 1, 0, 0)) * lightProj;
}

float CascadedShadowMap::ComputeCascadeRadius(XMVECTOR* corners, XMVECTOR center, ShadowRadiusStrategy strategy, float padding)
{
    switch (strategy)
    {
    case ShadowRadiusStrategy::FitBoundingSphere:
    {
        // すべてのコーナー点と中心点の距離の最大値を求める（球で包む）
        float r = 0.0f;
        // 最大半径を求めてシャドウ範囲を確保
        for (int i = 0; i < 8; ++i)
        {
            float d = XMVectorGetX(XMVector3Length(corners[i] - center));
            r = max(r, d);
        }
        // 安定化のためにパディングとスナップ単位で切り上げ
        return ceilf((r + padding) * 16.0f) / 16.0f;
    }

    case ShadowRadiusStrategy::FitBoundingBox:
    {
        // AABB（軸に沿ったバウンディングボックス）を作成
        XMVECTOR minPt = corners[0];
        XMVECTOR maxPt = corners[0];
        for (int i = 1; i < 8; ++i)
        {
            minPt = XMVectorMin(minPt, corners[i]);
            maxPt = XMVectorMax(maxPt, corners[i]);
        }

        XMVECTOR extent = (maxPt - minPt) * 0.5f;
        float r = max(max(XMVectorGetX(extent), XMVectorGetY(extent)), XMVectorGetZ(extent));
        return ceilf((r + padding) * 16.0f) / 16.0f;
    }

    case ShadowRadiusStrategy::PaddingBuffer:
        // 固定のバッファを使う（シンプルだが非効率）
        return padding;

    default:
        return 1.0f;
    }
}

bool CascadedShadowMap::IsAABBInsideLightFrustum(const BOUNDING_BOX& worldAABB, int cascadeIndex)
{
    // AABBの8つの頂点を取得
    XMVECTOR cornerWS[8];
    worldAABB.GetCorners(cornerWS);

    for (int i = 0; i < 8; ++i)
    {
        // カスケードスライスの中心を原点としたローカル空間へ変換
        XMVECTOR localCorner = XMVectorSubtract(cornerWS[i], m_cascadePrivateData[cascadeIndex].viewFrustumCenter);

        // ローカル空間のライトビュー射影行列で変換
        XMVECTOR clipPos = XMVector3TransformCoord(localCorner, m_cascadePrivateData[cascadeIndex].lightViewProjLocal);

        // 正規化デバイス座標（NDC）に変換（wで割る）
        XMVECTOR ndcPos = clipPos / XMVectorGetW(clipPos);

        float x = XMVectorGetX(ndcPos);
        float y = XMVectorGetY(ndcPos);
        float z = XMVectorGetZ(ndcPos);

        // ひとつでも視野内に入っていれば採用
        if (x >= -1.0f && x <= 1.0f &&
            y >= -1.0f && y <= 1.0f &&
            z >= 0.0f && z <= 1.0f)
        {
            return true;
        }
    }

    // 全ての点が視野外
    return false;
}


XMMATRIX CascadedShadowMap::ComputeCSMViewProjMatrixWithMaxDistance(float maxDistance)
{
    float fovY = m_camera.GetFov();
    float aspect = m_camera.GetAspectRatio();
    float nearZ = m_camera.GetNearZ();

    // 定義されたmaxDistanceで遠抜を要求
    float farZ = maxDistance;

    XMMATRIX view = XMLoadFloat4x4(&m_camera.GetViewMatrix());
    XMMATRIX proj = XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);

    return view * proj; // これを逆変換して使います
}

//=============================================================================
// ライトビュープロジェクション行列を加算する関数
// スライスのコーナーをライトビュー空間に変換して
// 最小値と最大値で包んだ正規投影行列を構築する
//=============================================================================
XMMATRIX CascadedShadowMap::ComputeLightProjMatrixFromSliceCorners(const XMVECTOR sliceCorners[8], const XMMATRIX& lightView, float padding)
{
    // 光源空間に変換
    XMVECTOR minPtLS = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
    XMVECTOR maxPtLS = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);

    for (int i = 0; i < 8; ++i)
    {
        XMVECTOR cornerLS = XMVector3TransformCoord(sliceCorners[i], lightView);
        minPtLS = XMVectorMin(minPtLS, cornerLS);
        maxPtLS = XMVectorMax(maxPtLS, cornerLS);
    }

    // パディングを適用
    XMVECTOR padVec = XMVectorReplicate(padding);
    minPtLS = XMVectorSubtract(minPtLS, padVec);
    maxPtLS = XMVectorAdd(maxPtLS, padVec);

    // OrthographicOffCenterLH を構築
    float minX = XMVectorGetX(minPtLS);
    float maxX = XMVectorGetX(maxPtLS);
    float minY = XMVectorGetY(minPtLS);
    float maxY = XMVectorGetY(maxPtLS);
    float minZ = XMVectorGetZ(minPtLS);
    float maxZ = XMVectorGetZ(maxPtLS);

    return XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);
}

void CascadedShadowMap::AdjustCascadePadding(void)
{
    // 基本のベースのパディング値
    float basePadding = m_config.maxShadowDistance * 0.01f; // 例: 最遠が40,000の場合は400.0f

    // スライス領域が大きくなるにつれて指数的に拡大
    for (int i = 0; i < m_config.numCascades; ++i)
    {
        float factor = powf(2.0f, static_cast<float>(i));
        m_config.padding[i] = basePadding * factor;
    }
}
