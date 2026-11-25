// Must be before including discordpp.h in hpps
#define DISCORDPP_IMPLEMENTATION
#include "DiscordIntegration/discord_social_sdk/include/discordpp.h"


#include "DiscordIntegration/Discord.hpp"
#include "DiscordIntegration/Hooks.hpp"
#include "DiscordIntegration/Entry.hpp"
#include "DiscordIntegration/LmpPresence.hpp"
#include "Common/Base.hpp"
#include "Common/Objects.hpp"
#include "LESDK/Common/Common.hpp"
#include "LESDK/Headers.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <string.h>
#include <wincred.h>
#include <simdutf.h>
#include <charconv>
#include <format>
#include <map>
#include <string_view>


namespace DiscordIntegration
{
	// Discord Application ID. LMP has its own application ID
#ifdef LEASI_GAME_LMP
	const uint64_t APPLICATION_ID = 1438008086085501019;
	const wchar_t* const CREDENTIAL_TARGET = L"MassEffectLMPDiscordIntegration";
#else
	const uint64_t APPLICATION_ID = 1438715716516974656;
	const wchar_t* const CREDENTIAL_TARGET = L"MassEffectLEDiscordIntegration";
#endif

	// ! Extern definitions
	// =======================

	bool discordSDKReady = false;
	std::shared_ptr<discordpp::Client> client;
	bool hasStatusUpdate = false;
	bool firstEventFired = false;
	bool startedDiscordInit = false;
	std::chrono::steady_clock::time_point firstEventTime;
	std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();

	// File-only variables
	// =======================

	bool useLocalClient = false;
	std::string currentState = "Unknown map";

	// Set while connecting with a bare access token saved by an older build
	bool connectingWithLegacyToken = false;

	// Gates client starting up again
	bool clientShutDown = false;

	// Detects if Discord is running by checking the list of active processes
	bool IsDiscordRunning() {
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot == INVALID_HANDLE_VALUE) {
			return false;
		}

