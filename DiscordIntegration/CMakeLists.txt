asi_target (DiscordIntegration
  SHARED
  GAMES "LE1" "LE2" "LE3" "LMP"
  SOURCES
    "Entry.cpp"
    "Entry.hpp"
    "Hooks.cpp"
    "Hooks.hpp"
    "Discord.hpp"
    "Discord.cpp"
    "LmpLobby.h"
    "LmpPresence.hpp"
    "LmpPresence.cpp"
    "discord_social_sdk/include/cdiscord.h"
    "discord_social_sdk/include/discordpp.h"
    "SharedVersion.h"
    "DiscordIntegration.rc"
  VLINKS
    "Common"
    "LESDK"
  LINKS
   "simdutf"
   "advapi32"
   "delayimp"
    "../../DiscordIntegration/discord_social_sdk/lib/release/discord_partner_sdk"
  )

# The Discord SDK DLL is delay-loaded so a missing DLL, or one from another SDK version, only
# disables this ASI (see Entry.cpp) instead of stopping the game from starting.
foreach (GAME IN ITEMS "LE1" "LE2" "LE3" "LMP")
  if (TARGET DiscordIntegration-${GAME})
    target_link_options (DiscordIntegration-${GAME} PRIVATE "/DELAYLOAD:discord_partner_sdk.dll")
  endif ()
endforeach ()
