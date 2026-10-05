#pragma once
//=============================================================================
//
// AABB（軸対称境界ボックス）ユーティリティ [AABBUtils.h]
// Author : 
// 境界ボックスの生成・交差判定・点包含判定・座標変換など、
// バウンディングボックスに関する各種計算処理を提供する
// 
//=============================================================================
#include "main.h"
#include "Utility/SimpleArray.h"

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct SKINNED_MESH_BOUNDING_BOX
{
    XMFLOAT3 minPoint;
    XMFLOAT3 maxPoint;
    XMFLOAT3 baseMinPoint;
    XMFLOAT3 baseMaxPoint;
    ID3D11Buffer* BBVertexBuffer;
    int	boneIdx;

    SKINNED_MESH_BOUNDING_BOX()
    {
        minPoint = XMFLOAT3(0.0f, 0.0f, 0.0f);
        maxPoint = XMFLOAT3(0.0f, 0.0f, 0.0f);
        baseMinPoint = XMFLOAT3(0.0f, 0.0f, 0.0f);
        baseMaxPoint = XMFLOAT3(0.0f, 0.0f, 0.0f);
        BBVertexBuffer = nullptr;
        boneIdx = 0;
    }

    SKINNED_MESH_BOUNDING_BOX(const XMFLOAT3& min, const XMFLOAT3& max)
    {
        minPoint = min;
        maxPoint = max;
        baseMinPoint = min;
        baseMaxPoint = max;
        BBVertexBuffer = nullptr;
        boneIdx = 0;
    }


    ~SKINNED_MESH_BOUNDING_BOX()
    {
        SafeRelease(&BBVertexBuffer);
    }

    // 指定した点がこの AABB 内に含まれるか判定する関数
    bool contains(const XMFLOAT3& point) const
    {
        return (point.x >= minPoint.x && point.x <= maxPoint.x &&
            point.y >= minPoint.y && point.y <= maxPoint.y &&
            point.z >= minPoint.z && point.z <= maxPoint.z);
    }

    // 他の AABB と交差するかどうか判定する関数
    bool intersects(const SKINNED_MESH_BOUNDING_BOX& other) const
    {
        return (minPoint.x <= other.maxPoint.x && maxPoint.x >= other.minPoint.x &&
            minPoint.y <= other.maxPoint.y && maxPoint.y >= other.minPoint.y &&
            minPoint.z <= other.maxPoint.z && maxPoint.z >= other.minPoint.z);
    }
};

struct BOUNDING_BOX
{
    XMFLOAT3 minPoint;
    XMFLOAT3 maxPoint;

    BOUNDING_BOX()
    {
        minPoint = XMFLOAT3(0.0f, 0.0f, 0.0f);
        maxPoint = XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    BOUNDING_BOX(const XMFLOAT3& min, const XMFLOAT3& max)
    {
        minPoint = min;
        maxPoint = max;
    }

    float GetWidth(void) const
    {
        return maxPoint.x - minPoint.x;
    }

    float GetDepth(void) const
    {
        return maxPoint.z - minPoint.z;
    }

	float GetHeight(void) const
	{
		return maxPoint.y - minPoint.y;
	}

	XMFLOAT3 GetSize(void) const
	{
		return XMFLOAT3(GetWidth(), GetHeight(), GetDepth());
	}

    void Expand(float margin)
    {
        minPoint.x -= margin;
        minPoint.y -= margin;
        minPoint.z -= margin;
        maxPoint.x += margin;
        maxPoint.y += margin;
        maxPoint.z += margin;
    }

    // 中心点を計算する関数
    XMFLOAT3 GetCenter(void) const
    {
        return XMFLOAT3(
            (minPoint.x + maxPoint.x) * 0.5f,
            (minPoint.y + maxPoint.y) * 0.5f,
            (minPoint.z + maxPoint.z) * 0.5f
        );
    }

    // 指定した点がこの AABB 内に含まれるか判定する関数
    bool contains(const XMFLOAT3& point) const
    {
        return (point.x >= minPoint.x && point.x <= maxPoint.x &&
            point.y >= minPoint.y && point.y <= maxPoint.y &&
            point.z >= minPoint.z && point.z <= maxPoint.z);
    }

    // 他の AABB と交差するかどうか判定する関数
    bool intersects(const BOUNDING_BOX& other) const
    {
        return (minPoint.x <= other.maxPoint.x && maxPoint.x >= other.minPoint.x &&
            minPoint.y <= other.maxPoint.y && maxPoint.y >= other.minPoint.y &&
            minPoint.z <= other.maxPoint.z && maxPoint.z >= other.minPoint.z);
    }

    // min/max を変換したAABBを返す（ワールド空間のAABB）
    BOUNDING_BOX TransformAABB(const XMMATRIX& worldMatrix) const
    {
        XMFLOAT3 corners[8];
        GetCorners(corners);

        XMVECTOR minV = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.0f);
        XMVECTOR maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f);

