//=============================================================================
//
// ビュー／プロジェクション行列および視点制御ユーティリティ [Camera.h]
// Author : 
// シーン／スカイボックスの視点切替、Zレンジ・FOV・Viewport管理、
// 視点移動および行列更新を統一的に制御するカメラ管理クラス
//
//=============================================================================
#pragma once

//*****************************************************************************
// インクルードファイル
//*****************************************************************************
#include "Core/Graphics/Renderer.h"
#include "Utility/SingletonBase.h"
#include "Core/Timer.h"
#include "Utility/InputManager.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define	VIEW_ANGLE_DEFAULT				(XMConvertToRadians(60.0f))						// ビュー平面の視野角
#define	VIEW_ANGLE_AIM					(XMConvertToRadians(30.0f))						// ビュー平面の視野角
#define	VIEW_ASPECT						((float)SCREEN_WIDTH / (float)SCREEN_HEIGHT)	// ビュー平面のアスペクト比	
#define	VIEW_NEAR_Z						(10.0f)											// ビュー平面のNearZ値
#define	VIEW_FAR_Z_SKYBOX				(800000.0f)										// ビュー平面のFarZ値
#define	VIEW_FAR_Z_SCENE				(50000.0f)										// ビュー平面のFarZ値

enum 
{
	TYPE_FULL_SCREEN,
	TYPE_LEFT_HALF_SCREEN,
	TYPE_RIGHT_HALF_SCREEN,
	TYPE_UP_HALF_SCREEN,
	TYPE_DOWN_HALF_SCREEN,
	TYPE_NONE,
};

// カメラ動作モード
enum class CameraMode
{
	Normal,		// 通常TPSモード
	Aiming,		// 照準モード（右クリック）
	ProjectileAiming,
	Returning	// 照準解除後の戻りモード
};

enum class CameraType
{
	SKYBOX,
	SCENE,
};


//*****************************************************************************
// プロトタイプ宣言
//*****************************************************************************

class Camera : public SingletonBase<Camera>
{
public:
	void Init(void);
	void Shutdown(void);
	void Update(void);
	void SetCamera(void);

	void SetViewPort(int type);
	int GetViewPortType(void);

	void SetCameraAT(XMFLOAT3 pos);

	XMFLOAT4X4 GetViewMatrix(void) { return m_mtxView; }
	XMFLOAT4X4 GetProjMatrix(void) { return m_mtxProjection; }
	XMFLOAT3 GetRotation(void) { return m_rot; }
	XMFLOAT3 GetPosition(void) { return m_pos; }
	XMFLOAT3 GetForward(void) const;
	XMFLOAT3 GetRight(void) const;
	XMFLOAT3 GetUp(void) const;
	XMMATRIX GetViewProjMtx(void) { return XMLoadFloat4x4(&m_mtxView) * XMLoadFloat4x4(&m_mtxProjection); }

	void SetCameraType(CameraType type);
	float GetNearZ(void) { return m_nearZ; }
	float GetFarZ(void) { return m_farZ; }
	float GetFov(void) { return m_fov; }
	float GetAspectRatio(void) { return VIEW_ASPECT; }

	// 照準モード制御
	void SetAimMode(CameraMode aimMode, bool enable);
	bool IsAiming(void) const { return m_mode == CameraMode::Aiming; }

	// 銃口位置を基準に、画面中心（照準）へ向かう方向を計算する関数
	void ComputeAimDirection(const XMFLOAT3& muzzlePos, XMFLOAT3* direction, XMFLOAT3* targetPoint) const;
	// 銃口を持たないスキル・手榴弾用の照準方向を計算する関数
	void ComputeAimDirectionCenter(XMFLOAT3* direction, XMFLOAT3* targetPoint) const;

	XMFLOAT3 GetThrowModeDirection(void) const { return m_throwModeDir; }

private:
	void UpdateCameraAdjustments(void);

	// プレイヤーの右肩基準でカメラ位置を計算する共通関数
	// localOffset: x=右肩オフセット, y=高さ, z=後方距離
	XMFLOAT3 ComputeShoulderCameraPos(const XMFLOAT3& playerPos, const XMFLOAT3& playerForward, const XMFLOAT3& localOffset, float horizontalAngleOffset = 0.0f) const;

	// 行列
	XMFLOAT4X4			m_mtxView{};		// ビューマトリックス
	XMFLOAT4X4			m_mtxInvView{};		// ビューマトリックス
	XMFLOAT4X4			m_mtxProjection{};	// プロジェクションマトリックス
	XMMATRIX			m_projScene{};
	XMMATRIX			m_projSkybox{};

	// カメラ位置・回転情報
	XMFLOAT3			m_pos{};			// カメラの視点(位置)
	XMFLOAT3			m_at{};				// カメラの注視点
	XMFLOAT3			m_up{};				// カメラの上方向ベクトル
	XMFLOAT3			m_rot{};			// カメラの回転

	// カメラ距離やFOV
	float				m_len = 0.0f;			// カメラの視点と注視点の距離
	float				m_fov = 0.0f;
	float				m_nearZ = 0.0f;
	float				m_farZ = 0.0f;

	// 照準用パラメータ
	CameraMode	m_mode = CameraMode::Normal;
	float		m_targetFov = VIEW_ANGLE_DEFAULT;
	XMFLOAT3	m_targetOffsetNormal = { 0.0f, 0.0f, 0.0f }; // 通常時のカメラオフセット
	XMFLOAT3	m_targetOffsetAim = { 0.5f, 1.6f, -2.0f };    // 照準時のカメラオフセット（右肩寄り）
	XMFLOAT3	m_currentOffset{};

	float		m_throwModeRotx = 0.0f; // 投擲モードの回転角度
	XMFLOAT3	m_throwModeDir = { 0.0f, 0.0f, 1.0f }; // 投擲モードの方向
	CameraType			m_CameraType = CameraType::SCENE;

	// 各種参照
	Timer& m_timer = Timer::get_instance();
	DebugProc& m_debugProc = DebugProc::get_instance();
	InputManager& m_inputManager = InputManager::get_instance();
	Renderer& m_renderer = Renderer::get_instance();
};