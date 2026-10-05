//=============================================================================
//
// カメラ処理 [Camera.cpp]
// Author : 
//
//=============================================================================
#include "main.h"
#include "Utility/InputManager.h"
#include "Utility/Debug/Debugproc.h"
#include "Core/Camera.h"
#include "Core/GameSystem.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define	POS_X_CAM			(0.0f)			// カメラの初期位置(X座標)
#define	POS_Y_CAM			(60.0f)			// カメラの初期位置(Y座標)
#define	POS_Z_CAM			(-350.0f)		// カメラの初期位置(Z座標)


#define	VALUE_MOVE_CAMERA	(2.0f)										// カメラの移動量
#define	VALUE_ROTATE_CAMERA	(XM_PI * 0.01f)								// カメラの回転量

#define _DEBUG_CAMERA		0

// カメラ調整のパラメータ設定
constexpr float MOUSE_SENSITIVITY = 0.003f;
constexpr float ZOOM_SENSITIVITY = 0.25f;
constexpr float MIN_CAMERA_DISTANCE = 160.0f;
constexpr float MAX_CAMERA_DISTANCE = 720.0f;
constexpr float CAMERA_APPROACH_SPEED = 3.0f;
constexpr float CAMERA_RETREAT_SPEED = 5.0f;
constexpr float GROUND_LIMIT_ANGLE = 0.3f;

// 垂直回転角度の制限 (正値は上方向、負値は下方向)
constexpr float MAX_VERTICAL_ANGLE = XM_PIDIV2 - 0.1f;
constexpr float MIN_VERTICAL_ANGLE = -0.1f;

// 照準モード専用の垂直角度制限（例：-60° ～ +45°）
constexpr float AIM_MIN_VERTICAL_ANGLE = XMConvertToRadians(-60.0f);
constexpr float AIM_MAX_VERTICAL_ANGLE = XMConvertToRadians(60.0f);

//*****************************************************************************
// グローバル変数
//*****************************************************************************
static int	g_ViewPortType = TYPE_FULL_SCREEN;
static BOOL isDragging = FALSE;
static long startX = 0, startY = 0;
static long currentX = 0, currentY = 0;
static long deltaX = 0, deltaY = 0;
GameSystem& g_gameSystem = GameSystem::get_instance();

//=============================================================================
// 初期化処理
//=============================================================================
void Camera::Init(void)
{
	m_pos = { POS_X_CAM, POS_Y_CAM, POS_Z_CAM };
	m_at  = { 0.0f, 0.0f, 0.0f };
	m_up  = { 0.0f, 1.0f, 0.0f };
	m_rot = { 0.0f, 0.0f, 0.0f };
	m_fov = VIEW_ANGLE_DEFAULT;

	// 視点と注視点の距離を計算
	float vx, vz;
	vx = m_pos.x - m_at.x;
	vz = m_pos.z - m_at.z;
	m_len = sqrtf(vx * vx + vz * vz);
	
	m_pos.x = m_at.x - sinf(m_rot.y) * m_len;
	m_pos.z = m_at.z - cosf(m_rot.y) * m_len;
	m_pos.y = m_at.y + sinf(m_rot.x) * m_len;

	m_nearZ = VIEW_NEAR_Z;
	m_farZ = VIEW_FAR_Z_SCENE;

	// ビューポートタイプの初期化
	SetViewPort(g_ViewPortType);


	// プロジェクションマトリックス設定
	m_projScene = XMMatrixPerspectiveFovLH(VIEW_ANGLE_DEFAULT, VIEW_ASPECT, m_nearZ, VIEW_FAR_Z_SCENE);
	m_projSkybox = XMMatrixPerspectiveFovLH(VIEW_ANGLE_DEFAULT, VIEW_ASPECT, m_nearZ, VIEW_FAR_Z_SKYBOX);

	SetCameraType(CameraType::SCENE);
}


//=============================================================================
// カメラの終了処理
//=============================================================================
void Camera::Shutdown(void)
{

}


