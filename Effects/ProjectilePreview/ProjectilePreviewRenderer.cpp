//=============================================================================
//
// [ProjectilePreviewRenderer.cpp]
// Author : 
//
//=============================================================================
#include "ProjectilePreviewRenderer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
constexpr float SPHERE_RADIUS = 1.0f;
constexpr float SPHERE_SCALE = 120.0f;
constexpr float RIBBON_WIDTH = 20.0f;
constexpr float RIBBON_THICKNESS = 5.0f;

ProjectilePreviewRenderer::ProjectilePreviewRenderer()
	: m_device(Renderer::get_instance().GetDevice()), m_context(Renderer::get_instance().GetDeviceContext()) 
{
	m_trajectoryVertices.reserve(m_maxTrajectoryPoints * 8);
	m_trajectoryIndices.reserve(m_maxTrajectoryPoints * 16); // 最悪でも線分で描画するので2倍程度は必要
}

ProjectilePreviewRenderer::~ProjectilePreviewRenderer()
{
	ShutDown();
}

void ProjectilePreviewRenderer::Initialize(void)
{
    if (!LoadShaders())
    {
        assert(false && "Failed to load shaders for ProjectilePreviewRenderer");
        return;
    }

    HRESULT hr = S_OK;

	// 軌跡線の頂点バッファを作成
	D3D11_BUFFER_DESC vbDesc{};
	vbDesc.Usage = D3D11_USAGE_DYNAMIC;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.ByteWidth = sizeof(ProjectilePreviewVertex) * m_maxTrajectoryPoints * 8;
	vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateBuffer(&vbDesc, nullptr, &m_trajectoryVB);
    assert(SUCCEEDED(hr));

	// 軌跡線のインデックスバッファを作成
	D3D11_BUFFER_DESC ibDesc{};
	ibDesc.Usage = D3D11_USAGE_DYNAMIC;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.ByteWidth = sizeof(UINT) * m_maxTrajectoryPoints * 16; // 最悪でも線分で描画するので2倍程度は必要
	ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = m_device->CreateBuffer(&ibDesc, nullptr, &m_trajectoryIB);
	assert(SUCCEEDED(hr));

	// 半球メッシュを作成 (緯度経度分割)
	CreateSphereBuffers(MAX_SPHERE_SLICES, MAX_SPHERE_STACKS); // 最大スライス数とスタック数を指定

    // CBPreview のバッファ作成
    D3D11_BUFFER_DESC cbd{};
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.ByteWidth = sizeof(CBPreviewData);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer(&cbd, nullptr, &m_cbPreview);
    assert(SUCCEEDED(hr));
}

void ProjectilePreviewRenderer::ShutDown(void)
{
	SafeRelease(&m_trajectoryVB);
	SafeRelease(&m_sphereVB);
	SafeRelease(&m_sphereIB);
}

