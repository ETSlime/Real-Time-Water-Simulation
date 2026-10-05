#pragma once
//=============================================================================
//
// [SPHVolumeBuilder.h]
// Author : 
// 
// 流体パーティクルからスカラー場（Scalar Field）を構築するための CPU クラス。
// Marching Cubes によるメッシュ再構築の元データとして使用される体積情報を生成する。
// 
// 機能一覧：
// - InitializeVolume()：Voxel 解像度・原点・半径などの体積パラメータを設定
// - AccumulateFromParticles()：稠密 3D グリッドにスカラー値を蓄積
// - AccumulateFromParticlesSparse()：スパースグリッド形式で効率的に蓄積
// - ComputeVolumeParamsFromParticles()：AABB から適切な体積構築パラメータを自動計算
//
// 出力：
// - GetVolume()：3次元配列形式（稠密）
// - GetScalarFieldMap()：HashMap<VoxelCoord, float> 形式（稀疎）
// - GetVolumeReconstructionParams()：使用したパラメータを取得
// 
//=============================================================================
#include "Effects/ParticleStructs.h"
#include "Effects/SPH/SPHGridUploader.h"
#include "Collision/VoxelGrid.h"

struct VolumeReconstructionParams
{
	bool useSparseGrid = true; // スパースグリッドを使用するかどうか
    float cellSize = 0.05f;                  // ボクセルのサイズ
    int gridX = 64, gridY = 64, gridZ = 64;  // グリッド解像度
	UINT activeVoxelCount = 0;            // アクティブなボクセル数
    float radius = 0.1f;                     // 粒子半径
    float densityScale = 1.0f;               // 密度スケール
	float isoLevel = 0.5f;                // 等値面のしきい値
    XMFLOAT3 origin = { -1.6f, 0.0f, -1.6f }; // グリッドの原点
};

class SPHVolumeBuilder
{
public:
    void Initialize(UINT maxParticles);
    void InitializeVolume(const VolumeReconstructionParams& params);
    void Clear(void);
    void ClearVolume(void);

	// 粒子からボリュームパラメータを計算
    static VolumeReconstructionParams ComputeVolumeParamsFromParticles(const BOUNDING_BOX& aabb,
        float particleRadius, float densityScale, float isoLevel, float cellSize);

	// 粒子からボリュームを構築
    void AccumulateFromParticles(const SimpleArray<WaterFluidParticle>& particles);
    void AccumulateFromParticlesSparse(const VolumeReconstructionParams& params, const SimpleArray<WaterFluidParticle>& particles);

	const VolumeReconstructionParams GetVolumeReconstructionParams(void) const { return m_params; }
    const SimpleArray<SimpleArray<SimpleArray<float>>>& GetVolume(void) const { return m_volume; }
	const HashMap<VoxelCoord, float, HashVoxelKey64, EqualVoxelKey64>& GetScalarFieldMap(void) const { return m_scalarFieldMap; }

private:
    void RegisterVoxel(const VoxelCoord& coord);

    VolumeReconstructionParams m_params;
    SimpleArray<SimpleArray<SimpleArray<float>>> m_volume;

    UINT m_maxParticles = 0; // 最大パーティクル数

	SimpleArray<VoxelCoord> m_writtenVoxelKeys; // 書き込まれたボクセルキーのリスト（スパースグリッド用）

    // ボクセルキーと三角形のマッピングを保持するハッシュマップ
    HashMap<VoxelCoord, float, HashVoxelKey64, EqualVoxelKey64> m_scalarFieldMap;
};