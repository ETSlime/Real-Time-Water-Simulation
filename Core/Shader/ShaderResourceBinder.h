#pragma once
//=============================================================================
//
// 定数バッファ・SRV・UAV の最適バインド管理 [ShaderResourceBinder.h]
// Author : 
// 各シェーダーステージにおけるリソースのバインド状態を追跡し、
// 冗長なバインドを回避してパフォーマンスを最適化するユーティリティ
// 
//=============================================================================
#include "main.h"
#include "Utility/SingletonBase.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_CB_SLOT_NUM             16
#define MAX_SRV_SLOT_NUM            128
#define MAX_SAMPLER_SLOT_NUM        16
#define MAX_UAV_SLOT_NUM            8

// ===== スロット定義 =====
#define SLOT_CB_WORLD_MATRIX                0
#define SLOT_CB_VIEW_MATRIX                 1
#define SLOT_CB_PROJECTION_MATRIX           2
#define SLOT_CB_MATERIAL                    3
#define SLOT_CB_LIGHT                       4
#define SLOT_CB_FOG                         5    
#define SLOT_CB_PROJECTILE_PREVIEW		    5 // 投擲モードのプレビュー用
#define SLOT_CB_EFFECT_UPDATE               5 // 粒子更新用
#define SLOT_CB_EFFECT_DRAW                 6 // 粒子描画用
#define SLOT_CB_FUCHI                       6   
#define SLOT_CB_CASCADE_DATA_ARRAY          6
#define SLOT_CB_CAMERA_POS                  7
#define SLOT_CB_EFFECT_PARTICLE             7
#define SLOT_CB_LASER                       7
#define SLOT_CB_EFFECT_SPH                  8
#define SLOT_CB_CASCADE_DATA                8
#define SLOT_CB_SKYBOX                      9  
#define SLOT_CB_DEBUG_BOUNDING_BOX          9  
#define SLOT_CB_FLUID_THICKNESS             9
#define SLOT_CB_LIGHT_MODE                  10 
#define SLOT_CB_EFFECT_EMIT                 10
#define SLOT_CB_BONE_MATRIX_ARRAY           11 
#define SLOT_CB_SPH_VOLUM_PARAMS            11
#define SLOT_CB_INSTANCED_DATA              12
#define SLOT_CB_FLUID_SURFACE               12
#define SLOT_CB_EFFECT_LAKE_SURFACE         12
#define SLOT_CB_RENDER_PROGRESS             13
#define SLOT_CB_VOXEL_META                  13
#define SLOT_CB_CLUSTER_REMAP               13

// ===== サンプラースロット定義 =====
#define SLOT_SAMPLER_DEFAULT        0
#define SLOT_SAMPLER_SHADOW         1
#define SLOT_SAMPLER_OPACITY        2

// ===== テクスチャスロット定義 =====
#define SLOT_TEX_DIFFUSE            0
#define SLOT_TEX_CSM                1
#define SLOT_TEX_NOISE              7
#define SLOT_TEX_LIGHT              8
#define SLOT_TEX_NORMAL             9
#define SLOT_TEX_BUMP               10
#define SLOT_TEX_OPACITY            11
#define SLOT_TEX_REFLECT            12
#define SLOT_TEX_TRANSLUCENCY       13
#define SLOT_TEX_SKYBOX_DAY         15
#define SLOT_TEX_SKYBOX_NIGHT       21
#define SLOT_TEX_NORMAL_2           22

