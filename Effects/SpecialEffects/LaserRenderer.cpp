//=============================================================================
// [LaserRenderer.cpp]
//
//=============================================================================
#include "Effects/SpecialEffects/LaserRenderer.h"
#include "Effects/DecalRenderer.h"
#include "Effects/EffectSystem.h"

void LaserRenderer::Initialize(void)
{
    if (!LoadShaders())
    {
        assert(false && "Failed to load shaders for ProjectilePreviewRenderer");
        return;
    }

    HRESULT hr = S_OK;

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.ByteWidth = sizeof(LaserVB) * 4;
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vbDesc.MiscFlags = 0;
    hr = m_device->CreateBuffer(&vbDesc, nullptr, &m_vb);
	assert(SUCCEEDED(hr));

    const uint16_t idx[6] = { 0, 1, 2,  2, 1, 3 };
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.ByteWidth = sizeof(idx);
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    D3D11_SUBRESOURCE_DATA ibInit = { idx, 0, 0 };
    hr = m_device->CreateBuffer(&ibDesc, &ibInit, &m_ib);
    assert(SUCCEEDED(hr));

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.ByteWidth = sizeof(CBLaserCPU);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_cbLaser);
    assert(SUCCEEDED(hr));
}

void LaserRenderer::Begin(const XMFLOAT3& start, const XMFLOAT3& dirNorm, const LaserParams& params, const XMFLOAT4& hdrColor)
{
    if (m_state.active) 
    {
        // すでに稼働中。二重Beginを避ける（必要なら End() してから）
        return;
    }

	// 状態初期化
    m_params = params;
    m_state.start = start;
    m_state.dir = dirNorm;
    m_state.traveled = 0.0f;
    m_state.active = true;
    m_state.stopped = false;
    m_state.hitPos = { 0,0,0 };
    m_state.hitNormal = { 0,0,0 };
    m_state.lastHit = LaserHitKind::None;
    m_color = hdrColor;

	// 方向追従の初期化
    DirectionTrackParams track{};
    track.preFireDelaySec = 0.20f;  // 0.2秒ロック
    track.maxDegPerSec = 45.0f;  // 最大45°/秒
    track.smoothTauSec = 0.5f;   // 緩やかに追従
    track.clampTurnRateDegPerSec = 60.0f;  // 追加の安全枠（任意）
    track.deadZoneDeg = 0.8f;   // 微小ノイズを無視

    XMFLOAT3 initialDir = dirNorm;
    ArmDirectionTracking(initialDir, track);

    if (m_trailEnabled)
    {
        m_trailPoints.clear();
        TrajectoryPoint tp; tp.pos = start; tp.hit = false;
        m_trailPoints.push_back(tp);
    }
}

