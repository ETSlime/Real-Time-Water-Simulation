#pragma once

#include "Projectile.h"

class Bullet : public Projectile {

public: 
	Bullet(Transform transform, XMFLOAT3 dir, float speed);

	~Bullet();

	void Update(void) override;

	void Draw(void) override;

private:
	FBXLoader& m_fbxLoader = FBXLoader::get_instance();

};