// ===== ストラクチャードバッファ定義 =====
#define SLOT_SRV_PARTICLE                   6   // パーティクル読み取りバッファ
#define SLOT_SRV_ALIVE_LIST                 7   // パーティクル生存リスト
#define SLOT_SRV_VOXEL_TRIANGLE_INDEX       8   // ボクセル三角形インデックスリスト
#define SLOT_SRV_VOXEL_TRIANGLE_STRUCT      9   // ボクセル三角形構造体リスト
#define SLOT_SRV_VOXEL_IDX_TRIPLET          10  // ボクセル三角形頂点リスト
#define SLOT_SRV_VOXEL_IDX_TRIPLET_PAGE     11  // ボクセル三角形インデックスページヘッダ
#define SLOT_SRV_SORTED_INDICES             12  // ソートされたパーティクルインデックス
#define SLOT_SRV_GRID_START_INDEX           13  // グリッド開始インデックス
#define SLOT_SRV_GRID_COUNTER               14  // グリッドカウンター
#define SLOT_SRV_SPH_GRID_PAGE_HEADER       15  // SPHグリッドページヘッダ
#define SLOT_SRV_SPH_INDEX_TRIPLETS         16  // SPHインデックストリプレット
#define SLOT_SRV_FLUID_THICKNESS            17  // 流体の厚み計算用SRV
#define SLOT_SRV_VOLUME_SCALAR_FIELD        18  // ボリュームスカラー場StructuredBuffer
#define SLOT_SRV_MARCHING_CUBES_VERTEX      19  // マーチングキューブ頂点バッファ
#define SLOT_SRV_MARCHING_CUBES_INDEX       20  // マーチングキューブインデックス
#define SLOT_SRV_VOXEL_CLUSTER_LABELS	    21  // ボクセルクラスタラベル
#define SLOT_SRV_CLUSTER_AABB_MIN			22  // クラスタAABB最小値
#define SLOT_SRV_CLUSTER_AABB_MAX			23  // クラスタAABB最大値
#define SLOT_SRV_CLUSTER_COUNT				24  // クラスタ数
#define SLOT_SRV_EDGE_TABLE                 25  // マーチングキューブのエッジテーブル 
#define SLOT_SRV_TRI_TABLE                  26  // マーチングキューブのエッジテーブルとトライテーブル
#define SLOT_SRV_VOXEL_CLUSTER_INDEX        27  // ボクセルクラスタインデックス
#define SLOT_SRV_CLUSTER_REMAP              28  // クラスタリマップ
#define SLOT_SRV_CLUSTER_ISVALID_FLAG       29  // クラスタ有効フラグ
#define SLOT_SRV_REMAP_PREFIX_SUM           30  // リマッププレフィックスサム
#define SLOT_SRV_REMAP_LOCAL_SUMS           31  // リマップローカルサム
#define SLOT_SRV_REMAP_GLOBAL_OFFSETS       32  // リマップグローバルオフセット

// ===== RWストラクチャードバッファ定義 =====
#define SLOT_UAV_PARTICLE                       0   // パーティクル更新バッファ
#define SLOT_UAV_ALIVE_LIST                     1   // パーティクル生存リスト
#define SLOT_UAV_FREE_LIST                      2   // パーティクルフリーリスト
#define SLOT_UAV_FREE_LIST_CONSUME              3   // フリーリスト消費用UAV
#define SLOT_UAV_SORTED_INDICES                 4   // ソートされたパーティクルインデックス
#define SLOT_UAV_GRID_START_INDEX               5   // グリッド開始インデックス
#define SLOT_UAV_GRID_COUNTER                   6   // グリッドカウンター
#define SLOT_UAV_FLUID_THICKNESS                7   // 流体厚み計算用UAV
#define SLOT_UAV_VOLUME_SCALAR_FIELD            7   // ボリュームスカラー場RWStructuredBuffer
#define SLOT_UAV_MARCHING_CUBES_VERTEX          1   // マーチングキューブ頂点バッファ
#define SLOT_UAV_MARCHING_CUBES_INDEX           2   // マーチングキューブインデックス
#define SLOT_UAV_MARCHING_CUBES_DRAWARGS        3   // マーチングキューブ描画引数
#define SLOT_UAV_MARCHING_CUBES_EDGE_CATCH      4   // マーチングキューブエッジキャッチ用UAV
#define SLOT_UAV_MARCHING_CUBES_VERTEX_COUNTER  5   // マーチングキューブ頂点カウンター
#define SLOT_UAV_VOXEL_CLUSTER_LABELS           0   // ボクセルクラスタラベルUAV  
#define SLOT_UAV_VOXEL_CLUSTER_INDEX            1   // ボクセルクラスタインデックスUAV
#define SLOT_UAV_CLUSTER_AABB_MIN               2   // クラスタAABB最小値UAV
#define SLOT_UAV_CLUSTER_AABB_MAX               3   // クラスタAABB最大値UAV
#define SLOT_UAV_CLUSTER_COUNT                  4   // クラスタ数UAV
#define SLOT_UAV_CLUSTER_REMAP  		        5   // クラスタリマップUAV
#define SLOT_UAV_CLUSTER_VALID_COUNTER		    6   // 有効クラスタカウンタUAV
#define SLOT_UAV_CLUSTER_ISVALID_FLAG           7   // 
#define SLOT_UAV_REMAP_PREFIX_SUM               2   // リマッププレフィックスサムUAV
#define SLOT_UAV_REMAP_LOCAL_SUMS               3   // リマップローカルサムUAV
#define SLOT_UAV_REMAP_GLOBAL_OFFSETS           4   // リマップグローバルオフセットUAV

