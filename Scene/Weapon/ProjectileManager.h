#pragma once
#include "Core/Graphics/ConstantBufferStructs.h"
#include "Scene/Weapon/Projectile.h"
#include "Utility/DoubleLinkedList.h"
#include "Utility/SingletonBase.h"

class ProjectileManager : public SingletonBase<ProjectileManager>, public IDebugUI {

public:
	void Init(void);
	void InitializeObjectsStatus(void);
	void Draw(void);
	void Update(void);

	Projectile* SpawnProjectile(ProjectileType projectileType, Transform trans, XMFLOAT3 dir, float speed);

	int GetTotalProjectileLoadCnt(void) const { return m_totalProjectileLoadCnt; }
	int GetProjectileLoadedCnt(void) const { return m_currentProjectileLoadCnt; }

private:

	HashMap<uint64_t, int, HashUInt64, EqualUInt64> projectileLoadMap =
		HashMap<uint64_t, int, HashUInt64, EqualUInt64>(
			static_cast<int>(ProjectileType::Max),
			HashUInt64(),
			EqualUInt64()
		);

	int m_totalProjectileLoadCnt = 0;
	int m_currentProjectileLoadCnt = 0;

	bool m_drawBoundingBox = true;

	DoubleLinkedList<Projectile*> m_projectileList;

	Renderer& m_renderer = Renderer::get_instance();

	virtual void RenderImGui(void) override;


};
