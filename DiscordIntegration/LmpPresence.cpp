#ifdef LEASI_GAME_LMP

#include "DiscordIntegration/LmpPresence.hpp"
#include "DiscordIntegration/LmpLobby.h"
#include "DiscordIntegration/Entry.hpp"
#include "Common/Base.hpp"

#include <chrono>
#include <cstring>
#include <utility>
#include <windows.h>


namespace DiscordIntegration::Lmp
{
	namespace
	{
		// Join secrets are "lmp1;<invite code>;<matchmaker url>". Matchmaker codes never contain ';'
		// (a lobby whose code does gets no secret), so the URL is everything after the second one.
		constexpr char k_secretPrefix[] = "lmp1;";
		constexpr std::size_t k_discordFieldMax = 128;  // Party IDs and secrets: 2-128 characters.

		// Discord runs this to start LMP for a join; Mod Manager boots the game and the join
		// callback then delivers the secret.
		constexpr wchar_t k_launchLink[] = L"me3tweaksmodmanager://lmplaunch";
		constexpr wchar_t k_launchHandlerKey[] = L"me3tweaksmodmanager\\shell\\open\\command";

		constexpr auto k_pollInterval = std::chrono::milliseconds(250);
		constexpr auto k_lookupInterval = std::chrono::seconds(1);

		LmpLobbyService const* g_service = nullptr;
		std::chrono::steady_clock::time_point g_nextPoll{};
		std::chrono::steady_clock::time_point g_nextLookup{};
		bool g_lookupFailureLogged = false;
		bool g_longSecretLogged = false;

		std::uint64_t g_seenGeneration = 0;
		LmpLobbyState g_state{};
		bool g_unsent = false;
		std::string g_lastJoinReport{};   // What was last logged about offering a Discord join.

		// A join that arrived before MilanHost's service was found.
		std::string g_pendingSecret{};

		// Set when LMP.Lobby turned out to come from an ASI other than MilanHost; the lookup then stops.
		bool g_providerRejected = false;

		// Only MilanHost owns the lobby. The proxy hands the name to whichever ASI registers it first,
		// so another ASI could take it before MilanHost attaches.
		constexpr wchar_t k_lobbyProviderName[] = L"MilanHost";

		std::string ToUtf8(std::wstring const& text);

		// The SPI name the ASI containing the table declares, and that ASI's file name, for the log.
		// Empty name when the table is not inside an SPI ASI.
		void DescribeProvider(const void* table, std::wstring& outName, std::wstring& outFile)
		{
			using SpiSupportDeclFn = void(wchar_t** name, wchar_t** author, wchar_t** version, int* gameFlags, int* minVersion);

			HMODULE module = nullptr;
			if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				static_cast<LPCWSTR>(table), &module))
			{
				outFile = L"<unknown module>";
				return;
			}

			wchar_t path[MAX_PATH]{};
			if (::GetModuleFileNameW(module, path, MAX_PATH) != 0)
			{
				wchar_t const* const fileName = std::wcsrchr(path, L'\\');
				outFile = fileName != nullptr ? fileName + 1 : path;
			}

