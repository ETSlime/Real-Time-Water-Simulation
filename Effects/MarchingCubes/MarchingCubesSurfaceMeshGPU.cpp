//=============================================================================
//
// [MarchingCubesSurfaceMeshGPU.h]
// Author : 
//
//=============================================================================
#include "Effects/MarchingCubes/MarchingCubesSurfaceMeshGPU.h"
#include "Effects/MarchingCubes/MarchingCubesTables.h"

void MarchingCubesSurfaceMeshGPU::Initialize(UINT maxParticles)
{
	m_buildOnGPU = true; // GPU上でのメッシュ生成を有効化

    // シェーダーの取得
    m_marchingCubesCS = ShaderManager::get_instance().GetComputeShader(ParticleComputeGroup::WaterFluid, ComputePassType::MarchingCubes);

    // クラスタアップローダーの初期化
    m_clusterUploader.Initialize(maxParticles);

    // 基底クラスの初期化
	MarchingCubesSurfaceMesh::Initialize(maxParticles);

	EnsureGlobalMeshCapacity(m_maxParticles); // グローバルメッシュ容量を確保

    // メッシュバッファの作成
	CreateMeshBuffers();

    // DrawArgsバッファ（RWStructuredBuffer<DrawIndexedInstancedArgs>）
    ParticleEffectRendererBase::CreateStructuredBuffer(m_device,
        sizeof(DrawInstancedArgs),          // 構造体サイズ
        1,                                  // 要素数は1つ（DrawIndexedInstancedArgs用）
        nullptr,                            // 初期データはなし
        &m_drawArgsBufferSet.buffer,        // RWStructuredBuffer<DrawIndexedInstancedArgs>
        nullptr,                            // SRV は不要
        &m_drawArgsBufferSet.uav,           // DrawIndexedInstancedArgs 用の UAV
		false,                              // Append UAV ではない
		false, 					            // CPU 書き込み不可（CS 専用）
		true                                // Counter バッファを有効化（DrawIndexedInstancedIndirect 用のカウンター）
    );

    ParticleEffectRendererBase::CreateStructuredBuffer(m_device,
        sizeof(UINT),                       // 構造体サイズ
        1,                                  // 要素数は1つ（MarchingCubesCS用）
        nullptr,                            // 初期データはなし
        &m_vertexCounterBufferSet.buffer,   // RWStructuredBuffer<UINT>
        nullptr,                            // SRV は不要
        &m_vertexCounterBufferSet.uav       // MarchingCubesCS 用の UAV
    );

	CreateDummyIndexBuffer(); // ダミーインデックスバッファを作成（DrawIndexedInstancedIndirect 用）

	CreateDrawArgsBuffer(); // DrawIndexedInstancedIndirect 用の引数バッファを作成
	CreateLookupTables(); // エッジテーブルとトライテーブルの作成

#ifdef _DEBUG
    // DebugVoxelInfo 用の StructuredBuffer を作成
    UINT DEBUG_VOXEL_COUNT = m_maxParticles * ACTIVE_VOXEL_FACTOR;

    ParticleEffectRendererBase::CreateStructuredBuffer(
        m_device,
        sizeof(DebugVoxelInfo),                // 構造体サイズ
        DEBUG_VOXEL_COUNT,                     // 要素数（voxel 数）
        nullptr,                               // 初期データなし
        &m_debugVoxelInfoBufferSet.buffer,     // 実バッファ
        nullptr,                               // SRV は不要
        &m_debugVoxelInfoBufferSet.uav         // UAV のみ使用
    );
#endif // _DEBUG

}

void MarchingCubesSurfaceMeshGPU::Release(void)
{
    ReleaseMeshBuffer();

    SafeRelease(&m_drawArgsBufferSet.buffer);
    SafeRelease(&m_drawArgsBufferSet.srv);
    SafeRelease(&m_drawArgsBufferSet.uav);
    SafeRelease(&m_vertexCounterBufferSet.buffer);
    SafeRelease(&m_vertexCounterBufferSet.srv);
    SafeRelease(&m_vertexCounterBufferSet.uav);
    SafeRelease(&m_drawIndirectArgsBuffer);
}