void LaserRenderer::Advance(float dt, const BOUNDING_BOX* playerAABB, XMFLOAT3* outHitPos, XMFLOAT3* outHitNormal)
{
    if (!m_voxelGrid)
        SetCollisionVoxelGrid(CollisionManager::get_instance().GetVoxelGrid());

    if (!m_state.active) return;

    if (outHitPos)    *outHitPos = { 0,0,0 };
    if (outHitNormal) *outHitNormal = { 0,0,0 };

    const float step = m_params.speed * max(0.0f, dt);
    const float prevLen = m_state.traveled;
    const float nextLen = min(prevLen + step, m_params.maxLength);

    const XMFLOAT3 p0 = m_state.start;
    const XMFLOAT3 p1 = Add(p0, Scale(m_state.dir, nextLen));


    // 候補ヒット情報（距離は pPrevEnd からの距離で比較）
    bool      hitAny = false;
    float     bestDistFromP0 = FLT_MAX; // p0 からの距離
    XMFLOAT3  bestPos = { 0,0,0 };
    XMFLOAT3  bestN = { 0,0,0 };
    LaserHitKind bestKind = LaserHitKind::None;

    // プレイヤーAABB
    if (playerAABB)
    {
        // 元のAABBを中心固定で縮小
        XMFLOAT3 bminS, bmaxS;
        ScaleAabbAboutCenter(playerAABB->minPoint, playerAABB->maxPoint, 0.6f, 0.6f, 0.6f, bminS, bmaxS);

        // 半径で膨張
        const float rad = m_params.radiusWorld;
        XMFLOAT3 bmin = XMFLOAT3(bminS.x - rad, bminS.y - rad, bminS.z - rad);
        XMFLOAT3 bmax = XMFLOAT3(bmaxS.x + rad, bmaxS.y + rad, bmaxS.z + rad);

        // SegmentAABBIntersect を p0→p1 で使用（t は 0..1）
        float tA = 1.0f; XMFLOAT3 nA = { 0,0,0 };
        if (SegmentAABBIntersect(p0, p1, bmin, bmax, &tA, &nA))
        {
            const float d = nextLen * max(0.0f, min(1.0f, tA)); // p0→ヒット
            if (!hitAny || d < bestDistFromP0)
            { 
                hitAny = true; 
                bestDistFromP0 = d;
                bestPos = Add(p0, Scale(m_state.dir, d));
                bestN = nA; 
                bestKind = LaserHitKind::Player; 
            }
        }
    }

    // 環境（VoxelGrid）
    if (m_voxelGrid)
    {
        XMFLOAT3 envPos{ 0,0,0 }, envN{ 0,0,0 };
        if (m_voxelGrid->Raycast(p0, p1, &envPos, &envN)) // 整段
        {
            const XMFLOAT3 v = Sub(envPos, p0);
            const float d = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); // p0→ヒット
            if (!hitAny || d < bestDistFromP0)
            { 
                hitAny = true; 
                bestDistFromP0 = d;
                bestPos = envPos; 
                bestN = envN; 
                bestKind = LaserHitKind::Environment; 
            }
        }
    }

    // 最前ヒットに合わせて“光束の長さ”を切り詰め（縮むケースに対応）
    float newLen = nextLen;
    if (hitAny)
        newLen = max(0.0f, min(nextLen, bestDistFromP0));

    // Trailサンプリング（任意）
    if (m_trailEnabled)
    {
        const float grow = newLen - prevLen; // 短縮なら負

        if (grow > 1e-6f)
        {
            const int steps = (int)floorf(grow / m_trailSpacing);
            if (steps > 0)
            {
                const XMFLOAT3 dseg = Scale(m_state.dir, m_trailSpacing);
                XMFLOAT3 cur = Add(p0, Scale(m_state.dir, prevLen));
                for (int i = 0; i < steps; ++i)
                {
                    cur = Add(cur, dseg);
                    TrajectoryPoint tp; tp.pos = cur; tp.hit = false;
                    m_trailPoints.push_back(tp);
                }
            }
        }

        // ヒットがあった場合は終点（bestPos）もサンプル
        if (hitAny)
        {
            TrajectoryPoint tp; tp.pos = bestPos; tp.hit = true; tp.hitNormal = bestN;
            m_trailPoints.push_back(tp);
        }
        else
        {
            // 未ヒット：候補終点 p1 を1点（任意）
            TrajectoryPoint tp; tp.pos = p1; tp.hit = false;
            m_trailPoints.push_back(tp);
        }
    }

    // 結果の反映
    if (hitAny)
    {
        // 命中点を最終点として追加（Trail 有効時）
        if (m_trailEnabled)
        {
            TrajectoryPoint tp; tp.pos = bestPos; tp.hit = true; tp.hitNormal = bestN;
            m_trailPoints.push_back(tp);
        }

        // p0→bestPos の絶対長さへ更新
        XMFLOAT3 vh = Sub(bestPos, p0);
        float L = sqrtf(vh.x * vh.x + vh.y * vh.y + vh.z * vh.z);

        m_state.traveled = L;
        m_state.hitPos = bestPos;
        m_state.hitNormal = bestN;
        m_state.lastHit = bestKind;

        if (outHitPos)    *outHitPos = bestPos;
        if (outHitNormal) *outHitNormal = bestN;

		// 環境命中のみデカール生成処理
		if (m_state.lastHit == LaserHitKind::Environment)
        {
            // デカール生成（間引き含む）
            HandleDecalOnHit(dt, bestPos, bestN);
        }
        else
        {
            m_state.firstHitSpawned = false; // 次回の「最初の一発」を許可
        }
    }
    else
    {
        // 末端点（p1）まで伸ばす
        m_state.traveled = nextLen;

        if (m_trailEnabled)
        {
            TrajectoryPoint tp; tp.pos = p1; tp.hit = false;
            m_trailPoints.push_back(tp);
        }

        if (m_state.traveled >= m_params.maxLength)
        {
            m_state.active = false;
            m_state.stopped = false;
            m_state.lastHit = LaserHitKind::MaxRange;
        }
        else
        {
            m_state.lastHit = LaserHitKind::None;
        }
    }
}

