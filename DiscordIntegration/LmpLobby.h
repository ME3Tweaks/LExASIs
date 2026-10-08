/**
 * @file        MilanHost/Public/LmpLobby.h
 * @header      The "LMP.Lobby" SPI service, which lets other ASIs read MilanHost's matchmaking
 *              lobby and ask it to join one, e.g. to share invites through Discord.
 *
 * Self-contained so other ASIs can copy it. Look the service up with
 *
 *     const void* Table = nullptr;
 *     SPIReturn const Rc = Spi->QueryService(LMP_LOBBY_SERVICE_NAME, LMP_LOBBY_SERVICE_VERSION, &Table, nullptr);
 *
 * and retry on FailureNotFound (e.g. once per tick): MilanHost registers the service during its
 * attach, which can run after yours. The table stays valid until the process exits.
 *
 * Every function may be called from any thread. Strings are ASCII and NUL-terminated.
 * Later versions only append to LmpLobbyService and LmpLobbyState.
 */

#pragma once
#include <stdint.h>


#define LMP_LOBBY_SERVICE_NAME      "LMP.Lobby"
#define LMP_LOBBY_SERVICE_VERSION   2   // 2 added the invite handlers.

// Join links leave out the server when it is this one.
#define LMP_LOBBY_DEFAULT_MATCHMAKER_URL "https://vps.me3tweaks.com"

// Field sizes, including the terminator.
#define LMP_LOBBY_URL_MAX           256
#define LMP_LOBBY_ID_MAX            64
#define LMP_LOBBY_CODE_MAX          65

// Results.
#define LMP_LOBBY_OK                0
#define LMP_LOBBY_INVALID_ARGUMENT  1
#define LMP_LOBBY_SHUTTING_DOWN     2
#define LMP_LOBBY_FAILED            3
#define LMP_LOBBY_LIMIT_REACHED     4   // Version 2: too many invite handlers.

// What an LmpLobbyInviteHandler did.
#define LMP_LOBBY_INVITE_NOT_SHOWN  0   // Nothing the player can see, e.g. it only logged or sent something.
#define LMP_LOBBY_INVITE_SHOWN      1   // Its own invite UI, so MilanHost does not show its invite code hint.

// LmpLobbyState::Role: this game's part in a matchmaker lobby.
#define LMP_LOBBY_ROLE_NONE         0
#define LMP_LOBBY_ROLE_HOST         1   // Hosting, whether or not the lobby is registered yet.
#define LMP_LOBBY_ROLE_MEMBER       2   // Joining or joined another game's lobby, including a quick match.

// LmpLobbyState::Flags.
#define LMP_LOBBY_FLAG_CONNECTED    (1u << 0)   // Hosting a game (listening), or joined to one.
#define LMP_LOBBY_FLAG_JOINABLE     (1u << 1)   // Host only: an invite would be accepted now: registered, not full
                                                // (counting players on their way in), and the level is not changing.
#define LMP_LOBBY_FLAG_PRIVATE      (1u << 2)   // Host only: the lobby is private (invite only).
#define LMP_LOBBY_FLAG_IN_PROGRESS  (1u << 3)   // Host only: the match has left the lobby.
#define LMP_LOBBY_FLAG_JOIN_PENDING (1u << 4)   // A join by invite code is queued for the multiplayer menu or underway.
#define LMP_LOBBY_FLAG_HOSTING      (1u << 5)   // Connected as the listen server, with or without a matchmaker.


typedef struct LmpLobbyState
{
    // Set by the caller to sizeof(LmpLobbyState) before GetState; fields past it are not written.
    // GetState sets it to the number of bytes it wrote.
    uint32_t    StructSize;
    uint32_t    Role;           // LMP_LOBBY_ROLE_*
    uint32_t    Flags;          // LMP_LOBBY_FLAG_*
    int32_t     PlayerCount;    // 0 when not connected.
    int32_t     MaxPlayers;     // 0 when unknown. Members report the lobby's size as of their reservation.

    // Empty when matchmaking is disabled (or the URL does not fit).
    char        MatchmakerUrl[LMP_LOBBY_URL_MAX];
    // The lobby's ID on the matchmaker: the host's registered lobby, or the one a member
    // holds a reservation for. Stays the same across host migration. Empty otherwise.
    char        LobbyId[LMP_LOBBY_ID_MAX];
    // The lobby's invite code. Members only know it when they joined with it, not after
    // a quick match. Empty otherwise.
    char        InviteCode[LMP_LOBBY_CODE_MAX];
} LmpLobbyState;


// Version 2. Called when the host presses Invite Friends while its lobby has an invite code,
// on the game thread inside the button's script call, so it must not block. State is the
// lobby as of the press. JoinLink is the link MilanHost has already tried to copy to the
// clipboard:
//
//     https://me3tweaks.com/modmanager/protocol/lmpjoin?code=<InviteCode>&server=<MatchmakerUrl>
//
// with both values percent-encoded, and without &server= when the matchmaker is
// LMP_LOBBY_DEFAULT_MATCHMAKER_URL; a link without one means that server. The page opens me3tweaksmodmanager://lmpjoin with the same
// query, which ME3Tweaks Mod Manager handles. Neither pointer outlives the call.
// Returns LMP_LOBBY_INVITE_SHOWN or LMP_LOBBY_INVITE_NOT_SHOWN.
typedef uint32_t (*LmpLobbyInviteHandler)(const LmpLobbyState* State, const char* JoinLink, void* Context);


typedef struct LmpLobbyService
{
    // Copies the current state. Returns LMP_LOBBY_INVALID_ARGUMENT if State is null or its
    // StructSize is smaller than the version 1 struct.
    uint32_t    (*GetState)(LmpLobbyState* State);

    // Changes whenever anything GetState reports changes, so callers can poll it each tick
    // and only call GetState when it moved. 0 until MilanHost first publishes a state.
    uint64_t    (*GetStateGeneration)(void);

    // Joins the lobby with InviteCode once the multiplayer menu is open, the same as -mmjoin=
    // or the control pipe. A non-empty MatchmakerUrl (http:// or https://) first becomes the
    // matchmaker; NULL or "" keeps the current one. Only queues: watch the state for the result.
    uint32_t    (*RequestJoin)(const char* MatchmakerUrl, const char* InviteCode);

    // Version 2: query the service with a minimum version of 2 before using these.
    //
    // Adds Handler to the ones Invite Friends calls, in the order they were added. Every handler
    // is called; MilanHost shows its invite code hint only if none returned LMP_LOBBY_INVITE_SHOWN.
    // Returns LMP_LOBBY_INVALID_ARGUMENT if Handler is null or the pair is already added, and
    // LMP_LOBBY_LIMIT_REACHED once 16 are added.
    uint32_t    (*AddInviteHandler)(LmpLobbyInviteHandler Handler, void* Context);

    // Removes a pair AddInviteHandler added; it is not called after this returns, unless
    // Invite Friends is already calling it. Returns LMP_LOBBY_INVALID_ARGUMENT if the pair
    // was not added. Handlers may call this on themselves.
    uint32_t    (*RemoveInviteHandler)(LmpLobbyInviteHandler Handler, void* Context);
} LmpLobbyService;