void MarchingCubesSurfaceMeshGPU::UpdateMesh(ID3D11ShaderResourceView* scalarFieldSRV, 
    const SimpleArray<SPHIndexTriplet>& triplets, const VolumeReconstructionParams& params)
{
	if (!m_marchingCubesCS.cs || !scalarFieldSRV)
	{
		OutputDebugStringA("[MarchingCubesSurfaceMeshGPU] シェーダーまたはスカラー場SRVが無効です。\n");
		return;
	}

	//m_clusterUploader.BuildClusterLabels(triplets); // クラスタラベルを構築
	//m_clusterUploader.DispatchLabelPropagation(); // ラベル伝播を実行
	//m_clusterUploader.DispatchClusterPartition(); // クラスタ分割を実行
	//m_clusterUploader.DispatchClusterRemap(); // クラスタリマップを構築
	//m_clusterUploader.DispatchPrefixSumRemap(); // プレフィックスサムリマップを実行

	m_lastParams = params; // パラメータを保存

    //const XMUINT3& fieldDim = XMUINT3(params.gridX, params.gridY, params.gridZ);
	//m_edgeCacheDim = fieldDim; // エッジキャッシュの次元を設定

    // 必要なバッファ容量を確保
	//EnsureBufferCapacity(fieldDim);

    // UAVの初期化（必要ならClear）
    UINT initCounts[4] = { 0, 0, 0, 0 };
    m_context->ClearUnorderedAccessViewUint(m_vertexBufferSet.uav, initCounts);
    m_context->ClearUnorderedAccessViewUint(m_indexBufferSet.uav, initCounts);
	m_context->ClearUnorderedAccessViewUint(m_vertexCounterBufferSet.uav, initCounts);

    UINT clearVal[4] = { 0xFFFFFFFF, 0, 0, 0 }; // -1
    m_context->ClearUnorderedAccessViewUint(m_edgeCacheBufferSet.uav, clearVal);

    UINT initArgs[5] = { 0 };
    //initArgs[1] = 1; // InstanceCount = 1
    m_context->ClearUnorderedAccessViewUint(m_drawArgsBufferSet.uav, initArgs);

    // CSにバインド
    m_context->CSSetShader(m_marchingCubesCS.cs, nullptr, 0);
    ID3D11UnorderedAccessView* uavs[] = { 
        m_vertexBufferSet.uav, 
        m_indexBufferSet.uav, 
        m_drawArgsBufferSet.uav, 
        m_edgeCacheBufferSet.uav,
        m_vertexCounterBufferSet.uav
    };
	// UnorderedAccessViewをバインド
    m_shaderResourceBinder.BindUnorderedAccessViews(SLOT_UAV_MARCHING_CUBES_VERTEX, 5, uavs);
    // スカラー場SRVをバインド
	m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_VOLUME_SCALAR_FIELD, scalarFieldSRV);
	// エッジテーブルとトライテーブルのSRVをバインド
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_EDGE_TABLE, m_edgeTableBufferSet.srv);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_TRI_TABLE, m_triTableBufferSet.srv);
	// 定数バッファのバインド
    m_shaderResourceBinder.BindConstantBuffer(ShaderStage::CS, SLOT_CB_FLUID_SURFACE, m_cbMarchingCubes);
    // クラスタバッファをバインド
	//m_clusterUploader.BindClusterBuffers();

#ifdef _DEBUG
    m_shaderResourceBinder.BindUnorderedAccessView(7, m_debugVoxelInfoBufferSet.uav);
#endif // _DEBUG


    // Dispatch
	UINT numActieVoxels = triplets.getSize();
    UINT threadGroupCount = GET_THREAD_GROUP_COUNT(numActieVoxels, THREAD_COUNT);
    m_context->Dispatch(threadGroupCount, 1, 1);

    // Unbind
    ID3D11UnorderedAccessView* nullUAV[5] = {};
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_shaderResourceBinder.BindUnorderedAccessViews(SLOT_UAV_MARCHING_CUBES_VERTEX, 5, nullUAV);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_VOLUME_SCALAR_FIELD, nullSRV);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_EDGE_TABLE, nullSRV);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::CS, SLOT_SRV_TRI_TABLE, nullSRV);
	m_clusterUploader.UnbindClusterBuffers();

#ifdef _DEBUG
    m_shaderResourceBinder.BindUnorderedAccessView(7, nullptr);
#endif // _DEBUG

    // 定数バッファの更新
    UpdateCBMarchingCubes();
}

