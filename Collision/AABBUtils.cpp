//=============================================================================
//
// AABBUtilsèàóù [AABBUtils.cpp]
// Author : 
//
//=============================================================================
#include "Collision/AABBUtils.h"

BOUNDING_BOX AABBUtils::CreateFromPoints(const SimpleArray<XMFLOAT3>& points)
{
    if (points.empty())
        return BOUNDING_BOX();

    XMFLOAT3 minPt(+FLT_MAX, +FLT_MAX, +FLT_MAX);
    XMFLOAT3 maxPt(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const auto& p : points)
    {
        minPt.x = min(minPt.x, p.x);
        minPt.y = min(minPt.y, p.y);
        minPt.z = min(minPt.z, p.z);

        maxPt.x = max(maxPt.x, p.x);
        maxPt.y = max(maxPt.y, p.y);
        maxPt.z = max(maxPt.z, p.z);
    }

    return BOUNDING_BOX(minPt, maxPt);
}

BOUNDING_BOX AABBUtils::CreateFromPoints(const SimpleArray<XMVECTOR>& points)
{
    if (points.empty())
        return BOUNDING_BOX();

    XMFLOAT3 minPt(+FLT_MAX, +FLT_MAX, +FLT_MAX);
    XMFLOAT3 maxPt(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const auto& v : points)
    {
        XMFLOAT3 p;
        XMStoreFloat3(&p, v);

        minPt.x = min(minPt.x, p.x);
        minPt.y = min(minPt.y, p.y);
        minPt.z = min(minPt.z, p.z);

        maxPt.x = max(maxPt.x, p.x);
        maxPt.y = max(maxPt.y, p.y);
        maxPt.z = max(maxPt.z, p.z);
    }

    return BOUNDING_BOX(minPt, maxPt);
}