		PROCESSENTRY32W pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32W);

		if (!Process32FirstW(hSnapshot, &pe32)) {
			CloseHandle(hSnapshot);
			return false;
		}

		bool found = false;
		do {
			if (_wcsicmp(pe32.szExeFile, L"Discord.exe") == 0) {
				found = true;
				break;
			}
		} while (Process32NextW(hSnapshot, &pe32));

		CloseHandle(hSnapshot);
		return found;
	}


	void setupDiscord() {
		if (clientShutDown || (GIsRequestingExit && *GIsRequestingExit)) {
			return; // Don't set up if we're shutting down
		}

		LEASI_INFO("Initializing Discord SDK...\n");

		client = std::make_shared<discordpp::Client>();
		client->SetApplicationId(APPLICATION_ID);

		// Set up logging callback
		client->AddLogCallback([](auto message, auto severity) {
			LEASI_INFO("[{}] {}", EnumToString(severity), message);
			}, discordpp::LoggingSeverity::Info);

#ifdef LEASI_GAME_LMP
		// Accept joins via Discord
		client->SetActivityJoinCallback([](std::string joinSecret) {
			Lmp::OnJoinSecret(joinSecret);
			});
		Lmp::RegisterLaunchCommand(*client, APPLICATION_ID);
#endif

		useLocalClient = IsDiscordRunning();

		if (!useLocalClient) {
			// Set up status callback to monitor client connection
			client->SetStatusChangedCallback([](discordpp::Client::Status status, discordpp::Client::Error error, int32_t errorDetail) {
				LEASI_INFO("Status changed: {}\n", discordpp::Client::StatusToString(status));

				if (status == discordpp::Client::Status::Ready) {
					LEASI_INFO("Client is now ready\n");
					connectingWithLegacyToken = false;
					discordSDKReady = true;
				}
				else if (error != discordpp::Client::Error::None) {
					LEASI_ERROR(" Connection Error : {} - Details : {}\n", discordpp::Client::ErrorToString(error), errorDetail);

					// The SDK reports a rejected token as UnexpectedClose
					if (connectingWithLegacyToken && status == discordpp::Client::Status::Disconnected
						&& error == discordpp::Client::Error::UnexpectedClose) {
						LEASI_INFO("Saved Discord token was rejected, authorization is required.\n");
						connectingWithLegacyToken = false;
						beginAuthorization();
					}
				}
			});

			// A saved refresh token avoids prompting the user again
			StoredTokens stored;
			if (getStoredTokens(stored)) {
				if (!stored.refreshToken.empty()) {
					LEASI_INFO("Refreshing saved Discord token...\n");
					refreshAndConnect(stored.refreshToken);
				}
				else
				{
					// Try older token...
					LEASI_INFO("Connecting with saved Discord token from an older version...\n");
					connectingWithLegacyToken = true;
					connectWithToken(stored.accessToken);
				}
				return;
			}

			beginAuthorization();
		}
		else {
			discordSDKReady = true;
		}
	}

	/// <summary>
	/// Runs the OAuth2 authorization flow, then saves the tokens and connects
	/// </summary>
	void beginAuthorization() {
		if (clientShutDown || !client) {
			return;
		}

		// Generate OAuth2 code verifier for authentication
		auto codeVerifier = client->CreateAuthorizationCodeVerifier();

		// Set up authentication arguments
		discordpp::AuthorizationArgs args{};
		args.SetClientId(APPLICATION_ID);
		// Discord doesn't let you use just activites.write, which is private,
		// even though its granted via social layer. Seems kind of dumb to have to
		// request more permissions.
		args.SetScopes(discordpp::Client::GetDefaultPresenceScopes());
		args.SetCodeChallenge(codeVerifier.Challenge());

		// Begin authentication process
		client->Authorize(args, [codeVerifier](auto result, auto code, auto redirectUri) {
			if (!result.Successful()) {
				LEASI_ERROR("Authentication Error: {}\n", result.Error());
				return;
			}
			if (clientShutDown || !client) {
				return;
			}

			LEASI_INFO("Authorization successful! Getting access token...\n");

			// Exchange auth code for access token
			client->GetToken(APPLICATION_ID, code, codeVerifier.Verifier(), redirectUri,
				[](discordpp::ClientResult result,
					std::string accessToken,
					std::string refreshToken,
					discordpp::AuthorizationTokenType tokenType,
					int32_t expiresIn,
					std::string scope) {
						LEASI_UNUSED_2(tokenType, scope);
						if (!result.Successful()) {
							LEASI_ERROR("Error getting access token: {}\n", result.Error());
							return;
						}

						LEASI_INFO("Access token received! Establishing connection...\n");
						storeTokens(accessToken, refreshToken, expiresIn);
						connectWithToken(accessToken);
				});
			});
	}

	/// <summary>
	/// Shuts down and exits the discord client.
	/// </summary>
	/// Safe to call more than once
	void ShutdownClient() {
		discordSDKReady = false; // This will prevent any other stuff from happening related to Discord
		startedDiscordInit = true; // Never set the client up again
		if (clientShutDown) {
			return;
		}
		clientShutDown = true;

		if (client) {
			LEASI_INFO("Discord client shutting down.");
			client->Disconnect();
			client->Drop();
			discordpp::RunCallbacks(); // Run loop to disconnect
			client.reset();
		}
	}

	// STATUS COMPONENT ================================================================
#ifdef SDK_TARGET_LE1
	// Cache of area map object name to its proper name for display
	std::map<const wchar_t*, FString> areaMapNameToName;

	using tTLKLookup = FString * (void* globalTalkTable, FString* outStr, int strRef, BOOL bParse);
	tTLKLookup* TLKLookup = nullptr;

	FString* getStringRef(int id, FString* outStr) {
		if (TLKLookup == nullptr) {
			// Capture lookup address on first tlk lookup as it will be initialized then.
			TLKLookup = ::DiscordIntegration::HookManager->ResolveTyped<tTLKLookup>(BUILTIN_TLKLOOKUP_RVA);
		}
		return TLKLookup(GTlkTable, outStr, id, 0);
	}

	using tSaveDataSomethingPre = void* (void* unk1, void* unk2);
	tSaveDataSomethingPre* SaveDataSomethingPre = nullptr;

	void GetMapLabels(FString* outMapName, FString* outParentMapName) {
		if (SaveDataSomethingPre == nullptr) {
			SaveDataSomethingPre = ::DiscordIntegration::HookManager->ResolveTyped<tSaveDataSomethingPre>(LEASI_RVA(0xad2f60));
		}

		SaveDataSomethingPre(outMapName, outParentMapName);
	}

