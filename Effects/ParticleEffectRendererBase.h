#pragma once
//=============================================================================
//
// パーティクルエフェクトの共通レンダラー基底クラス [ParticleEffectRendererBase.h]
// Author : 
// GPUベースのパーティクル（Emit / Update / Draw）処理を統一管理する、
// 全パーティクル系エフェクトの基底クラス
//
//=============================================================================
#include "Core/Timer.h"
#include "Core/Camera.h"
#include "Effects/IEffectRenderer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MAX_PARTICLES       (99999) // 最大パーティクル数

//*********************************************************
// 構造体
//*********************************************************
struct CBParticleUpdate
{
    XMFLOAT3 acceleration;
    float scale;
    
    float deltaTime;
    float totalTime;

    float lifeMin;
    float lifeMax;
    float spawnRateMin;
    float spawnRateMax;

    UINT maxParticlesCount;
    UINT particlesToEmitThisFrame;

    XMFLOAT4 startColor;
    XMFLOAT4 endColor;
};

struct CBParticleDraw
{
    // 行列情報
    XMMATRIX world;
    XMMATRIX worldInv;
    XMMATRIX viewProj;

	// カメラ情報
    XMFLOAT3 cameraPos;
    float padding;
};

struct DrawArgs
{
    UINT VertexCountPerInstance; // 各インスタンスあたりの頂点数（ビルボードなら4）
    UINT InstanceCount; // インスタンス数（アクティブなパーティクル数）
    UINT StartVertexLocation;  // 頂点開始位置（通常は0）
    UINT StartInstanceLocation; // インスタンス開始位置（通常は0）
};

// パーティクルシェーダーグループ
struct ParticleBufferSet
{
    ID3D11Buffer* buffer = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11UnorderedAccessView* uav = nullptr;
};


struct ParticleEffectParams;

// パーティクルエフェクト専用基底クラス
class ParticleEffectRendererBase : public IEffectRenderer
{
public:
    ParticleEffectRendererBase(void) {}

    virtual bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
    virtual void Shutdown(void) override;
    virtual void Destroy(void) override { Shutdown(); }
    virtual bool IsShutdown(void) const override { return m_shutdown; }
    virtual void Update(void) final override; // 継承クラスでオーバーライド禁止
    virtual void Draw(void) final override; // 継承クラスでオーバーライド禁止
    virtual EffectType GetEffectType(void) const override { return EffectType::None; } // 基底クラスはNoneを返す
    virtual bool UseDrawIndirect(void) const { return false; } // DrawInstancedIndirectを使用するかどうか
	virtual void ConfigureEffect(const ParticleEffectParams& params); // パーティクルエフェクトの設定を行う

    void SetScale(float scale) { m_scale = scale; }
    void SetPosition(const XMFLOAT3& pos) { m_position = pos; }
    void SetAcceleration(const XMFLOAT3& acc) { m_acceleration = acc; }
    void SetSpawnRateRange(float min, float max) { m_spawnRateMin = min; m_spawnRateMax = max; }
    void SetLifeRange(float min, float max) { m_lifeMin = min; m_lifeMax = max; }
    void SetParticleNum(UINT numParticles) { m_maxParticles = numParticles; }

    void SetupPipeline(void);  // パイプライン設定

    const XMFLOAT3& GetPosition(void) const { return m_position; }

    ParticleShaderGroup GetShaderGroupForEffect(EffectType type) const;
    ParticleComputeGroup GetComputeGroupForEffect(EffectType type) const;

    // パイプライン状態リセット
    static void ResetPipelineState(ParticleShaderGroup group) { s_pipelineBoundMap[TO_UINT64(group)] = false; }

    // 汎用的な構造体バッファを作成する
    static void CreateStructuredBuffer(ID3D11Device* device,
        UINT structSize,
        UINT count,
        const void* initData,
        ID3D11Buffer** ppBuffer,
        ID3D11ShaderResourceView** ppSRV,
        ID3D11UnorderedAccessView** ppUAV,
        bool allowAppendUAV = false,
        bool cpuWritable = false,
        bool useCounterUAV = false);


protected:

    void DrawIndirect(void); // DrawInstancedIndirectを使用する場合の描画関数

    // パーティクル更新用CB
    bool CreateConstantBuffer(void);

    // DrawInstancedIndirect() に必要な描画引数を GPU 側で保持するためのバッファ
    bool CreateDrawArgsBuffer(void);

    // GPU 上でアクティブなパーティクルのインデックスリスト（UINT 配列）を保持するバッファ
    // Compute Shader で書き込み、VS 側で参照してインスタンス ID から粒子 ID を取得します
    bool CreateAliveListBuffer(void);

    // GPU 上でフリーなパーティクルのインデックスリスト（UINT 配列）を保持するバッファ
    bool CreateFreeListBuffer(void);

    // アクティブなパーティクルリストを初期化する
    void InitializeFreeList(void);

    bool CreateParticleBuffer(void);  // パーティクルバッファを作成する
    virtual void DispatchComputeShader(void) = 0; // コンピュートシェーダーをディスパッチする
    virtual void DrawParticles(void); // パーティクルを描画する
    virtual void DispatchEmitParticlesCS(void); // パーティクル発生用コンピュートシェーダーをディスパッチする

    // パーティクル描画前の共通バインド
    virtual void BindParticleDrawResources(void);

    void BindCommonComputeResources(void); // コンピュートシェーダー用の共通リソースをバインドする
    void UnbindComputeUAVs(void); // コンピュートシェーダーのUAVをアンバインドする
    void UnbindAllShaderSRVs(void); // 全てのSRVをアンバインドする
    void PrepareForEmitCS(void); // 発生用CSの準備をする
    void ClearInternalUAVs(void); // UAVをクリアする