void ProjectilePreviewRenderer::Update(const SimpleArray<TrajectoryPoint>& trajectoryPoints, const XMFLOAT3& hitPos, const XMFLOAT3& hitNormal)
{
    //XMFLOAT3 insetPos = {
    //hitPos.x + hitNormal.x * 1.0f,
    //hitPos.y + hitNormal.y * 1.0f,
    //hitPos.z + hitNormal.z * 5.0f
    //};
    m_hitPos = hitPos;

    // 軌跡点数を更新
    m_currentTrajectoryPoints = trajectoryPoints.getSize();
    if (m_currentTrajectoryPoints > m_maxTrajectoryPoints)
        m_currentTrajectoryPoints = m_maxTrajectoryPoints;

    // 命中点の有無を判定 (最後の点がhitならtrue)
    m_hasHit = false;
    if (!trajectoryPoints.empty()) 
    {
        if (trajectoryPoints.back().hit) 
        {
            // 命中点の法線を使って半球メッシュを更新
			UpdateSphereMesh(hitNormal, SPHERE_RADIUS);
            m_hasHit = true;
        }
    }

	// 軌跡頂点データを更新
	m_trajectoryVertices.clear();
    m_trajectoryIndices.clear();

    const XMFLOAT3 camRight = m_camera.GetRight();
    const XMFLOAT3 camUp = m_camera.GetUp();

    const XMVECTOR rightVec = XMVectorScale(XMLoadFloat3(&camRight), RIBBON_WIDTH * 0.5f);
    const XMVECTOR upVec = XMVectorScale(XMLoadFloat3(&camUp), RIBBON_THICKNESS * 0.5f);

    for (UINT i = 0; i + 1 < m_currentTrajectoryPoints; ++i) 
    {
        const XMFLOAT3& p0 = trajectoryPoints[i].pos;
        const XMFLOAT3& p1 = trajectoryPoints[i + 1].pos;

        XMVECTOR base0 = XMLoadFloat3(&p0);
        XMVECTOR base1 = XMLoadFloat3(&p1);

        // 4 頂点をカメラ方向で展開（帯状）
        XMVECTOR v0_LT = base0 - rightVec + upVec;
        XMVECTOR v0_LB = base0 - rightVec - upVec;
        XMVECTOR v0_RT = base0 + rightVec + upVec;
        XMVECTOR v0_RB = base0 + rightVec - upVec;

        XMVECTOR v1_LT = base1 - rightVec + upVec;
        XMVECTOR v1_LB = base1 - rightVec - upVec;
        XMVECTOR v1_RT = base1 + rightVec + upVec;
        XMVECTOR v1_RB = base1 + rightVec - upVec;

        UINT baseIndex = m_trajectoryVertices.getSize();

        auto Push = [&](XMVECTOR pos, XMFLOAT3 normal)
            {
                ProjectilePreviewVertex vert;
                XMStoreFloat3(&vert.pos, pos);
                vert.normal = normal;
                m_trajectoryVertices.push_back(vert);
            };

        // 法線は上向きで統一（必要なら後で変更）
        const XMFLOAT3 normal = { 0, 1, 0 };

        // 左→右、近→遠の順に8頂点追加
        Push(v0_LT, normal);
        Push(v0_LB, normal);
        Push(v0_RT, normal);
        Push(v0_RB, normal);
        Push(v1_LT, normal);
        Push(v1_LB, normal);
        Push(v1_RT, normal);
        Push(v1_RB, normal);

        // インデックス追加（帯状に4面）
        // 左側面
        m_trajectoryIndices.push_back(baseIndex + 0);
        m_trajectoryIndices.push_back(baseIndex + 1);
        m_trajectoryIndices.push_back(baseIndex + 5);
        m_trajectoryIndices.push_back(baseIndex + 0);
        m_trajectoryIndices.push_back(baseIndex + 5);
        m_trajectoryIndices.push_back(baseIndex + 4);

        // 上側面
        m_trajectoryIndices.push_back(baseIndex + 0);
        m_trajectoryIndices.push_back(baseIndex + 4);
        m_trajectoryIndices.push_back(baseIndex + 6);
        m_trajectoryIndices.push_back(baseIndex + 0);
        m_trajectoryIndices.push_back(baseIndex + 6);
        m_trajectoryIndices.push_back(baseIndex + 2);

        // 右側面
        m_trajectoryIndices.push_back(baseIndex + 2);
        m_trajectoryIndices.push_back(baseIndex + 6);
        m_trajectoryIndices.push_back(baseIndex + 7);
        m_trajectoryIndices.push_back(baseIndex + 2);
        m_trajectoryIndices.push_back(baseIndex + 7);
        m_trajectoryIndices.push_back(baseIndex + 3);

        // 下側面
        m_trajectoryIndices.push_back(baseIndex + 1);
        m_trajectoryIndices.push_back(baseIndex + 3);
        m_trajectoryIndices.push_back(baseIndex + 7);
        m_trajectoryIndices.push_back(baseIndex + 1);
        m_trajectoryIndices.push_back(baseIndex + 7);
        m_trajectoryIndices.push_back(baseIndex + 5);
    }

	m_trajectoryIndexCount = m_trajectoryIndices.getSize();

    // 頂点バッファに書き込み
    if (m_trajectoryVB && m_currentTrajectoryPoints > 0) 
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(m_context->Map(m_trajectoryVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            memcpy(mapped.pData, m_trajectoryVertices.data(), sizeof(ProjectilePreviewVertex) * m_trajectoryVertices.getSize());
            m_context->Unmap(m_trajectoryVB, 0);
        }
    }

    // インデックスバッファ更新
    if (m_trajectoryIB && m_trajectoryIndexCount > 0)
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(m_context->Map(m_trajectoryIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) 
        {
            memcpy(mapped.pData, m_trajectoryIndices.data(), sizeof(UINT) * m_trajectoryIndices.getSize());
            m_context->Unmap(m_trajectoryIB, 0);
        }
    }

    // CBPreview のデータを更新
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(m_context->Map(m_cbPreview, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) 
    {
        CBPreviewData* cb = reinterpret_cast<CBPreviewData*>(mapped.pData);
        cb->hitPos = m_hitPos;
        cb->scale = SPHERE_SCALE; // 半球のスケール (必要なら外部で設定)
		cb->color = XMFLOAT4(0.8f, 0.0f, 0.0f, 0.5f); // ピンク色 (必要なら外部で設定)
        cb->time = m_timer.GetDeltaTime();
        cb->padding = XMFLOAT2(0, 0);
        m_context->Unmap(m_cbPreview, 0);
    }
}

void ProjectilePreviewRenderer::Draw(void)
{
    // VS/PS へバインド
    m_shaderBinder.BindConstantBuffer(ShaderStage::VS, SLOT_CB_PROJECTILE_PREVIEW, m_cbPreview);
    m_shaderBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_PROJECTILE_PREVIEW, m_cbPreview);

	m_context->IASetInputLayout(m_shaderSet.inputLayout);
    m_context->VSSetShader(m_shaderSet.vs, nullptr, 0);
    m_context->PSSetShader(m_shaderSet.ps, nullptr, 0);

    // 軌跡を描画
    if (m_trajectoryVB && m_currentTrajectoryPoints > 1) 
    {
        // CBPreview のデータを更新
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(m_context->Map(m_cbPreview, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            CBPreviewData* cb = reinterpret_cast<CBPreviewData*>(mapped.pData);
            cb->renderType = 0; // Trajectory
            m_context->Unmap(m_cbPreview, 0);
        }

        UINT stride = sizeof(ProjectilePreviewVertex);
        UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, &m_trajectoryVB, &stride, &offset);
        m_context->IASetIndexBuffer(m_trajectoryIB, DXGI_FORMAT_R32_UINT, 0);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_context->DrawIndexed(m_trajectoryIndexCount, 0, 0);
    }

    // 半球を描画
    if (m_hasHit && m_sphereVB && m_sphereIB) 
    {
        // CBPreview のデータを更新
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(m_context->Map(m_cbPreview, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            CBPreviewData* cb = reinterpret_cast<CBPreviewData*>(mapped.pData);
            cb->renderType = 1; // Sphere
            m_context->Unmap(m_cbPreview, 0);
        }

        // 半球の呼吸アニメーション (スケール変化など) はシェーダー側で実装予定
        UINT stride = sizeof(ProjectilePreviewVertex);
        UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, &m_sphereVB, &stride, &offset);
        m_context->IASetIndexBuffer(m_sphereIB, DXGI_FORMAT_R32_UINT, 0);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_context->DrawIndexed(m_sphereIndexCount, 0, 0);
    }
}

