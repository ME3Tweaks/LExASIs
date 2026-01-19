#pragma once

#include <string>
#include <LESDK/Headers.hpp>
#include "ActionQueue.hpp"
#include "Utilities.hpp"
#include "Common/Base.hpp"
#include "Common/Objects.hpp"
#include "Common/Utils.hpp"
#include "AdditionalFunctions.hpp"

namespace LEXInterop
{
    // Action to cause a console event in kismet
    struct CauseEventAction final : ActionBase
    {
        std::wstring EventName;

        explicit CauseEventAction(std::wstring eventName) : EventName(std::move(eventName)) {}

        void Execute() override
        {
            auto player = Common::FindFirstObject<ABioPlayerController>();
            if (player)
            {
                SFXName foundName(EventName.c_str(), 0, true);
                player->CauseEvent(foundName);
            }
        }
    };

    // Action to cause a remote event in kismet
    struct RemoteEventAction final : ActionBase
    {
        std::wstring EventName;

        explicit RemoteEventAction(std::wstring eventName) : EventName(std::move(eventName)) {}

        void Execute() override
        {
            SFXName eventName(EventName.c_str(), 0, true);
            auto player = Common::FindFirstObject<ABioPlayerController>();
			Common::CauseRemoteEvent(player, eventName);
        }
    };

    // Action to execute a console command
    struct ConsoleCommandAction final : ActionBase
    {
        std::wstring Command;

        explicit ConsoleCommandAction(std::wstring command) : Command(std::move(command)) {}

        void Execute() override
        {
            auto player = Common::FindFirstObject<ABioPlayerController>();
            if (player)
            {
                LEASI_INFO(L"Issued console command: {}", Command);
                FString cmdStr(Command.c_str());
                player->ConsoleCommand(cmdStr, false);
            }
            else
            {
                LEASI_WARN(L"Failed to issue console command: {}", Command);
            }
        }
    };

#if defined(SDK_TARGET_LE1) 
    //struct CachePackageAction final : ActionBase
    //{
    //    std::wstring PackageName;

    //    explicit CachePackageAction(std::wstring packageName) : PackageName(std::move(packageName)) {}

    //    void Execute() override
    //    {
    //        CacheContent(CacheContentWrapperClassPointer, PackageName.data(), true, true);
    //    }
    //};
#endif

    // Action to stream a level in/out
    struct StreamLevelAction final : ActionBase
    {
        enum class Kind { StreamOut, StreamIn, OnlyLoad };

        std::wstring LevelName;
        Kind ChangeKind;

        StreamLevelAction(std::wstring levelName, Kind kind)
            : LevelName(std::move(levelName)), ChangeKind(kind) {}

        void Execute() override
        {
            auto player = Common::FindFirstObject<APlayerController>();
            if (!player || !player->CheatManager) return;

            SFXName levelName(LevelName.c_str(), 0, true);

            // Check if level is already in streaming levels
            for (auto streamingLevel : player->WorldInfo->StreamingLevels)
            {
                if (streamingLevel && streamingLevel->PackageName == levelName)
                {
					StreamLevel(player, levelName);
                    return;
                }
            }

            SFXName name_none(L"", 0);
            const auto lsk = StaticConstructObject<ULevelStreamingKismet>(*GWorld, name_none);
            lsk->PackageName = levelName;
            player->WorldInfo->StreamingLevels.Add(lsk);
            StreamLevel(player, levelName);
        }

	private:
        void StreamLevel(APlayerController* player, SFXName levelName)
        {
            switch (ChangeKind)
            {
            case Kind::StreamOut:
                player->CheatManager->StreamLevelOut(levelName);
                return;
            case Kind::StreamIn:
                player->CheatManager->StreamLevelIn(levelName);
                return;
            case Kind::OnlyLoad:
                player->CheatManager->OnlyLoadLevel(levelName);
                return;
            }
        }
    };

    // Action to load a package
    struct LoadPackageAction final : ActionBase
    {
        std::wstring PackageName;

        explicit LoadPackageAction(std::wstring packageName) : PackageName(std::move(packageName)) {}

        void Execute() override
        {
#if defined(SDK_TARGET_LE2) || defined(SDK_TARGET_LE3)
            auto sfxGame = Common::FindFirstObject<ASFXGame>();
            if (sfxGame)
            {
                LEASI_INFO(L"Loading package {}", PackageName);
                FString fstr(PackageName.c_str());
                sfxGame->LoadPackage(fstr);
            }
#else
            LEASI_WARN("LoadPackage not supported on LE1");
#endif
        }
    };