//=============================================================================
// カメラの更新処理
//=============================================================================
void Camera::Update(void)
{
	if (!GetWindowActive() 
		|| GameSystem::get_instance().IsPaused()
		|| GameSystem::get_instance().IsAltDown()
		|| GameSystem::get_instance().GetMode() != GameMode::GAME)
		return; // 非アクティブならカメラ更新スキップ

	// カメラの調整を行う
	UpdateCameraAdjustments();

	// プレイヤー視点
	Player* player = g_gameSystem.GetPlayer();
	if (!player) return;

	// プレイヤー情報
	const XMFLOAT3& playerPos = player->GetTransform().pos;
	const BOUNDING_BOX& playerBB = player->GetBoundingBox();
	float playerHeight = playerBB.GetHeight();
	float playerWidth = playerBB.GetWidth();

	XMFLOAT3 lookAt = playerPos;
	lookAt.y += playerHeight * 0.56f;
	m_at = lookAt;

	if (m_mode == CameraMode::Normal)
	{
		// カメラ位置の更新
		m_pos.x = m_at.x - sinf(m_rot.y) * cosf(m_rot.x) * m_len;
		m_pos.z = m_at.z - cosf(m_rot.y) * cosf(m_rot.x) * m_len;
		m_pos.y = m_at.y + sinf(m_rot.x) * m_len;
	}
	else
	{

		XMFLOAT3 targetCamPos{};

		if (m_mode == CameraMode::Aiming)
		{
			// 照準モード用の局所オフセット（右肩基準）
			XMFLOAT3 localOffset = {
				playerWidth * 0.45f,		// 右肩寄り
				playerHeight * 0.8f,		// 頭付近
			   -playerHeight * 2.0f			// 後方距離（半身視点）
			};

			// プレイヤーのforward/right計算
			XMFLOAT3 playerForward = player->GetTransform().GetForward();
			XMFLOAT3 playerRight = player->GetTransform().GetRight();
			XMVECTOR forwardVec = XMLoadFloat3(&playerForward);
			XMVECTOR rightVec = XMLoadFloat3(&playerRight);

			// LookAtをforward方向にずらす
			XMVECTOR playerPosVec = XMLoadFloat3(&playerPos);
			targetCamPos = ComputeShoulderCameraPos(playerPos, playerForward, localOffset, -11.0f);

			// 基本的なlook targetはプレイヤー前方
			float lookAheadDist = playerBB.GetHeight() * 2.0f; // 2倍程度前方を見る
			XMVECTOR flatForward = XMVector3Normalize(XMVectorSetY(forwardVec, 0.0f)); // 水平成分のみ
			XMVECTOR lookTarget = playerPosVec + flatForward * lookAheadDist;

			// プレイヤー頭部の基準Y座標
			float headY = XMVectorGetY(playerPosVec) + playerHeight * 0.9f;

			// カメラ上下回転を反映した視線Y
			float adjustedY = headY + (-sinf(m_rot.x) * lookAheadDist);

			// プレイヤー頭が画面から外れないようにClamp
			float minVisibleY = headY - playerHeight * 0.8f;
			float maxVisibleY = headY + playerHeight * 0.5f;
			if (adjustedY < minVisibleY) adjustedY = minVisibleY;
			if (adjustedY > maxVisibleY) adjustedY = maxVisibleY;

			// 最終的なターゲットを設定
			lookTarget = XMVectorSetY(lookTarget, adjustedY);

			// 仰角に応じたカメラ高さ補正
			float verticalAdjust = sinf(m_rot.x) * (playerHeight * 0.6f);
			targetCamPos.y += verticalAdjust;

			// m_atを更新
			XMStoreFloat3(&m_at, lookTarget);
		}
		else if (m_mode == CameraMode::ProjectileAiming)
		{
			// 照準モード用の局所オフセット（右肩基準）
			XMFLOAT3 localOffset = {
				playerWidth * 0.45f,		// 右肩寄り
				playerHeight * 0.8f,		// 頭付近
			   -playerHeight * 1.7f			// 後方距離（半身視点）
			};

			XMFLOAT3 playerForward = player->GetTransform().GetForward();
			targetCamPos = ComputeShoulderCameraPos(playerPos, playerForward, localOffset);

			// プレイヤー位置ベクトル
			XMVECTOR posVec = XMLoadFloat3(&playerPos);
			XMVECTOR forwardVec = XMLoadFloat3(&playerForward);
			XMVECTOR flatForward = XMVector3Normalize(XMVectorSetY(forwardVec, 0.0f));

			// 投擲用の射出方向 dir を計算
			// yaw: プレイヤーのforward, pitch: m_throwModeRotx
			XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), flatForward));
			XMVECTOR upVec = XMVectorSet(0, 1, 0, 0);

			XMVECTOR dirVec =
				flatForward * cosf(m_throwModeRotx) +    // 前方成分
				upVec * sinf(m_throwModeRotx);           // 上下成分

			dirVec = XMVector3Normalize(dirVec);
			XMStoreFloat3(&m_throwModeDir, dirVec);

			// lookTarget: 視点の前方 (カメラは固定、視線のみ調整)
			float lookAheadDist = playerHeight * 2.0f; // 2倍程度前方を見る
			XMVECTOR lookTarget = posVec + flatForward * lookAheadDist;

			// プレイヤー頭部の基準Y座標
			float headY = XMVectorGetY(posVec) + playerHeight * 0.9f;
			// 投擲モードは上下補正せず、常に頭の高さ基準
			lookTarget = XMVectorSetY(lookTarget, headY);

			// m_atを更新
			XMStoreFloat3(&m_at, lookTarget);
		}
		else if (m_mode == CameraMode::Returning)
		{
			// 戻りモード用のオフセット（プレイヤーの後方）
			targetCamPos.x = m_at.x - sinf(m_rot.y) * cosf(m_rot.x) * m_len;
			targetCamPos.z = m_at.z - cosf(m_rot.y) * cosf(m_rot.x) * m_len;
			targetCamPos.y = m_at.y + sinf(m_rot.x) * m_len;

			float dist = sqrtf((m_pos.x - targetCamPos.x) * (m_pos.x - targetCamPos.x) +
				(m_pos.y - targetCamPos.y) * (m_pos.y - targetCamPos.y) +
				(m_pos.z - targetCamPos.z) * (m_pos.z - targetCamPos.z));
			bool fovClose = fabs(m_fov - VIEW_ANGLE_DEFAULT) < 0.1f;

			float targetRotX = m_rot.x;
			if (targetRotX < MIN_VERTICAL_ANGLE) targetRotX = MIN_VERTICAL_ANGLE;
			if (targetRotX > MAX_VERTICAL_ANGLE) targetRotX = MAX_VERTICAL_ANGLE;

			float t = min(1.0f, m_timer.GetScaledDeltaTime() * 5.0f); // 補間スピード調整
			m_rot.x = Lerp(m_rot.x, targetRotX, t);

			if (dist < 5.0f && fovClose)
			{
				m_mode = CameraMode::Normal;
			}

		}

		float lerpSpeed = 16.0f;
		float t = min(1.0f, m_timer.GetDeltaTime() * lerpSpeed);
		

		// FOVの補間
		m_fov = Lerp(m_fov, m_targetFov, t);
		// カメラ位置の補間
		m_pos.x = Lerp(m_pos.x, targetCamPos.x, t);
		m_pos.y = Lerp(m_pos.y, targetCamPos.y, t);
		m_pos.z = Lerp(m_pos.z, targetCamPos.z, t);

		// プロジェクション更新
		m_projScene = XMMatrixPerspectiveFovLH(m_fov, VIEW_ASPECT, m_nearZ, m_farZ);
		XMStoreFloat4x4(&m_mtxProjection, m_projScene);

	}

	SetCamera();

