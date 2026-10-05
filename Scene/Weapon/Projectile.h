#pragma once

#include "Scene/GameObject.h"
#include "Utility/SimpleArray.h"
#include "Core/Timer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
enum class ProjectileType
{
	Bullet,
	SunBullet,
	IceShard,
	Max
};

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct ProjectileAttributes
{
	ProjectileType projectileType;
	Transform	initTrans;
	XMFLOAT3 dir;
	float speed;
	float duration;
	XMFLOAT3 potentialHitPos; // 着弾予測位置
	bool isHoming; // ホーミングするかどうか
	bool isShot; // 発射されたかどうか 
};

class Projectile : public GameObject<ModelInstance> 
{

public:

	Projectile(ProjectileType projectileType, Transform transform, XMFLOAT3 dir, float speed);
	~Projectile();

	virtual void Initialize(void);

	void SetActive(bool enable);
	void SetPotentialHitPos(XMFLOAT3 pos) { m_projectileAttributes.potentialHitPos = pos; }
	void SetHoming(bool homing) { m_projectileAttributes.isHoming = homing; }
	void SetShot(bool shot) { m_projectileAttributes.isShot = shot; instance.collider.enable = shot; }
	bool GetShot(void) { return m_projectileAttributes.isShot; }
	void SetDir(XMFLOAT3 dir) { m_projectileAttributes.dir = dir; }

	void Update(void) override;
	void Draw(void) override;

	void PlayEffect(HitColliderType type, const XMFLOAT3& hitNormal);

	inline const ProjectileAttributes& GetProjectileAttributes(void) { return m_projectileAttributes; }

protected:
	// コリジョンイベントコールバック関数
	static void OnHitGroundCallback(const CollisionEvent& event, void* context);

	ProjectileAttributes m_projectileAttributes;


};