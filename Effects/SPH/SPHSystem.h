#pragma once
//=============================================================================
//
// [SPHSystem.h]
// Author : 
// 
// SPH（Smoothed Particle Hydrodynamics）による流体シミュレーションを管理する中核クラス。
// 粒子データの更新、グリッド構築、厚み描画などを統合的に制御する。
// 
//=============================================================================
// 
// 【SPH 粒子グリッド構築と検索の流れ】
//
// 1. CPU 側で ReadBack により、前フレームの生存パーティクルを取得。
// 2. 各パーティクルの位置から、空間グリッドを構築（gridMin を更新し、負の座標を防ぐ）。
// 3. グリッド上の各セルに対し、Morton 番号を計算し、mortonMap にパーティクル ID を蓄積。
// 4. mortonMap の全要素を TempTriplet に変換し、Morton 番号でソート。
// 5. ソート済みの Triplet を元に、SPHIndexTriplet を構築。
// 6. SPHIndexTriplet から PagedIndexTripletBuffer を生成し、GPU にアップロード。
// 
// ─ 次フレーム以降 ─
// 
// 7. 各パーティクルは、前フレームの prevPosition を用いて、周囲セルの Morton 番号を取得。
// 8. PagedIndexTripletBuffer を用いて Morton 番号で近傍セルを探索（FindGridPageHeader）。
// 9. 対象セルに含まれるパーティクル ID を g_SortedIndicesSRV から参照し、
//    g_ParticlesUAV で情報を取得して近傍粒子との相互作用を計算する。
//
// ※ gridMin は BuildGrid 実行後に確定し、CBuffer に反映される必要がある点に注意！
//=============================================================================
#include "Effects/ParticleEffectRendererBase.h"
#include "Effects/SPH/SPHGridUploader.h"
#include "Effects/SpecialEffects/Water/FluidThicknessRenderer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_PARTICLES_PER_CELL      64          // 1 セルあたりの最大パーティクル数（SPH 粒子グリッドのセルあたりの最大粒子数）
#define PRESSURE_KERNEL_SCALE       1200.0f     // カーネルスケール係数（SPH カーネルのスケーリング）
#define VISCOSITY_KERNEL_SCALE      500.0f       // 粘性カーネルスケール係数（SPH 粘性カーネルのスケーリング）
#define PRESSURE_FORCE_SCALE        100.0f      // 圧力力スケール係数（圧力計算に使用されるスケーリング係数）100.0f      // 圧力力スケール係数（圧力計算に使用されるスケーリング係数）
#define VISCOSITY_FORCE_SCALE       0.25f       // 粘性力スケール係数（粘性力の計算に使用されるスケーリング係数）
#define TAIT_EXPONENT               7.0f        // Tait 式の指数（圧力計算用）
#define MAX_PRESSURE                10000.0f    // 最大圧力制限（過度な圧力を防ぐため）
#define XSPH_FACTOR                 0.05f       // XSPH 粘性補正係数（0.0f で無効化）
#define MAX_ACCELERATION            500.0f      // 最大加速度（過度な加速度を防ぐため）
#define MAX_VELOCITY                300.0f      // 最大速度（過度な速度を防ぐため）

//*********************************************************
// 構造体
//*********************************************************
struct CBSPHSimParams
{
    float    smoothingRadius;           // h (SPH 平滑半径)
    float    particleMass;              // 質量 m
    float    restDensity;               // 目標密度 ρ0
    float    pressureMultiplier;        // 圧力係数 k (圧力 = k * (密度 - ρ0))

	float    viscosity;                 // 粘性係数 ν (粘性力 = ν * (速度差 / 距離))
    XMFLOAT3 acceleration;              // 外力 (重力など)

    XMUINT3  gridDim;                   // グリッド分割数 (x,y,z)
	UINT     gridCellStart;             // グリッド開始インデックス (0 から始まる)