#ifdef _DEBUG
	m_debugProc.PrintDebugProc("Camere RotX: %f RotY: %f, RotZ: %f\n", m_rot.x, m_pos.y, m_rot.z);
	m_debugProc.PrintDebugProc("Camera look at: %f\n", m_at);
	m_debugProc.PrintDebugProc("Camera len: %f\n", m_len);
#endif // _DEBUG
}


//=============================================================================
// カメラの更新
//=============================================================================
void Camera::SetCamera(void)
{
	// ビューマトリックス設定
	XMMATRIX mtxView;
	mtxView = XMMatrixLookAtLH(XMLoadFloat3(&m_pos), XMLoadFloat3(&m_at), XMLoadFloat3(&m_up));
	m_renderer.SetViewMatrix(&mtxView);
	XMStoreFloat4x4(&this->m_mtxView, mtxView);

	XMMATRIX mtxInvView;
	mtxInvView = XMMatrixInverse(nullptr, mtxView);
	XMStoreFloat4x4(&this->m_mtxInvView, mtxInvView);

	m_renderer.SetShaderCamera(m_pos);

	XMMATRIX mtxProjection = XMLoadFloat4x4(&m_mtxProjection);
	m_renderer.SetProjectionMatrix(&mtxProjection);
}

//=============================================================================
// ビューポートの設定
//=============================================================================
void Camera::SetViewPort(int type)
{
	ID3D11DeviceContext *g_ImmediateContext = m_renderer.GetDeviceContext();
	D3D11_VIEWPORT vp;

	g_ViewPortType = type;

	// ビューポート設定
	switch (g_ViewPortType)
	{
	case TYPE_FULL_SCREEN:
		vp.Width = (FLOAT)SCREEN_WIDTH;
		vp.Height = (FLOAT)SCREEN_HEIGHT;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		break;

	case TYPE_LEFT_HALF_SCREEN:
		vp.Width = (FLOAT)SCREEN_WIDTH / 2;
		vp.Height = (FLOAT)SCREEN_HEIGHT;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		break;

	case TYPE_RIGHT_HALF_SCREEN:
		vp.Width = (FLOAT)SCREEN_WIDTH / 2;
		vp.Height = (FLOAT)SCREEN_HEIGHT;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = (FLOAT)SCREEN_WIDTH / 2;
		vp.TopLeftY = 0;
		break;

	case TYPE_UP_HALF_SCREEN:
		vp.Width = (FLOAT)SCREEN_WIDTH;
		vp.Height = (FLOAT)SCREEN_HEIGHT / 2;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		break;

	case TYPE_DOWN_HALF_SCREEN:
		vp.Width = (FLOAT)SCREEN_WIDTH;
		vp.Height = (FLOAT)SCREEN_HEIGHT / 2;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = (FLOAT)SCREEN_HEIGHT / 2;
		break;


	}
	g_ImmediateContext->RSSetViewports(1, &vp);

}


