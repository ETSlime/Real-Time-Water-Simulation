#pragma once
//=============================================================================
//
// [FluidHeightFieldBuilder.h]
// Author : 
//
//=============================================================================
#include "Effects/ParticleStructs.h"
#include "Utility/SimpleArray.h"
#include "Collision/AABBUtils.h"

//*********************************************************
// 構造体
//*********************************************************
struct Cell
{
	float heightSum = 0.0f; // 粒子の高さの合計
	float weightSum = 0.0f; // 粒子の重みの合計
	float finalHeight = NAN;
};

class FluidHeightFieldBuilder
{
public:
	FluidHeightFieldBuilder();
	~FluidHeightFieldBuilder();

	void Initialize(float minX, float maxX, float minZ, float maxZ, int gridX, int gridZ);
	void Initialize(const BOUNDING_BOX& bounds, float cellSize);
	void InitializeBySlidingWindow(const BOUNDING_BOX& particleBounds, float cellSize, int fixedGridX, int fixedGridZ);
	
	void Clear(void);

	void AccumulateParticles(const SimpleArray<WaterFluidParticle>& particles);
	void Finalize(void);
	float GetHeight(int gx, int gz) const;

	inline float GetHeightClamped(int gx, int gz) const
	{
		int clampedX = Clamp<int>(gx, 0, m_gridX - 1);
		int clampedZ = Clamp<int>(gz, 0, m_gridZ - 1);
		float h = GetHeight(clampedX, clampedZ);
		return isnan(h) ? 0.0f : h;
	}

	int GetGridX(void) const { return m_gridX; }
	int GetGridZ(void) const { return m_gridZ; }
	float GetMinX(void) const { return m_minX; }
	float GetMinZ(void) const { return m_minZ; }
	float GetCellSizeX(void) const { return m_cellSizeX; }
	float GetCellSizeZ(void) const { return m_cellSizeZ; }

private:
	bool IsEdgeCell(int gx, int gz) const;
	void ApplyGaussianSmoothingToEdges(void);

	SimpleArray<SimpleArray<Cell>> m_grid;
	SimpleArray<SimpleArray<float>> m_smoothBuffer;
	int m_gridX = 0, m_gridZ = 0;
	float m_minX = FLT_MAX, m_maxX = FLT_MIN;
	float m_minZ = FLT_MAX, m_maxZ = FLT_MIN;
	float m_cellSizeX = 0.0f, m_cellSizeZ = 0.0f;
	int m_activeGridX = 0;
	int m_activeGridZ = 0;
};