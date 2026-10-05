#pragma once
//=============================================================================
// [LaserRenderer.h]
//  レーザー専用の描画クラス
//  - Billboard インポスターで“円断面”のビームを表現
//  - HDR + Bloom 前提。加算ブレンド / 深度書き込みオフ推奨。
//=============================================================================
#include "Effects/ProjectilePreview/ProjectilePredictor.h"
#include "Core/Camera.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************

// ヒット種別：レンダリング分岐に使用
enum class LaserHitKind : uint8_t
{
    None = 0,       // ヒットなし（進行中）
    Player,         // プレイヤーAABBに命中
    Environment,    // VoxelGrid（環境）に命中
    MaxRange,       // 射程到達で停止
};

//*****************************************************************************
// 構造体定義
//*****************************************************************************

// レーザーのパラメータ
struct LaserParams
{
    // レーザー挙動
    float speed = 1600.0f;   // 1秒あたり進行距離（直線）
    float maxLength = 10000.0f;  // 最大射程
    float headJitter = 0.0f;    // 先端揺らぎ（任意）

    // ビジュアル
    float radiusWorld = 35.0f;   // ビーム半径（ワールド単位）
    float corePower = 1.0f;    // 中心部の強度（PSで使用）
    float flowSpeed = 1.5f;    // UV流速
};

// レーザーの状態
struct LaserState
{
    XMFLOAT3 start = { 0,0,0 };     // 起点
    XMFLOAT3 dir = { 0,0,1 };       // 正規化済み
    float    traveled = 0.0f;       // 現在長さ
    bool     active = false;        // 稼働中
    bool     stopped = false;       // 命中で停止したか
    XMFLOAT3 hitPos = { 0,0,0 };
    XMFLOAT3 hitNormal = { 0,0,0 };
    LaserHitKind lastHit = LaserHitKind::None; // 直近の停止理由/命中先

    float    decalTimer = 0.0f;   // 連続ヒット中の経過タイマ
    bool     firstHitSpawned = false; // 初回ヒットで一度だけ即生成
    bool     hasLastDecal = false;
    XMFLOAT3 lastDecalPos{};
    XMFLOAT3 lastDecalN{};
};

// 追尾パラメータ
struct DirectionTrackParams
{
    // 追尾を始めるまでの遅延（秒）: この間は初期方向をロック
    float preFireDelaySec = 0.15f;

    // 角速度制限（度/秒）。0以下なら無効
    float maxDegPerSec = 60.0f;

    // 指数平滑の時間定数 τ（秒）。0以下なら無効
    float smoothTauSec = 0.4f;

    // 安全用：1フレームの最大角速度（度/秒）。0なら無効（smoothのみで制御）
    float clampTurnRateDegPerSec = 0.0f;

    // 微小角差の無視（度）。ノイズ抑制
    float deadZoneDeg = 0.5f;
};

// 内部追尾ステート
struct DirectionTrackState
{
    bool  armed = false;           // 追尾アーム済み（Beginでtrue）
    float delayLeft = 0.0f;        // 残り遅延
    XMFLOAT3 lockedDir = { 0,0,1 };  // 遅延中に保持する固定方向
    DirectionTrackParams params{}; // 現在の設定
};

// 頂点構造体（インポスター帯用）：位置 + "帯ローカルUV"(s,v)
struct LaserVB
{
    XMFLOAT3 pos;
    XMFLOAT2 sv; // (s, v)  長手s 0..1, r半径軸 -1..+1
};

// PS側の CBLaser と一致させる
struct CBLaserCPU
{
    XMFLOAT4 ColorHDR;   // g_ColorHDR
    float    Time;       // g_Time
    float    FlowSpeed;  // g_FlowSpeed
    float    CorePower;  // g_CorePower
    float    Radius;     // g_Radius
};

// デカールの発生ルール
struct DecalParams
{
    float cooldownSec = 0.12f;  // 連続ヒット中の間隔（秒）
    float minDistance = 5.0f;  // 前回デカール位置からの最小距離
    float samePlaneDot = 0.995f;// 同一平面とみなす法線ドット閾（5.7°）
    float minNormalDot = 0.98f; // 「法線が十分違う」とみなす閾（11.5°）
    float sizeMin = 95.0f;  // 生成サイズ最小（任意）
    float sizeMax = 140.0f;  // 生成サイズ最大（任意）
};

class LaserRenderer
{
public:
	LaserRenderer() : m_device(Renderer::get_instance().GetDevice()),
		m_context(Renderer::get_instance().GetDeviceContext()) {}

    void Initialize(void);
    void Shutdown(void);

    // 発射開始
    void Begin(const XMFLOAT3& start, const XMFLOAT3& dirNorm,
        const LaserParams& params, const XMFLOAT4& hdrColor);

