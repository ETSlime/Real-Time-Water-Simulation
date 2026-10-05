//=============================================================================
//
// 空間加速構造体ちゃん [VoxelGrid.cpp]
// Author : 
// ボクセルベースの空間分割によって、三角形メッシュを3Dグリッドに整理し、
// 高速なクエリ（衝突判定・視界探索・地形サンプリングなど）を可能にする
//
//=============================================================================
#include "Collision/VoxelGrid.h"

// 全ノード共通の静的レジストリ
HashMap<const Triangle*, VoxelHandle*, TrianglePtrHash, TrianglePtrEquals> VoxelGrid::s_registry =
HashMap<const Triangle*, VoxelHandle*, TrianglePtrHash, TrianglePtrEquals>(
    MAX_VOXEL_NUM, // 初期バケット数
    TrianglePtrHash(), // ハッシュ関数
    TrianglePtrEquals() // 等価比較関数
);
SpinLock VoxelGrid::s_registryLock;

VoxelGrid::~VoxelGrid()
{
	m_voxelMap.clear(); // ボクセルマップをクリア
	m_triangles.clear(); // 登録された三角形をクリア
}

void VoxelGrid::Initialize(float voxelSize)
{
	// ボクセルサイズの設定
	m_voxelSize = voxelSize;
    // ボクセルマップの初期化
	m_voxelMap.clear();
    // 登録された三角形のリストの初期化
	m_triangles.clear();
	m_triangles.reserve(MAX_VOXEL_TRIANGLE_NUM); // 初期容量を設定（必要に応じて調整）
	// グリッドの原点を初期化
    m_voxelBounds.minPoint = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
    m_voxelBounds.maxPoint = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
}

void VoxelGrid::BuildTriangles(const SimpleArray<Triangle*>& triangles)
{
    UINT count = triangles.getSize();
    for (UINT i = 0; i < count; ++i)
    {
        // 実質Insertの高速ループ
		InsertTriangle(triangles[i]);
    }
}

void VoxelGrid::InsertTriangle(const Triangle* tri)
{
    // 既にボクセルマップが構築済みなら、全体再構築せずに増分経路へ委譲
    if (m_isReady)
    {
        // 増分挿入は内部で必要なロックを取得する実装前提
        // （範囲外などで失敗する可能性がある場合は戻り値を見てログ等を出す）
        InsertTriangleIncremental(tri);
        return;
    }

    // まだ未構築: 三角形リストに蓄積し、ボクセル境界のみ更新
    if (m_threadSafe)
    {
        SpinLockGuard guard(m_lock); // スピンロックで排他制御
        m_triangles.push_back(tri); // 三角形を追加

		// ボクセルの境界を更新
        ComputeVoxelBounds(tri);
    }
    else
    {
        m_triangles.push_back(tri); // 三角形を追加

		// ボクセルの境界を更新
        ComputeVoxelBounds(tri);
    }
}

void VoxelGrid::BuildVoxelMapByTriangles(void)
{
	if (m_isReady) return; // すでにビルド済みなら何もしない（初期ビルドは1回想定）

    // グリッドの原点を決定（負数を避けて切り下げ）
    m_origin.x = floor(m_voxelBounds.minPoint.x / m_voxelSize) * m_voxelSize;
    m_origin.y = floor(m_voxelBounds.minPoint.y / m_voxelSize) * m_voxelSize;
    m_origin.z = floor(m_voxelBounds.minPoint.z / m_voxelSize) * m_voxelSize;

    UINT count = m_triangles.getSize();
    for (UINT i = 0; i < count; ++i)
    {
		const Triangle* tri = m_triangles[i]; // 現在の三角形を取得
        const BOUNDING_BOX& triBox = tri->aabb; // 三角形のAABBを取得

        // 三角形の境界ボックスからボクセルキーを計算
        VoxelKey minKey = GetVoxelKey(triBox.minPoint);
        VoxelKey maxKey = GetVoxelKey(triBox.maxPoint);

#ifdef _DEBUG
        // EncodeMorton3 は負のキー許容しない
        assert(minKey.x >= 0 && minKey.y >= 0 && minKey.z >= 0);
#endif // DEBUG

        // ボクセルキーの範囲を計算
        for (int x = minKey.x; x <= maxKey.x; ++x)
        {
            for (int y = minKey.y; y <= maxKey.y; ++y)
            {
                for (int z = minKey.z; z <= maxKey.z; ++z)
                {
                    VoxelKey key{ x, y, z };
                    VoxelEntry& voxel = m_voxelMap[key]; // 新規または既存

					voxel.triangles.push_back(tri); // 三角形を追加
                }
            }
        }
    }  

	m_isReady = true; // ビルド完了フラグを設定
}