// ===== シェーダーステージ定義 =====
enum class ShaderStage
{
    VS,
    PS,
    GS,
    CS,
};

class ShaderResourceBinder : public SingletonBase<ShaderResourceBinder>
{
public:

    void Initialize(ID3D11DeviceContext* context);
    void Reset(void);

    // 定数バッファバインド
    void BindConstantBuffer(ShaderStage stage, UINT slot, ID3D11Buffer* buffer);

    // SRVバインド
    void BindShaderResource(ShaderStage stage, UINT slot, ID3D11ShaderResourceView* srv);

    // サンプラーバインド
    void BindSampler(ShaderStage stage, UINT slot, ID3D11SamplerState* sampler);

    // ===== バッチバインド（複数同時）=====
    void BindConstantBuffers(ShaderStage stage, UINT startSlot, UINT numBuffers, ID3D11Buffer* const* buffers);
    void BindShaderResources(ShaderStage stage, UINT startSlot, UINT numSRVs, ID3D11ShaderResourceView* const* srvs);
    void BindSamplers(ShaderStage stage, UINT startSlot, UINT numSamplers, ID3D11SamplerState* const* samplers);

    // ===== UAVバインド（Compute Shader専用）=====
    void BindUnorderedAccessView(UINT slot, ID3D11UnorderedAccessView* uav);
    void BindUnorderedAccessViews(UINT startSlot, UINT numUAVs, ID3D11UnorderedAccessView* const* uavs);
    void BindCounterUnorderedAccessView(UINT slot, ID3D11UnorderedAccessView* uav, UINT initialCount = 0);

private:

    ID3D11DeviceContext* m_context = nullptr;

    // 現在のバインド状態
    ID3D11Buffer* m_currentCB_VS[MAX_CB_SLOT_NUM]{};
    ID3D11Buffer* m_currentCB_PS[MAX_CB_SLOT_NUM]{};
    ID3D11Buffer* m_currentCB_GS[MAX_CB_SLOT_NUM]{};
    ID3D11Buffer* m_currentCB_CS[MAX_CB_SLOT_NUM]{};

    ID3D11ShaderResourceView* m_currentSRV_VS[MAX_SRV_SLOT_NUM]{};
    ID3D11ShaderResourceView* m_currentSRV_PS[MAX_SRV_SLOT_NUM]{};
    ID3D11ShaderResourceView* m_currentSRV_GS[MAX_SRV_SLOT_NUM]{};
    ID3D11ShaderResourceView* m_currentSRV_CS[MAX_SRV_SLOT_NUM]{};

    ID3D11SamplerState* m_currentSampler_VS[MAX_SAMPLER_SLOT_NUM]{};
    ID3D11SamplerState* m_currentSampler_PS[MAX_SAMPLER_SLOT_NUM]{};
    ID3D11SamplerState* m_currentSampler_GS[MAX_SAMPLER_SLOT_NUM]{};
    ID3D11SamplerState* m_currentSampler_CS[MAX_SAMPLER_SLOT_NUM]{};

    ID3D11UnorderedAccessView* m_currentUAV_CS[MAX_UAV_SLOT_NUM]{}; // CS専用UAVバインド追跡

    // ダーティフラグ
    bool m_dirtySRV_VS[MAX_SRV_SLOT_NUM]{};
    bool m_dirtySRV_PS[MAX_SRV_SLOT_NUM]{};
    bool m_dirtySRV_GS[MAX_SRV_SLOT_NUM]{};
    bool m_dirtySRV_CS[MAX_SRV_SLOT_NUM]{};
    bool m_dirtyUAV_CS[MAX_UAV_SLOT_NUM]{};

    // 内部ヘルパ
    ID3D11Buffer** GetCBSlotArray(ShaderStage stage);
    ID3D11ShaderResourceView** GetSRVSlotArray(ShaderStage stage);
    ID3D11SamplerState** GetSamplerSlotArray(ShaderStage stage);
    bool* GetSRVDirtyArray(ShaderStage stage);
};