    // Action to show loading indicator
    struct ShowLoadingIndicatorAction final : ActionBase
    {
        void Execute() override
        {
#if defined(SDK_TARGET_LE2)
            auto guiManager = Common::FindFirstObject<UMassEffectGuiManager>();
            if (guiManager)
            {
                auto saveLoadWidget = guiManager->eventGetSaveLoadWidget();
                if (saveLoadWidget)
                {
					auto loadingStrRef = 170447; // "Loading..."
                    saveLoadWidget->ShowLoadingMessage(TRUE, loadingStrRef);
                }
            }
#elif defined(SDK_TARGET_LE3)
            auto interaction = Common::FindFirstObject<USFXGUIInteraction>();
            if (interaction)
            {
                auto saveLoadWidget = interaction->eventGetSaveLoadWidget();
                if (saveLoadWidget)
                {
                    saveLoadWidget->ShowLoadingMessage(TRUE, 0);
                }
            }
#endif
        }
    };

    // Action to hide loading indicator
    struct HideLoadingIndicatorAction final : ActionBase
    {
        void Execute() override
        {
#if defined(SDK_TARGET_LE2)
            auto guiManager = Common::FindFirstObject<UMassEffectGuiManager>();
            if (guiManager)
            {
                auto saveLoadWidget = guiManager->eventGetSaveLoadWidget();
                if (saveLoadWidget)
                {
                    saveLoadWidget->HideLoadingMessage(TRUE);
                }
            }
#elif defined(SDK_TARGET_LE3)
            auto interaction = Common::FindFirstObject<USFXGUIInteraction>();
            if (interaction)
            {
                auto saveLoadWidget = interaction->eventGetSaveLoadWidget();
                if (saveLoadWidget)
                {
                    saveLoadWidget->HideLoadingMessage(TRUE);
                }
            }
#endif
        }
    };

    // Handler for generic commands from the pipe
    class GenericCommands
    {
    public:
        static bool HandleCommand(char* command)
        {
#if defined(SDK_TARGET_LE1)
            // 'CACHEPACKAGE <PackageFileFullPath>'
            // Registers a package so game methods can find it
            //if (IsCmd(&command, "CACHEPACKAGE "))
            //{
            //    QueueAction(new CachePackageAction(s2ws(command)));
            //    return true;
            //}
#endif
            // CAUSEEVENT <EventName>
            if (IsCmd(&command, "CAUSEEVENT "))
            {
                QueueAction(new CauseEventAction(s2ws(command)));
                return true;
            }

            // REMOTEEVENT <EventName>
            if (IsCmd(&command, "REMOTEEVENT "))
            {
                QueueAction(new RemoteEventAction(s2ws(command)));
                return true;
            }

            // CONSOLECOMMAND <Command>
            if (IsCmd(&command, "CONSOLECOMMAND "))
            {
                QueueAction(new ConsoleCommandAction(s2ws(command)));
                return true;
            }

            // STREAMLEVELIN <LevelName>
            if (IsCmd(&command, "STREAMLEVELIN "))
            {
                QueueAction(new StreamLevelAction(s2ws(command), StreamLevelAction::Kind::StreamIn));
                return true;
            }

            // STREAMLEVELOUT <LevelName>
            if (IsCmd(&command, "STREAMLEVELOUT "))
            {
                QueueAction(new StreamLevelAction(s2ws(command), StreamLevelAction::Kind::StreamOut));
                return true;
            }

            // ONLYLOADLEVEL <LevelName>
            if (IsCmd(&command, "ONLYLOADLEVEL "))
            {
                QueueAction(new StreamLevelAction(s2ws(command), StreamLevelAction::Kind::OnlyLoad));
                return true;
            }

            // LOADPACKAGE <PackageName>
            if (IsCmd(&command, "LOADPACKAGE "))
            {
                QueueAction(new LoadPackageAction(s2ws(command)));
                return true;
            }

            // SHOWLOADINGINDICATOR
            if (IsCmd(&command, "SHOWLOADINGINDICATOR"))
            {
                QueueAction(new ShowLoadingIndicatorAction());
                return true;
            }

            // HIDELOADINGINDICATOR
            if (IsCmd(&command, "HIDELOADINGINDICATOR"))
            {
                QueueAction(new HideLoadingIndicatorAction());
                return true;
            }

            return false;
        }

        GenericCommands() = delete;
    };
}