    // パーティクル更新用CB更新（Dispatch前に呼び出す）
    void UpdateParticleUpdateCB(void);
    // パーティクル発生器の更新
    void UpdateEmitter(void);
    // パーティクル描画用CB更新（Draw前に呼び出す）
    void UpdateParticleDrawCB(void);
    // DrawInstancedIndirect用のインスタンス数を更新
    void UpdateDrawArgsInstanceCount(void);
    // シェーダーを読み込む
    bool LoadShaders(void);

    // 登録型関数ポインタ
    using CreateBufferFunc = bool (*)(ParticleEffectRendererBase*);
    static CreateBufferFunc s_bufferFactoryTable[static_cast<UINT>(ParticleShaderGroup::Count)];

	// パーティクルバッファの作成関数
    static bool CreateBillboardSimpleBuffer(ParticleEffectRendererBase* self);
    static bool CreateBillboardFlipbookBuffer(ParticleEffectRendererBase* self);
	static bool CreateWaterFluidBuffer(ParticleEffectRendererBase* self);
    static bool CreateFallingParticlesBuffer(ParticleEffectRendererBase* self);
    void RegisterarticleBufferFactories(void);

    // 汎用パーティクルバッファ作成関数（構造体・初期化データ対応）
    // T : 粒子構造体の型（例：BillboardSimpleParticleなど）
    template<typename T>
    static bool CreateParticleBuffer(ParticleEffectRendererBase* self, const SimpleArray<T>& initData);

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    ID3D11Buffer* m_cbParticleUpdate = nullptr;  // 更新用定数バッファ
    ID3D11Buffer* m_cbParticleDraw = nullptr;    // 描画用定数バッファ
    ID3D11Buffer* m_drawArgsBuffer = nullptr;    // 描画用引数バッファ

	ParticleBufferSet m_particleBufferSet; // パーティクルバッファセット
	ParticleBufferSet m_aliveListBufferSet; // アクティブなパーティクルリストバッファセット
	ParticleBufferSet m_freeListBufferSet; // フリーリストバッファセット
    ID3D11UnorderedAccessView* m_freeListConsumeUAV = nullptr; // フリーリスト消費用UAV

    ID3D11Buffer* m_cbParticleEffect = nullptr; // 常量バッファ

    ParticleShaderGroup m_shaderGroup = ParticleShaderGroup::None;
    ParticleComputeGroup m_computeGroup = ParticleComputeGroup::None;
    ComputeShaderSet m_updateParticlesCS; // コンピュートシェーダー
    ComputeShaderSet m_emitParticlesCS; // パーティクル発生用コンピュートシェーダー
    ShaderSet m_shaderSet; // VS/GS/PSセット
    BLEND_MODE m_blendMode = BLEND_MODE_NONE;

    UINT m_maxParticles = MAX_PARTICLES; // パーティクル最大数
	float m_duration = -1.0f; // パーティクルの持続時間
    float m_scale = 1.0f; // パーティクルスケール
    float m_lifeMin = 0.0f; // 最小寿命
    float m_lifeMax = 0.0f; // 最大寿命
    float m_spawnRateMin = 0.0f; // 最小生成レート
    float m_spawnRateMax = 0.0f; // 最大生成レート
    XMFLOAT3 m_acceleration = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 m_position = { 0.0f, 0.0f, 0.0f };
    XMFLOAT4 m_startColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    XMFLOAT4 m_endColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    float m_accumulatedEmitCount = 0.0f; // 積算発生カウンタ（小数を保持）
    UINT m_particlesToEmitThisFrame = 0; // 今フレーム発生するパーティクル数
    UINT m_countBufferSizeBytes = sizeof(UINT);
    bool m_shutdown = false;
    static HashMap<uint64_t, bool, HashUInt64, EqualUInt64> s_pipelineBoundMap;

    ShaderResourceBinder& m_shaderResourceBinder = ShaderResourceBinder::get_instance();
    Renderer& m_renderer = Renderer::get_instance();
    DebugProc& m_debugProc = DebugProc::get_instance();
    Timer& m_timer = Timer::get_instance();
    Camera& m_camera = Camera::get_instance();
    ID3D11Buffer* m_drawArgsStaging = nullptr;
};

template<typename T>
inline bool ParticleEffectRendererBase::CreateParticleBuffer(ParticleEffectRendererBase* self, const SimpleArray<T>& initData)
{
    HRESULT hr = S_OK;

    // 粒子構造体のサイズ
    UINT stride = sizeof(T);

    // バッファ定義
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = stride * self->m_maxParticles;
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = stride;

    D3D11_SUBRESOURCE_DATA subData = {};
    subData.pSysMem = initData.data();

    // StructuredBuffer作成（RWStructuredBuffer用）
    hr = self->m_device->CreateBuffer(&bufferDesc, &subData, &self->m_particleBufferSet.buffer);
    if (FAILED(hr)) return false;

    // SRV作成
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.NumElements = self->m_maxParticles;
    hr = self->m_device->CreateShaderResourceView(self->m_particleBufferSet.buffer, &srvDesc, &self->m_particleBufferSet.srv);
    if (FAILED(hr)) return false;

    // UAV作成
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = self->m_maxParticles;
    hr = self->m_device->CreateUnorderedAccessView(self->m_particleBufferSet.buffer, &uavDesc, &self->m_particleBufferSet.uav);
    if (FAILED(hr)) return false;

    return true;
}
