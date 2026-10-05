#include "Scene/Item/Item.h"
#include "Effects/EffectSystem.h"
#include "Effects/SpecialEffects/FireBallEffectRenderer.h"
#include "Effects/DecalRenderer.h"
#include "Core/AudioManager.h"

Item::Item(ItemType itemType, Transform trans)
{
	m_itemAttributes.itemType = itemType;
	instance.use = true;		// true:¶‚«‚Ä‚é

	m_itemAttributes.initTrans.pos = trans.pos;
	m_itemAttributes.initTrans.rot = trans.rot;
	m_itemAttributes.initTrans.scl = trans.scl;
}

Item::~Item()
{
}

void Item::Initialize(bool enableCollider)
{
	if (m_itemAttributes.initialized)
		return;

	GameObjectConfig config;

	switch (m_itemAttributes.itemType)
	{
	case ItemType::Key:
		config.modelPath = "data/MODEL/Item/Key/Key.obj";
		config.scale = XMFLOAT3(5.0f, 5.0f, 5.0f);
		config.rotation = XMFLOAT3(90.0f, 0.0f, 0.0f);
		break;
	case ItemType::Seed:
		config.modelPath = "data/MODEL/Item/Seed/Seed.obj";
		config.scale = XMFLOAT3(15.0f, 15.0f, 15.0f);
		config.rotation = XMFLOAT3(90.0f, 0.0f, 0.0f);
		break;
	case ItemType::Interactable:
		config.modelPath = "data/MODEL/Item/Vines/vines.obj";
		config.scale = XMFLOAT3(20.0f, 20.0f, 20.0f);
		config.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
		break;
	case ItemType::IntSoil:
		config.modelPath = "data/MODEL/Item/Soil/soil.obj";
		config.scale = XMFLOAT3(20.0f, 20.0f, 20.0f);
		config.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
		break;
	case ItemType::IntPSeed:
		config.modelPath = "data/MODEL/Item/Seed/Seed.obj";
		config.scale = XMFLOAT3(5.0f, 5.0f, 5.0f);
		config.rotation = XMFLOAT3(90.0f, 0.0f, 0.0f);
		break;
	case ItemType::IntVines:
		config.modelPath = "data/MODEL/Item/Vines/vines.obj";
		config.scale = XMFLOAT3(20.0f, 20.0f, 20.0f);
		config.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
		break;
	case ItemType::IntAutumnDoorL:
		config.modelPath = "data/MODEL/Item/Door/door_left.obj";
		config.scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
		config.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
		break;
	case ItemType::IntAutumnDoorR:
		config.modelPath = "data/MODEL/Item/Door/door_right.obj";
		config.scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
		config.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
		break;
	case ItemType::IntGate:
		config.modelPath = "data/MODEL/Gate/final_door_f.obj";
		config.scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
		config.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
		break;
	case ItemType::Bullet:
		config.modelPath = "data/MODEL/Item/Bullet/sun_bullet.obj";
		config.scale = XMFLOAT3(15.0f, 15.0f, 15.0f);
		config.rotation = XMFLOAT3(90.0f, 0.0f, 0.0f);
		break;
	}

	instance.renderProgress.isRandomFade = false;
	instance.renderProgress.progress = 1.0f;

	config.collisionType = ObjectCollisionType::COLLIDER_BOUNDING_BOX;
	config.colliderTag = ColliderTag::ITEM;
	config.modelType = ModelType::Default;
	config.enableCollider = enableCollider;

	if (m_itemAttributes.itemType >= ItemType::Interactable)
		config.colliderTag = ColliderTag::DEFAULT;

	Instantiate(config);

	instance.transform.pos = m_itemAttributes.initTrans.pos;
	instance.transform.rot = m_itemAttributes.initTrans.rot;
	instance.transform.scl = m_itemAttributes.initTrans.scl;

	m_itemAttributes.initialized = true;

	instance.collider.AddCollisionCallback(OnItemCollisionCallback, this, CollisionCallbackID::Item);
}

void Item::Draw(void)
{
	GameObject::Draw();
}

