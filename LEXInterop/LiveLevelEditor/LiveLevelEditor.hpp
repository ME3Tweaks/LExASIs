#pragma once

#include <set>
#include <string>
#include <LESDK/Headers.hpp>
#include "LEXInterop/ActionQueue.hpp"
#include "LiveLevelEditorActions.hpp"

namespace LEXInterop
{
    // Live Level Editor - allows real-time manipulation of actors from LEX
    class LiveLevelEditor
    {
    public:
        static inline bool IsActive = false;
        static inline bool IsPendingDeactivation = false;
        static inline AActor* SelectedActor = nullptr;
        static inline SFXName SelectedActorMapName{};
        static inline int SelectedCollectionComponentIndex = -1;
        static inline int SelectedActorComponentIndex = -1;
        static inline bool DrawLineToSelected = true;
        static inline FLinearColor TraceLineColor{ 1.0f, 1.0f, 0.0f, 1.0f };
        static inline float TraceWidth = 3.0f;
        static inline float CoordinateScale = 100.0f;
        static inline std::set<SFXName> LevelsLoadedByLLE{};

        static bool HandleCommand(char* command);
        static bool ProcessEvent(UObject* Context, UFunction* Function, void* Parms, void* Result);
    };
}
