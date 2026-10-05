#include "Scene/Weapon/Projectile.h"
#include "Effects/DecalRenderer.h"
#include "Effects/EffectSystem.h"
#include "Effects/SpecialEffects/FireBallEffectRenderer.h"
#include "Core/GameSystem.h"
#include "Core/AudioManager.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MODEL_BULLET_PATH			"data/MODEL/Weapon/Bullet/cube.obj"
#define MODEL_SUN_BULLET_PATH		"data/MODEL/Weapon/SunBullet/cube.obj"
#define MODEL_ICE_SHARD_PATH		"data/MODEL/Weapon/IceShard/Ice_Shard.obj"

Projectile::Projectile(ProjectileType projectileType, Transform transform, XMFLOAT3 dir, float speed)
{
	m_projectileAttributes.projectileType = projectileType;
	instance.use = true;		// true:生きてる

	m_projectileAttributes.initTrans.pos = transform.pos;
	m_projectileAttributes.initTrans.rot = transform.rot;
	m_projectileAttributes.initTrans.scl = transform.scl;
	m_projectileAttributes.dir = dir;
	m_projectileAttributes.speed = speed;
	m_projectileAttributes.duration = 20.0f; // 20秒で消える
	m_projectileAttributes.isShot = true;

	GameObjectConfig config;

	switch (projectileType)
	{
	case ProjectileType::Bullet:
		config.modelPath = MODEL_BULLET_PATH;
		config.collisionType = ObjectCollisionType::COLLIDER_BOUNDING_BOX;
		config.colliderTag = ColliderTag::PLAYER_PROJECTILE;
		config.modelType = ModelType::Default;
		instance.renderProgress.isRandomFade = false;
		instance.renderProgress.progress = 1.0f;
		break;
	case ProjectileType::SunBullet:
		config.modelPath = MODEL_SUN_BULLET_PATH;
		config.collisionType = ObjectCollisionType::COLLIDER_BOUNDING_BOX;
		config.colliderTag = ColliderTag::PLAYER_PROJECTILE;
		config.modelType = ModelType::Default;
		instance.renderProgress.isRandomFade = false;
		instance.renderProgress.progress = 1.0f;
		break;
	case ProjectileType::IceShard:
		config.modelPath = MODEL_ICE_SHARD_PATH;
		config.collisionType = ObjectCollisionType::COLLIDER_BOUNDING_BOX;
		config.colliderTag = ColliderTag::ENEMY_PROJECTILE;
		config.modelType = ModelType::Default;
		instance.renderProgress.isRandomFade = true;
		instance.renderProgress.progress = 0.0f;
		break;
	default:
		return;
	}


	Instantiate(config);
	instance.collider.enable = false;

	Initialize();

}

Projectile::~Projectile()
{

}

void Projectile::Initialize(void)
{
	instance.transform.pos = m_projectileAttributes.initTrans.pos;
	instance.transform.rot = m_projectileAttributes.initTrans.rot;
	instance.transform.scl = m_projectileAttributes.initTrans.scl;

	instance.castShadow = true; // 影を落とす

	instance.collider.AddCollisionCallback(OnHitGroundCallback, this, CollisionCallbackID::ProjectileHit);
}

void Projectile::SetActive(bool enable)
{
	instance.collider.enable = enable;
	instance.castShadow = enable;
	instance.attributes.use = enable;
}

void Projectile::Update(void)
{
	if (!instance.load)
	{
		GameObject::Update();

		if (instance.load)
			instance.collider.enable = true;

		return;
	}

	if (instance.use == true)
	{
		if (m_projectileAttributes.isShot)
		{
			if (m_projectileAttributes.isHoming)
			{
				const XMFLOAT3 playerPos = GameSystem::get_instance().GetPlayer()->GetTransform().pos;
				XMFLOAT3& pos = instance.transform.pos;

				XMVECTOR vPos = XMLoadFloat3(&pos);
				XMVECTOR vPlayer = XMLoadFloat3(&playerPos);
				XMVECTOR vToTarget = XMVectorSubtract(vPlayer, vPos); // プレイヤーへのベクトル
				XMVECTOR vToTargetN = XMVector3Normalize(vToTarget); // 正規化

				// 現在の進行方向
				XMVECTOR vCurDir = XMLoadFloat3(&m_projectileAttributes.dir);
				vCurDir = XMVector3Normalize(vCurDir);

				const float kMaxTurnRateRadPerSec = XM_PI / 4.0f; // 1秒あたりの最大旋回角度（ラジアン）
				const float dt = m_timer.GetScaledDeltaTime();

				float angle = 0.0f;
				// angleBetweenVectors ∈ [0, π]
				XMVECTOR vAngle = XMVector3AngleBetweenVectors(vCurDir, vToTargetN);
				XMStoreFloat(&angle, vAngle);

				// 二つのベクトルがほぼ同じ方向を向いている場合、または反対方向を向いている場合の処理
				float maxTurnThisFrame = kMaxTurnRateRadPerSec * dt;
				float t = (angle < 1e-5f) ? 1.0f : min(1.0f, maxTurnThisFrame / angle);

				XMVECTOR vNewDir = XMVector3Normalize(XMVectorLerp(vCurDir, vToTargetN, t)); // 補間

				// 新しい方向を保存
				float speed = m_projectileAttributes.speed * dt;
				XMVECTOR vStep = XMVectorScale(vNewDir, speed);
				XMVECTOR vNewPos = XMVectorAdd(vPos, vStep);
				XMStoreFloat3(&m_projectileAttributes.dir, vNewDir);
				XMStoreFloat3(&pos, vNewPos);

				// 角度を更新
				auto approachAngle = [](float cur, float tgt, float maxDelta)
					{
						// curをtgtにmaxDelta以内で近づける
						float delta = XMScalarModAngle(tgt - cur);
						delta = Clamp(delta, -maxDelta, maxDelta);
						return cur + delta;
					};

				// 現在のピッチとロールを取得し、目標のピッチとロールに向けて徐々に近づける
				float targetPitch = m_projectileAttributes.dir.x, targetRoll = m_projectileAttributes.dir.z;
				instance.transform.rot.x = approachAngle(instance.transform.rot.x, targetPitch, maxTurnThisFrame);
				instance.transform.rot.z = approachAngle(instance.transform.rot.z, targetRoll, maxTurnThisFrame);

				CollisionManager::get_instance().RaycastForTargetPoint(
					instance.transform.pos, 
					m_projectileAttributes.dir, 
					&m_projectileAttributes.potentialHitPos
				);
			}
			else
			{
				float speed = m_projectileAttributes.speed * m_timer.GetScaledDeltaTime();

				instance.transform.pos.x += m_projectileAttributes.dir.x * speed;
				instance.transform.pos.y += m_projectileAttributes.dir.y * speed;
				instance.transform.pos.z += m_projectileAttributes.dir.z * speed;
			}

			m_projectileAttributes.duration -= m_timer.GetDeltaTime();

			if (m_projectileAttributes.duration < 0) 
			{
				SetActive(false);
			}
		}

		GameObject::Update();

		//instance.transform.rot.y = 10 * timer.GetScaledDeltaTime();

	}
}