void VoxelGrid::Query(const XMFLOAT3& center, float radius, SimpleArray<const Triangle*>& outTriangles) const
{
	VoxelKey centerKey = GetVoxelKey(center);
	int radiusVoxels = static_cast<int>(ceil(radius / m_voxelSize));

	// 中心ボクセルから半径分の範囲を計算
    for (int dx = -radiusVoxels; dx <= radiusVoxels; ++dx)
    {
        for (int dy = -radiusVoxels; dy <= radiusVoxels; ++dy)
        {
            for (int dz = -radiusVoxels; dz <= radiusVoxels; ++dz)
            {
				VoxelKey key{ centerKey.x + dx, centerKey.y + dy, centerKey.z + dz };

				// ボクセルキーが存在する場合、対応する三角形を出力
                auto it = m_voxelMap.find(key);
                if (it != m_voxelMap.end())
				{
                    const VoxelEntry& voxel = it->value;

                    // 三角形のリストを出力配列に追加
                    for (const Triangle* tri : voxel.triangles)
                        outTriangles.push_back(tri);
				}
			}
		}
	}
}

bool VoxelGrid::Raycast(const XMFLOAT3& start, const XMFLOAT3& end, XMFLOAT3* hitPos, XMFLOAT3* hitNormal) const
{
    // レイ始点・終点をロード
    XMVECTOR p0 = XMLoadFloat3(&start);
    XMVECTOR p1 = XMLoadFloat3(&end);

    // レイ方向ベクトルを計算
    XMVECTOR dirVec = XMVectorSubtract(p1, p0);
    float length;
    XMStoreFloat(&length, XMVector3Length(dirVec));
    if (length < 1e-6f) return false;

    // 正規化された方向ベクトル
    XMVECTOR dirNormVec = XMVectorScale(dirVec, 1.0f / length);
    XMFLOAT3 dir;
    XMStoreFloat3(&dir, dirNormVec);

    // 始点・終点のボクセル座標
    VoxelKey voxel = GetVoxelKey(start);
    VoxelKey target = GetVoxelKey(end);

    // ボクセル座標の範囲チェック
    auto IsValidVoxel = [&](const VoxelKey& vk) -> bool {
        return (vk.x >= 0 && vk.x < 1024 &&
            vk.y >= 0 && vk.y < 1024 &&
            vk.z >= 0 && vk.z < 1024);
        };

    // ボクセル座標の範囲チェック
    if (!IsValidVoxel(voxel))
		return false; // ボクセルキーが範囲外なら終了


    // DDA用の軸方向ステップ (+1 or -1)
    int stepX = (dir.x > 0) ? 1 : -1;
    int stepY = (dir.y > 0) ? 1 : -1;
    int stepZ = (dir.z > 0) ? 1 : -1;

    // 軸ごとの境界までの距離を計算するラムダ
    auto NextBoundaryDist = [&](float pos, float dirComp, int step) -> float {
        if (fabs(dirComp) < 1e-8f) return FLT_MAX; // 方向成分がゼロなら無限大
        float voxelEdge = (step > 0)
            ? (floorf(pos / m_voxelSize) * m_voxelSize + m_voxelSize)
            : (floorf(pos / m_voxelSize) * m_voxelSize);
        return (voxelEdge - pos) / dirComp;
        };

    // 最初の境界距離
    float tMaxX = NextBoundaryDist(start.x, dir.x, stepX);
    float tMaxY = NextBoundaryDist(start.y, dir.y, stepY);
    float tMaxZ = NextBoundaryDist(start.z, dir.z, stepZ);

    // 各軸で1ボクセル進むごとのtの増加量
    float tDeltaX = (fabs(dir.x) < 1e-8f) ? FLT_MAX : fabs(m_voxelSize / dir.x);
    float tDeltaY = (fabs(dir.y) < 1e-8f) ? FLT_MAX : fabs(m_voxelSize / dir.y);
    float tDeltaZ = (fabs(dir.z) < 1e-8f) ? FLT_MAX : fabs(m_voxelSize / dir.z);

    float traveled = 0.0f;
    float closestT = FLT_MAX;
    XMFLOAT3 closestHit{}, closestHitNormal{};

    // DDAループ: レイが通過するボクセルを順次チェック
    while (traveled <= length) 
    {
        if (!IsValidVoxel(voxel))
			return false; // ボクセルキーが範囲外なら終了

        auto entryIt = m_voxelMap.find(voxel);
        if (entryIt != m_voxelMap.end()) 
        {
            // ボクセル内の三角形と交差判定
            const VoxelEntry& entry = entryIt->value;
            for (const Triangle* tri : entry.triangles) 
            {
                float t;
                XMFLOAT3 hit, hitNormal;
                if (RayIntersectsTriangle(start, dir, tri, &t, &hit, &hitNormal))
                {
                    if (t < closestT && t >= 0.0f && t <= length) 
                    {
                        closestT = t;
                        closestHit = hit;
						closestHitNormal = hitNormal;
                    }
                }
            }
            // 最も近い交点が見つかったら終了
            if (closestT < FLT_MAX) 
            {
                if (hitPos) *hitPos = closestHit;
				if (hitNormal) *hitNormal = closestHitNormal;
                return true;
            }
        }

        // 次のボクセルに進む (tMax が最小の軸を選ぶ)
        if (tMaxX < tMaxY) 
        {
            if (tMaxX < tMaxZ) 
            {
                voxel.x += stepX;
                traveled = tMaxX;
                tMaxX += tDeltaX;
            }
            else 
            {
                voxel.z += stepZ;
                traveled = tMaxZ;
                tMaxZ += tDeltaZ;
            }
        }
        else 
        {
            if (tMaxY < tMaxZ) 
            {
                voxel.y += stepY;
                traveled = tMaxY;
                tMaxY += tDeltaY;
            }
            else 
            {
                voxel.z += stepZ;
                traveled = tMaxZ;
                tMaxZ += tDeltaZ;
            }
        }
    }

    return false;
}