    // 前進：線分[p0,p1]で AABB と交差すれば停止（outHit* に返す）
    void Advance(float dt, const BOUNDING_BOX* playerAABB = nullptr,
        XMFLOAT3* outHitPos = nullptr, XMFLOAT3* outHitNormal = nullptr);

    // 強制終了
    void End(void) { m_state.active = false; }

    // 描画（インポスター：1本のビルボード帯で表現）
    void Draw(int planeCount = 12, float brightnessScale = 0.8f);

    // 点列サンプリングのON/OFF（投擲リボン流用やデバッグ用）
    void EnableTrailSampling(bool enable, float pointSpacing = 0.25f);

    // 命中種別の問い合わせ
    LaserHitKind GetLastHitKind(void) const { return m_state.lastHit; }

    // 状態取得
    const LaserState& GetState(void) const { return m_state; }

    // 方向をスムーズに変更する（newDirNormは正規化済み想定）
    void SetDirectionSmooth(const XMFLOAT3& newDirNorm, float smooth = 0.2f);

    // 発射開始時に呼ぶ：追尾をアーム（遅延ロック開始）
    void ArmDirectionTracking(const XMFLOAT3& initialDirNorm,
        const DirectionTrackParams& params);

    // 毎フレーム呼ぶ：プレイヤー方向 desiredDir を受けて内部の dir を更新
    void UpdateDirection(const XMFLOAT3& desiredDirNorm, float dtSec);

private:
    // 段の局所基底（t, r, u）を構築（t=dir, r=横, u=厚み）
    void BuildAxis(XMVECTOR& outT, XMVECTOR& outR, XMVECTOR& outU) const;

    // 1本の帯（2三角形）を生成（CPU側で4頂点構築）
    void BuildQuadVerticesAtAngle(float theta);

    // シェーダー読み込み
    bool LoadShaders(void);

    // 環境衝突（VoxelGrid）を使う場合はセットする（nullptr 可）
    void SetCollisionVoxelGrid(const VoxelGrid* grid) { m_voxelGrid = grid; }

    // 線分とAABBの交差判定
    bool SegmentAABBIntersect(const XMFLOAT3& p0, const XMFLOAT3& p1,
        const XMFLOAT3& bmin, const XMFLOAT3& bmax,
        float* outT, XMFLOAT3* outN);

	// 1平面分の描画
    void DrawOnePlane(float theta, float weight);

	// 定数バッファ更新
	void UpdateConstantBuffer(float weight);

    // 角速度制限で d0→dTarget を step だけ回す
    XMVECTOR RotateTowardWithMaxStep(XMVECTOR d0, XMVECTOR dTarget,
        float maxStepRad, float deadZoneRad);

    // 環境ヒット時のデカール生成（間引き）
    void HandleDecalOnHit(float dt, const XMFLOAT3& hitPos, const XMFLOAT3& hitN);

	// AABBを中心で拡大縮小
    void ScaleAabbAboutCenter(const XMFLOAT3& inMin, const XMFLOAT3& inMax,
        float sx, float sy, float sz, XMFLOAT3& outMin, XMFLOAT3& outMax,
        float extEps = 1e-4f);

    LaserParams m_params{};
    LaserState  m_state{};
	DecalParams m_decal{}; // デカール発生ルール
    XMFLOAT4    m_color = { 1,0.35f,0.8f,1 }; // HDR推奨

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;

    // ジオメトリ（4頂点の動的VB / 6インデックスのIB）
    ID3D11Buffer* m_vb = nullptr;
    ID3D11Buffer* m_ib = nullptr;

    ID3D11Buffer* m_cbLaser = nullptr;     // PS用 定数バッファ

    // シェーダ/PSO は ShaderManager 経由で取得する想定
    ShaderSet m_shaderSet{};

    // レーザー用の環境衝突グリッド（任意）
	const VoxelGrid* m_voxelGrid = nullptr;

    float m_axisSmoothing = 0.2f; // 0..1（大きいほどすばやく追従）

    DirectionTrackState m_track;

    // 点列サンプリング（任意機能）
    bool  m_trailEnabled = false;
    float m_trailSpacing = 0.25f;       // 1点あたりの距離間隔
    SimpleArray<TrajectoryPoint> m_trailPoints;

	mutable XMVECTOR m_prevR = XMVectorSet(1, 0, 0, 0); // 前回の横方向ベクトル（スムーズ回転用）

    Camera& m_camera = Camera::get_instance();
    Renderer& m_renderer = Renderer::get_instance();
    Timer& m_timer = Timer::get_instance();
	ShaderResourceBinder& m_shaderBinder = ShaderResourceBinder::get_instance();
};