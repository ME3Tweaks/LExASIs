#pragma once

#include <string>
#include <atomic>
#include <unordered_map>
#include <shared_mutex>
#include <Windows.h>
#include <LESDK/Headers.hpp>
#include "ActionQueue.hpp"
#include "Utilities.hpp"
#include "Common/Base.hpp"
#include "Common/Memory.hpp"

namespace LEXInterop
{
    // Thread-safe reference counter for synchronizing access to precached data
    #pragma pack(push, 4)
    class ThreadSafeCounter
    {
    public:
        ThreadSafeCounter() : Counter(0) {}

        void Increment()
        {
            InterlockedIncrement((LPLONG)&Counter);
        }

        void Decrement()
        {
            InterlockedDecrement((LPLONG)&Counter);
        }

        ThreadSafeCounter(const ThreadSafeCounter& Other) = delete;
        void operator=(const ThreadSafeCounter & Other) = delete;

    private:
        volatile int Counter;
    };

    // Information about a precached package
    struct FPackagePreCacheInfo
    {
        ThreadSafeCounter* SynchronizationObject;
        void* PackageData;
        int PackageDataSize;

        FPackagePreCacheInfo() : SynchronizationObject(nullptr), PackageData(nullptr), PackageDataSize(0) {}

        ~FPackagePreCacheInfo()
        {
            if (SynchronizationObject)
            {
                sdkFree(SynchronizationObject);
            }
        }
    };
#pragma pack(pop)

    inline TMap<FString, FPackagePreCacheInfo>* PackagePrecacheMap = nullptr;

    // Action to add package data to the precache map (executed on game thread)
    struct AddToPrecacheMapAction final : ActionBase
    {
        FString FileName;
        void* PackageData;
        int PackageSize;

        explicit AddToPrecacheMapAction(FString fileName, void* packageData, int packageSize)
            : FileName(std::move(fileName))
            , PackageData(packageData)
            , PackageSize(packageSize)
        {
        }

        void Execute() override
        {
            FPackagePreCacheInfo* existingInfo = PackagePrecacheMap->Find(std::move(FileName));
            if (existingInfo)
            {
                // Entry exists - update it
                existingInfo->SynchronizationObject->Increment();

                // Free old package data
                if (existingInfo->PackageData)
                {
                    sdkFree(existingInfo->PackageData);
                }

                existingInfo->PackageData = PackageData;
                existingInfo->PackageDataSize = PackageSize;
                existingInfo->SynchronizationObject->Decrement();

                LEASI_DEBUG(L"Updated precached package: {}", FileName);
            }
            else
            {
                // Create new entry
                FPackagePreCacheInfo blank;
                auto precacheInfo = PackagePrecacheMap->Set(FileName, blank);

                precacheInfo->SynchronizationObject = new(sdkMalloc(4)) ThreadSafeCounter;
                precacheInfo->SynchronizationObject->Increment();

                precacheInfo->PackageData = PackageData;
                precacheInfo->PackageDataSize = PackageSize;

                precacheInfo->SynchronizationObject->Decrement();

                LEASI_DEBUG(L"Added precached package: {} ({} bytes)", FileName, PackageSize);
            }
        }
    };

    // Handler for file loading commands
    class FileLoader
    {
    public:

        static bool HandleCommand(char* command, DWORD bytesRead, HANDLE pipe)
        {
            const char* start = command;

            if (IsCmd(&command, "PRECACHE_FILE "))
            {
                // Parse: PRECACHE_FILE <filename> <size>\0<binary data>
                const char* fileName = strtok_s(command, " ", &command);
                if (!fileName)
                {
                    LEASI_WARN("PRECACHE_FILE: missing filename");
                    return true;
                }

                const auto packageSize = strtol(command, &command, 10);
                if (packageSize <= 0)
                {
                    LEASI_WARN("PRECACHE_FILE: invalid package size");
                    return true;
                }

                // Move past the null terminator after the size to reach the binary data
                command += strlen(command) + 1;

                // Allocate package data using game allocator
                void* packageData = sdkMalloc(static_cast<DWORD>(packageSize));
                if (!packageData)
                {
                    LEASI_ERROR("PRECACHE_FILE: failed to allocate {} bytes", packageSize);
                    return true;
                }

                const auto remainingBytesInBuffer = bytesRead - static_cast<DWORD>(command - start);

                if (static_cast<long>(remainingBytesInBuffer) >= packageSize)
                {
                    // All data is already in the buffer
                    memcpy(packageData, command, packageSize);
                }
                else
                {
                    // Need to read more data from pipe
                    auto dataPtr = static_cast<BYTE*>(packageData);
                    const auto endPtr = dataPtr + packageSize;

                    // Copy what we have
                    memcpy(dataPtr, command, remainingBytesInBuffer);
                    dataPtr += remainingBytesInBuffer;

                    // Read the rest
                    while (dataPtr < endPtr)
                    {
                        DWORD readBytes = 0;
                        if (!ReadFile(pipe, dataPtr, static_cast<DWORD>(endPtr - dataPtr), &readBytes, nullptr))
                        {
                            LEASI_ERROR("PRECACHE_FILE: failed to read package data");
                            sdkFree(packageData);
                            return true;
                        }
                        dataPtr += readBytes;
                    }
                }

                LEASI_INFO("Precaching file: {} ({} bytes)", fileName, packageSize);
                QueueAction(new AddToPrecacheMapAction(FString::Printf(L"%S", fileName), packageData, packageSize));
                return true;
            }

            return false;
        }

        // Checks if package is in Precache map
        static bool FindPrecachedPackage(const wchar_t* packageName)
        {
            if (PackagePrecacheMap->Find(FString(packageName)))
            {
                return true;
            }

            return false;
        }

        static void InitializePackagePrecacheMap(::LESDK::Initializer& Init)
        {
            auto precacheMapAddr = ::LESDK::Address::FromInstrRelative(
#if defined(SDK_TARGET_LE1) 
                "48 8d 0d e8 15 5b 01 e8 fb fa ff ff"
#elif defined(SDK_TARGET_LE2)
                "48 8d 0d 88 35 5d 01 e8 fb fa ff ff"
#elif defined(SDK_TARGET_LE3)
                "48 8d 0d c8 24 70 01 e8 fb fa ff ff"
#endif
            );

            PackagePrecacheMap = Init.ResolveTyped<TMap<FString, FPackagePreCacheInfo>>(precacheMapAddr);
            CHECK_RESOLVED(PackagePrecacheMap);
        }

        FileLoader() = delete;
    };
}