#endif

	/// <summary>
	/// Caches the map name to a std::string so Discord activity can read it
	/// </summary>
	/// <param name="mapString"></param>
	void setMapName(std::wstring mapString) {
		if (mapString.length() > 0) {
			size_t utf8_size = 0;
			// size_t input_size_bytes = mapString.length() * sizeof(wchar_t);
			utf8_size = simdutf::utf8_length_from_utf16le(reinterpret_cast<const char16_t*>(mapString.c_str()), mapString.length());
			if (utf8_size > 0) {
				std::string result(utf8_size, '\0');
				simdutf::convert_utf16le_to_utf8(reinterpret_cast<const char16_t*>(mapString.c_str()), mapString.length(), result.data());
				currentState = result;
			}
		}
	}

#ifdef SDK_TARGET_LE1
	// Looks up map name by the given label and caches it.
	// Returns false if it couldn't be found
	bool lookupMapNameByLabel(const FString& mapLabel) {
		// Not found in cache, look it up via 2DAs
		Common::TypedObjectIterator<UBio2DANumberedRows> Iterator{};

		for (; Iterator; ++Iterator)
		{
			auto bio2da = (UBio2DANumberedRows*)*Iterator;
			auto bio2daName = bio2da->GetName();
			if (bio2daName.StartsWith(L"AreaMap_AreaMap", true)) {
				// its an areamap 2da, enumerate rows
				// strref is column 7
				auto rowCount = bio2da->GetNumRows();
				for (int j = 0; j < rowCount; j++)
				{
					// auto rowIndex = bio2da->GetRowNumber(j);
					if (SFXName label; bio2da->GetNameEntryII(j, 0, &label)) {
						// Now check label...
						auto asciiString = std::string(label.GetName());
						// Create a wide string (e.g., std::wstring) to hold the Unicode representation

						std::wstring unicodeString;
						unicodeString.reserve(asciiString.length()); // Pre-allocate memory for efficiency
						for (char c : asciiString) {
							unicodeString += static_cast<wchar_t>(c);
						}

						if (unicodeString == mapLabel.Chars()) {
							// We have match on label, it is this row
							if (int strRefId; bio2da->GetIntEntryII(j, 7, &strRefId)) {
								FString outStr;
								auto formatted = getStringRef(strRefId, &outStr);
								if (formatted->Length() > 0)
									areaMapNameToName.emplace(mapLabel.Chars(), *formatted); // Cache it
								return true;
							}
							break;
						}
					}
				}
			}
		}

		// Was not found
		return false;
	}
