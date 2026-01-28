#include "Hooks.hpp"
#include "ActionQueue.hpp"
#include "SharedData.hpp"
#include "LEXCommunications.hpp"
#include "PathfindingGPS.hpp"
#include "LiveLevelEditor/LiveLevelEditor.hpp"
#include "AssetViewer.hpp"
#include "FileLoader.hpp"
#include "LiveLevelEditor/LiveLevelEditorActions.hpp"
#include "ScriptDebugger/ScriptDebugger.hpp"

namespace LEXInterop
{
    // ProcessEvent hook
    t_UObject_ProcessEvent* UObject_ProcessEvent_orig = nullptr;

    void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result)
    {
        // Process through all handlers - each returns true if other handlers should also process
        // Short-circuit evaluation means handlers after a false return won't be called
        SharedData::ProcessEvent(Context, Function, Parms, Result)
            && LEXCommunications::ProcessEvent(Context, Function, Parms, Result)
            && PathfindingGPS::ProcessEvent(Context, Function, Parms, Result)
            && LiveLevelEditor::ProcessEvent(Context, Function, Parms, Result)
            && AssetViewer::ProcessEvent(Context, Function, Parms, Result);

        // Process any pending actions from the pipe thread
        ProcessPendingActions();

        // Call original
        UObject_ProcessEvent_orig(Context, Function, Parms, Result);
    }

    // FindPackageFile hook - allows loading precached packages from LEX
    t_FindPackageFile* FindPackageFile_orig = nullptr;

    unsigned FindPackageFile_hook(void* This, wchar_t* InName, FGuid* Guid, FString* OutFileName, const wchar_t* Language, unsigned Unk)
    {
        // First try the original file lookup
        auto result = FindPackageFile_orig(This, InName, Guid, OutFileName, Language, Unk);

        // If not found on disk and we have a valid name, check precache map
        if (!result && InName != nullptr && wcslen(InName) > 0)
        {
            if (FileLoader::FindPrecachedPackage(InName))
            {
                // Package is precached - set the output filename so the engine finds it
                // The engine will then load from PackagePrecacheMap using this name
                *OutFileName = FString(InName);
                result = TRUE;
                LEASI_INFO(L"FindPackageFile: using precached package {}", InName);
            }
        }

        return result;
    }

    // SetLinker hook - tracks which files assets loaded from
    t_SetLinker* SetLinker_orig = nullptr;

    void SetLinker_hook(UObject* Context, ULinkerLoad* Linker, int LinkerIndex)
    {
        // Track the file origin before calling original
        if (Context && Context->Linker)
        {
            auto filename = Context->Linker->Filename;
            if (filename.Length() > 0)
            {
                auto fullPath = Context->GetFullName();
                ObjFullNameToFileNameMap.insert_or_assign(fullPath, filename);
            }
        }

        SetLinker_orig(Context, Linker, LinkerIndex);
    }

    t_GameEngineTick* GameEngineTick_orig = nullptr;

    void GameEngineTick_hook(UGameEngine* Context, float deltaSeconds)
    {
        // Process pending debugger attach/detach
        ScriptDebugger::ProcessPendingState();

        GameEngineTick_orig(Context, deltaSeconds);
    }
}