    XMFLOAT3 gridMin;                   // ワールド空間でのグリッド最小座標
	UINT     maxParticleCount;          // 総粒子数

    float    cellSize;                  // 1 セルの大きさ (= h 推奨)
    UINT     maxParticlesPerCell;       // 1 セルあたりの最大パーティクル数

	float    deltaTime;                 // タイムステップ (Δt)
	UINT     totalPageHeaderCount;      // グリッドページヘッダの総数

	float    pressureKernelScale;       // 圧力カーネルスケール係数（SPH カーネルのスケーリング）
	float    viscosityKernelScale;      // 粘性カーネルスケール係数（SPH 粘性カーネルのスケーリング）
    float    taitExponent;              // Tait 式の指数（圧力計算用）
    float    maxPressure;               // 最大圧力制限（過度な圧力を防ぐため）
	float    xsphFactor;                // XSPH 粘性補正係数（0.0f で無効化）

    float    restitution;               // 反発係数（衝突時の挙動制御用）
    float    friction;                  // 摩擦係数（衝突時の摩擦挙動制御用）
	float    pressureForceScale;        // 圧力力スケール係数（圧力計算に使用されるスケーリング係数）
	float    viscosityForceScale;       // 粘性力スケール係数（粘性力の計算に使用されるスケーリング係数）

	float    maxAccel;                  // 最大加速度（過度な加速度を防ぐため）
	float    maxVelocity;               // 最大速度（過度な速度を防ぐため）

    float       padding;                // 16バイトアライメント用のパディング
};

// SPH シミュレーションの設定
struct SPHConfig
{
    bool     useSparseGrid = false; // スパースグリッドを使用するかどうか
	bool     recordActiveVoxelCoords = false; // アクティブなボクセル座標を記録するかどうか
    UINT     maxParticles = 0;  // 最大パーティクル数
    XMFLOAT3 gridMin = { -100.0f, -100.0f, -100.0f }; // グリッドの最小座標
    float    cellSize = 1.0f; // グリッドセルのサイズ
    XMUINT3  gridDim = { 128, 128, 128 };  // グリッドの分割数 (x, y, z)
    UINT     maxParticlesPerCell = MAX_PARTICLES_PER_CELL; // セルあたりの最大パーティクル数
    float    smoothingRadius = 1.0f; // SPH 平滑半径 (h)
    float    particleMass = 1.0f; // パーティクル質量
    float    restDensity = 1.0f; // 静止時の密度
    float    pressureMultiplier = 0.1f; // 圧力係数
    float    viscosity = 0.01f; // 粘性係数
	float    friction = 0.1f; // 摩擦係数   
	float    restitution = 0.5f; // 反発係数
    XMFLOAT3 acceleration = { 0.0f, -9.81f, 0.0f }; // 重力などの外力
};

class SPHSystem
{
public:
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const SPHConfig& config, 
        ParticleBufferSet particleBufferSet, ParticleBufferSet aliveListBufferSet);
    void Update(void);
	void Draw(void);
    void Shutdown(void);
    void SetConfig(const SPHConfig& config); // 設定を更新
    void SwapGridBuffers(void) { m_currentGridIndex = 1 - m_currentGridIndex; }
	const SimpleArray<WaterFluidParticle>& GetAliveParticles(void) const { return m_prevFrameAliveListCPU; }
    const BOUNDING_BOX& ComputeBoundsFromAliveParticles(void); // 粒子のAABBを計算する関数（CPU側）
	const float GetCellSize(void) const { return m_config.cellSize; } // セルサイズを取得
	const float GetSmoothingRadius(void) const { return m_config.smoothingRadius; } // 平滑化半径を取得
	ID3D11ShaderResourceView* GetThicknessSRV(void) const { return m_thicknessRenderer.GetSRV(); } // 流体の厚みを描画するためのSRVを取得
	const UINT GetActiveVoxelCount(void) { return m_gridUploader.GetActiveTripletCount(); } // アクティブなボクセル数を取得
    const SimpleArray<SPHIndexTriplet>& GetGridTriplets(void) const { return m_gridUploader.GetGridTriplets(); }
    void ExportInitialClusterLabels(SimpleArray<UINT>& outLabels) const { m_gridUploader.ExportInitialClusterLabels(outLabels); }

