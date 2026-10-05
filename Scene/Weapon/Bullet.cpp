#include "Bullet.h"

Bullet::Bullet(Transform transform, XMFLOAT3 dir, float speed) :Projectile(ProjectileType::Bullet, transform, dir, speed) {

	GameObjectConfig config;
	config.modelPath = "data/MODEL/Weapon/Bullet/cube.obj";
	config.collisionType = ObjectCollisionType::COLLIDER_BOUNDING_BOX;
	config.colliderTag = ColliderTag::PLAYER_PROJECTILE;
	config.scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
	config.modelType = ModelType::Default;
	config.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	Instantiate(config);

	instance.collider.enable = false;

	Initialize();

}

Bullet::~Bullet()
{

}

void Bullet::Update(void)
{
	Projectile::Update();
}

void Bullet::Draw(void)
{
	Projectile::Draw();
}
