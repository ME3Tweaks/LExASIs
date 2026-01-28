#pragma once
#include "DiscordIntegration/discord_social_sdk/include/discordpp.h"
#include <chrono>

namespace DiscordIntegration
{
	extern bool discordSDKReady;
	extern std::shared_ptr<discordpp::Client> client;
	extern bool hasStatusUpdate;

	extern bool firstEventFired;
	extern bool startedDiscordInit;
	extern std::chrono::steady_clock::time_point firstEventTime;
	extern std::chrono::steady_clock::time_point lastUpdate;

	void setupDiscord();
	bool IsDiscordRunning();
	void updateStatus();
	void ShutdownClient();

	char* getUserSecret();
	void connectWithSecret(bool refresh);
	void setUserSecret(char* secret);
}