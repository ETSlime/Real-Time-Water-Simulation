#include "Gate.h"
#include "Scene/Player.h"
#include "Core/GameSystem.h"

void Gate::Interact()
{
	Player* player = GameSystem::get_instance().GetPlayer();

	if (player->GetPlayerAttributes().keyCount >= 3) {
	
		GameSystem::get_instance().ChangeMode(GameMode::RESULT, true);

	}
}

char* Gate::GetPromptTextureLocation()
{
	Player* player = GameSystem::get_instance().GetPlayer();

	int keyCount = player->GetPlayerAttributes().keyCount;
	
	switch (keyCount) {
		
	case 0:
		return "data/TEXTURE/UI/interactUI-key0_3.png";
		break;
	case 1:
		return "data/TEXTURE/UI/interactUI-key1_3.png";
		break;
	case 2:
		return "data/TEXTURE/UI/interactUI-key2_3.png";
		break;
	default:
		return Interactable::GetPromptTextureLocation();
		break;

	}
	
}