#endif

	void updateStatus()
	{
#ifndef LEASI_GAME_LMP
		if (GWorld && *GWorld) {
			auto package = (*GWorld)->GetPackageName();
			auto packageName = package.GetName();

			// Are we in entry menu?
			if (_strcmpi(packageName, "entrymenu") == 0) {
				currentState = "In main menu";
#ifdef SDK_TARGET_LE1
				// Check if character creator?
#endif
			}
			else {
				currentState = "Unknown map";
#ifdef SDK_TARGET_LE1
				// UEngine* engine = reinterpret_cast<UEngine*>(UEngine::StaticClass()->ClassDefaultObject);
				// ABioWorldInfo* worldInfo = reinterpret_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
				//auto game = worldInfo->CurrentGame;
				FString mapLabel{};
				FString parentMapLabel{};
				GetMapLabels(&mapLabel, &parentMapLabel);

				if (mapLabel.Length() > 0) {

					// Check cache first
					auto it = areaMapNameToName.find(mapLabel.Chars());
					if (it != areaMapNameToName.end()) {
						// Found in cache, set it
						setMapName(it->second.Chars());
					}
					else {
						bool hasLabel = lookupMapNameByLabel(mapLabel);
						bool hasParentLabel = parentMapLabel.Length() > 0 ? lookupMapNameByLabel(parentMapLabel) : false;

						if (!hasLabel) {
							// Couldn't be found
						}
						else if (hasParentLabel) {
							// Mixed
							std::wstring combo = areaMapNameToName.find(parentMapLabel.Chars())->second.Chars();
							combo += L": ";
							combo += areaMapNameToName.find(mapLabel.Chars())->second.Chars();
							setMapName(combo);
						}
						else {
							// Just the label
							setMapName(areaMapNameToName.find(mapLabel.Chars())->second.Chars());
						}

					}
				}
#endif
#if defined(SDK_TARGET_LE2) || defined(SDK_TARGET_LE3)
				auto save = USFXSFHandler_Save::StaticClass();
				if (save) {
					auto saveDefaults = (USFXSFHandler_Save*)save->ClassDefaultObject;
					if (saveDefaults) {
						// Find name in the defaults
						UINT i = 0;
						for (i = 0; i < saveDefaults->AreaData.Count(); i++) {
							auto default = saveDefaults->AreaData.GetData()[i];
							auto areaName = default.AreaName.GetName();
							if (_stricmp(areaName, packageName) == 0) {
								auto strRefId = default.AreaStrRef;
								auto sfxgame = (ASFXGame*)::Common::FindFirstObject<ASFXGame>();
								auto formatted = sfxgame->GetSimpleString(strRefId, 0);
								setMapName(formatted.Chars());
								break;
							}
						}
					}
				}
#endif
			}
		}
#endif


		// Send activity update to Discord
		discordpp::Activity activity;
		discordpp::ActivityAssets assets;
#ifdef LEASI_GAME_LMP
		// The name defaults to the LMP application's
		assets.SetLargeImage("lmp");
		activity.SetName("Legendary Edition Multiplayer");
#elif defined SDK_TARGET_LE1
		assets.SetLargeImage("le1");
		activity.SetName("Mass Effect");
#elif defined SDK_TARGET_LE2
		assets.SetLargeImage("le2");
		activity.SetName("Mass Effect 2");
#elif defined SDK_TARGET_LE3
		assets.SetLargeImage("le3");
		activity.SetName("Mass Effect 3");
#endif
		activity.SetAssets(assets);
		activity.SetType(discordpp::ActivityTypes::Playing);

#ifdef LEASI_GAME_LMP
		Lmp::FillActivity(activity);
#else
		activity.SetDetails(currentState);
#endif

		// Update rich presence
		client->UpdateRichPresence(activity, [](discordpp::ClientResult result) {
			if (result.Successful()) {
				LEASI_INFO("Rich Presence updated successfully!\n");
			}
			else {
				LEASI_ERROR("Rich Presence update failed");
			}
		});
	}