void ProjectilePreviewRenderer::UpdateSphereMesh(const XMFLOAT3& hitNormal, float radius, UINT sliceCount, UINT stackCount)
{
    // 安全対策：初期確保した最大を超えない
    sliceCount = min(sliceCount, m_sphereMaxSlice);
    stackCount = min(stackCount, m_sphereMaxStack);

    // 1、法線に揃えたローカル基底（T, B, N）を作る
    XMVECTOR N = XMVector3Normalize(XMLoadFloat3(&hitNormal));  // 面の法線
    // N と平行に近い軸を避けるための一時アップベクトルを選ぶ
    XMVECTOR tmpUp = (fabs(XMVectorGetX(N)) < 0.9f) ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 1, 0, 0);
    XMVECTOR T = XMVector3Normalize(XMVector3Cross(tmpUp, N));  // 接線（左右）
    XMVECTOR B = XMVector3Cross(N, T);                          // 従法線（奥行き）

    // 2、頂点グリッド生成（上半球: φ ∈ [0, π/2], θ ∈ [0, 2π]）
    // 位置と法線を N を“上”とする座標系で構成 → ワールドへはT・B・Nの線形結合
    const float r = radius;

    const UINT vertexCount = (stackCount + 1) * (sliceCount + 1);
    const UINT indexCount = stackCount * sliceCount * 6;

    // VBに直接書き込む
    D3D11_MAPPED_SUBRESOURCE mappedVB{};
    if (FAILED(m_context->Map(m_sphereVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVB)))
        return;

    auto* vtx = reinterpret_cast<ProjectilePreviewVertex*>(mappedVB.pData);
    UINT v = 0;

    for (UINT stack = 0; stack <= stackCount; ++stack)
    {
        // φ: 上半球（0 → π/2）
        float phi = (XM_PI * 0.5f) * (float(stack) / float(stackCount));
        float s = sinf(phi); // N方向成分
        float c = cosf(phi); // 水平円周成分（T/B）

        for (UINT slice = 0; slice <= sliceCount; ++slice)
        {
            float theta = XM_2PI * (float(slice) / float(sliceCount));
            float ct = cosf(theta);
            float st = sinf(theta);

            // ローカル球（Nが上）の成分:
            // pos = r * ( c*ct*T + s*N + c*st*B )
            XMVECTOR pos =
                XMVectorScale(T, r * c * ct) +
                XMVectorScale(N, r * s) +
                XMVectorScale(B, r * c * st);

            XMVECTOR nrm = XMVector3Normalize(
                XMVectorScale(T, c * ct) +
                XMVectorScale(N, s) +
                XMVectorScale(B, c * st));

            XMStoreFloat3(&vtx[v].pos, pos);
            XMStoreFloat3(&vtx[v].normal, nrm);
            ++v;
        }
    }
    m_context->Unmap(m_sphereVB, 0);

    // 3、インデックス生成
    D3D11_MAPPED_SUBRESOURCE mappedIB{};
    if (FAILED(m_context->Map(m_sphereIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedIB)))
        return;

    auto* idx = reinterpret_cast<UINT*>(mappedIB.pData);
    UINT k = 0;

    for (UINT stack = 0; stack < stackCount; ++stack)
    {
        for (UINT slice = 0; slice < sliceCount; ++slice)
        {
            UINT i0 = stack * (sliceCount + 1) + slice;
            UINT i1 = i0 + sliceCount + 1;
            UINT i2 = i0 + 1;
            UINT i3 = i1 + 1;

            // 反時計回り（カリングなし推奨だが、統一しておく）
            idx[k++] = i0; idx[k++] = i1; idx[k++] = i2;
            idx[k++] = i2; idx[k++] = i1; idx[k++] = i3;
        }
    }
    m_context->Unmap(m_sphereIB, 0);

    m_sphereIndexCount = indexCount;
}

