#pragma once
//=============================================================================
//
// [WaterFluidParticleRenderer.h]
// Author : 
// 
// 水の流体表現を担うパーティクルエフェクトクラス。
// SPH（Smoothed Particle Hydrodynamics）システムを内包し、
// 粒子の放出・更新・描画・表面再構築を一括して行う。
// 
// 特徴：
// - DrawIndirect による GPU ベースの描画
// - SPH + Marching Cubes による高精度かつ視覚的に美しい水表現を実現
// - 複数のメッシュ生成方式（CPU / GPU）を切り替え可能
//
//=============================================================================
#include "Effects/ParticleEffectRendererBase.h"
#include "Effects/SPH/SPHSystem.h"
#include "Effects/MarchingCubes/MarchingCubesSurfaceMeshGPU.h"
#include "Effects/SPH/SPHVolumeBuilderGPU.h"
#include "Collision/VoxelBufferUploader.h"


//*********************************************************
// 構造体
//*********************************************************
struct WaterFluidCB
{
    float supportRadius;      // 影響範囲（半径）
    float restDensity;        // 目標密度（静止状態）
    float particleMass;       // 粒子質量（SPH/PBF共用）

    float stiffness;          // 圧力係数（PBF用）
    float friction;           // 地形との摩擦係数
    float padding1;
    float padding2;
    float padding3;
}; // 水の流体パーティクル用の定数バッファ

struct EmitCB
{
    UINT emitCount;
    XMFLOAT3 emitCenter;
    float  emitRadius;
	XMFLOAT3 emitDirection; // 放出方向（正規化ベクトル）
	float  emitSpeed;       // 放出速度
    XMFLOAT3 _padding; // 16byte alignment
};

class WaterFluidParticleRenderer : public ParticleEffectRendererBase, IDebugUI
{
public:
    // 初期化処理
	virtual bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
    // シャットダウン処理
    virtual void Shutdown(void) override;
    // エフェクトの種類を返す
	EffectType GetEffectType() const override { return EffectType::WaterFluid; }
    // 描画はIndirectで行う
	bool UseDrawIndirect(void) const override { return true; } 
    // パーティクルエフェクトの設定
    virtual void ConfigureEffect(const ParticleEffectParams& params) override;
    // 水の流体パーティクルを指定位置に放出
    void EmitWater(const XMFLOAT3& dir, float speed, float radius, int emitCount, const XMFLOAT3& center = XMFLOAT3{});
    // 放出するパーティクル数を設定
	void SetEmitCount(UINT count) { m_emitCBData.emitCount = count; }


private:
    virtual void DispatchComputeShader(void) override; // パーティクル更新（Dispatch）
	virtual void DrawParticles(void) override; // パーティクル描画（Draw）

	// パーティクル更新用のCompute Shaderをディスパッチ
    virtual void DispatchEmitParticlesCS(void) override;

	bool CreateWaterFluidCB(void); // 水の流体パーティクル用の定数バッファを作成
	void UploadWaterFluidCB(void); // 水の流体パーティクル用の定数バッファをアップロード
	bool CreateEmitCB(void); // パーティクル放出用の定数バッファを作成
	void UploadEmitCB(void); // パーティクル放出用の定数バッファをアップロード
	void UpdateVolumMeshFromSPH(void); // SPHシステムからボリュームメッシュを更新

    virtual void RenderImGui(void) override;
    virtual const char* GetPanelName(void) const override { return "WaterFluid Renderer"; };

    float m_supportRadius = 15.0f;     // 近傍半径（PBF）
    float m_restDensity = 300.0f;       // 静止時の密度
    float m_particleMass = 1.0f;      // 粒子質量
    float m_stiffness = 200.0f;         // 圧力（硬さ）
    float m_friction = 0.05f;         // 地形との摩擦
	float m_viscosity = 0.01f;        // 粘性係数（SPH用）
	float m_restitution = 0.5f; // 反発係数（衝突時の挙動制御用）

	float m_volumeMeshScale = 1.2f; // ボリュームメッシュのスケール
	float m_densityScale = 4.0f; // 密度スケール（ボリュームメッシュ生成用）
	float m_isoLevel = 0.5f; // 等値面のしきい値（ボリュームメッシュ生成用）

	SPHSystem m_sphSystem; // SPHシステム（PBF用）

	SPHVolumeBuilder m_volumeBuilder; // SPHボリュームビルダー（高さフィールドの生成用）
    SPHVolumeBuilderGPU m_volumeBuilderGPU; // SPHボリュームビルダー（高さフィールドの生成用）
	MarchingCubesSurfaceMesh m_volumeMesh; // ボリュームメッシュ生成用
    MarchingCubesSurfaceMeshGPU m_volumeMeshGPU; // ボリュームメッシュ生成用

	EmitCB m_emitCBData;              // パーティクル放出用の定数バッファデータ
    ID3D11ShaderResourceView* m_waterDotTextureSRV = nullptr;
    ID3D11Buffer* m_cbWaterFluid = nullptr;
    ID3D11Buffer* m_cbEmit = nullptr;

    VoxelBufferUploader& m_voxelBufferUploader = VoxelBufferUploader::get_instance();
};