void LaserRenderer::Draw(int planeCount, float brightnessScale)
{
	// 非アクティブなら描かない
	if (!m_state.active) return;

    // 長さがゼロ相当なら描かない
    if (m_state.traveled <= 1e-4f) return;

    if (!m_context || !m_vb || !m_ib || !m_cbLaser) return;

    // 入力アセンブラ設定
    UINT stride = sizeof(LaserVB);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
    m_context->IASetIndexBuffer(m_ib, DXGI_FORMAT_R16_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 入力レイアウト・シェーダ設定
    m_context->IASetInputLayout(m_shaderSet.inputLayout);
    m_context->VSSetShader(m_shaderSet.vs, nullptr, 0);
    m_context->PSSetShader(m_shaderSet.ps, nullptr, 0);

	m_shaderBinder.BindConstantBuffer(ShaderStage::PS, SLOT_CB_LASER, m_cbLaser);

    m_renderer.SetBlendState(BLEND_MODE_ADD);

    // N本ループ：θ ∈ [0, π) を等間隔にサンプリング ===
    const float dTheta = XM_PI / float(planeCount);
    // 総輝度 ≒ brightnessScale に合わせて1本あたりの寄与を決める
    const float weightPer = brightnessScale / float(planeCount);

    for (int i = 0; i < planeCount; ++i)
    {
        float theta = dTheta * float(i);
        DrawOnePlane(theta, weightPer);
    }

	m_renderer.SetBlendState(BLEND_MODE_ALPHABLEND);
}

void LaserRenderer::EnableTrailSampling(bool enable, float pointSpacing)
{
    m_trailEnabled = enable;
    m_trailSpacing = pointSpacing;
    m_trailPoints.clear();
}

void LaserRenderer::BuildAxis(XMVECTOR& outT, XMVECTOR& outR, XMVECTOR& outU) const
{
    XMVECTOR t = XMVector3Normalize(XMLoadFloat3(&m_state.dir));
    XMVECTOR camF = XMVector3Normalize(XMLoadFloat3(&m_camera.GetForward()));

    // カメラ基準の r
    XMVECTOR r_cam = XMVector3Cross(camF, t);
    float rLen2 = XMVectorGetX(XMVector3LengthSq(r_cam));

    XMVECTOR r;
    if (rLen2 < 1e-6f) 
    {
        // 完全並行: カメラに依らない安定軸（worldUp）で生成
        XMVECTOR up0 = XMVectorSet(0, 1, 0, 0);
        r = XMVector3Cross(up0, t);
        if (XMVectorGetX(XMVector3LengthSq(r)) < 1e-6f) {
            XMVECTOR up1 = XMVectorSet(1, 0, 0, 0);
            r = XMVector3Cross(up1, t);
        }
    }
    else 
    {
        r = r_cam;
    }
    r = XMVector3Normalize(r);

    // 視線 // 進行 付近は前フレーム r と補間してスムージング
    float parallel = fabsf(XMVectorGetX(XMVector3Dot(camF, t))); // 0..1
    if (parallel > 0.98f) 
    {
        // ランタイム初期化（m_prevR 未設定対策）
        if (XMVectorGetX(XMVector3LengthSq(m_prevR)) < 1e-6f) 
        {
            m_prevR = r;
        }
        r = XMVector3Normalize(XMVectorLerp(m_prevR, r, m_axisSmoothing));
    }
    m_prevR = r;

    XMVECTOR u = XMVector3Normalize(XMVector3Cross(t, r));

    outT = t; outR = r; outU = u;
}

void LaserRenderer::BuildQuadVerticesAtAngle(float theta)
{
    // ローカル基底を構築（t: 進行方向, r/u: 断面の2軸）
    XMVECTOR t, r, u;
    BuildAxis(t, r, u);

    // 起点・終点
    XMVECTOR p0 = XMLoadFloat3(&m_state.start);
    XMVECTOR p1 = XMVectorAdd(p0, XMVectorScale(t, m_state.traveled));

    // 半径方向ベクトル（rとuの線形結合を正規化）
    float c = cosf(theta);
    float s = sinf(theta);
    XMVECTOR off = XMVector3Normalize(
        XMVectorAdd(XMVectorScale(r, c), XMVectorScale(u, s))
    );
    XMVECTOR half = XMVectorScale(off, m_params.radiusWorld);

    // 4頂点生成
    LaserVB vtx[4];
    auto put = [&](int i, XMVECTOR pos, float s, float v) {
        XMStoreFloat3(&vtx[i].pos, pos);
        vtx[i].sv = XMFLOAT2(s, v);
        };

    // s=0（起点）: v=±1
    put(0, XMVectorAdd(p0, half), 0.0f, +1.0f);
    put(1, XMVectorSubtract(p0, half), 0.0f, -1.0f);
    // s=1（終点）: v=±1
    put(2, XMVectorAdd(p1, half), 1.0f, +1.0f);
    put(3, XMVectorSubtract(p1, half), 1.0f, -1.0f);


    // VB へ書き込み（WRITE_DISCARD）
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(m_context->Map(m_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, vtx, sizeof(vtx));
        m_context->Unmap(m_vb, 0);
    }
}

bool LaserRenderer::LoadShaders(void)
{
    bool loadShaders = true;

    loadShaders &= ShaderManager::get_instance().HasShaderSet(ShaderSetID::Laser);
    if (loadShaders)
        m_shaderSet = ShaderManager::get_instance().GetShaderSet(ShaderSetID::Laser);

    return loadShaders;
}

bool LaserRenderer::SegmentAABBIntersect(const XMFLOAT3& p0, const XMFLOAT3& p1, const XMFLOAT3& bmin, const XMFLOAT3& bmax, float* outT, XMFLOAT3* outN)
{
    XMFLOAT3 d = { p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
    float tmin = 0.0f, tmax = 1.0f;
    XMFLOAT3 n = { 0,0,0 };

    auto axis = [&](float p, float dp, float mn, float mx, XMFLOAT3 axN)->bool {
        if (fabsf(dp) < 1e-6f) { // 平行
            if (p < mn || p > mx) return false;
            return true;
        }
        float ood = 1.0f / dp;
        float t1 = (mn - p) * ood;
        float t2 = (mx - p) * ood;
        XMFLOAT3 n1 = axN, n2 = { -axN.x, -axN.y, -axN.z };
        if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }
        if (t1 > tmin) { tmin = t1; n = n1; }
        if (t2 < tmax) { tmax = t2; }
        if (tmin > tmax) return false;
        return true;
        };

    if (!axis(p0.x, d.x, bmin.x, bmax.x, { -1,0,0 })) return false;
    if (!axis(p0.y, d.y, bmin.y, bmax.y, { 0,-1,0 })) return false;
    if (!axis(p0.z, d.z, bmin.z, bmax.z, { 0,0,-1 })) return false;

    if (outT) *outT = tmin;
    if (outN) *outN = n;
    return true;
}

void LaserRenderer::DrawOnePlane(float theta, float weight)
{
    // ジオメトリ更新（先端位置に合わせて4頂点を作る）
    BuildQuadVerticesAtAngle(theta);
    UpdateConstantBuffer(weight);

    // ドロー（2トライアングル）
    m_context->DrawIndexed(6, 0, 0);
}

void LaserRenderer::UpdateConstantBuffer(float weight)
{
    CBLaserCPU cb{};
    cb.ColorHDR = XMFLOAT4(m_color.x * weight, m_color.y * weight, m_color.z * weight, m_color.w); // HDR色
    cb.Time = m_timer.GetElapsedTime(); // アニメ用
    cb.FlowSpeed = m_params.flowSpeed;
    cb.CorePower = m_params.corePower;
    cb.Radius = m_params.radiusWorld;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(m_context->Map(m_cbLaser, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &cb, sizeof(cb));
        m_context->Unmap(m_cbLaser, 0);
    }
}

XMVECTOR LaserRenderer::RotateTowardWithMaxStep(XMVECTOR d0, XMVECTOR dTarget, float maxStepRad, float deadZoneRad)
{
    d0 = XMVector3Normalize(d0);
    dTarget = XMVector3Normalize(dTarget);

    float c = XMVectorGetX(XMVector3Dot(d0, dTarget));
    c = Clamp(c, -1.0f, 1.0f);
    float angle = acosf(c);

    if (angle <= deadZoneRad || maxStepRad <= 0.0f) {
        return dTarget; // ほぼ同方向 or 制限なし
    }

    float step = min(angle, maxStepRad);

    // 回転軸 d0×dTarget（退化対策）
    XMVECTOR axis = XMVector3Cross(d0, dTarget);
    if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-8f) 
    {
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        axis = XMVector3Cross(d0, up);
        if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-8f) 
        {
            up = XMVectorSet(1, 0, 0, 0);
            axis = XMVector3Cross(d0, up);
        }
    }
    axis = XMVector3Normalize(axis);

    XMVECTOR q = XMQuaternionRotationAxis(axis, step);
    XMVECTOR dNew = XMVector3Normalize(XMVector3Rotate(d0, q));
    return dNew;
}