bool VoxelGrid::RemoveTriangleIncremental(const Triangle* tri)
{
    if (!tri) return false;

    VoxelHandle* h = nullptr;
    {
        SpinLockGuard rg(s_registryLock);
        auto it = s_registry.find(tri);
        if (it == s_registry.end()) return false;
        h = it->value;
    }
    if (!h) return false;

    // 参照スナップショット（ハンドル側の RemoveRef と干渉させない）
    SimpleArray<VoxelRef> refs = h->refs;
    bool removedAny = false;

    for (int i = 0; i < refs.getSize(); ++i)
    {
        const VoxelKey key = refs[i].key;
        const int idx = refs[i].indexInCell;

        // 体素から tri を O(1) で削除（グリッド単一ロック）
        const Triangle* moved = nullptr;
        int newIdx = -1;
        if (!RemoveTriFromCell(key, idx, moved, newIdx))
            continue;

        removedAny = true;

        // moved が存在するなら、そのハンドルの当該セル内 index を回写
        if (moved != nullptr)
        {
            VoxelHandle* movedH = nullptr;
            {
                SpinLockGuard rg(s_registryLock);
                auto it = s_registry.find(moved);
                movedH = (it != s_registry.end()) ? it->value : nullptr;
            }
            if (movedH)
                movedH->UpdateIndex(key, newIdx);
        }

        // ハンドルからもこのセル参照を削除
        h->RemoveRef(key);
    }

    // tri → handle をレジストリから消し、ハンドルを破棄
    {
        SpinLockGuard rg(s_registryLock);
		auto it = s_registry.find(tri);
        if (it == s_registry.end()) return false;
        s_registry.erase(it);
    }
    h->Clear();
    SAFE_DELETE(h);

    return removedAny;
}

VoxelKey VoxelGrid::GetVoxelKey(const XMFLOAT3& pos) const
{
    // 負数の座標を避けるため、origin をオフセットとする
    XMFLOAT3 local = XMFLOAT3(
        (pos.x - m_origin.x) / m_voxelSize,
        (pos.y - m_origin.y) / m_voxelSize,
        (pos.z - m_origin.z) / m_voxelSize
    );

	// 指定された位置からボクセルキーを計算
    return VoxelKey{
        static_cast<int>(floorf(local.x)),
        static_cast<int>(floorf(local.y)),
        static_cast<int>(floorf(local.z))
    };
}