int Camera::GetViewPortType(void)
{
	return g_ViewPortType;
}



// カメラの視点と注視点をセット
void Camera::SetCameraAT(XMFLOAT3 pos)
{
	// カメラの注視点を引数の座標にしてみる
	m_at = pos;

	// カメラの視点をカメラのY軸回転に対応させている
	pos.x = m_at.x - sinf(m_rot.y) * m_len;
	pos.z = m_at.z - cosf(m_rot.y) * m_len;

}

XMFLOAT3 Camera::GetForward(void) const
{
	XMVECTOR pos = XMLoadFloat3(&m_pos);
	XMVECTOR at = XMLoadFloat3(&m_at);

	XMVECTOR forward = XMVector3Normalize(at - pos);

	XMFLOAT3 result;
	XMStoreFloat3(&result, forward);
	return result;
}

XMFLOAT3 Camera::GetRight(void) const
{
	XMVECTOR forward = XMLoadFloat3(&GetForward());
	XMVECTOR up = XMLoadFloat3(&m_up);
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));

	XMFLOAT3 result;
	XMStoreFloat3(&result, right);
	return result;
}

XMFLOAT3 Camera::GetUp(void) const
{
	XMVECTOR right = XMLoadFloat3(&GetRight());
	XMVECTOR forward = XMLoadFloat3(&GetForward());
	XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));

	XMFLOAT3 result;
	XMStoreFloat3(&result, up);
	return result;
}

