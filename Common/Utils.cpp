#include "Common/Utils.hpp"

namespace Common
{
    void CauseRemoteEvent(ABioPlayerController* const player, const SFXName eventName)
    {
        if (!player || !player->WorldInfo) return;

        USequence* gameSeq = player->WorldInfo->GetGameSequence();
        if (!gameSeq) return;

        TArray<USeqEvent_RemoteEvent*> remoteEvents;
        Common::FindSeqObjectsByClass<USeqEvent_RemoteEvent>(gameSeq, remoteEvents, true);

        for (auto remoteEvent : remoteEvents)
        {
            if (remoteEvent && remoteEvent->EventName == eventName && remoteEvent->bEnabled)
            {
#if defined(SDK_TARGET_LE3)
                remoteEvent->CheckActivate(player, player->Pawn, FALSE, FALSE, FALSE, nullptr);
#else
                remoteEvent->CheckActivate(player, player->Pawn, FALSE, FALSE, nullptr);
#endif
            }
        }
	}
}
