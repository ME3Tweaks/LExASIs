#include "ScriptDebugger.hpp"
#include "LEXInterop/Utilities.hpp"
#include "LEXInterop/LiveLevelEditor/LiveLevelEditorActions.hpp"
#include "Common/Base.hpp"

#include <LESDK/Init.hpp>

#include <Windows.h>
#include <stack>
#include <thread>

namespace LEXInterop::ScriptDebugger
{
    // Internal state
    static BreakPointContainer s_BreakpointMap;
    static std::stack<DebuggerFrame*> s_DebuggerStack;

    static std::atomic<bool> s_PendingAttach{ false };
    static std::atomic<bool> s_PendingDetach{ false };
    static std::atomic<bool> s_IsStepping{ false };
    static std::atomic<int> s_SteppingMinStackDepth{ 0 };
    static std::atomic<bool> s_Resume{ false };
    static std::atomic<bool> s_IsAttached{ false };
    static std::atomic<bool> s_StatusRequested{ false };

    static void* s_ExecutionLoopAddress = nullptr;
    static std::vector<BYTE> s_OriginalExecutionLoopBytes;

    // Pipe thread for debugger commands
    static std::thread s_DebuggerPipeThread;
    static std::atomic<bool> s_DebuggerPipeRunning{ false };
    static std::atomic<bool> s_DebuggerPipeStopRequested{ false };

    // WM_COPYDATA identifiers for debugger
#if defined(SDK_TARGET_LE1)
    constexpr unsigned long SENT_FROM_DEBUGGER = 0x02AC00D7;
#elif defined(SDK_TARGET_LE2)
    constexpr unsigned long SENT_FROM_DEBUGGER = 0x02AC00D8;
#elif defined(SDK_TARGET_LE3)
    constexpr unsigned long SENT_FROM_DEBUGGER = 0x02AC00D9;
#endif

    const wchar_t* GetDebuggerPipeName()
    {
#if defined(SDK_TARGET_LE1)
        return L"\\\\.\\pipe\\LEX_LE1_SCRIPTDEBUG_PIPE";
#elif defined(SDK_TARGET_LE2)
        return L"\\\\.\\pipe\\LEX_LE2_SCRIPTDEBUG_PIPE";
#elif defined(SDK_TARGET_LE3)
        return L"\\\\.\\pipe\\LEX_LE3_SCRIPTDEBUG_PIPE";
#endif
    }