void MarchingCubesSurfaceMeshGPU::Draw(void)
{
    // パイプラインのセットアップ
    SetupPipeline();

    m_context->IASetIndexBuffer(m_dummyIB, DXGI_FORMAT_R32_UINT, 0);

    m_shaderResourceBinder.BindShaderResource(ShaderStage::VS, SLOT_SRV_MARCHING_CUBES_VERTEX, m_vertexBufferSet.srv);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::VS, SLOT_SRV_MARCHING_CUBES_INDEX, m_indexBufferSet.srv);

    // m_drawArgsBufferSet.uav->m_drawIndirectArgsBuffer
    //m_context->CopyStructureCount(m_drawIndirectArgsBuffer, 4, m_drawArgsBufferSet.uav);
    m_context->CopyResource(m_drawIndirectArgsBuffer, m_drawArgsBufferSet.buffer);
    m_context->DrawInstancedIndirect(m_drawIndirectArgsBuffer, 0);

    // Unbind
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_shaderResourceBinder.BindShaderResource(ShaderStage::VS, SLOT_SRV_MARCHING_CUBES_VERTEX, nullSRV);
    m_shaderResourceBinder.BindShaderResource(ShaderStage::VS, SLOT_SRV_MARCHING_CUBES_INDEX, nullSRV);
}

void MarchingCubesSurfaceMeshGPU::ComputeMeshRequirementsByFieldDim(const XMUINT3& fieldDim, 
    UINT* outVertexCount, UINT* outIndexCount, UINT* outEdgeCacheCount)
{
    // 有効なセル数（各方向に-1）を計算
    const UINT cellCount = (fieldDim.x - 1) * (fieldDim.y - 1) * (fieldDim.z - 1);

    // 1セルあたり最大5個の三角形 → 最大15頂点
    const UINT maxTrianglesPerCell = 5;
    const UINT maxVerticesPerCell = maxTrianglesPerCell * 3;

    // 頂点数とインデックス数（実質同じ）を出力
    *outVertexCount = cellCount * maxVerticesPerCell;
    *outIndexCount = *outVertexCount;
    // エッジキャッシュ数：各セルに12本のエッジがある
    *outEdgeCacheCount = cellCount * 12;
}

void MarchingCubesSurfaceMeshGPU::EnsureGlobalMeshCapacity(UINT maxParticles)
{
    const float RESERVE_SCALE = 1.5f; // バッファの冗長確保スケール

    // 各 voxel に対して最大何個の三角形が生成されるかの経験的上限
    const UINT maxTrianglesPerVoxel = 5; // 通常は最大 5 個程度が目安
    const UINT maxVerticesPerVoxel = maxTrianglesPerVoxel * 3; // 最大頂点数（ユニークではない）
    const UINT maxIndicesPerVoxel = maxTrianglesPerVoxel * 9; // 最大インデックス数

    // 活性ボクセル数の見積もり：粒子数の 27 倍（近傍を含めた影響範囲）
    UINT estimatedVoxels = static_cast<UINT>(maxParticles * ACTIVE_VOXEL_FACTOR);

    // バッファ容量の算出（スケーリング含む）
    m_vertexCapacity = static_cast<UINT>(estimatedVoxels * maxVerticesPerVoxel * RESERVE_SCALE);
    m_indexCapacity = static_cast<UINT>(estimatedVoxels * maxIndicesPerVoxel * RESERVE_SCALE);

    // キャッシュは通常 voxel ごとに 12 個の辺に対してキャッシュする
    m_edgeCacheCapacity = estimatedVoxels * 12;
}

void MarchingCubesSurfaceMeshGPU::EnsureBufferCapacity(const XMUINT3& fieldDim)
{
    const float RESERVE_SCALE = 1.5f; // 冗長確保用スケーリング係数

	// 必要な頂点数とインデックス数を計算
    UINT requiredVertices, requiredIndices, requiredEdgeCache;
    ComputeMeshRequirementsByFieldDim(fieldDim, &requiredVertices, &requiredIndices, &requiredEdgeCache);

    if (requiredVertices > m_vertexCapacity ||
        requiredIndices > m_indexCapacity ||
        requiredEdgeCache > m_edgeCacheCapacity)
    {
        // 必要な頂点数またはインデックス数が現在の容量を超える場合、バッファを再作成
        m_vertexCapacity = static_cast<UINT>(requiredVertices * RESERVE_SCALE);
        m_indexCapacity = static_cast<UINT>(requiredIndices * RESERVE_SCALE);
        CreateMeshBuffers();
    }
}

