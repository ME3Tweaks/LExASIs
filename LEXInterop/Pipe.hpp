#pragma once

#include <atomic>
#include <thread>

namespace LEXInterop
{
    // Named pipe communication with LEX
    class PipeServer
    {
    public:
        static void Start();
        static void Stop();
        static bool IsRunning();

    private:
        static void PipeThreadFunc();
        static void ProcessCommand(char* command, unsigned long bytesRead, void* pipe);

        static inline std::thread s_PipeThread;
        static inline std::atomic<bool> s_Running{ false };
        static inline std::atomic<bool> s_StopRequested{ false };
    };
}
