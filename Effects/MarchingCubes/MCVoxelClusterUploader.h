#pragma once
//=============================================================================
//
//  [SPHGridUploader.h]
// Author : 
//
//=============================================================================
#include "Effects/SPH/SPHGridUploader.h"

// 各クラスタが持つ情報
struct ClusterInfo
{
    UINT clusterLabel;     // ラベル番号
    UINT voxelStartIndex;  // g_SortedIndices 上の voxel 開始インデックス
    UINT voxelCount;       // このクラスタに属する voxel の数
};

struct CBPrefixSumInfo
{
    UINT numGroups;         // g_NumGroups（Dispatch 時の Group 数）
	XMFLOAT3 padding;   // 16バイトアライメントのためのパディング
};

class MCVoxelClusterUploader
{
public:
    MCVoxelClusterUploader() = default;
    ~MCVoxelClusterUploader() { ReleaseBuffers(); }

    void Initialize(UINT maxParticles);
    void BuildClusterLabels(const SimpleArray<SPHIndexTriplet>& triplets);
    void DispatchLabelPropagation(void); // ラベル伝播
	void DispatchClusterPartition(void); // クラスタ分割
	void DispatchClusterRemap(void); // ラベルのリマップ
	void DispatchPrefixSumRemap(void); // プレフィックスサムのリマップ
	void BindClusterBuffers(void); // クラスタバッファをバインド
	void UnbindClusterBuffers(void); // クラスタバッファをアンバインド

    // ラベル配列へのアクセス（外部から書き込み用）
	SimpleArray<UINT>& GetInitialLabelBuffer(void) { return m_initialLabels; }

private:
	// すべてのバッファを解放
	void ReleaseBuffers(void);
    // ParticleBufferSet を安全に再生成
    void RecreateBuffer(ParticleBufferSet& bufferSet, UINT elementSize, UINT elementCount);
	// バッファの容量を確保
    void EnsureLabelBufferCapacity(UINT requiredCount);
	// クラスタのAABBを初期化
    void InitClusterAABB(void);
	void CreatePrefixSumCB(void);

    // ラベル伝播用のバッファセット
	ParticleBufferSet m_labelBufferSet[2]; // Ping-Pong 用に2つ確保
	int m_labelCurrentBufferIndex = 0; // 現在のラベルバッファインデックス（0 or 1）

    // クラスタ分割用のバッファセット
    ParticleBufferSet m_clusterCountBufferSet;          // 各クラスタのボクセル数格納用
    ParticleBufferSet m_clusterMinBufferSet;            // クラスタのAABB最小値
    ParticleBufferSet m_clusterMaxBufferSet;            // クラスタのAABB最大値
    ParticleBufferSet m_voxelClusterIndexBufferSet;     // 各ボクセルが属するクラスタID
	ParticleBufferSet m_clusterRemapBufferSet;          // クラスタリマップ用のバッファ
	ParticleBufferSet m_validClusterCounterBufferSet;   // 有効クラスタカウンタ（1要素）
	ParticleBufferSet m_isValidClusterBufferSet;        // 有効クラスタフラグ（各クラスタの有効性を示すフラグ）
	ParticleBufferSet m_prefixSumBufferSet;             // プレフィックスサム計算用のバッファ
	ParticleBufferSet m_localSumsBufferSet;             // ローカルサム計算用のバッファ
	ParticleBufferSet m_globalOffsetsBufferSet;         // グローバルオフセット計算用のバッファ

	ID3D11Buffer* m_cbPrefixSumInfo = nullptr; // プレフィックスサム計算用のCB

    UINT m_numActiveVoxels = 0;
    UINT m_bufferCapacity = 0;
	SimpleArray<UINT> m_initialLabels; // 初期ラベルの配列
	SimpleArray<XMFLOAT3> m_clusterMinAABBs; // クラスタのAABB最小値
	SimpleArray<XMFLOAT3> m_clusterMaxAABBs; // クラスタのAABB最大値
    ComputeShaderSet m_labelPropagationCS; 	// ラベル伝播用のコンピュートシェーダー
	ComputeShaderSet m_clusterPartitionCS; // クラスタ分割用のシェーダー
	ComputeShaderSet m_remapCS; // ラベルのリマップ用シェーダー     
	ComputeShaderSet m_prefixSumLocalCS; // プレフィックスサム計算用のローカルシェーダー
	ComputeShaderSet m_prefixSumGlobalCS; // プレフィックスサム計算用のグローバルシェーダー
	ComputeShaderSet m_prefixSumAddOffsetCS; // プレフィックスサムのオフセット追加用シェーダー

    ID3D11Device* m_device = Renderer::get_instance().GetDevice();
    ID3D11DeviceContext* m_context = Renderer::get_instance().GetDeviceContext();

    ShaderResourceBinder& m_shaderResourceBinder = ShaderResourceBinder::get_instance();
};