void LaserRenderer::HandleDecalOnHit(float dt, const XMFLOAT3& hitPos, const XMFLOAT3& hitN)
{
    // タイマ更新（ヒット中のみ進める）
    m_state.decalTimer += max(0.0f, dt);

    // サイズ乱数（Begin 毎にシードを変えるなら別途）
    float size = m_decal.sizeMin;
    if (m_decal.sizeMax > m_decal.sizeMin) 
    {
		float t = GetRandFloat(0.0f, 1.0f);
        size = m_decal.sizeMin + (m_decal.sizeMax - m_decal.sizeMin) * t;
    }

    XMFLOAT3 normal = NormalizeSafe(hitN);

    ParticleEffectParams smokeParams;
    smokeParams.type = EffectType::Smoke;
    smokeParams.position = hitPos;
    smokeParams.position.y += 80.0f; // 少し上に配置
    smokeParams.scale = 250.0f;
    smokeParams.acceleration = XMFLOAT3(0.0f, 15.0f, 0.0f);
    smokeParams.spawnRateMin = 2.0f;
    smokeParams.spawnRateMax = 5.0f;
    smokeParams.duration = 3.0f; // 短い時間で消える
    smokeParams.lifeMin = 1.5f;
    smokeParams.lifeMax = 2.0f;
    smokeParams.startColor = XMFLOAT4(0.3f, 0.3f, 0.3f, 0.1f); // 煙色

	// 最初の一発を生成
    if (!m_state.firstHitSpawned)
    {
        m_state.firstHitSpawned = true;
        m_state.decalTimer = 0.0f;
        m_state.hasLastDecal = true;
        m_state.lastDecalPos = hitPos;
        m_state.lastDecalN = normal;

		DecalRenderer::get_instance().AddDecal(
            m_state.lastDecalPos,
            m_state.lastDecalN,
			size,
			40.0f
		);
        EffectSystem::get_instance().SpawnParticleEffect(smokeParams);
        return;
    }

    // 連続ヒット：クールダウンと空間条件で間引き
    bool canSpawn = false;

    if (!m_state.hasLastDecal)
    {
        // 念のためのフォールバック
        canSpawn = (m_state.decalTimer >= m_decal.cooldownSec);
    }
	else
	{
        // 法線が十分変わった（＝面が変わった等）→ 即時許可
        const float dotLast = Dot(normal, m_state.lastDecalN);
        if (dotLast <= m_decal.minNormalDot)
        {
            canSpawn = true;
        }
        else
        {
            // 通常ケース：クールダウン + 3D距離で間引き
            const float d = Dist(hitPos, m_state.lastDecalPos);
            if (m_state.decalTimer >= m_decal.cooldownSec && d >= m_decal.minDistance)
            {
                canSpawn = true;
            }
        }
	}

    if (canSpawn)
    {
        m_state.decalTimer = 0.0f;
        m_state.hasLastDecal = true;
        m_state.lastDecalPos = hitPos;
        m_state.lastDecalN = normal;

        DecalRenderer::get_instance().AddDecal(
            m_state.lastDecalPos,
            m_state.lastDecalN,
            size,
            40.0f
        );
        EffectSystem::get_instance().SpawnParticleEffect(smokeParams);
    }
}

