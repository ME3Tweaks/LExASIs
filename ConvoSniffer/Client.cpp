#include <Windows.h>
#include <wininet.h>
#include <simdutf.h>
#include "ConvoSniffer/Client.hpp"

namespace ConvoSniffer
{
    SnifferClient* gp_snifferClient = nullptr;


    // ! HttpClient implementation.
    // ========================================

    HttpClient::HttpClient(char const* const InHost, int const InPort)
        : Host{}
        , Port{ InPort }
        , RequestQueue{}
        , ProcessThread{}
        , bWantsExit{}
    {
        LEASI_CHECKW(InHost != nullptr, L"", L"");
        Host.AppendAnsi(InHost);

        Win32Internet = InternetOpenW(L"ConvoSniffer-LE1.asi", 0u, NULL, NULL, 0u);
        LEASI_CHECKW(Win32Internet != NULL, L"failed to initialize internet handle: {}", GetLastError());

        Win32Session = InternetConnectW(Win32Internet, *Host, (uint16_t)Port, NULL, NULL, INTERNET_SERVICE_HTTP, 0u, NULL);
        LEASI_CHECKW(Win32Session != NULL, L"failed to initialize http session: {}", GetLastError());

        ProcessThread = std::thread([this]() -> void
            {
                while (!bWantsExit.test())
                {
                    std::unique_lock QueueLock(QueueMutex);
                    RequestCondition.wait(QueueLock);

                    if (!RequestQueue.empty())
                    {
                        Request const& HttpRequest = RequestQueue.front();
                        LEASI_INFO(L"request: method = {}, path = {}", HttpRequest.Method, *HttpRequest.Path);

                        Response HttpResponse{};
                        if (SendHttp(HttpRequest, &HttpResponse))
                            LEASI_INFO(L"response: status = {}, text = {}", HttpResponse.Status, *HttpResponse.Body);

                        RequestQueue.pop_front();
                    }
                }
            });
    }

    HttpClient::~HttpClient() noexcept
    {
        bWantsExit.test_and_set();
        RequestCondition.notify_all();

        if (InternetCloseHandle(Win32Session) == FALSE)
        {
            // TODO: Log last error code...
        }

        if (InternetCloseHandle(Win32Internet) == FALSE)
        {
            // TODO: Log last error code...
        }
    }

    void HttpClient::QueueRequest(wchar_t const* const InMethod, wchar_t const* const InPath, FString&& InBody)
    {
        LEASI_CHECKW(InMethod != nullptr, L"", L"");
        LEASI_CHECKW(InPath != nullptr, L"", L"");

        if (!bWantsExit.test())
        {
            std::unique_lock QueueLock(QueueMutex);
            RequestQueue.push_back(Request{ InMethod, FString(InPath), std::move(InBody) });

            QueueLock.unlock();
            RequestCondition.notify_all();
        }
    }

    bool HttpClient::SendHttp(Request const& InRequest, Response* const OutResponse) const
    {
        bool bSuccess = true;

        HINTERNET const Request = HttpOpenRequestW(
            Win32Session,
            InRequest.Method,
            *InRequest.Path,
            L"HTTP/1.1",
            NULL,
            NULL,
            0u,
            NULL
        );

        if (Request == NULL)
        {
            LEASI_ERROR(L"failed to initialize http request ('{}' {}): {}",
                InRequest.Method, *InRequest.Path, GetLastError());
            return false;
        }

        std::string BodyUtf8{};
        if (!StringToUtf8(InRequest.Body, BodyUtf8))
        {
            LEASI_ERROR(L"failed to convert http request body to utf-8 ('{}' {})",
                InRequest.Method, *InRequest.Path);
            bSuccess = false;
        }

        if (bSuccess && HttpSendRequestW(Request, NULL, 0u, BodyUtf8.data(), (DWORD)BodyUtf8.length()) == FALSE)
        {
            LEASI_ERROR(L"failed to send http request ('{}' {}): {}",
                InRequest.Method, *InRequest.Path, GetLastError());
            bSuccess = false;
        }

        if (bSuccess && OutResponse != nullptr)
        {
            DWORD CodeBufferSize = sizeof(OutResponse->Status);

            if (!HttpQueryInfoW(Request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                &OutResponse->Status, &CodeBufferSize, 0u))
            {
                LEASI_BREAK_SAFE();
                bSuccess = false;
            }

            FString TextBuffer{};
            DWORD TextBufferSize = 1024;
            TextBuffer.Assign(L'\0', TextBufferSize);

            if (!HttpQueryInfoW(Request, HTTP_QUERY_STATUS_TEXT,
                (WCHAR*)TextBuffer.Chars(), &TextBufferSize, 0u))
            {
                LEASI_BREAK_SAFE();
                bSuccess = false;
            }

            OutResponse->Body.Clear();
            OutResponse->Body.Append(TextBuffer.Chars(), TextBuffer.Chars() + TextBufferSize / sizeof(WCHAR));
        }

        if (InternetCloseHandle(Request) == FALSE)
        {
            LEASI_ERROR(L"failed to close http request ('{}' {}): {}",
                InRequest.Method, *InRequest.Path, GetLastError());
        }

        return bSuccess;
    }

