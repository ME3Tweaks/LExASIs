// Must be before including discordpp.h in hpps
#define DISCORDPP_IMPLEMENTATION
#include "DiscordIntegration/discord_social_sdk/include/discordpp.h"


#include "DiscordIntegration/Discord.hpp"
#include "DiscordIntegration/Hooks.hpp"
#include "DiscordIntegration/Entry.hpp"
#include "Common/Base.hpp"
#include "Common/Objects.hpp"
#include "LESDK/Common/Common.hpp"
#include "LESDK/Headers.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <string.h>
#include <wincred.h>
#include <simdutf.h>
#include <map>


namespace DiscordIntegration
{
	// Discord Application ID
	const uint64_t APPLICATION_ID = 1438715716516974656;

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

	// Detects if Discord is running by checking the list of active processes.
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
		if (GIsRequestingExit && *GIsRequestingExit) {
			return; // Don't set up if we're shutting down
		}

		LEASI_INFO("Initializing Discord SDK...\n");

		client = std::make_shared<discordpp::Client>();
		client->SetApplicationId(APPLICATION_ID);

		// Set up logging callback
		client->AddLogCallback([](auto message, auto severity) {
			LEASI_INFO("[{}] {}", EnumToString(severity), message);
			}, discordpp::LoggingSeverity::Info);

		useLocalClient = IsDiscordRunning();

		if (!useLocalClient) {
			// Set up status callback to monitor client connection
			client->SetStatusChangedCallback([](discordpp::Client::Status status, discordpp::Client::Error error, int32_t errorDetail) {
				LEASI_INFO("Status changed: {}\n", discordpp::Client::StatusToString(status));

				if (status == discordpp::Client::Status::Ready) {
					LEASI_INFO("Client is ready! You can now call SDK functions.\n");
					discordSDKReady = true;
				}
				else if (error != discordpp::Client::Error::None) {
					LEASI_ERROR(" Connection Error : {} - Details : {}\n", discordpp::Client::ErrorToString(error), errorDetail);
				}
				});

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


			auto existingToken = getUserSecret();
			if (existingToken != nullptr) {
				connectWithSecret(true);
				return;
			}

			// Begin authentication process
			client->Authorize(args, [codeVerifier](auto result, auto code, auto redirectUri) {
				if (!result.Successful()) {
					LEASI_ERROR("Authentication Error: {}\n", result.Error());
					return;
				}
				else {
					LEASI_INFO("Authorization successful! Getting access token...\n");

					// Exchange auth code for access token
					client->GetToken(APPLICATION_ID, code, codeVerifier.Verifier(), redirectUri,
						[](discordpp::ClientResult result,
							std::string accessToken,
							std::string refreshToken,
							discordpp::AuthorizationTokenType tokenType,
							int32_t expiresIn,
							std::string scope) {
								LEASI_UNUSED_5(result, refreshToken, tokenType, expiresIn, scope);
								LEASI_INFO("Access token received! Establishing connection...\n");
								setUserSecret((char*)accessToken.c_str());
								connectWithSecret(false);
						});
				}
				});
		}
		else {
			discordSDKReady = true;
		}
	}

	/// <summary>
	/// Shuts down and exits the discord client.
	/// </summary>
	void ShutdownClient() {
		LEASI_INFO("Discord client shutting down.");
		if (client) {
			client->Disconnect();
			client->Drop();
			discordpp::RunCallbacks(); // Run loop to disconnect
		}

		discordSDKReady = false; // This will prevent any other stuff from happening related to Discord
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


		// Send activity update to Discord
		discordpp::Activity activity;
		discordpp::ActivityAssets assets;
#ifdef SDK_TARGET_LE1
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

		activity.SetDetails(currentState);

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
	/// <summary>
	/// Fetches the user token from Credential Manager
	/// </summary>
	/// <returns></returns>
	char* getUserSecret() {
		static char credential[256];

		PCREDENTIALW pcred;
		BOOL ok = ::CredReadW(L"MassEffectLEDiscordIntegration", CRED_TYPE_GENERIC, 0, &pcred);
		wprintf(L"CredRead() - errno %d\n", ok ? 0 : ::GetLastError());
		if (!ok) {
			return nullptr;
		}
		wprintf(L"Read secret='%S' (%d bytes)\n", (char*)pcred->CredentialBlob, pcred->CredentialBlobSize);
		memcpy(credential, pcred->CredentialBlob, pcred->CredentialBlobSize);
		// Memory allocated by CredRead() must be freed!
		::CredFree(pcred);
		return credential;
	}

	/// <summary>
	/// Stores a null-terminated secret string in the Windows Credential Manager under the fixed target name "MassEffectLEDiscordIntegration".
	/// </summary>
	/// <param name="secret">The user secret</param>
	void setUserSecret(char* secret) {
		DWORD cbCreds = 1 + (DWORD) strlen(secret);

		CREDENTIALW cred = { 0 };
		cred.Type = CRED_TYPE_GENERIC;
		cred.TargetName = L"MassEffectLEDiscordIntegration";
		cred.CredentialBlobSize = cbCreds;
		cred.CredentialBlob = (LPBYTE)secret;
		cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

		BOOL ok = ::CredWriteW(&cred, 0);
		wprintf(L"CredWrite() - errno %d\n", ok ? 0 : ::GetLastError());
	}

	void connectWithSecret(bool refresh) {
		char* secret = getUserSecret();
		if (refresh) {
			client->RefreshToken(
				APPLICATION_ID, secret,
				[secret](discordpp::ClientResult result, std::string accessToken,
					std::string refreshToken,
					discordpp::AuthorizationTokenType tokenType, int32_t expiresIn,
					std::string scope) {
						LEASI_UNUSED_5(accessToken, refreshToken, tokenType, expiresIn, scope);
						if (!result.Successful()) {
							LEASI_ERROR("❌ Error refreshing token: {}", result.Error());
							return;
						}

						setUserSecret(secret);
						connectWithSecret(false); // Re-enter method but don't refresh.
				}
			);
		}

		client->UpdateToken(discordpp::AuthorizationTokenType::Bearer, secret, [](discordpp::ClientResult result) {
			if (result.Successful()) {
				LEASI_INFO("Connecting to Discord...");
				client->Connect();
			}
			});
	}
#pragma endregion
}