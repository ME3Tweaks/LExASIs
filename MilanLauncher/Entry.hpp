#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include <SPI.h>
#include <string>

// Typedefs
//using tRegisterSWFCommand = void* (void* param_1, void* param_2, int unknown, char* cmdName, byte param_5, void* funcPtr);
using tInvokeActionScript = bool(void* pInterface, const char* fullMethodName, int alwaysZero, void* gfxValueParam, int conversionType);
using tSingleIntBool = bool(int param);
using tLog1 = void(char* param_1, char* param_2, char* param_3, char* param_4);
using tLogScriptError = void(char* param_1, char* param_2, char* param_3, char* param_4);
using tCreateString = void*(void*, char*, size_t);



extern tCreateString* CreateString_orig;
extern tLog1* Log1_orig;
extern tLogScriptError* LogScriptError_orig;
extern tSingleIntBool* IsResumeCheck_orig;

// RVAs
#define Log1_RVA                        LEASI_RVA(0x2a8a30)
#define LogScriptError_RVA              LEASI_RVA(0x46c720)
#define IsResumeCheck_RVA               LEASI_RVA(0x2a73d0)
#define CreateString_RVA                LEASI_RVA(0x29a0d0)
#define InternalInvokeActionScript_RVA  LEASI_RVA(0x2c7ba0)
#define LauncherApp_RVA                 LEASI_RVA(0xd530d8)

namespace MilanLauncher
{
    void InitializeHooks(::LESDK::Initializer& Init);
    void* ComputeAddress(void* base, long long offset);
    void** GetGLauncherApp();
    void InvokeActionScriptMethod(std::string fullMethodName);
    void SetGameNum(int num);
    void Log1_hook(char* baseStr, char* param_2, const char* param_3, char* param_4);
    void LogScriptError_hook(char* baseStr, char* param_2, const char* param_3, char* param_4);
    bool IsResumeCheck_hook(int gameNum);
    void CheckMPInstalled();
    void* CreateString_hook(void* storage, char* data, size_t dataLen);
}