void ProjectilePreviewRenderer::CreateSphereBuffers(UINT maxSliceCount, UINT maxStackCount)
{
    m_sphereMaxSlice = maxSliceCount;
    m_sphereMaxStack = maxStackCount;

    // 最大頂点/インデックス数（上半球）
    // 頂点: (stack+1) * (slice+1)
    const UINT maxVertexCount = (maxStackCount + 1) * (maxSliceCount + 1);
    // 三角形: stack * slice * 2 → インデックス: *3
    const UINT maxIndexCount = maxStackCount * maxSliceCount * 6;

    // 動的VB
    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;                     // 毎フレーム書き込むため
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vbDesc.ByteWidth = sizeof(ProjectilePreviewVertex) * maxVertexCount;
    m_device->CreateBuffer(&vbDesc, nullptr, &m_sphereVB);

    // 動的IB
    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.Usage = D3D11_USAGE_DYNAMIC;                     // 毎フレーム書き込むため
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ibDesc.ByteWidth = sizeof(UINT) * maxIndexCount;
    m_device->CreateBuffer(&ibDesc, nullptr, &m_sphereIB);

    m_sphereIndexCount = 0; // 実際の枚数は毎フレームの更新で決まる
}

bool ProjectilePreviewRenderer::LoadShaders(void)
{
    bool loadShaders = true;

    loadShaders &= ShaderManager::get_instance().HasShaderSet(ShaderSetID::ProjectilePreview);
    if (loadShaders)
        m_shaderSet = ShaderManager::get_instance().GetShaderSet(ShaderSetID::ProjectilePreview);

    return loadShaders;
}
