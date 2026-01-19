#pragma once

#include <concurrent_queue.h>
#include <memory>

namespace LEXInterop
{
    // Base class for actions that are queued from the pipe thread
    // and executed on the game thread during ProcessEvent.
    struct ActionBase
    {
        virtual ~ActionBase() = default;
        virtual void Execute() = 0;
    };

    // Thread-safe queue for actions posted from the pipe thread.
    // Actions are popped, executed, and deleted from the game thread.
    inline concurrency::concurrent_queue<ActionBase*> g_ActionQueue{};

    // Processes all pending actions in the queue.
    // Call this from the game thread (e.g., in ProcessEvent hook).
    inline void ProcessPendingActions()
    {
        ActionBase* action = nullptr;
        while (g_ActionQueue.try_pop(action))
        {
            if (action)
            {
                action->Execute();
                delete action;
            }
        }
    }

    // Helper to queue an action safely.
    inline void QueueAction(ActionBase* action)
    {
        if (action)
        {
            g_ActionQueue.push(action);
        }
    }
}