void LaserRenderer::ScaleAabbAboutCenter(const XMFLOAT3& inMin, const XMFLOAT3& inMax, float sx, float sy, float sz, XMFLOAT3& outMin, XMFLOAT3& outMax, float extEps)
{
    // 中心と半径（半サイズ）
    const XMFLOAT3 c = XMFLOAT3((inMin.x + inMax.x) * 0.5f,
        (inMin.y + inMax.y) * 0.5f,
        (inMin.z + inMax.z) * 0.5f);
    XMFLOAT3 e = XMFLOAT3((inMax.x - inMin.x) * 0.5f,
        (inMax.y - inMin.y) * 0.5f,
        (inMax.z - inMin.z) * 0.5f);

    // スケール適用（過縮小のクランプ）
    e.x = max(extEps, e.x * sx);
    e.y = max(extEps, e.y * sy);
    e.z = max(extEps, e.z * sz);

    // 復元
    outMin = XMFLOAT3(c.x - e.x, c.y - e.y, c.z - e.z);
    outMax = XMFLOAT3(c.x + e.x, c.y + e.y, c.z + e.z);
}

void LaserRenderer::SetDirectionSmooth(const XMFLOAT3& newDirNorm, float smooth)
{
    const float a = ScalarSaturate(1.0f - smooth);

    XMVECTOR d0 = XMVector3Normalize(XMLoadFloat3(&m_state.dir));
    XMVECTOR d1 = XMVector3Normalize(XMLoadFloat3(&newDirNorm));

    // 線形補間 → 正規化（近似slerp）
    XMVECTOR d = XMVector3Normalize(XMVectorLerp(d0, d1, a));
    XMStoreFloat3(&m_state.dir, d);

	m_axisSmoothing = smooth;
}

