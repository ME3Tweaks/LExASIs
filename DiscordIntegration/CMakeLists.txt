asi_target (DiscordIntegration
  SHARED
  GAMES "LE1" "LE2" "LE3"
  SOURCES
    "Entry.cpp"
    "Entry.hpp"
    "Hooks.cpp"
    "Hooks.hpp"
    "Discord.hpp"
    "Discord.cpp"
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
    "../../DiscordIntegration/discord_social_sdk/lib/release/discord_partner_sdk"
  )
