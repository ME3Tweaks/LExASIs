#pragma once

#include <fstream>
#include <iostream>
#include <cstdio>
#include <csignal>
#include <windows.h>
#include <wincred.h>
#include <wchar.h>
#pragma comment(lib, "advapi32.lib")  // Or pass it to the cl command line.

#include "../../Shared-ASI/Common.h"
#include "../../Shared-ASI/Interface.h"
#include "../../Shared-ASI/ME3Tweaks/ME3TweaksHeader.h"

#undef min
#undef max
#include "simdutf.cpp"
#include "simdutf.h"

#define DISCORDPP_IMPLEMENTATION
#include "discordpp.h"
#include <signal.h>
#include <map>

#define MYHOOK "DiscordIntegration_"
#if defined GAMELE1
#define SPI_GAME SPI_GAME_LE1
#define GAMETAG "LE1"
#elif defined GAMELE2
#define SPI_GAME SPI_GAME_LE2
#define GAMETAG "LE2"
#elif defined GAMELE3
#define SPI_GAME SPI_GAME_LE3
#define GAMETAG "LE3"
#endif

#define ASINAME L"DiscordIntegration"
#define ASIVERSION "1"
#define ASIDEV L"Mgamerz"

SPI_PLUGINSIDE_SUPPORT(ASINAME, ASIVERSION L".0.0", ASIDEV, SPI_GAME, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;

std::string currentState = "Unknown map";
bool hasStatusUpdate = false;
std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();

// GAME VARIABLES ====================================================================
static UWorld** GWorldPtr;

static UWorld* GWorld() {
    if (GWorldPtr) return *GWorldPtr;
    constexpr auto byte_pattern =
#ifdef GAMELE1 
        "48 8b 0d 6b 2b 47 01 e8 2e 63 30 00 0f 28 f8 48 8b 0d 5c 2b 47 01";
#elif defined(GAMELE2)
        "48 8b 0d e6 a7 64 01 e8 a1 51 7b 00";
#elif defined(GAMELE3)
        "48 8b 0d ee 33 78 01 e8 21 f3 78 00";
#endif
    GWorldPtr = static_cast<UWorld**>(findAddressLeaMov("GWorld", byte_pattern));
    return *GWorldPtr;
}

static bool* GIsRequestingExit;

#ifdef GAMELE1
// TLK table - LE1 only as it's simple lookup doesn't work for what we need
static void** GTlkTablePtr;
static void* GTlkTable() {
    if (GTlkTablePtr) return *GTlkTablePtr;
    constexpr auto byte_pattern = "48 8b 0d e7 a9 a2 00 e8 d2 74 42 ff 48 8b f8";
    GTlkTablePtr = static_cast<void**>(findAddressLeaMov("GTlkTable", byte_pattern));
    return *GTlkTablePtr;
}
#endif

// DISCORD COMPONENT ==============================================================
const uint64_t APPLICATION_ID = 1438715716516974656;
bool discordSDKReady = false;
std::shared_ptr<discordpp::Client>client;
// If we should update via RPC or via server
bool useLocalClient = false;

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
    DWORD cbCreds = 1 + strlen(secret);

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
                    if (!result.Successful()) {
                        std::cout << "❌ Error refreshing token: " << result.Error()
                            << std::endl;
                        return;
                    }

                    setUserSecret(secret);
                    connectWithSecret(false); // Re-enter method but don't refresh.
            });
    }

    client->UpdateToken(discordpp::AuthorizationTokenType::Bearer, secret, [](discordpp::ClientResult result) {
        if (result.Successful()) {
            std::cout << "Connecting to Discord...\n";
            client->Connect();
        }
        });
}