void VoxelGrid::ComputeVoxelBounds(const Triangle* tri)
{
    const BOUNDING_BOX& triBox = tri->aabb; // 三角形のAABBを取得

    m_voxelBounds.minPoint.x = min(m_voxelBounds.minPoint.x, triBox.minPoint.x);
    m_voxelBounds.minPoint.y = min(m_voxelBounds.minPoint.y, triBox.minPoint.y);
    m_voxelBounds.minPoint.z = min(m_voxelBounds.minPoint.z, triBox.minPoint.z);

    m_voxelBounds.maxPoint.x = max(m_voxelBounds.maxPoint.x, triBox.maxPoint.x);
    m_voxelBounds.maxPoint.y = max(m_voxelBounds.maxPoint.y, triBox.maxPoint.y);
    m_voxelBounds.maxPoint.z = max(m_voxelBounds.maxPoint.z, triBox.maxPoint.z);
}

void VoxelGrid::print_debug_stats(void) const
{
    char buffer[512];

    sprintf_s(buffer, "[ハッシュマップ統計情報]\n");
    OutputDebugStringA(buffer);

    sprintf_s(buffer, "◆ 要素数           : %zu\n", m_voxelMap.getSize());
    OutputDebugStringA(buffer);

    sprintf_s(buffer, "◆ バケット数       : %d\n", m_voxelMap.bucketCount());
    OutputDebugStringA(buffer);

    sprintf_s(buffer, "◆ 負荷率 (load)    : %.3f\n", m_voxelMap.load_factor());
    OutputDebugStringA(buffer);

    sprintf_s(buffer, "◆ 使用中のバケット : %d\n", m_voxelMap.used_bucket_count());
    OutputDebugStringA(buffer);

    sprintf_s(buffer, "◆ ハッシュ衝突数   : %d\n", m_voxelMap.collision_count());
    OutputDebugStringA(buffer);

    sprintf_s(buffer, "◆ 最大チェーン長   : %d\n", m_voxelMap.max_chain_length());
    OutputDebugStringA(buffer);

    sprintf_s(buffer, "◆ 平均チェーン長   : %.3f\n", m_voxelMap.average_chain_length());
    OutputDebugStringA(buffer);
}

bool VoxelGrid::RayIntersectsTriangle(const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDir, const Triangle* tri, 
    float* outT, XMFLOAT3* outHit, XMFLOAT3* outHitNormal) const
{
    // 頂点座標をロード
    XMVECTOR v0 = XMLoadFloat3(&tri->v0);
    XMVECTOR v1 = XMLoadFloat3(&tri->v1);
    XMVECTOR v2 = XMLoadFloat3(&tri->v2);

    // レイの始点・方向ベクトルをロード
    XMVECTOR orig = XMLoadFloat3(&rayOrigin);
    XMVECTOR dir = XMLoadFloat3(&rayDir);

    // エッジベクトルを計算
    XMVECTOR edge1 = XMVectorSubtract(v1, v0);
    XMVECTOR edge2 = XMVectorSubtract(v2, v0);

    // ハーフベクトル (pvec) を計算 (dir と edge2 の外積)
    XMVECTOR pvec = XMVector3Cross(dir, edge2);

    // デターミナントを計算
    float det;
    XMStoreFloat(&det, XMVector3Dot(edge1, pvec));

    // レイと平行（もしくは裏面カリングする場合）なら無視
    if (fabs(det) < 1e-8f) return false;
    float invDet = 1.0f / det;

    // tvec: レイ始点からv0までのベクトル
    XMVECTOR tvec = XMVectorSubtract(orig, v0);

    // u バリューを計算 (barycentric coordinate)
    float u;
    XMStoreFloat(&u, XMVector3Dot(tvec, pvec));
    u *= invDet;
    if (u < 0.0f || u > 1.0f) return false;

    // qvec: tvec と edge1 の外積
    XMVECTOR qvec = XMVector3Cross(tvec, edge1);

    // v バリューを計算 (barycentric coordinate)
    float v;
    XMStoreFloat(&v, XMVector3Dot(dir, qvec));
    v *= invDet;
    if (v < 0.0f || u + v > 1.0f) return false;

    // レイとの交差距離 t を計算
    float t;
    XMStoreFloat(&t, XMVector3Dot(edge2, qvec));
    t *= invDet;

    if (t < 0.0f) return false; // レイの後方なら無視

    if (outT) *outT = t;

    // 交差点座標を計算
    if (outHit) 
    {
        XMVECTOR hitPos = XMVectorAdd(orig, XMVectorScale(dir, t));
        XMStoreFloat3(outHit, hitPos);
    }

    // 面法線（正規化）を計算
    if (outHitNormal)
    {
        XMVECTOR normal = XMVector3Normalize(XMVector3Cross(edge1, edge2));

        // 法線をレイの方向に合わせて反転（もし内向きなら反転）
        // dot(dir, normal) > 0 の場合、normalはレイの進行方向と同じ向き → 反転
        if (XMVectorGetX(XMVector3Dot(dir, normal)) > 0.0f)
        {
            normal = XMVectorNegate(normal);
        }

        XMStoreFloat3(outHitNormal, normal);
    }

    return true;
}

