#pragma once
//=============================================================================
//
// Townèàóù [Town.h]
// Author : 
//
//=============================================================================
#include "Scene/GameObject.h"
#include "Core/Camera.h"

class Town
{
public:
	Town();

	void Update();
	void Draw();

	SimpleArray<GameObject<SkinnedMeshModelInstance>*> models;

	Renderer& m_Renderer = Renderer::get_instance();
	Camera& m_Camera = Camera::get_instance();
};