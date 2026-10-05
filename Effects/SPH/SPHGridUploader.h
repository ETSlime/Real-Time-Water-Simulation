#pragma once
//=============================================================================
//
//  [SPHGridUploader.h]
// Author : 
//
// SPH流体パーティクルからモートン順に並んだ稀疎グリッド構造を生成・管理するクラス。
// グリッドはページ単位で構成され、粒子と対応するボクセルを GPU にアップロード可能。
// 
// - BuildGrid()：粒子から Morton キーを生成し、SPHIndexTriplet の配列とページヘッダを構築
// - MergeSortTriplets()：Triplet を mortonIndex 昇順でソート（安定性あり）
// - ExportInitialClusterLabels()：初期クラスタラベルを出力（クラスタ分割・伝播に使用）
// - m_mortonMap / m_voxelLabelMap によって、各 voxel とラベルIDを記録可能
// - UploadAndBindGridBuffers() により GPU 側の StructuredBuffer にアップロードされる
//
// 主に Marching Cubes のクラスタラベル付与や SPH 粒子とのボクセルマッピング、
// DrawIndirect・クラスタ再構築等の処理において基盤となるデータ構造を形成する。
// 
//=============================================================================
#include "main.h"
#include "Collision/VoxelBufferUploader.h"
#include "Effects/ParticleStructs.h"
#include "Effects/ParticleEffectRendererBase.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_SPH_GRID_PAGE_SIZE      1024 // 1ページあたりの最大セル数
#define ACTIVE_VOXEL_FACTOR         27.0f // アクティブなボクセルの容量拡張係数

//*****************************************************************************
// 構造体定義
//*****************************************************************************

// 20ビットのモートンコードを2ビットずつ分割する関数（64ビット版）
uint64_t Part1By2_64(uint32_t n);

// 3次元座標をモートンコードにエンコードする関数（64ビット版）
uint64_t EncodeMorton3_64(uint32_t x, uint32_t y, uint32_t z);

struct SPHIndexTriplet
{
	XMUINT3 gridCoord;      // グリッド座標（x, y, z）
    XMUINT2 mortonIndex;    // 64-bit Morton（下位: x, 上位: y）
	UINT startOffset;       // g_GridSortedIndices[] の開始位置
	UINT count;             // そのセルに含まれる粒子数
};

// 一時トリプレットをハッシュ表から収集
struct TempTriplet
{
    XMUINT3 gridCoord;      // グリッド座標（x, y, z）
	XMUINT2 morton;         // モートンコード（2ビットずつ分割された20ビット）
    UINT count;
    const SimpleArray<UINT>* indices; // 粒子IDリストへのポインタ
};

// 64ビット版のIndexTripletPageHeader（モートンコードを64ビットで扱う）
struct IndexTripletPageHeaderVec2
{
    XMUINT2 pageStartMorton;    // ページ最小 Morton 値
    XMUINT2 pageEndMorton;      // ページ最大 Morton 値
	UINT pageOffset;            // IndexTriplet の開始位置
    UINT pageCount;             // IndexTriplet の個数
};

struct VoxelCoord
{
    UINT x, y, z;
}; // ボクセルのキーを表す構造体

struct HashVoxelKey64
{
    size_t operator()(const VoxelCoord& key) const
    {
        return static_cast<size_t>(EncodeMorton3_64(key.x, key.y, key.z));
    }
};

struct EqualVoxelKey64
{
    bool operator()(const VoxelCoord& a, const VoxelCoord& b) const
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
};


class SPHGridUploader
{
public:
    void Initialize(float cellSize, UINT maxParticles, XMFLOAT3* gridMin);
	void Shutdown(void);
    void Clear(void);
    void BuildGrid(const SimpleArray<WaterFluidParticle>& particles);
    void UploadAndBindGridBuffers(void);
    void UploadSortedIndicesTo(ID3D11Buffer* dstBuffer) const;
    void ExportInitialClusterLabels(SimpleArray<UINT>& outLabels) const;