#pragma region Credential Storage
	// Credential blob layout: "<header>\n<accessToken>\n<refreshToken>\n<expiresAt>". Older builds
	// stored a bare null-terminated access token, which is returned with no refresh token
	constexpr std::string_view CREDENTIAL_BLOB_HEADER = "LEASI-DISCORD-2";

	/// <summary>
	/// Reads the saved OAuth tokens from Credential Manager.
	/// </summary>
	/// <returns>
	/// True if a token was found. refreshToken is empty for a token saved by an older build
	/// </returns>
	bool getStoredTokens(StoredTokens& out) {
		PCREDENTIALW pcred = nullptr;
		if (!::CredReadW(CREDENTIAL_TARGET, CRED_TYPE_GENERIC, 0, &pcred)) {
			DWORD error = ::GetLastError();
			if (error != ERROR_NOT_FOUND) {
				LEASI_ERROR("CredReadW() failed - error {}", error);
			}
			return false;
		}

		std::string blob;
		if (pcred->CredentialBlob != nullptr && pcred->CredentialBlobSize <= CRED_MAX_CREDENTIAL_BLOB_SIZE) {
			blob.assign(reinterpret_cast<const char*>(pcred->CredentialBlob), pcred->CredentialBlobSize);
		}
		// Memory allocated by CredRead() must be freed!
		::CredFree(pcred);

		out = {};

		// Older builds - bare access token, written with null terminator
		if (blob.find('\n') == std::string::npos) {
			const size_t end = blob.find('\0');
			out.accessToken = blob.substr(0, end);
			return !out.accessToken.empty();
		}

		// Split into exactly four newline-separated fields
		std::string_view fields[4];
		std::string_view rest = blob;
		for (size_t i = 0; i < 4; i++) {
			const size_t end = i < 3 ? rest.find('\n') : rest.size();
			if (end == std::string_view::npos) {
				LEASI_ERROR("Saved Discord credential is malformed, authorization is required.");
				return false;
			}
			fields[i] = rest.substr(0, end);
			rest.remove_prefix(i < 3 ? end + 1 : end);
		}

		if (fields[0] != CREDENTIAL_BLOB_HEADER || fields[2].empty()) {
			LEASI_ERROR("Saved Discord credential is malformed, authorization is required.");
			return false;
		}

		out.accessToken = fields[1];
		out.refreshToken = fields[2];
		std::from_chars(fields[3].data(), fields[3].data() + fields[3].size(), out.expiresAt);
		return true;
	}

	/// <summary>
	/// Saves the OAuth tokens in the Windows Credential Manager under CREDENTIAL_TARGET
	/// </summary>
	/// <param name="expiresIn">
	/// Seconds until the access token expires, as returned by the token exchange.
	/// </param>
	void storeTokens(const std::string& accessToken, const std::string& refreshToken, int32_t expiresIn) {
		const int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		std::string blob = std::format("{}\n{}\n{}\n{}", CREDENTIAL_BLOB_HEADER, accessToken, refreshToken, now + expiresIn);
		if (blob.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
			LEASI_ERROR("Discord tokens are too large to save ({} bytes)", blob.size());
			return;
		}

		CREDENTIALW cred = { 0 };
		cred.Type = CRED_TYPE_GENERIC;
		cred.TargetName = const_cast<LPWSTR>(CREDENTIAL_TARGET);
		cred.CredentialBlobSize = static_cast<DWORD>(blob.size());
		cred.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
		cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

		if (!::CredWriteW(&cred, 0)) {
			LEASI_ERROR("CredWriteW() failed - error {}", ::GetLastError());
		}
	}

	/// <summary>
	/// Exchanges the refresh token for a new token pair, saves it, and connects with the new access token
	/// </summary>
	void refreshAndConnect(const std::string& refreshToken) {
		if (clientShutDown || !client) {
			return;
		}
		client->RefreshToken(APPLICATION_ID, refreshToken,
			[](discordpp::ClientResult result, std::string accessToken,
				std::string newRefreshToken,
				discordpp::AuthorizationTokenType tokenType, int32_t expiresIn,
				std::string scope) {
					LEASI_UNUSED_2(tokenType, scope);
					if (!result.Successful()) {
						LEASI_ERROR("Error refreshing Discord token: {}", result.Error());
						// A rejected refresh token (revoked, or already used) needs a new authorization.
						// Network failures don't, so don't prompt the user for those
						if (result.Type() == discordpp::ErrorType::HTTPError && !result.Retryable()) {
							beginAuthorization();
						}
						return;
					}

					storeTokens(accessToken, newRefreshToken, expiresIn);
					connectWithToken(accessToken);
			}
		);
	}

	/// <summary>
	/// Hands the access token to the SDK and connects once it has been applied
	/// </summary>
	void connectWithToken(const std::string& accessToken) {
		if (clientShutDown || !client) {
			return;
		}
		client->UpdateToken(discordpp::AuthorizationTokenType::Bearer, accessToken, [](discordpp::ClientResult result) {
			if (!result.Successful()) {
				LEASI_ERROR("Error setting Discord token: {}", result.Error());
				return;
			}
			if (clientShutDown || !client) {
				return;
			}
			LEASI_INFO("Connecting to Discord...");
			client->Connect();
		});
	}
#pragma endregion
}