void Item::PlayEffect(HitColliderType type, const XMFLOAT3& hitNormal)
{
	if (m_itemAttributes.itemType == ItemType::Seed)
	{
		ParticleEffectParams smokeParams;
		smokeParams.type = EffectType::Smoke;
		smokeParams.duration = 7.5f;
		smokeParams.position = instance.transform.pos;
		smokeParams.position.y += 80.0f; // ­‚µã‚É”z’u
		smokeParams.scale = 250.0f;
		smokeParams.acceleration = XMFLOAT3(0.0f, 15.0f, 0.0f);
		smokeParams.spawnRateMin = 2.0f;
		smokeParams.spawnRateMax = 5.0f;
		smokeParams.lifeMin = 2.0f;
		smokeParams.lifeMax = 4.0f;
		smokeParams.startColor = XMFLOAT4(0.6f, 0.6f, 0.6f, 0.3f); // ‰ŒF

		if (type == HitColliderType::Enemy)
		{
			smokeParams.startColor = XMFLOAT4(0.6f, 0.2f, 0.2f, 0.3f); // “G‚É“–‚½‚Á‚½Žž‚ÍÔ‚¢‰Œ
			smokeParams.duration = 3.0f; // ’Z‚¢ŽžŠÔ‚ÅÁ‚¦‚é
			smokeParams.lifeMin = 1.5f;
			smokeParams.lifeMax = 2.0f;
		}

		IEffectRenderer* smoke = EffectSystem::get_instance().SpawnParticleEffect(smokeParams);

		FireBallEffectParams fireparams;
		fireparams.type = EffectType::FireBall;
		fireparams.duration = 5.0f;
		fireparams.position = instance.transform.pos;
		fireparams.scale = 43.0f;
		fireparams.lifeMin = 1.0f;
		fireparams.lifeMax = 1.0f;
		fireparams.spawnRateMin = 0.0f;
		fireparams.spawnRateMax = 10.0f;
		fireparams.acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);
		fireparams.startColor = XMFLOAT4(1.0f, 1.0f, 0.3f, 1.0f);
		fireparams.endColor = XMFLOAT4(1.2f, 0.5f, 0.0f, 0.0f);
		fireparams.tilesX = FIREBALL_ANIM_TILE_X;
		fireparams.tilesY = FIREBALL_ANIM_TILE_Y;
		fireparams.coneAngleDegree = 25.0f;
		fireparams.coneRadius = 0.6f;
		fireparams.coneLength = 5.0f;
		fireparams.frameLerpCurve = 1.0f;
		fireparams.rotationSpeed = 0.0f;
		fireparams.startSpeedMin = 2.0f;
		fireparams.startSpeedMax = 3.0f;

		if (type == HitColliderType::Enemy)
		{
			fireparams.startColor = XMFLOAT4(1.0f, 0.5f, 0.2f, 1.0f); // “G‚É“–‚½‚Á‚½Žž‚ÍÔ‚¢‰Î‰Š
			fireparams.endColor = XMFLOAT4(1.0f, 0.2f, 0.2f, 0.0f); // ’Z‚¢Žõ–½
			fireparams.duration = 1.5f; // ’Z‚¢ŽžŠÔ‚ÅÁ‚¦‚é
		}

		IEffectRenderer* effect = EffectSystem::get_instance().SpawnParticleEffect(fireparams);

		if (type == HitColliderType::Environment)
		{
			// ’n–Ê‚É“–‚½‚Á‚½Žž‚ÍƒfƒJ[ƒ‹‚à’Ç‰Á
			DecalRenderer::get_instance().AddDecal(instance.transform.pos, hitNormal, 175.0f, 50.0f);
		}
	}

}

void Item::Pickup(void)
{
	SetActive(false);
	m_itemAttributes.pickedUp = true;
}

void Item::SetActive(bool enable)
{
	instance.collider.enable = enable;
	instance.attributes.use = enable;
	instance.castShadow = enable;
}

void Item::OnItemCollisionCallback(const CollisionEvent& event, void* context)
{
	if (event.colliderA->tag == ColliderTag::PLAYER || event.colliderB->tag == ColliderTag::PLAYER)
	{
		Item* self = static_cast<Item*>(context);
		if (self == nullptr || !self->instance.use || !self->instance.load || self->GetIsThrew())
			return;

		self->Pickup();
		if (self->GetItemType() == ItemType::Key)
			AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_get_key);
		else
			AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_get_seed);
	}
	else if (event.colliderA->tag == ColliderTag::ENEMY || event.colliderB->tag == ColliderTag::ENEMY)
	{
		Item* self = static_cast<Item*>(context);
		if (self == nullptr || !self->instance.use || !self->instance.load || !self->GetIsThrew())
			return;

		self->PlayEffect(HitColliderType::Enemy);
		self->SetDestroy(true);
		AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_boom_long);
	}

}

void Item::Update(void)
{
	if (instance.use == true)
	{

		GameObject::Update();

	}

}