void Camera::SetCameraType(CameraType type)
{
	m_CameraType = type;

	switch (type)
	{
	case CameraType::SKYBOX:
		m_farZ = VIEW_FAR_Z_SKYBOX;
		XMStoreFloat4x4(&m_mtxProjection, m_projSkybox);
		break;
	case CameraType::SCENE:
		m_farZ = VIEW_FAR_Z_SCENE;
		XMStoreFloat4x4(&m_mtxProjection, m_projScene);
		break;
	default:
		break;
	}

	XMMATRIX mtxProjection = XMLoadFloat4x4(&this->m_mtxProjection);
	m_renderer.SetProjectionMatrix(&mtxProjection);
}

void Camera::SetAimMode(CameraMode aimMode, bool enable)
{
	if (enable)
	{
		// 指定された照準モードに入る
		m_mode = aimMode;
		m_targetFov = VIEW_ANGLE_AIM;
		m_throwModeRotx = 0.3f; // 投擲モードの回転角度をリセット
	}
	else
	{
		// 照準モードから通常モードへ戻る処理
		if (m_mode == aimMode)
		{
			// 戻りモードに遷移
			m_mode = CameraMode::Returning;
			m_targetFov = VIEW_ANGLE_DEFAULT;
		}
	}
}

void Camera::ComputeAimDirection(const XMFLOAT3& muzzlePos, XMFLOAT3* direction, XMFLOAT3* targetPoint) const
{
	// 画面中心 (NDC)，Z=0.1（近平面付近）で使うと精度高い
	XMVECTOR ndcCenter = XMVectorSet(0.0f, 0.0f, 0.1f, 1.0f);

	// ビュー・プロジェクション逆行列を計算
	XMMATRIX view = XMLoadFloat4x4(&m_mtxView);
	XMMATRIX proj = XMLoadFloat4x4(&m_mtxProjection);
	XMMATRIX invViewProj = XMMatrixInverse(nullptr, view * proj);

	// NDC座標をワールド空間へ変換
	XMVECTOR worldPos = XMVector3TransformCoord(ndcCenter, invViewProj);

	// カメラ位置からワールド座標への射線方向
	XMVECTOR camPos = XMLoadFloat3(&m_pos);
	XMVECTOR rayDir = XMVector3Normalize(worldPos - camPos);
	XMFLOAT3 rayDirF;
	XMStoreFloat3(&rayDirF, rayDir);

	XMVECTOR tgt;
	XMFLOAT3 targetPointOut;
	if (CollisionManager::get_instance().RaycastForTargetPoint(m_pos, rayDirF, &targetPointOut))
	{
		// ヒットした場合はその位置を目標点とする
		tgt = XMLoadFloat3(&targetPointOut);
	}
	else
	{
		// デフォルトの目標点：カメラ前方10000m
		tgt = camPos + rayDir * 10000.0f;
	}

	//// 微調整用オフセット
	//XMFLOAT3 offset = { 0.0f, 0.0f, 0.0f };
	//XMVECTOR offsetVec = XMLoadFloat3(&offset);
	//XMVECTOR tgtAdjusted = tgt + offsetVec;

	// 銃口位置から目標点への最終発射方向
	XMVECTOR muzzle = XMLoadFloat3(&muzzlePos);
	XMVECTOR finalDir =  XMVector3Normalize(tgt - muzzle);

	// 結果を出力
	if (direction) XMStoreFloat3(direction, finalDir);
	if (targetPoint) XMStoreFloat3(targetPoint, tgt);
}

