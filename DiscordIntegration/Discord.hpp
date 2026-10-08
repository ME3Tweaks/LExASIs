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

	// OAuth tokens persisted in Credential Manager under CREDENTIAL_TARGET.
	struct StoredTokens {
		std::string accessToken;
		std::string refreshToken;
		int64_t expiresAt = 0; // Unix seconds
	};

	bool getStoredTokens(StoredTokens& out);
	void storeTokens(const std::string& accessToken, const std::string& refreshToken, int32_t expiresIn);
	void beginAuthorization();
	void refreshAndConnect(const std::string& refreshToken);
	void connectWithToken(const std::string& accessToken);
}