bool IsDiscordRunning() {
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;

    // Take a snapshot of all processes in the system.
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        std::cerr << "CreateToolhelp32Snapshot failed." << std::endl;
        return false;
    }

    // Set the size of the structure before using it.
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Retrieve information about the first process, and exit if unsuccessful.
    if (!Process32First(hProcessSnap, &pe32)) {
        std::cerr << "Process32First failed." << std::endl;
        CloseHandle(hProcessSnap);
        return false;
    }

    // Now walk the snapshot of processes, and display information about each.
    do {
        // Convert the process name to a std::string for easier comparison.
        pe32.szExeFile;
        if (_wcsicmp(pe32.szExeFile, L"Discord.exe") == 0) {
            CloseHandle(hProcessSnap);
            return true; // Discord is running
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return false; // Discord is not running
}

void setupDiscord() {
    if (GIsRequestingExit && *GIsRequestingExit) {
        return; // Don't set up if we're shutting down
    }

    std::cout << "Initializing Discord SDK...\n";

    client = std::make_shared<discordpp::Client>();
    client->SetApplicationId(APPLICATION_ID);

    // Set up logging callback
    client->AddLogCallback([](auto message, auto severity) {
        std::cout << "[" << EnumToString(severity) << "] " << message << std::endl;
        }, discordpp::LoggingSeverity::Info);

    useLocalClient = IsDiscordRunning();

    if (!useLocalClient) {
        // Set up status callback to monitor client connection
        client->SetStatusChangedCallback([](discordpp::Client::Status status, discordpp::Client::Error error, int32_t errorDetail) {
            std::cout << "Status changed: " << discordpp::Client::StatusToString(status) << std::endl;

            if (status == discordpp::Client::Status::Ready) {
                std::cout << "Client is ready! You can now call SDK functions.\n";
                discordSDKReady = true;
            }
            else if (error != discordpp::Client::Error::None) {
                std::cerr << " Connection Error : " << discordpp::Client::ErrorToString(error) << " - Details : " << errorDetail << std::endl;
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
                std::cerr << "Authentication Error: " << result.Error() << std::endl;
                return;
            }
            else {
                std::cout << "Authorization successful! Getting access token...\n";

                // Exchange auth code for access token
                client->GetToken(APPLICATION_ID, code, codeVerifier.Verifier(), redirectUri,
                    [](discordpp::ClientResult result,
                        std::string accessToken,
                        std::string refreshToken,
                        discordpp::AuthorizationTokenType tokenType,
                        int32_t expiresIn,
                        std::string scope) {
                            std::cout << "Access token received! Establishing connection...\n";
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
    std::cout << "Discord client shutting down.\n";
    if (client) {
        client->Disconnect();
        client->Drop();
        discordpp::RunCallbacks(); // Run loop to disconnect
    }

    discordSDKReady = false; // This will prevent any other stuff from happening related to Discord
}

// STATUS COMPONENT ================================================================
#ifdef GAMELE1
// Cache of area map object name to its proper name for display
std::map<wchar_t*, FString> areaMapNameToName;

typedef FString* (*tTLKLookup)(void* globalTalkTable, FString* outStr, int strRef, BOOL bParse);
tTLKLookup TLKLookup = nullptr;

FString* getStringRef(int id, FString* outStr) {
    if (TLKLookup == nullptr) {
        TLKLookup = (tTLKLookup) SDKInitializer::Instance()->GetAbsoluteAddress(0xb24470);
    }
    return TLKLookup(GTlkTable(), outStr, id, 0);
}

typedef void* (*tSaveDataSomethingPre)(void* unk1, void* unk2);
tSaveDataSomethingPre SaveDataSomethingPre = nullptr;

void GetMapLabels(UBioSaveGame* saveGame, FString* outMapName, FString* outParentMapName) {
    if (SaveDataSomethingPre == nullptr) {
        SaveDataSomethingPre = (tSaveDataSomethingPre)SDKInitializer::Instance()->GetAbsoluteAddress(0xad2f60);
    }

    SaveDataSomethingPre(outMapName, outParentMapName);
}

#endif

/// <summary>
/// Caches the map name to a std::string so Discord activity can read it
/// </summary>
/// <param name="mapString"></param>
void setMapName(FString mapString) {
    if (mapString.Count > 0) {
        size_t utf8_size = 0;
        size_t input_size_bytes = mapString.Count * sizeof(wchar_t);
        utf8_size = simdutf::utf8_length_from_utf16le(reinterpret_cast<const char16_t*>(mapString.Data), mapString.Count);
        if (utf8_size > 0) {
            std::string result(utf8_size, '\0');
            simdutf::convert_utf16le_to_utf8(reinterpret_cast<const char16_t*>(mapString.Data), mapString.Count, result.data());
            currentState = result;
        }
    }
}

#ifdef GAMELE1
// Looks up map name by the given label and caches it.
// Returns false if it couldn't be found
bool lookupMapNameByLabel(const FString& mapLabel) {
    // Not found in cache, look it up via 2DAs
    auto twoDAs = FindObjectsOfType(UBio2DANumberedRows::StaticClass());
    auto buffer = new char[512];

    for (int i = 0; i < twoDAs.Count; i++)
    {
        auto bio2da = (UBio2DANumberedRows*)twoDAs.Data[i];
        auto bio2daName = bio2da->GetName();
        if (strncmp(bio2daName, "AreaMap_AreaMap", 15) == 0) {
            // its an areamap 2da, enumerate rows
            // strref is column 7
            auto rowCount = bio2da->GetNumRows();
            for (int j = 0; j < rowCount; j++)
            {
                auto rowIndex = bio2da->GetRowNumber(j);
                if (FName label; bio2da->GetNameEntryII(j, 0, &label)) {
                    // Now check label...
                    auto asciiString = std::string(label.GetName());
                    // Create a wide string (e.g., std::wstring) to hold the Unicode representation

                    std::wstring unicodeString;
                    unicodeString.reserve(asciiString.length()); // Pre-allocate memory for efficiency
                    for (char c : asciiString) {
                        unicodeString += static_cast<wchar_t>(c);
                    }

                    if (wcscmp(unicodeString.c_str(), mapLabel.Data) == 0) {
                        // We have match on label, it is this row
                        if (int strRefId; bio2da->GetIntEntryII(j, 7, &strRefId)) {
                            FString outStr;
                            auto formatted = getStringRef(strRefId, &outStr);
                            if (formatted->Data != nullptr) {
                                areaMapNameToName.emplace(mapLabel.Data, *formatted); // Cache it
                                return true;
                            }
                            break;
                        }
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
    auto world = GWorld();
    if (world) {
        auto package = world->GetPackageName();
        auto packageName = package.GetName();

        // Are we in entry menu?
        if (_strcmpi(packageName, "entrymenu") == 0) {
            currentState = "In main menu";
#ifdef GAMELE1
            // Check if character creator?
#endif
        } else {
            currentState = "Unknown map";
#ifdef GAMELE1
            UEngine* engine = reinterpret_cast<UEngine*>(UEngine::StaticClass()->ClassDefaultObject);
            ABioWorldInfo* worldInfo = reinterpret_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
            auto game = worldInfo->CurrentGame;
            FString mapLabel{};
            FString parentMapLabel{};
            GetMapLabels(game, &mapLabel, &parentMapLabel);
                
            if (mapLabel.Count > 0) {

                // Check cache first
                auto it = areaMapNameToName.find(mapLabel.Data);
                if (it != areaMapNameToName.end()) {
                    // Found in cache, set it
                    setMapName(it->second);
                } else {
                    bool hasLabel = lookupMapNameByLabel(mapLabel);
                    bool hasParentLabel = parentMapLabel.Count > 0 ? lookupMapNameByLabel(parentMapLabel) : false;

                    if (!hasLabel) {
                        // Couldn't be found
                    }
                    else if (hasParentLabel) {
                        // Mixed
						std::wstring combo = areaMapNameToName.find(parentMapLabel.Data)->second.Data;
                        combo += L": ";
                        combo += areaMapNameToName.find(mapLabel.Data)->second.Data;
						setMapName(combo);
                    }
                    else {
                        // Just the label
                        setMapName(areaMapNameToName.find(mapLabel.Data)->second);
                    }

				}
            }
#endif
#if defined(GAMELE2) || defined(GAMELE3)
            auto save = USFXSFHandler_Save::StaticClass();
            if (save) {
                auto saveDefaults = (USFXSFHandler_Save*)save->ClassDefaultObject;
                if (saveDefaults) {
                    // Find name in the defaults
                    int i = 0;
                    for (i = 0; i < saveDefaults->AreaData.Count; i++) {
                        auto default = saveDefaults->AreaData.Data[i];
                        auto areaName = default.AreaName.GetName();
                        if (_stricmp(areaName, packageName) == 0) {
                            auto strRefId = default.AreaStrRef;
                            auto sfxgame = (ASFXGame*)FindObjectOfType(ASFXGame::StaticClass());
                            auto formatted = sfxgame->GetSimpleString(strRefId, 0);
                            setMapName(formatted);
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
#ifdef GAMELE1
    assets.SetLargeImage("le1");
    activity.SetName("Mass Effect");
#elif defined GAMELE2
    assets.SetLargeImage("le2");
    activity.SetName("Mass Effect 2");
#elif defined GAMELE3
    assets.SetLargeImage("le3");
    activity.SetName("Mass Effect 3");
#endif
    activity.SetAssets(assets);
    activity.SetType(discordpp::ActivityTypes::Playing);

    activity.SetDetails(currentState);

    // Update rich presence
    client->UpdateRichPresence(activity, [](discordpp::ClientResult result) {
        if (result.Successful()) {
            std::cout << "Rich Presence updated successfully!\n";
        }
        else {
            std::cerr << "Rich Presence update failed";
        }
    });
}



// GAME COMPONENT ================================================================

bool firstEventFired = false;
bool startedDiscordInit = false;
typedef void (*tProcessEvent)(UObject* Context, UFunction* Function, void* Parms, void* Result);
tProcessEvent ProcessEvent = nullptr;
tProcessEvent ProcessEvent_orig = nullptr;
std::chrono::steady_clock::time_point firstEventTime;
void ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result)
{
    // std::cout << string_format("[U] %s->%s()\n", Context->GetFullPath(), Function->GetFullName(false)).c_str();

    // Setup Discord on first viewport draw
    if (!firstEventFired) {
        firstEventFired = true;
        firstEventTime = std::chrono::steady_clock::now();
    }

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds> (now - lastUpdate).count();

    if (!startedDiscordInit) {
        auto startDelta = std::chrono::duration_cast<std::chrono::milliseconds> (now - firstEventTime).count();
        if (startDelta > 10000) {
            startedDiscordInit = true;
            setupDiscord();
        }
    }

    // Only do stuff if Discord SDK is ready

    if (discordSDKReady) {
        if (GIsRequestingExit && *GIsRequestingExit) {
            ShutdownClient();
        }
        else {
            if (delta >= 8000) {
                lastUpdate = now;
                updateStatus();
            }
        }
    }

    // We must run callbacks, or SDK won't be able to mark
    // itself as ready
    if (delta >= 500) {
        discordpp::RunCallbacks();
    }

    // Run the original event
    ProcessEvent_orig(Context, Function, Parms, Result);
}

#if defined(GAMELE3) || defined(GAMELE1)
typedef void (*t_BioRequestExit)(UBOOL bForce, INT ExitCode, INT Unused);
t_BioRequestExit BioRequestExit = nullptr;
t_BioRequestExit BioRequestExit_orig = nullptr;
void BioRequestExit_hook(UBOOL bForce, INT ExitCode, INT Unused) 
{
    ShutdownClient();
    BioRequestExit_orig(bForce, ExitCode, Unused);
}
#endif

SPI_IMPLEMENT_ATTACH
{
#ifdef ASI_DEBUG
	Common::OpenConsole();
#endif

    INIT_CHECK_SDK();

	// Hook ProcessEvent as we will be accessing things on game tick
	INIT_FIND_PATTERN_POSTHOOK(ProcessEvent, LE_PATTERN_POSTHOOK_PROCESSEVENT);
	INIT_HOOK_PATTERN(ProcessEvent);

    // Hook application exit
    const auto RequestExitOffset =
#if defined(GAMELE1)
        0x16b66f0;
#elif defined(GAMELE2)
        0x168a47c;
#elif defined(GAMELE3)
        0x17d5698;
#endif

    GIsRequestingExit = (bool*)SDKInitializer::Instance()->GetAbsoluteAddress(RequestExitOffset);

    // Setup GMalloc
    UnrealMalloc::GMalloc.CreateFree = CreateFree;
    UnrealMalloc::GMalloc.CreateMalloc = CreateMalloc;
    UnrealMalloc::GMalloc.CreateRealloc = CreateRealloc;

	return true;
}

SPI_IMPLEMENT_DETACH
{
#ifdef ASI_DEBUG
	Common::CloseConsole();
#endif	
    return true;
}