void Camera::ComputeAimDirectionCenter(XMFLOAT3* direction, XMFLOAT3* targetPoint) const
{
	// NDC空間の画面中心点 (z=1.0fは遠クリップ面)
	XMVECTOR ndcCenter = XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);

	// ビュー・プロジェクション逆行列を計算
	XMMATRIX view = XMLoadFloat4x4(&m_mtxView);
	XMMATRIX proj = XMLoadFloat4x4(&m_mtxProjection);
	XMMATRIX invViewProj = XMMatrixInverse(nullptr, view * proj);

	// NDC座標をワールド空間へ変換
	XMVECTOR worldPos = XMVector3TransformCoord(ndcCenter, invViewProj);

	// カメラ位置からワールド座標への射線方向
	XMVECTOR camPos = XMLoadFloat3(&m_pos);
	XMVECTOR rayDir = XMVector3Normalize(worldPos - camPos);

	// デフォルトの目標点：カメラ前方1000m
	XMVECTOR tgt = camPos + rayDir * 1000.0f;

	// カメラ位置から目標点への方向（単位ベクトル）
	XMVECTOR finalDir = XMVector3Normalize(tgt - camPos);

	// 結果を出力
	if (direction) XMStoreFloat3(direction, finalDir);
	if (targetPoint) XMStoreFloat3(targetPoint, tgt);
}