    bool HttpClient::StringToUtf8(FString const& InString, std::string& OutString)
    {
        auto const Chars = reinterpret_cast<char16_t const*>(InString.Chars());
        auto const Length = static_cast<std::size_t>(InString.Length());

        if (!simdutf::validate_utf16le(Chars, Length))
            return false;

        auto const OutLength = simdutf::utf8_length_from_utf16le(Chars, Length);
        OutString.resize(OutLength);

        // We've already validated input string...
        simdutf::convert_utf16le_to_utf8(Chars, Length, OutString.data());

        return true;
    }

    bool HttpClient::StringFromUtf8(FString& OutString, std::string const& InString)
    {
        auto const Chars = InString.c_str();
        auto const Length = InString.length();

        if (!simdutf::validate_utf8(Chars, Length))
            return false;

        auto const OutLength = simdutf::utf16_length_from_utf8(Chars, Length);
        OutString.Assign(L'\0', (DWORD)OutLength);

        // We've already validated input string...
        simdutf::convert_utf8_to_utf16le(Chars, Length, (char16_t*)OutString.Chars());

        return true;
    }


    // ! SnifferClient implementation.
    // ========================================

    SnifferClient::SnifferClient(char const* const InHost, int const InPort)
        : Http{ InHost, InPort }
        , Conversation{}
    {
        // ...
    }

    void SnifferClient::OnStartConversation(UBioConversation* const InConversation)
    {
        if (Conversation == nullptr)
        {
            Conversation = InConversation;
            Http.QueueRequest(L"POST", L"/conversation", FString());
        }
        else
        {
            LEASI_ERROR("a conversation is already in-progress");
            LEASI_BREAK_SAFE();
        }
    }

    void SnifferClient::OnEndConversation(UBioConversation* const InConversation)
    {
        if (Conversation == InConversation)
        {
            Http.QueueRequest(L"DELETE", L"/conversation", FString());
            Conversation = nullptr;
        }
        else if (Conversation != nullptr)
        {
            LEASI_ERROR("conversation {:p} is in flight but {:p} is to end", (void*)Conversation, (void*)InConversation);
            LEASI_BREAK_SAFE();
        }
        else
        {
            LEASI_ERROR("no conversations are in-progress");
            LEASI_BREAK_SAFE();
        }
    }

    void SnifferClient::OnQueueReply(UBioConversation* const InConversation, int const Reply)
    {
        LEASI_UNUSED(InConversation);
        LEASI_UNUSED(Reply);
    }

    void SnifferClient::OnUpdateConversation(UBioConversation* const InConversation)
    {
        if (Conversation == InConversation)
        {
            // ...
        }
        else
        {
            LEASI_ERROR("conversation {:p} is in flight but {:p} is to update", (void*)Conversation, (void*)InConversation);
            LEASI_BREAK_SAFE();
        }
    }

    // ...
}
