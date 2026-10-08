#pragma once

#ifdef LEASI_GAME_LMP
#include "DiscordIntegration/discord_social_sdk/include/discordpp.h"
#include <cstdint>
#include <string>

// LMP presence, built from MilanHost's "LMP.Lobby" SPI service (LmpLobby.h): shows the lobby as a
// Discord party, advertises a join secret while the lobby can be joined, and joins from Discord.
// Everything here runs on the game thread, from the UObject::ProcessEvent hook.
namespace DiscordIntegration::Lmp
{
	// Call often; it polls the lobby at most every 250 ms.
	void Tick();

	// True when the lobby changed since the last FillActivity.
	bool HasUnsentChange();

	// Sets details, state, party, and join secret from the latest lobby state.
	void FillActivity(discordpp::Activity& activity);

	// Discord's join callback: the user accepted an invite or asked to join, and was let in.
	void OnJoinSecret(std::string const& secret);

	// Has Discord start LMP through ME3Tweaks Mod Manager when a join comes in while the game is closed.
	void RegisterLaunchCommand(discordpp::Client& client, uint64_t applicationId);
}
#endif