void Camera::UpdateCameraAdjustments(void)
{
	// マウスの前回位置を保持する静的変数
	static long prevMouseX = 0;
	static long prevMouseY = 0;
	// 元のカメラ距離を保存
	static float originalDistance = m_len;
	static bool rotated = false;

	if (m_inputManager.IsMouseRecentered())
	{
		prevMouseX = GetMousePosX();
		prevMouseY = GetMousePosY();
		m_inputManager.SetMouseRecentered(false);  // 次のフレームから有効
	}
	else
	{
		long currentMouseX = GetMousePosX();
		long currentMouseY = GetMousePosY();

		// マウスの移動量（フレーム間の差分）を計算
		float deltaX = static_cast<float>(currentMouseX - prevMouseX);
		float deltaY = static_cast<float>(currentMouseY - prevMouseY);

		// 前回のマウス位置を更新
		prevMouseX = currentMouseX;
		prevMouseY = currentMouseY;

		// 巨大ジャンプを無視（ウィンドウ切替やバグ時の対策）
		if (fabs(deltaX) > 2000.0f) deltaX = 0.0f;
		if (fabs(deltaY) > 2000.0f) deltaY = 0.0f;

		// 水平方向の回転処理 (マウス左右移動)
		m_rot.y += deltaX * MOUSE_SENSITIVITY * m_timer.GetScaledDeltaTime();

		// 水平方向の回転角度を -π 〜 π に正規化
		if (m_rot.y > XM_PI)
			m_rot.y -= XM_2PI;
		if (m_rot.y < -XM_PI)
			m_rot.y += XM_2PI;

		// 垂直方向の回転処理 (マウス上下移動)
		if (m_mode == CameraMode::Aiming)
		{
			// 照準モードの上下視点制御
			float desiredRotX = m_rot.x + deltaY * MOUSE_SENSITIVITY * m_timer.GetScaledDeltaTime();

			// 制限内にClamp
			if (desiredRotX < AIM_MIN_VERTICAL_ANGLE)
				desiredRotX = AIM_MIN_VERTICAL_ANGLE;
			else if (desiredRotX > AIM_MAX_VERTICAL_ANGLE)
				desiredRotX = AIM_MAX_VERTICAL_ANGLE;

			// 照準時はカメラ距離(m_len)を調整せず、角度だけ更新
			m_rot.x = desiredRotX;
		}
		else if (m_mode == CameraMode::ProjectileAiming)
		{
			m_rot.x = 0.3f; // 投擲モードでは仰角を固定
			
			float desiredRotX = m_throwModeRotx - deltaY * MOUSE_SENSITIVITY * m_timer.GetScaledDeltaTime() * 0.1f;
			m_throwModeRotx = desiredRotX;

		}
		else
		{
			float desiredRotX = m_rot.x + deltaY * MOUSE_SENSITIVITY * m_timer.GetScaledDeltaTime();
			if (desiredRotX > MIN_VERTICAL_ANGLE && desiredRotX <= MAX_VERTICAL_ANGLE)
			{
				if (m_len < originalDistance && deltaY > 0)
				{
					m_len += CAMERA_RETREAT_SPEED * m_timer.GetScaledDeltaTime();
					if (m_len > originalDistance) m_len = originalDistance;
				}
				else
					m_rot.x = desiredRotX;
			}
			else if (desiredRotX <= MIN_VERTICAL_ANGLE)
			{
				// 地面に近づきすぎた場合の処理
				if (deltaY < 0)
				{
					m_len -= CAMERA_APPROACH_SPEED * m_timer.GetScaledDeltaTime();
					if (m_len < MIN_CAMERA_DISTANCE)
						m_len = MIN_CAMERA_DISTANCE;
				}
				else if (deltaY > 0 && m_len < originalDistance)
				{

					m_len += CAMERA_RETREAT_SPEED * m_timer.GetScaledDeltaTime();
					if (m_len > originalDistance)
						m_len = originalDistance;
				}
			}
			else if (desiredRotX > MAX_VERTICAL_ANGLE)
			{
				// 最大仰角を超えないように制限
				m_rot.x = MAX_VERTICAL_ANGLE;
			}
		}


		// マウスホイールによるズーム処理
		long wheelDelta = m_inputManager.GetMouseZ();
		if (wheelDelta != 0 && m_mode == CameraMode::Normal)
		{
			m_len -= wheelDelta * ZOOM_SENSITIVITY * m_timer.GetScaledDeltaTime();

			if (m_len < MIN_CAMERA_DISTANCE) m_len = MIN_CAMERA_DISTANCE;
			if (m_len > MAX_CAMERA_DISTANCE) m_len = MAX_CAMERA_DISTANCE;

			originalDistance = m_len;
		}


		// マウス位置を中央に戻す
		m_inputManager.SetMousePosCenter();
	}


#if _DEBUG_CAMERA
	if (m_inputManager.GetKeyboardPress(DIK_Z))
	{// 視点旋回「左」
		m_rot.y += VALUE_ROTATE_CAMERA;
		if (m_rot.y > XM_PI)
		{
			m_rot.y -= XM_2PI;
		}

		m_pos.x = m_at.x - sinf(m_rot.y) * m_len;
		m_pos.z = m_at.z - cosf(m_rot.y) * m_len;
	}

	if (m_inputManager.GetKeyboardPress(DIK_C))
	{// 視点旋回「右」
		m_rot.y -= VALUE_ROTATE_CAMERA;
		if (m_rot.y < -XM_PI)
		{
			m_rot.y += XM_2PI;
		}

		m_pos.x = m_at.x - sinf(m_rot.y) * m_len;
		m_pos.z = m_at.z - cosf(m_rot.y) * m_len;
	}

	if (m_inputManager.GetKeyboardPress(DIK_Y))
	{// 視点移動「上」
		m_pos.y += VALUE_MOVE_CAMERA;
	}

	if (m_inputManager.GetKeyboardPress(DIK_N))
	{// 視点移動「下」
		m_pos.y -= VALUE_MOVE_CAMERA;
	}

	if (m_inputManager.GetKeyboardPress(DIK_Q))
	{// 注視点旋回「左」
		m_rot.y -= VALUE_ROTATE_CAMERA;
		if (m_rot.y < -XM_PI)
		{
			m_rot.y += XM_2PI;
		}

		m_at.x = m_pos.x + sinf(m_rot.y) * m_len;
		m_at.z = m_pos.z + cosf(m_rot.y) * m_len;
	}

	if (m_inputManager.GetKeyboardPress(DIK_E))
	{// 注視点旋回「右」
		m_rot.y += VALUE_ROTATE_CAMERA;
		if (m_rot.y > XM_PI)
		{
			m_rot.y -= XM_2PI;
		}

		m_at.x = m_pos.x + sinf(m_rot.y) * m_len;
		m_at.z = m_pos.z + cosf(m_rot.y) * m_len;
	}

	if (m_inputManager.GetKeyboardPress(DIK_LCONTROL))
	{
		if (m_inputManager.IsMouseLeftTriggered())
		{
			isDragging = TRUE;
			startX = GetMousePosX();
			startY = GetMousePosY();
			currentX = startX;
			currentY = startY;
			deltaX = 0;
			deltaY = 0;
		}
		else if (m_inputManager.IsMouseLeftPressed() && isDragging == TRUE)
		{
			long newX = GetMousePosX();
			long newY = GetMousePosY();
			deltaX = newX - currentX;
			deltaY = newY - currentY;
			currentX = newX;
			currentY = newY;

		}
		else if (!m_inputManager.IsMouseLeftPressed())
		{
			isDragging = FALSE;
			deltaX = 0L;
			deltaY = 0L;
		}

		m_rot.x -= deltaY * VALUE_ROTATE_CAMERA * 0.1f * m_timer.GetScaledDeltaTime();
		m_rot.y -= deltaX * VALUE_ROTATE_CAMERA * 0.1f * m_timer.GetScaledDeltaTime();
		if (m_rot.y < -XM_PI)
		{
			m_rot.y += XM_2PI;
		}
		else if (m_rot.y > XM_PI)
		{
			m_rot.y -= XM_2PI;
		}

		m_pos.x = m_at.x - sinf(m_rot.y) * m_len;
		m_pos.z = m_at.z - cosf(m_rot.y) * m_len;
		m_pos.y = m_at.y + sinf(m_rot.x) * m_len;
	}


	if (m_inputManager.GetKeyboardPress(DIK_T))
	{// 注視点移動「上」
		m_at.y += VALUE_MOVE_CAMERA;
	}

	if (m_inputManager.GetKeyboardPress(DIK_B))
	{// 注視点移動「下」
		m_at.y -= VALUE_MOVE_CAMERA;
	}

	if (m_inputManager.GetKeyboardPress(DIK_U))
	{// 近づく
		m_len -= VALUE_MOVE_CAMERA;
		m_pos.x = m_at.x - sinf(m_rot.y) * m_len;
		m_pos.z = m_at.z - cosf(m_rot.y) * m_len;
	}

	if (m_inputManager.GetKeyboardPress(DIK_M))
	{// 離れる
		m_len += VALUE_MOVE_CAMERA;
		m_pos.x = m_at.x - sinf(m_rot.y) * m_len;
		m_pos.z = m_at.z - cosf(m_rot.y) * m_len;
	}

	// カメラを初期に戻す
	if (m_inputManager.GetKeyboardPress(DIK_R))
	{
		Shutdown();
		Init();
	}

#endif
}

XMFLOAT3 Camera::ComputeShoulderCameraPos(const XMFLOAT3& playerPos, const XMFLOAT3& playerForward, const XMFLOAT3& localOffset, float horizontalAngleOffset) const
{
	XMVECTOR f = XMLoadFloat3(&playerForward);
	XMVECTOR up = XMVectorSet(0, 1, 0, 0);
	XMVECTOR r = XMVector3Normalize(XMVector3Cross(up, f));

	XMVECTOR rawCameraOffset =
		r * localOffset.x +  // 右方向（肩越し）
		up * localOffset.y + // 高さ
		f * localOffset.z;   // 前後距離

	XMVECTOR player = XMLoadFloat3(&playerPos);
	XMVECTOR rotateOrigin = player + f;
	XMMATRIX rotation = XMMatrixRotationAxis(up, XMConvertToRadians(horizontalAngleOffset));
	XMVECTOR rotatedCameraPos = XMVector3TransformCoord(player + rawCameraOffset - rotateOrigin, rotation) + rotateOrigin;

	XMFLOAT3 targetPos;
	XMStoreFloat3(&targetPos, rotatedCameraPos);
	return targetPos;
}