void MarchingCubesSurfaceMeshGPU::CreateMeshBuffers(void)
{
	ReleaseMeshBuffer();

    // 頂点バッファ（RWStructuredBuffer<MarchingCubesVertex>）
    // - Compute Shader が三角形頂点を出力するために書き込むバッファ
    // - Vertex Shader が SRV 経由で参照し、頂点位置・法線などを利用
    // - DrawIndexedInstancedIndirect では IA バインドせず、SRV 経由で頂点取得
    ParticleEffectRendererBase::CreateStructuredBuffer(m_device, 
		sizeof(MarchingCubesVertex),        // 頂点のサイズ
        m_vertexCapacity,                   // バッファ内に確保する最大頂点数（Dispatch 範囲に応じて拡張）
		nullptr,                            // 初期データはなし
		&m_vertexBufferSet.buffer,          // 実バッファ本体
		&m_vertexBufferSet.srv,             // ShaderResourceView：VS で頂点を読み取る用
		&m_vertexBufferSet.uav              // UnorderedAccessView：CS で頂点を書き込む用
    );

    // インデックスバッファ（RWStructuredBuffer<uint>）
    // - Compute Shader が三角形のインデックス情報（頂点番号）を書き込むバッファ
    // - VS 内で SV_VertexID を使ってこのインデックスを参照し、該当頂点にアクセス
    // - SRV 経由で読み込めるように、インデックスバッファにも SRV を生成しておく
    ParticleEffectRendererBase::CreateStructuredBuffer(m_device, 
		sizeof(UINT),                       // インデックスのサイズ
        m_indexCapacity,                    // 最大インデックス数（三角形数×3 を想定して確保）
		nullptr,                            // 初期データはなし
		&m_indexBufferSet.buffer,           // 実バッファ本体
		&m_indexBufferSet.srv,              // ShaderResourceView：VSでインデックス取得用
		&m_indexBufferSet.uav               // UnorderedAccessView：CSでインデックス書き込み用
    );


    ParticleEffectRendererBase::CreateStructuredBuffer(m_device,
        sizeof(int),                        // 構造体サイズ
        m_edgeCacheCapacity,
        nullptr,                            // 初期データはなし
        &m_edgeCacheBufferSet.buffer,
        nullptr,                            // SRV は不要
        &m_edgeCacheBufferSet.uav
    );
}

void MarchingCubesSurfaceMeshGPU::CreateLookupTables(void)
{
    ParticleEffectRendererBase::CreateStructuredBuffer(
        m_device,
        sizeof(UINT),                     // 構造体サイズ = UINT
        256,                              // 要素数 = 256
        edgeTable,                        // 初期データ
        &m_edgeTableBufferSet.buffer,
        &m_edgeTableBufferSet.srv,
        nullptr                           // UAV は不要
    );

    ParticleEffectRendererBase::CreateStructuredBuffer(
        m_device,
        sizeof(int),                     // 構造体サイズ = int
        256 * 16,                        // 要素数 = 4096
        triTable[0],                     // 初期データのポインタ（2D配列でも OK）
        &m_triTableBufferSet.buffer,
        &m_triTableBufferSet.srv,
        nullptr                          // UAV は不要
    );
}

void MarchingCubesSurfaceMeshGPU::CreateDrawArgsBuffer(void)
{
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(DrawInstancedArgs);
    desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = 0; // UAV SRV 用にバインドフラグはなし
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

    HRESULT hr = m_device->CreateBuffer(&desc, nullptr, &m_drawIndirectArgsBuffer);
    assert(SUCCEEDED(hr) && "DrawIndirectArgsBuffer の作成に失敗しました！");
}

void MarchingCubesSurfaceMeshGPU::CreateDummyIndexBuffer(void)
{
    UINT dummyIndex = 0;
    D3D11_SUBRESOURCE_DATA subData = { &dummyIndex };
    D3D11_BUFFER_DESC desc = {};
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.ByteWidth = sizeof(UINT);
    desc.Usage = D3D11_USAGE_IMMUTABLE;

    m_device->CreateBuffer(&desc, &subData, &m_dummyIB);
}

void MarchingCubesSurfaceMeshGPU::ReleaseMeshBuffer(void)
{
	SafeRelease(&m_vertexBufferSet.buffer);
	SafeRelease(&m_vertexBufferSet.srv);
	SafeRelease(&m_vertexBufferSet.uav);
	SafeRelease(&m_indexBufferSet.buffer);
	SafeRelease(&m_indexBufferSet.srv);
	SafeRelease(&m_indexBufferSet.uav);
    SafeRelease(&m_edgeCacheBufferSet.buffer);
    SafeRelease(&m_edgeCacheBufferSet.srv);
    SafeRelease(&m_edgeCacheBufferSet.uav);
}