private:
	bool CreateConstantBuffer(void);    // 定数バッファの作成
	bool CreateReadbackBuffers(void);   // 生存パーティクルリストの読み取り用バッファの作成
	void UpdateSPHSimParamsCB(void);    // SPH シミュレーションパラメータの更新
    bool LoadComputeShaders(void);      // ComputeShaderの読み込み
    void BindResources_BuildGrid(void); // グリッド構築ステージに必要なリソースをバインド
    void BindResources_Simulate(void);  // SPH シミュレーションステージに必要なリソースをバインド
	void UpdateAliveList(void);         // 生存パーティクルリストの読み取り

    SPHConfig m_config;             // SPH シミュレーション設定
	SPHGridUploader m_gridUploader; // グリッドアップローダー

	BOUNDING_BOX m_bounds; // 粒子のAABB（Axis-Aligned Bounding Box）
	float m_pressureKernelScale = PRESSURE_KERNEL_SCALE; // 圧力カーネルスケール係数（SPH カーネルのスケーリング）
	float m_viscosityKernelScale = VISCOSITY_KERNEL_SCALE; // 粘性カーネルスケール係数（SPH 粘性カーネルのスケーリング）
    float m_taitExponent = TAIT_EXPONENT;// Tait 式の指数（圧力計算用）
	float m_maxPressure = MAX_PRESSURE; // 最大圧力制限（過度な圧力を防ぐため）
	float m_xsphFactor = XSPH_FACTOR; // XSPH 粘性補正係数（0.0f で無効化）
	float m_maxAccel = MAX_ACCELERATION; // 最大加速度（過度な加速度を防ぐため）
	float m_maxVelocity = MAX_VELOCITY; // 最大速度（過度な速度を防ぐため）

	SimpleArray<WaterFluidParticle> m_prevFrameAliveListCPU;    // 前フレームの生存パーティクルリスト (CPU側)

    ParticleBufferSet m_aliveListBufferSet;             // 生存パーティクルリスト用バッファセット
	ID3D11Buffer* m_aliveListReadbackBuffer = nullptr;  // 生存パーティクルリストの読み取り用バッファ
	ID3D11Buffer* m_particleReadbackBuffer = nullptr;   // パーティクルデータの読み取り用バッファ

    FluidThicknessRenderer m_thicknessRenderer; // 流体の厚みを描画するためのレンダラー
    // GPU Alive Count
    ID3D11Buffer* m_aliveCountBuffer = nullptr;   // CopyStructureCount 用
	UINT m_aliveListCount = 0;          // 生存パーティクルの数

    UINT m_totalGridCells = 0;  // 総グリッドセル数
    UINT m_gridCellStart = 0;   // グリッド開始インデックス (0 から始まる)

	ID3D11Buffer* m_cbSPHParams = nullptr; // SPH シミュレーションパラメータ用定数バッファ

    ParticleBufferSet m_sortedIndicesBufferSet;     // パーティクルソート用インデックスバッファセット
	ParticleBufferSet m_gridCounterBufferSet;       // グリッドカウンターバッファセット
    ParticleBufferSet m_gridStartIndexBufferSet[2]; // グリッド開始インデックスバッファセット
    int m_currentGridIndex = 0; // 現在のグリッドバッファインデックス (0 または 1)

    // パーティクルバッファセット
    ParticleBufferSet m_particleBufferSet;

    ComputeShaderSet m_buildGridCS;
    ComputeShaderSet m_simulateCS;

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    Timer& m_timer = Timer::get_instance();
    ShaderResourceBinder& m_shaderResourceBinder = ShaderResourceBinder::get_instance();
};
