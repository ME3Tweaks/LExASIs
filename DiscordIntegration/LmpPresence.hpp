#pragma once

#ifdef LEASI_GAME_LMP
#include "DiscordIntegration/discord_social_sdk/include/discordpp.h"
#include <cstdint>
#include <string>

// Discord presence and joins from MilanHost's LMP.Lobby service. Game thread only.
namespace DiscordIntegration::Lmp
{
	// Polls the lobby at most every 250 ms
	void Tick();

	bool HasUnsentChange();

	void FillActivity(discordpp::Activity& activity);

	// Discord join callback
	void OnJoinSecret(std::string const& secret);

	// Lets Discord start LMP via Mod Manager for a join
	void RegisterLaunchCommand(discordpp::Client& client, uint64_t applicationId);
}
#endif