void LaserRenderer::ArmDirectionTracking(const XMFLOAT3& initialDirNorm, const DirectionTrackParams& params)
{
    // 初期向きを正規化して保持
    XMVECTOR d = XMVector3Normalize(XMLoadFloat3(&initialDirNorm));
    XMFLOAT3 dn; XMStoreFloat3(&dn, d);

    m_track.armed = true;
    m_track.params = params;
    m_track.delayLeft = max(0.0f, params.preFireDelaySec);
    m_track.lockedDir = dn;

    // レーザー本体の方向も初期化（遅延中はこれで固定）
    m_state.dir = dn;
}

void LaserRenderer::UpdateDirection(const XMFLOAT3& desiredDirNorm, float dtSec)
{
    // ★ 遅延中：初期方向を保持してタイマーを減らす
    if (m_track.armed && m_track.delayLeft > 0.0f)
    {
        m_track.delayLeft = max(0.0f, m_track.delayLeft - max(0.0f, dtSec));
        // ロック中は m_state.dir を固定
        m_state.dir = m_track.lockedDir;
        return;
    }

    // 目標と現在
    XMVECTOR d0 = XMVector3Normalize(XMLoadFloat3(&m_state.dir));
    XMVECTOR d1 = XMVector3Normalize(XMLoadFloat3(&desiredDirNorm));

    // ---- 1) 指数平滑（任意）-------------------------------------
    // a = 1 - exp(-dt/τ)（τ<=0 ならスキップ）
    XMVECTOR dAfterSmooth = d1;
    if (m_track.params.smoothTauSec > 0.0f)
    {
        float a = 1.0f - expf(-max(dtSec, 0.0f) / m_track.params.smoothTauSec);
        a = Clamp(a, 0.0f, 1.0f);
        // 近似slerp：線形補間→正規化
        dAfterSmooth = XMVector3Normalize(XMVectorLerp(d0, d1, a));
    }

    // ---- 2) 角速度制限（任意）-----------------------------------
    XMVECTOR dNew = dAfterSmooth;
    if (m_track.params.maxDegPerSec > 0.0f)
    {
        float maxStep = DegToRad(m_track.params.maxDegPerSec) * max(dtSec, 0.0f);
        float dead = DegToRad(max(0.0f, m_track.params.deadZoneDeg));

        dNew = RotateTowardWithMaxStep(d0, dAfterSmooth, maxStep, dead);
    }

    // ---- 3) 安全用クランプ（平滑だけ指定時の暴れ抑制）-----------
    if (m_track.params.clampTurnRateDegPerSec > 0.0f)
    {
        float maxStep = DegToRad(m_track.params.clampTurnRateDegPerSec) * max(dtSec, 0.0f);
        float dead = DegToRad(max(0.0f, m_track.params.deadZoneDeg));

        dNew = RotateTowardWithMaxStep(d0, dNew, maxStep, dead);
    }

    XMFLOAT3 dn; XMStoreFloat3(&dn, dNew);
    m_state.dir = dn;
}