    UINT GetTotalGridPageHeaderSize(void) const { return m_indexTripletPageHeaders.getSize(); }
    const SimpleArray<UINT>& GetGridSortedIndices(void) const { return m_gridSortedIndices; }
    const SimpleArray<IndexTripletPageHeaderVec2>& GetPageHeaders(void) const { return m_indexTripletPageHeaders; }
	const UINT GetActiveTripletCount(void) const { return m_sphIndexTriplets.getSize(); }
    const SimpleArray<SPHIndexTriplet>& GetGridTriplets(void) const { return m_sphIndexTriplets; }
	void SetRecordActiveVoxelCoords(bool record) { m_recordActiveVoxelCoords = record; }

private:
    // IndexTripletをページ単位で構築
	void BuildPagedIndexTripletBuffer(void);
    // MergeSort による IndexTriplet ソート（mortonIndex 昇順）
    void MergeSortTriplets(SimpleArray<TempTriplet>& triplets, int left, int right);
	// モートンコードの比較関数（64ビット版）
    inline bool CompareMorton64(const XMUINT2& a, const XMUINT2& b)
    {
        return (a.y < b.y) || (a.y == b.y && a.x < b.x); // 上位優先
    }

    float m_cellSize = 1.0f;
    XMFLOAT3* m_gridMin = nullptr;

	SimpleArray<UINT> m_gridSortedIndices; // ソートされた粒子インデックスの配列
	SimpleArray<SPHIndexTriplet> m_sphIndexTriplets; // モートンキーとパーティクルインデックスの対応付け
    SimpleArray<IndexTripletPageHeaderVec2> m_indexTripletPageHeaders; // IndexTripletのページヘッダ
	SimpleArray<uint64_t> m_labelMortonKeysToRemove; // 削除対象のモートンキー

	UINT m_indexTripletCapacity = 0; // IndexTripletの最大容量
	ParticleBufferSet m_gridPageBufferSet; // グリッドページヘッダ用バッファセット
	ParticleBufferSet m_indexTripletBufferSet; // IndexTriplet用バッファセット

	bool m_recordActiveVoxelCoords = false; // アクティブなボクセル座標を記録するかどうか

    ID3D11Device* m_device = Renderer::get_instance().GetDevice();
    ID3D11DeviceContext* m_context = Renderer::get_instance().GetDeviceContext();
    ShaderResourceBinder& m_shaderResourceBinder = ShaderResourceBinder::get_instance();

    struct MortonMapEntry
    {
		XMUINT3 gridCoord; // グリッド座標（x, y, z）
		SimpleArray<uint32_t> particleIDs; // 対応する粒子IDのリスト
		UINT labelID = 0; // ラベルID（初期値は0）
		MortonMapEntry() : gridCoord(0, 0, 0), particleIDs() {}
		MortonMapEntry(XMUINT3 coord, const SimpleArray<uint32_t>& ids)
			: gridCoord(coord), particleIDs(ids) {
		}
    };

    // morton キー → particleIDs
    HashMap<uint64_t, MortonMapEntry, HashUInt64, EqualUInt64> m_mortonMap =
        HashMap<uint64_t, MortonMapEntry, HashUInt64, EqualUInt64>(
            MAX_PARTICLES, // 初期バケット数
            HashUInt64(), // ハッシュ関数
            EqualUInt64() // 等価比較関数
        );

	// ボクセルラベルマップ（モートンコード → ラベルID）
    HashMap<uint64_t, UINT, HashUInt64, EqualUInt64> m_voxelLabelMap = 
		HashMap<uint64_t, UINT, HashUInt64, EqualUInt64>(
			MAX_PARTICLES, // 初期バケット数
			HashUInt64(), // ハッシュ関数
			EqualUInt64() // 等価比較関数
		);
};
