#pragma once
//=============================================================================
//
// [VoxelBufferUploader.h]
// Author : 
//
// Voxel Grid 構造から GPU 用の三角形マッピング情報を構築・アップロードするクラス。
// 三角形の構造体配列、Voxel ごとの三角形インデックス、IndexTriplet のページ構造など、
// 体素ベースの衝突・再構築に必要な StructuredBuffer 群を自動生成し、
// Shader 内での高速なアクセスを可能にする。
// 
//=============================================================================
#include "Collision/VoxelGrid.h" 
#include "Core/Graphics/Renderer.h"

//*****************************************************************************
// 構造体定義
//*****************************************************************************

struct VoxelMetaCbuffer
{
	XMFLOAT3 origin;            // ボクセルグリッドの原点座標
	float voxelSize;            // ボクセルのサイズ（1辺の長さ）
	UINT totalIndexTripletSize; // m_indexTriplets の総サイズ
	UINT totalPageHeaderSize;   // m_indexTripletPages の総サイズ
	XMFLOAT2 padding;           // 16バイト境界に合わせるためのパディング
};

// HLSL用Triangle構造体
struct TriangleStructuredBuffer
{
    XMFLOAT3 v0;
    float pad0;

    XMFLOAT3 v1;
    float pad1;

    XMFLOAT3 v2;
    float pad2;

    XMFLOAT3 normal;
    float pad3;
};

// モートンインデックスごとの三角形バッファの範囲を表す三元組
struct IndexTriplet
{
    UINT voxelIndex;    // Morton Index（EncodeMorton3 の結果）
    UINT startIndex;    // g_TriangleIndices の開始位置
    UINT triangleCount; // 三角形の数
	UINT padding;       // 16バイト境界に合わせるためのパディング
};

// IndexTripletPageHeader：IndexTriplet をページ単位で管理するヘッダ
struct IndexTripletPageHeader
{
    UINT pageStartMorton; // このページ内での最小 Morton インデックス
    UINT pageEndMorton;   // このページ内での最大 Morton インデックス
    UINT pageOffset;      // IndexTriplet 配列内のオフセット
    UINT pageCount;       // ページ内の IndexTriplet の数
};

struct HashPtr
{
    size_t operator()(const void* ptr) const noexcept
    {
        return reinterpret_cast<size_t>(ptr);
    }
};

struct EqualPtr
{
    bool operator()(const void* a, const void* b) const noexcept
    {
        return a == b;
    }
};

class VoxelBufferUploader : public SingletonBase<VoxelBufferUploader>
{
public:
    VoxelBufferUploader(void);
    ~VoxelBufferUploader(void) {};

    // Voxelメタデータをアップロードする関数
    void UploadVoxelMeta(const XMFLOAT3& origin, float voxelSize);

	// Voxelリソースをバインドする関数
    void BindVoxelResources(void);

	// 非同期マッピングビルドを登録する関数
    void RegisterAsyncMappingBuild(void);

	void SetCachedGrid(const VoxelGrid* grid) { m_cachedGrid = grid; } // キャッシュされたVoxelGridを設定
	bool IsBuffersUploaded(void) const { return m_isBuffersUploaded; } // バッファがアップロードされたかどうか
	void ShutDown(void); // Voxelリソースを解放

private:
    // StructuredBuffer作成
    ID3D11Buffer* CreateStructuredBuffer(UINT structSize, UINT elementCount, const void* initData, ID3D11ShaderResourceView** outSRV);

    // Voxel Grid からマッピングデータ生成 + StructuredBufferへアップロード
    void GenerateVoxelTriangleMapping(void* param);

	// Voxel Grid が準備完了かどうかをチェックする関数
    bool IsVoxelGridReady(void* param);

	// Voxelデータをアップロードする関数
    void UploadBuffers(void);

    // ページヘッダを定数バッファにアップロード
    //void UploadPageHeaders(void);

    void BuildPagedIndexTripletBuffer(void);

    // MergeSort による IndexTriplet ソート（voxelIndex 昇順）
    void MergeSortTriplets(int left, int right);

    // キャッシュされたVoxelGridへのポインタ
	const VoxelGrid* m_cachedGrid = nullptr;

    // Voxel三角形インデックス配列（すべてのVoxelに属する三角形ID）
    ID3D11Buffer* m_voxelTriangleIndexBuffer = nullptr;
    ID3D11ShaderResourceView* m_voxelTriangleIndexSRV = nullptr;

	// Voxelインデックス三元組（voxelIndex, startOffset, triangleCount）
    ID3D11Buffer* m_indexTripletBuffer = nullptr;
    ID3D11ShaderResourceView* m_indexTripletSRV = nullptr;

	// IndexTripletのページヘッダ（IndexTripletPageHeader）
    ID3D11Buffer* m_indexTripletPageBuffer = nullptr;
    ID3D11ShaderResourceView* m_indexTripletPageSRV = nullptr;

	// Voxel三角形構造体配列（StructuredBuffer）
    ID3D11Buffer* m_triangleStructBuffer = nullptr;
    ID3D11ShaderResourceView* m_triangleStructSRV = nullptr;

    // Voxelメタデータ用定数バッファ
	ID3D11Buffer* m_cbVoxelMeta = nullptr;

	SimpleArray<UINT> m_triangleIndices; // Voxelに属する三角形のインデックス配列
	SimpleArray<TriangleStructuredBuffer> m_triangleStructs; // 三角形構造体の配列
	SimpleArray<IndexTriplet> m_indexTriplets; // 三角形インデックスのオフセット情報
	SimpleArray<IndexTripletPageHeader> m_indexTripletPageHeaders; // IndexTripletのページヘッダ

    UINT m_currentTriangleIndex = 0; // 現在の三角形インデックス
    UINT m_totalIndexTripletSize = 0; // m_indexTriplets の総サイズ
    UINT m_totalIndexTripletPageSize = 0; // m_indexTripletPages の総サイズ

	bool m_isBuffersUploaded = false; // バッファがアップロードされたかどうか

    Renderer& m_renderer = Renderer::get_instance();
    ID3D11Device* m_device = Renderer::get_instance().GetDevice();
	ShaderResourceBinder& m_shaderResourceBinder = ShaderResourceBinder::get_instance();

    HashMap<const void*, uint32_t, HashPtr, EqualPtr> m_triangleToIndex =
        HashMap<const void*, uint32_t, HashPtr, EqualPtr>(
            MAX_VOXEL_TRIANGLE_NUM, // 初期バケット数
            HashPtr(), // ハッシュ関数
            EqualPtr() // 等価比較関数
        );
};