			auto const declare = reinterpret_cast<SpiSupportDeclFn*>(::GetProcAddress(module, "SpiSupportDecl"));
			if (declare == nullptr)
				return;
			wchar_t* name = nullptr;
			wchar_t* author = nullptr;
			wchar_t* version = nullptr;
			int gameFlags = 0;
			int minVersion = 0;
			declare(&name, &author, &version, &gameFlags, &minVersion);
			if (name != nullptr)
				outName = name;
		}

		// MilanHost registers the service during its own attach, which can come after ours, so
		// this keeps looking (once a second) until it appears.
		bool ResolveService()
		{
			if (g_service != nullptr)
				return true;
			if (g_providerRejected)
				return false;

			auto const now = std::chrono::steady_clock::now();
			if (now < g_nextLookup)
				return false;
			g_nextLookup = now + k_lookupInterval;

			ISharedProxyInterface* const proxy = HookManager != nullptr ? HookManager->ProxyInstance : nullptr;
			unsigned long proxyVersion = 0;
			if (proxy == nullptr || proxy->GetVersion(&proxyVersion) != SPIReturn::Success || proxyVersion < SPI_VERSION_V4)
				return false;

			const void* table = nullptr;
			std::uint32_t version = 0;
			// Only version 1 functions are used, so older MilanHost builds still work.
			SPIReturn const rc = proxy->QueryService(LMP_LOBBY_SERVICE_NAME, 1, &table, &version);
			if (rc == SPIReturn::Success)
			{
				std::wstring providerName{};
				std::wstring providerFile{};
				DescribeProvider(table, providerName, providerFile);
				if (providerName != k_lobbyProviderName)
				{
					// Its state and joins would not be MilanHost's, so Discord joins stay off.
					g_providerRejected = true;
					LEASI_ERROR("{} is provided by {} ('{}'), not MilanHost; Discord joins are disabled",
						LMP_LOBBY_SERVICE_NAME, ToUtf8(providerFile), ToUtf8(providerName));
					if (!g_pendingSecret.empty())
						LEASI_ERROR("dropping the Discord join that was waiting for MilanHost");
					g_pendingSecret.clear();
					return false;
				}

				g_service = static_cast<LmpLobbyService const*>(table);
				LEASI_INFO("found {} v{} in {}", LMP_LOBBY_SERVICE_NAME, version, ToUtf8(providerFile));
				return true;
			}

			// Not found yet is expected while MilanHost attaches; anything else will not fix itself.
			if (rc != SPIReturn::FailureNotFound && !g_lookupFailureLogged)
			{
				g_lookupFailureLogged = true;
				LEASI_WARN("{} is unavailable (SPI error {}, registered version {})", LMP_LOBBY_SERVICE_NAME, static_cast<int>(rc), version);
			}
			return false;
		}

		bool ParseSecret(std::string const& secret, std::string& outCode, std::string& outUrl)
		{
			constexpr std::size_t codeStart = sizeof(k_secretPrefix) - 1;
			if (secret.rfind(k_secretPrefix, 0) != 0)
				return false;
			std::size_t const split = secret.find(';', codeStart);
			if (split == std::string::npos || split == codeStart)
				return false;
			outCode = secret.substr(codeStart, split - codeStart);
			outUrl = secret.substr(split + 1);
			return true;
		}

		void SubmitJoin(std::string const& secret)
		{
			std::string code{};
			std::string url{};
			if (!ParseSecret(secret, code, url))
			{
				LEASI_ERROR("ignoring a Discord join with an unrecognized secret");
				return;
			}

			// An empty URL keeps the current matchmaker.
			std::uint32_t const rc = g_service->RequestJoin(url.c_str(), code.c_str());
			if (rc == LMP_LOBBY_OK)
				LEASI_INFO("Discord join: invite code {} queued with MilanHost", code);
			else
				LEASI_ERROR("Discord join: MilanHost refused invite code {} (error {})", code, rc);
		}

		std::string BuildJoinSecret(LmpLobbyState const& state)
		{
			if (std::strchr(state.InviteCode, ';') != nullptr)
				return {};
			std::string secret = std::string{ k_secretPrefix } + state.InviteCode + ";" + state.MatchmakerUrl;
			if (secret.size() > k_discordFieldMax)
			{
				if (!g_longSecretLogged)
				{
					g_longSecretLogged = true;
					LEASI_WARN("the matchmaker URL is too long for a Discord join secret; Discord joins are disabled");
				}
				return {};
			}
			return secret;
		}

		std::string ToUtf8(std::wstring const& text)
		{
			int const size = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
			std::string result(static_cast<std::size_t>(size > 0 ? size : 0), '\0');
			if (size > 0)
				::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
			return result;
		}
	}

	void Tick()
	{
		auto const now = std::chrono::steady_clock::now();
		if (now < g_nextPoll)
			return;
		g_nextPoll = now + k_pollInterval;

		if (!ResolveService())
			return;

		if (!g_pendingSecret.empty())
			SubmitJoin(std::exchange(g_pendingSecret, {}));

		std::uint64_t const generation = g_service->GetStateGeneration();
		if (generation == g_seenGeneration)
			return;

		LmpLobbyState next{};
		next.StructSize = sizeof(next);
		if (g_service->GetState(&next) != LMP_LOBBY_OK)
			return;
		g_seenGeneration = generation;
		g_state = next;
		g_unsent = true;

		static constexpr char const* k_roles[] = { "none", "host", "member" };
		LEASI_INFO("lobby state: role {}, flags {:#x}, players {}/{}, lobby id {}, invite code {}",
			next.Role < 3 ? k_roles[next.Role] : "?", next.Flags, next.PlayerCount, next.MaxPlayers,
			next.LobbyId[0] != '\0' ? next.LobbyId : "(none)", next.InviteCode[0] != '\0' ? "known" : "(none)");
	}

	bool HasUnsentChange()
	{
		return g_unsent;
	}

	void FillActivity(discordpp::Activity& activity)
	{
		g_unsent = false;
		LmpLobbyState const& state = g_state;
		bool const connected = (state.Flags & LMP_LOBBY_FLAG_CONNECTED) != 0;
		bool const hosting = (state.Flags & LMP_LOBBY_FLAG_HOSTING) != 0;

		// Logs only when the answer changes, so the log shows why Discord does or does not offer a join.
		auto const reportJoin = [](std::string report)
		{
			if (report != g_lastJoinReport)
			{
				LEASI_INFO("Discord join: {}", report);
				g_lastJoinReport = std::move(report);
			}
		};

		if (!connected)
		{
			reportJoin("not offered (not in a game)");
			activity.SetDetails((state.Flags & LMP_LOBBY_FLAG_JOIN_PENDING) != 0 ? "Joining a game" : "In menus");
			return;
		}

		if (hosting)
			activity.SetDetails((state.Flags & LMP_LOBBY_FLAG_IN_PROGRESS) != 0 ? "In a match" : "In the lobby");
		else
			activity.SetDetails("In a game");
		activity.SetState(hosting ? "Hosting" : "Joined");

		// Members of the same matchmaker lobby share the party; a game without one is shown alone.
		if (state.LobbyId[0] == '\0' || state.PlayerCount < 1)
		{
			reportJoin("not offered (the lobby is not registered with the matchmaker)");
			return;
		}

		discordpp::ActivityParty party{};
		party.SetId(std::string{ "lmp-" } + state.LobbyId);
		party.SetCurrentSize(state.PlayerCount);
		party.SetMaxSize(state.MaxPlayers > 0 ? state.MaxPlayers : 0);

		// Only the host knows whether the lobby takes players, so only it hands out the secret.
		std::string const secret = (state.Flags & LMP_LOBBY_FLAG_JOINABLE) != 0 && state.InviteCode[0] != '\0'
			? BuildJoinSecret(state) : std::string{};
		bool const isPublic = !secret.empty() && (state.Flags & LMP_LOBBY_FLAG_PRIVATE) == 0;
		party.SetPrivacy(isPublic ? discordpp::ActivityPartyPrivacy::Public : discordpp::ActivityPartyPrivacy::Private);
		activity.SetParty(party);

		if (!hosting)
			reportJoin("not offered (only the host offers joins)");
		else if ((state.Flags & LMP_LOBBY_FLAG_JOINABLE) == 0)
			reportJoin("not offered (MilanHost reports the lobby as not joinable)");
		else if (secret.empty())
			reportJoin("not offered (no usable join secret)");
		else
			reportJoin(isPublic ? "offered (public: friends can join directly)" : "offered (private: invite only)");

		if (!secret.empty())
		{
			discordpp::ActivitySecrets secrets{};
			secrets.SetJoin(secret);
			activity.SetSecrets(secrets);
			activity.SetSupportedPlatforms(discordpp::ActivityGamePlatforms::Desktop);
		}
	}

	void OnJoinSecret(std::string const& secret)
	{
		LEASI_INFO("Discord join accepted");
		if (g_providerRejected)
			LEASI_ERROR("cannot join: {} is not provided by MilanHost", LMP_LOBBY_SERVICE_NAME);
		else if (g_service != nullptr)
			SubmitJoin(secret);
		else
			g_pendingSecret = secret; // Tick submits it once MilanHost's service is found.
	}

	void RegisterLaunchCommand(discordpp::Client& client, uint64_t applicationId)
	{
		// Mod Manager registers "<exe>" --m3link "%1" for its links. HKEY_CLASSES_ROOT merges the
		// per-user registration it makes with any machine-wide one.
		wchar_t handler[1024]{};
		DWORD size = sizeof(handler);
		LSTATUS const status = ::RegGetValueW(HKEY_CLASSES_ROOT, k_launchHandlerKey, nullptr, RRF_RT_REG_SZ, nullptr, handler, &size);
		if (status != ERROR_SUCCESS)
		{
			LEASI_WARN("ME3Tweaks Mod Manager has not registered its link handler (error {}); Discord cannot start LMP for a join", status);
			return;
		}

		std::wstring command{ handler };
		std::size_t const argument = command.find(L"%1");
		if (argument == std::wstring::npos)
		{
			LEASI_WARN("ME3Tweaks Mod Manager's link handler has no %1 argument; Discord cannot start LMP for a join");
			return;
		}
		command.replace(argument, 2, k_launchLink);

		std::string const utf8 = ToUtf8(command);
		if (client.RegisterLaunchCommand(applicationId, utf8))
			LEASI_INFO("Discord launch command: {}", utf8);
		else
			LEASI_WARN("Discord did not accept the launch command {}", utf8);
	}
}

#endif