bool VoxelGrid::InsertTriangleIncremental(const Triangle* tri)
{
    if (!tri) return false;

    // すでに登録済みかチェック（全体レジストリ）
    {
        SpinLockGuard reg(s_registryLock);
        if (s_registry.find(tri) != s_registry.end())
            return false; // 二重挿入は拒否
    }

    // 範囲外の扱い：ここでは簡易に拒否（必要なら Expand を別途実装）
    const BOUNDING_BOX& triBox = tri->aabb;
	if (!m_voxelBounds.intersects(triBox))
        return false;

    // 新規ハンドル確保（堆 or プール）
    VoxelHandle* handle = new VoxelHandle();

    // キー範囲の算出
    VoxelKey minKey = GetVoxelKey(triBox.minPoint);
    VoxelKey maxKey = GetVoxelKey(triBox.maxPoint);

#ifdef _DEBUG
    assert(minKey.x >= 0 && minKey.y >= 0 && minKey.z >= 0);
#endif

    bool any = false;

    // 対象ボクセルへ追加（AddTriToCell_ 内で各種ロックを最小限に取得）
    for (int x = minKey.x; x <= maxKey.x; ++x)
        for (int y = minKey.y; y <= maxKey.y; ++y)
            for (int z = minKey.z; z <= maxKey.z; ++z)
            {
                VoxelKey key{ x, y, z };
                if (AddTriToCell(tri, key, handle))
                    any = true;
            }

    if (!any)
    {
        delete handle;
        return false;
    }

    // tri→handle をレジストリに登録
    {
        SpinLockGuard reg(s_registryLock);
        s_registry[tri] = handle;
    }

    // 任意：m_triangles にも保持したい場合はここで push_back
    // m_triangles.push_back(tri);

    return true;
}

bool VoxelGrid::AddTriToCell(const Triangle* tri, const VoxelKey& key, VoxelHandle* handle)
{
    if (!tri || !handle) return false;

    int idx = -1;
    {
        SpinLockGuard g(m_lock);               // グリッド全体の自旋ロック

        VoxelEntry& entry = m_voxelMap[key];   // 新規 or 既存（unordered_map 構造変更もこのロック内）
        idx = entry.triangles.getSize();       // ボクセル内インデックスを決定
        entry.triangles.push_back(tri);        // 追加
        handle->AddRef(key, idx);              // ハンドルに参照を記録（swap-pop のために必要）
    }
    return true;
}

bool VoxelGrid::RemoveTriFromCell(const VoxelKey& key, int indexInCell, const Triangle*& outMoved, int& outNewIdx)
{
    outMoved = nullptr;
    outNewIdx = -1;

    SpinLockGuard g(m_lock);

    auto it = m_voxelMap.find(key);
    if (it == m_voxelMap.end()) return false;

    VoxelEntry& entry = it->value;
    int last = entry.triangles.getSize() - 1;
    if (indexInCell < 0 || indexInCell > last) return false;

    outMoved = entry.triangles[last];
    entry.triangles[indexInCell] = outMoved;   // 最後尾をスワップ
    entry.triangles.pop_back();
    outNewIdx = indexInCell;                   // moved の新しい位置

    return true;
}