void Projectile::Draw(void)
{
	GameObject::Draw();
}

void Projectile::PlayEffect(HitColliderType type, const XMFLOAT3& hitNormal)
{
	// エフェクト再生
	switch (m_projectileAttributes.projectileType)
	{
	case ProjectileType::Bullet:
	{
		if (type == HitColliderType::Environment)
			DecalRenderer::get_instance().AddDecal(m_projectileAttributes.potentialHitPos, hitNormal, 35.0f, 25.0f, 0.1f);
		break;
	}
	case ProjectileType::SunBullet:
	{
		FireBallEffectParams fireparams;
		fireparams.type = EffectType::FireBall;
		fireparams.duration = 1.5f;
		fireparams.position = instance.transform.pos;
		fireparams.scale = 23.0f;
		fireparams.lifeMin = 1.0f;
		fireparams.lifeMax = 1.0f;
		fireparams.spawnRateMin = 0.0f;
		fireparams.spawnRateMax = 10.0f;
		fireparams.acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);
		fireparams.startColor = XMFLOAT4(1.0f, 0.5f, 0.2f, 1.0f);
		fireparams.endColor = XMFLOAT4(1.0f, 0.2f, 0.2f, 0.0f);
		fireparams.tilesX = FIREBALL_ANIM_TILE_X;
		fireparams.tilesY = FIREBALL_ANIM_TILE_Y;
		fireparams.coneAngleDegree = 25.0f;
		fireparams.coneRadius = 0.6f;
		fireparams.coneLength = 5.0f;
		fireparams.frameLerpCurve = 1.0f;
		fireparams.rotationSpeed = 0.0f;
		fireparams.startSpeedMin = 2.0f;
		fireparams.startSpeedMax = 3.0f;

		IEffectRenderer* effect = EffectSystem::get_instance().SpawnParticleEffect(fireparams);

		ParticleEffectParams smokeParams;
		smokeParams.type = EffectType::Smoke;
		smokeParams.duration = 3.0f; // 短い時間で消える
		smokeParams.position = instance.transform.pos;
		smokeParams.position.y += 30.0f; // 少し上に配置
		smokeParams.scale = 95.0f;
		smokeParams.acceleration = XMFLOAT3(0.0f, 25.0f, 0.0f);
		smokeParams.spawnRateMin = 2.0f;
		smokeParams.spawnRateMax = 3.0f;
		smokeParams.lifeMin = 1.5f;
		smokeParams.lifeMax = 2.0f;
		smokeParams.startColor = XMFLOAT4(0.5f, 0.2f, 0.2f, 0.3f);

		IEffectRenderer* smoke = EffectSystem::get_instance().SpawnParticleEffect(smokeParams);

		if (type == HitColliderType::Environment)
			DecalRenderer::get_instance().AddDecal(m_projectileAttributes.potentialHitPos, hitNormal, 60.0f, 45.0f, 0.8f);
		break;
	}
	default:
		break;
	}


}

void Projectile::OnHitGroundCallback(const CollisionEvent& event, void* context)
{
	Projectile* self = static_cast<Projectile*>(context);
	if (self == nullptr || !self->instance.use || !self->instance.load)
		return;

	self->SetActive(false);

	if (!event.additionalData) return;

	// 衝突した面の法線
	XMFLOAT3 hitNormal = *static_cast<const XMFLOAT3*>(event.additionalData);
	// 衝突エフェクト再生
	self->PlayEffect(HitColliderType::Environment, hitNormal);
	AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_boom_short);
}