    void SendMsgToLEX(const std::wstring& wstr, DebuggerFrame* stackFrame)
    {
        HWND handle = FindWindowW(nullptr, L"Legendary Explorer");
        if (handle)
        {
            LexDebuggerMsg msg{};
            msg.msgLength = wstr.length();
            wcsncpy_s(msg.msg, wstr.c_str(), msg.msgLength + 1);
            msg.currentFrame = stackFrame;
            msg.NamePool = const_cast<SFXNameEntry**>(SFXName::GBioNamePools);

            COPYDATASTRUCT cds{};
            cds.dwData = SENT_FROM_DEBUGGER;
            cds.cbData = sizeof(msg);
            cds.lpData = &msg;
            SendMessageTimeoutW(handle, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds), 0, 10, nullptr);
        }
    }

    static int GetCurrentStackDepth()
    {
        return static_cast<int>(s_DebuggerStack.size());
    }

    static void BreakState()
    {
        s_IsStepping.store(false);

        SendMsgToLEX(L"Break", s_DebuggerStack.top());

        MSG msg{};
        bool gotQuit = false;
        while (true)
        {
            if (s_Resume.load())
            {
                s_Resume.store(false);
                break;
            }
            if (s_StatusRequested.load())
            {
                SendMsgToLEX(L"Break", s_DebuggerStack.top());
				s_StatusRequested.store(false);
            }
            if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT || msg.message == WM_CLOSE)
                {
                    gotQuit = true;
                    s_PendingDetach.store(true);
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        if (gotQuit)
        {
            PostQuitMessage(static_cast<int>(msg.wParam));
        }
    }

    static bool ShouldBreak(const std::string& nodePathString, const unsigned short location)
    {
        return (s_IsStepping.load() && GetCurrentStackDepth() <= s_SteppingMinStackDepth.load())
            || s_BreakpointMap.HasBreakPoint(nodePathString, location);
    }

    static std::string FStringToStdString(const FString& str)
    {
        auto const ansi_len = LESDK::GetAnsiLengthWide(str.Chars(), str.Length());
        std::string ansi_str(ansi_len, '\0');

        DWORD error = 0;
        if (LESDK::EncodeAnsiFromWide(str.Chars(), str.Length(), ansi_str.data(), ansi_len, &error)) {
            return ansi_str;
        }
        return "<encoding error>";
    }

    void ExecutionLoop_hook(UObject* Context, FFrame* Stack, void* Result, tNative* const* LocalGNatives)
    {
        UStruct* node = Stack->Node;
        std::string nodePathString = FStringToStdString(node->GetFullPath());
        BYTE* beginOffset = node->Script.GetData();

        DebuggerFrame debugFrame{};
        debugFrame.Node = node;
        debugFrame.Object = Stack->Object;
        debugFrame.Locals = Stack->Locals;
        debugFrame.OutParms = Stack->OutParms;
        debugFrame.CodeBaseAddr = beginOffset;
        debugFrame.CurrentPosition = 0;
        debugFrame.PreviousFrame = s_DebuggerStack.empty() ? nullptr : s_DebuggerStack.top();
        debugFrame.NativeFunc = nullptr;
        debugFrame.NodePath = nodePathString.c_str();
        debugFrame.NodePathLength = static_cast<unsigned short>(nodePathString.length());

        // Look up file name from the shared map (populated by SetLinker_hook)
        FString nodeFullName = node->GetFullName();
        if (const auto pair = ObjFullNameToFileNameMap.find(nodeFullName); pair != ObjFullNameToFileNameMap.end())
        {
            debugFrame.FileName = pair->second.Chars();
            debugFrame.FileNameLength = static_cast<unsigned short>(pair->second.Length());
        }
        else
        {
            debugFrame.FileName = nullptr;
            debugFrame.FileNameLength = 0;
        }

        s_DebuggerStack.push(&debugFrame);

        if (s_StatusRequested.load())
        {
            SendMsgToLEX(L"Running", nullptr);
            s_StatusRequested.store(false);
        }

        BYTE buff[64];
        while (*Stack->Code != static_cast<BYTE>(OpCode::Return))
        {
            if (!s_PendingDetach.load())
            {
                const auto location = static_cast<unsigned short>(Stack->Code - beginOffset);
                debugFrame.CurrentPosition = location;
                if (ShouldBreak(nodePathString, location))
                {
                    BreakState();
                }
            }
            const int idx = *Stack->Code++;
            LocalGNatives[idx](Context, Stack, buff);
        }

        if (!s_PendingDetach.load())
        {
            const auto location = static_cast<unsigned short>(Stack->Code - beginOffset);
            debugFrame.CurrentPosition = location;
            if (ShouldBreak(nodePathString, location))
            {
                BreakState();
            }
        }

        Stack->Code++; // skip Return opcode
        const int idx = *Stack->Code++;
        LocalGNatives[idx](Context, Stack, Result); // execute statement that places return value in Result

        s_DebuggerStack.pop();
        if (s_SteppingMinStackDepth.load() < 1)
        {
            s_SteppingMinStackDepth.store(1);
        }
    }

    void AttachDebugger()
    {
        if (s_IsAttached.load() || s_ExecutionLoopAddress == nullptr || GNatives == nullptr)
        {
            return;
        }

        // Build the patch that calls our hook
        // LEA R9, [GNatives] - load the address of the native function array into 4th arg (RIP-relative)
        // MOV R8, RBP - Result pointer into 3rd arg
        // MOV RDX, RBX - FFrame pointer into 2nd arg
        // MOV RCX, RDI - this pointer into 1st arg
        // MOV R14, <ExecutionLoop_hook address>
        // CALL R14
        // NOPs to fill remaining space

        BYTE patch[] = {
            0x4c, 0x8d, 0x0d, 0x00, 0x00, 0x00, 0x00, // LEA R9, [rip+offset] - offset filled at runtime
            0x4C, 0x8B, 0xC5, //MOV R8, RBP //Move the Result pointer into the 3rd argument register
            0x48, 0x8B, 0xD3, //MOV RDX, RBX //Move the FFrame pointer into the 2nd argument register
            0x48, 0x89, 0xF9, //MOV RCX, RDI //Move the this pointer into the 1st argument register
            0x49, 0xBE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //MOV R14, 0xFFFFFFFFFFFFFFFF //put address of ExecutionLoop_hook into R14 (actual address filled in at runtime) 
            0x41, 0xFF, 0xD6,  //CALL R14 //Call ExecutionLoop_hook      
#if defined(SDK_TARGET_LE3)
            0x4c, 0x8b, 0xb4, 0x24, 0x80, 0x00, 0x00, 0x00, // MOV R14,qword ptr [RSP + 0x80] //restore R14
#endif
            //remaining bytes are NOPs of various sizes: https://stackoverflow.com/questions/43991155/what-does-nop-dword-ptr-raxrax-x64-assembly-instruction-do/50594130#50594130
            0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
#if defined(SDK_TARGET_LE3)
            0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00
#else
            0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x0F, 0x1F, 0x44, 0x00, 0x00
#endif
        };

        
        // Save original bytes for restoration
        s_OriginalExecutionLoopBytes.resize(sizeof(patch));
        memcpy(s_OriginalExecutionLoopBytes.data(), s_ExecutionLoopAddress, sizeof(patch));

        // Calculate the RIP-relative offset for LEA R9, [rip+offset]
        // LEA instruction is 7 bytes. RIP points to next instruction after LEA.
        // offset = target - (LEA_address + 7)
        auto leaAddress = reinterpret_cast<uintptr_t>(s_ExecutionLoopAddress);
        auto gnativesAddr = reinterpret_cast<uintptr_t>(GNatives);
        auto ripAfterLea = leaAddress + 7;
        int32_t gnativesOffset = static_cast<int32_t>(gnativesAddr - ripAfterLea);
        memcpy(patch + 3, &gnativesOffset, sizeof(gnativesOffset));

        // Place the absolute address of ExecutionLoop_hook into the patch (at offset 18)
        void* funcPtr = reinterpret_cast<void*>(ExecutionLoop_hook);
        memcpy(patch + 18, &funcPtr, sizeof(funcPtr));

        if (PatchMemory(s_ExecutionLoopAddress, patch, sizeof(patch)))
        {
            s_IsAttached.store(true);
            LEASI_INFO("ScriptDebugger attached successfully");
        }
        else
        {
            LEASI_ERROR("ScriptDebugger failed to attach - memory patch failed");
        }
    }

    void DetachDebugger()
    {
        if (!s_IsAttached.load() || s_ExecutionLoopAddress == nullptr || s_OriginalExecutionLoopBytes.empty())
        {
            return;
        }

        if (PatchMemory(s_ExecutionLoopAddress, s_OriginalExecutionLoopBytes.data(), s_OriginalExecutionLoopBytes.size()))
        {
            s_IsAttached.store(false);
            LEASI_INFO("ScriptDebugger detached successfully");
        }
        else
        {
            LEASI_ERROR("ScriptDebugger failed to detach - memory patch failed");
        }
    }

    bool IsAttached()
    {
        return s_IsAttached.load();
    }

    bool IsPendingAttach()
    {
        return s_PendingAttach.load();
    }

    bool IsPendingDetach()
    {
        return s_PendingDetach.load();
    }

    template<typename T>
    static T ReadFromBuffer(BYTE* ptr, int offset)
    {
        return *reinterpret_cast<T*>(ptr + offset);
    }

    static void ProcessDebuggerCommand(BYTE* buffer, DWORD len)
    {
        if (len == 0) return;

        auto cmd = static_cast<PipeCommand>(buffer[0]);

        switch (cmd)
        {
        case PipeCommand::AttachDebugger:
            s_PendingAttach.store(true);
            LEASI_INFO("ScriptDebugger: Attach requested");
            break;

        case PipeCommand::DetachDebugger:
            s_PendingDetach.store(true);
            s_Resume.store(true);
            LEASI_INFO("ScriptDebugger: Detach requested");
            break;

        case PipeCommand::Breakpoint:
        {
            auto op = static_cast<PipeCommand>(ReadFromBuffer<BYTE>(buffer, 1));
            auto breakOffset = ReadFromBuffer<unsigned short>(buffer, 2);
            auto functionPath = reinterpret_cast<char*>(buffer + 4);

            if (op == PipeCommand::Add)
            {
                s_BreakpointMap.AddBreakPoint(functionPath, breakOffset);
                LEASI_DEBUG("ScriptDebugger: Added breakpoint at {}:{}", functionPath, breakOffset);
            }
            else if (op == PipeCommand::Remove)
            {
                s_BreakpointMap.RemoveBreakPoint(functionPath, breakOffset);
                LEASI_DEBUG("ScriptDebugger: Removed breakpoint at {}:{}", functionPath, breakOffset);
            }
            break;
        }

        case PipeCommand::BreakImmediate:
            s_SteppingMinStackDepth.store(INT_MAX);
            s_IsStepping.store(true);
            LEASI_DEBUG("ScriptDebugger: Break immediate requested");
            break;

        case PipeCommand::StepInto:
            s_SteppingMinStackDepth.store(INT_MAX);
            s_IsStepping.store(true);
            s_Resume.store(true);
            break;

        case PipeCommand::StepOver:
            s_SteppingMinStackDepth.store(GetCurrentStackDepth());
            s_IsStepping.store(true);
            s_Resume.store(true);
            break;

        case PipeCommand::StepOut:
            s_SteppingMinStackDepth.store(GetCurrentStackDepth() - 1);
            s_IsStepping.store(true);
            s_Resume.store(true);
            break;

        case PipeCommand::Resume:
            s_Resume.store(true);
            break;

        case PipeCommand::ReSync:
            s_StatusRequested.store(true);
            break;

        default:
            LEASI_WARN("ScriptDebugger: Unknown command {}", static_cast<int>(cmd));
            break;
        }
    }

    static void DebuggerPipeThreadFunc()
    {
        s_DebuggerPipeRunning.store(true);

        constexpr DWORD BUFFER_SIZE = 1024;
        BYTE buffer[BUFFER_SIZE]{};
        DWORD bytesRead;

        HANDLE hPipe = CreateNamedPipeW(
            GetDebuggerPipeName(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,
            BUFFER_SIZE * 16,
            BUFFER_SIZE * 16,
            NMPWAIT_USE_DEFAULT_WAIT,
            nullptr);

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            LEASI_ERROR("ScriptDebugger: Failed to create named pipe: {}", GetLastError());
            s_DebuggerPipeRunning.store(false);
            return;
        }

        LEASI_INFO("ScriptDebugger: Named pipe created successfully");

        while (!s_DebuggerPipeStopRequested.load() && hPipe != INVALID_HANDLE_VALUE)
        {
            if (ConnectNamedPipe(hPipe, nullptr) != FALSE || GetLastError() == ERROR_PIPE_CONNECTED)
            {
                LEASI_TRACE("ScriptDebugger: Client connected to pipe");

                while (!s_DebuggerPipeStopRequested.load() &&
                       ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) != FALSE)
                {
                    buffer[bytesRead] = '\0';
                    ProcessDebuggerCommand(buffer, bytesRead);
                }

                LEASI_TRACE("ScriptDebugger: Client disconnected from pipe");
            }

            DisconnectNamedPipe(hPipe);
        }

        CloseHandle(hPipe);
        s_DebuggerPipeRunning.store(false);
    }

    // Called from GameEngineTick_hook to process pending attach/detach
    void ProcessPendingState()
    {
        if (s_PendingAttach.load())
        {
            s_PendingAttach.store(false);
            AttachDebugger();
            SendMsgToLEX(L"Attached");
        }
        else if (s_PendingDetach.load())
        {
            s_PendingDetach.store(false);
            s_Resume.store(false);
            s_BreakpointMap.ClearBreakPoints();
            DetachDebugger();
            SendMsgToLEX(L"Detached");
        }
    }

    void Initialize(::LESDK::Initializer& Init)
    {
#if defined(SDK_TARGET_LE1)
        constexpr auto executionLoopPattern = "4c 8d 35 5c f2 5a 01 80 38 04 74 30 0f 1f 80 00 00 00 00";
#elif defined(SDK_TARGET_LE2)
        constexpr auto executionLoopPattern = "4c 8d 35 1c 08 5d 01 80 38 04 74 30 0f 1f 80 00 00 00 00";
#elif defined(SDK_TARGET_LE3)
        constexpr auto executionLoopPattern = "4c 8d 35 14 f8 6f 01 80 38 04 74 29 48 8b 43 28";
#endif

        s_ExecutionLoopAddress = Init.Resolve(::LESDK::Address::FromPattern(executionLoopPattern));
        if (s_ExecutionLoopAddress)
        {
            LEASI_INFO("ScriptDebugger: ExecutionLoop found at {}", s_ExecutionLoopAddress);
        }
        else
        {
            LEASI_ERROR("ScriptDebugger: Failed to find ExecutionLoop pattern");
        }

        // Start the debugger pipe thread
        s_DebuggerPipeStopRequested.store(false);
        s_DebuggerPipeThread = std::thread(DebuggerPipeThreadFunc);
        s_DebuggerPipeThread.detach();

        LEASI_INFO("ScriptDebugger initialized");
    }
}