        for (int i = 0; i < 8; ++i)
        {
            XMVECTOR corner = XMLoadFloat3(&corners[i]);
            XMVECTOR transformed = XMVector3Transform(corner, worldMatrix);

            minV = XMVectorMin(minV, transformed);
            maxV = XMVectorMax(maxV, transformed);
        }

        BOUNDING_BOX result;
        XMStoreFloat3(&result.minPoint, minV);
        XMStoreFloat3(&result.maxPoint, maxV);
        return result;
    }


    void GetCorners(XMVECTOR outCorners[8]) const
    {
        const XMVECTOR minV = XMLoadFloat3(&minPoint);
        const XMVECTOR maxV = XMLoadFloat3(&maxPoint);

        outCorners[0] = XMVectorSelect(maxV, minV, XMVectorSelectControl(1, 1, 1, 0)); // (minX, minY, minZ)
        outCorners[1] = XMVectorSelect(minV, maxV, XMVectorSelectControl(0, 1, 1, 0)); // (maxX, minY, minZ)
        outCorners[2] = XMVectorSelect(minV, maxV, XMVectorSelectControl(1, 0, 1, 0)); // (minX, maxY, minZ)
        outCorners[3] = XMVectorSelect(minV, maxV, XMVectorSelectControl(0, 0, 1, 0)); // (maxX, maxY, minZ)
        outCorners[4] = XMVectorSelect(minV, maxV, XMVectorSelectControl(1, 1, 0, 0)); // (minX, minY, maxZ)
        outCorners[5] = XMVectorSelect(minV, maxV, XMVectorSelectControl(0, 1, 0, 0)); // (maxX, minY, maxZ)
        outCorners[6] = XMVectorSelect(minV, maxV, XMVectorSelectControl(1, 0, 0, 0)); // (minX, maxY, maxZ)
        outCorners[7] = XMVectorSelect(minV, maxV, XMVectorSelectControl(0, 0, 0, 0)); // (maxX, maxY, maxZ)
    }

    void GetCorners(XMFLOAT3 outCorners[8]) const
    {
        const float xMin = minPoint.x;
        const float yMin = minPoint.y;
        const float zMin = minPoint.z;

        const float xMax = maxPoint.x;
        const float yMax = maxPoint.y;
        const float zMax = maxPoint.z;

        outCorners[0] = XMFLOAT3(xMin, yMin, zMin);
        outCorners[1] = XMFLOAT3(xMax, yMin, zMin);
        outCorners[2] = XMFLOAT3(xMin, yMax, zMin);
        outCorners[3] = XMFLOAT3(xMax, yMax, zMin);
        outCorners[4] = XMFLOAT3(xMin, yMin, zMax);
        outCorners[5] = XMFLOAT3(xMax, yMin, zMax);
        outCorners[6] = XMFLOAT3(xMin, yMax, zMax);
        outCorners[7] = XMFLOAT3(xMax, yMax, zMax);
    }

    // AABBとレイの交差判定
    bool IntersectRay(const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDir, float* dist, XMFLOAT3* hitPos) const
    {
        float tmin = 0.0f;
        float tmax = FLT_MAX;

        // XYZそれぞれの軸に対してスラブ法を使用
        for (int i = 0; i < 3; ++i)
        {
            float origin = (&rayOrigin.x)[i];
            float direction = (&rayDir.x)[i];
            float minVal = (&minPoint.x)[i];
            float maxVal = (&maxPoint.x)[i];

            if (fabsf(direction) < 1e-6f)
            {
                // レイが平行でAABBのスラブ外 → 当たらない
                if (origin < minVal || origin > maxVal)
                    return false;
            }
            else
            {
                float invD = 1.0f / direction;
                float t1 = (minVal - origin) * invD;
                float t2 = (maxVal - origin) * invD;

                if (t1 > t2) std::swap(t1, t2);

                tmin = max(tmin, t1);
                tmax = min(tmax, t2);

                if (tmin > tmax)
                    return false; // 分離している
            }
        }

        if (dist) *dist = tmin;

        if (hitPos)
        {
            XMVECTOR origin = XMLoadFloat3(&rayOrigin);
            XMVECTOR dir = XMLoadFloat3(&rayDir);
            XMVECTOR point = XMVectorAdd(origin, XMVectorScale(dir, tmin));
            XMStoreFloat3(hitPos, point);
        }

        return true;
    }
};

