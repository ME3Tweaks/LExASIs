#pragma once

#include <LESDK/Headers.hpp>

namespace Common
{
    // Recursively find sequence objects of a given type within a sequence
    template<std::derived_from<UObject> T>
    void FindSeqObjectsByClass(USequence* sequence, TArray<T*>& OutputObjects, bool bRecursive = true)
    {
        if (!sequence) return;

        UClass* cls = T::StaticClass();
        for (auto sequenceObject : sequence->SequenceObjects)
        {
            if (sequenceObject && sequenceObject->IsA(cls))
            {
                OutputObjects.Add(static_cast<T*>(sequenceObject));
            }
        }

        if (bRecursive)
        {
            for (auto nestedSequence : sequence->NestedSequences)
            {
                if (nestedSequence)
                {
                    FindSeqObjectsByClass<T>(nestedSequence, OutputObjects, bRecursive);
                }
            }
        }
    }

    void CauseRemoteEvent(ABioPlayerController* const player, const SFXName eventName);
}
