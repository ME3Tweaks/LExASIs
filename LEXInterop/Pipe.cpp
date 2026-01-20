#include "Pipe.hpp"
#include "Utilities.hpp"
#include "GenericCommands.hpp"
#include "FileLoader.hpp"
#include "PathfindingGPS.hpp"
#include "AssetViewer.hpp"
#include "LiveLevelEditor/LiveLevelEditor.hpp"
#include "Common/Base.hpp"

#include <Windows.h>

namespace LEXInterop
{
    constexpr DWORD PIPE_BUFFER_SIZE = 1024;

    void PipeServer::Start()
    {
        if (s_Running.load())
        {
            LEASI_WARN("Pipe server already running");
            return;
        }

        s_StopRequested.store(false);
        s_PipeThread = std::thread(PipeThreadFunc);
        s_PipeThread.detach();
        LEASI_INFO("Pipe server thread started");
    }

    void PipeServer::Stop()
    {
        if (!s_Running.load())
        {
            return;
        }

        s_StopRequested.store(true);

        LEASI_INFO("Pipe server thread stopped");
    }

    bool PipeServer::IsRunning()
    {
        return s_Running.load();
    }

    void PipeServer::PipeThreadFunc()
    {
        s_Running.store(true);

        char buffer[PIPE_BUFFER_SIZE]{};
        DWORD bytesRead;

        HANDLE hPipe = CreateNamedPipeW(
            GetPipeName(),
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,
            PIPE_BUFFER_SIZE * 16,
            PIPE_BUFFER_SIZE * 16,
            NMPWAIT_USE_DEFAULT_WAIT,
            nullptr);

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            LEASI_ERROR("Failed to create named pipe: {}", GetLastError());
            s_Running.store(false);
            return;
        }

        LEASI_INFO("Named pipe created successfully");

        while (!s_StopRequested.load() && hPipe != INVALID_HANDLE_VALUE)
        {
            // Wait for a client to connect
            if (ConnectNamedPipe(hPipe, nullptr) != FALSE || GetLastError() == ERROR_PIPE_CONNECTED)
            {
                LEASI_TRACE("Client connected to pipe");

                // Read commands from the pipe
                while (!s_StopRequested.load() &&
                       ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) != FALSE)
                {
                    buffer[bytesRead] = '\0';
                    ProcessCommand(buffer, bytesRead, hPipe);
                }

                LEASI_TRACE("Client disconnected from pipe");
            }

            DisconnectNamedPipe(hPipe);
        }

        CloseHandle(hPipe);
        s_Running.store(false);
    }

    void PipeServer::ProcessCommand(char* command, unsigned long bytesRead, void* pipe)
    {
        LEASI_DEBUG("Received command: {}", command);

        HANDLE hPipe = static_cast<HANDLE>(pipe);

        // Try each command handler in order
        bool handled =
            FileLoader::HandleCommand(command, bytesRead, hPipe) ||
            GenericCommands::HandleCommand(command) ||
            PathfindingGPS::HandleCommand(command) ||
            LiveLevelEditor::HandleCommand(command) ||
            AssetViewer::HandleCommand(std::string(command));

        if (!handled)
        {
            LEASI_WARN("Unhandled command: {}", command);
        }
    }
}