struct Triangle
{
    XMFLOAT3 v0, v1, v2;
    BOUNDING_BOX aabb;
    XMFLOAT3 normal;

	Triangle() : v0(XMFLOAT3(0.0f, 0.0f, 0.0f)), v1(XMFLOAT3(0.0f, 0.0f, 0.0f)), v2(XMFLOAT3(0.0f, 0.0f, 0.0f)), normal(XMFLOAT3(0.0f, 1.0f, 0.0f))
	{
		aabb.minPoint = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
		aabb.maxPoint = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	}

    // AABBを計算
    Triangle(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, bool alwaysFaceUp = false) : v0(a), v1(b), v2(c)
    {
        aabb.minPoint.x = min(min(v0.x, v1.x), v2.x);
        aabb.minPoint.y = min(min(v0.y, v1.y), v2.y);
        aabb.minPoint.z = min(min(v0.z, v1.z), v2.z);
        aabb.maxPoint.x = max(max(v0.x, v1.x), v2.x);
        aabb.maxPoint.y = max(max(v0.y, v1.y), v2.y);
        aabb.maxPoint.z = max(max(v0.z, v1.z), v2.z);

        // 法線を計算する（頂点座標から交叉積を利用して正規化する）
        XMVECTOR v0Vec = XMLoadFloat3(&v0);
        XMVECTOR v1Vec = XMLoadFloat3(&v1);
        XMVECTOR v2Vec = XMLoadFloat3(&v2);
        XMVECTOR edge1 = XMVectorSubtract(v1Vec, v0Vec);
        XMVECTOR edge2 = XMVectorSubtract(v2Vec, v0Vec);
        XMVECTOR normVec = XMVector3Cross(edge1, edge2);
        normVec = XMVector3Normalize(normVec);

        // 法線のy成分を確認し、負の場合は反転
        if (XMVectorGetY(normVec) < 0.0f && alwaysFaceUp)
        {
            normVec = XMVectorNegate(normVec);  // 法線反転
        }
        XMStoreFloat3(&normal, normVec);
    }

    // コンストラクタ：三角形の法線を直接指定できるようにする
    Triangle(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, const XMFLOAT3& norm)
        : v0(a), v1(b), v2(c), normal(norm)
    {
        aabb.minPoint.x = min(min(v0.x, v1.x), v2.x);
        aabb.minPoint.y = min(min(v0.y, v1.y), v2.y);
        aabb.minPoint.z = min(min(v0.z, v1.z), v2.z);
        aabb.maxPoint.x = max(max(v0.x, v1.x), v2.x);
        aabb.maxPoint.y = max(max(v0.y, v1.y), v2.y);
        aabb.maxPoint.z = max(max(v0.z, v1.z), v2.z);
    }
};

class AABBUtils 
{
public:
    
    // 点の集合からバウンディングボックスを作成する関数
    static BOUNDING_BOX CreateFromPoints(const SimpleArray<XMFLOAT3>& points);
    // 点の集合からバウンディングボックスを作成する関数（XMVECTOR版）
    static BOUNDING_BOX CreateFromPoints(const SimpleArray<XMVECTOR>& points);
};