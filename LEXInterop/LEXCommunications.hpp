#pragma once

#include <sstream>
#include <LESDK/Headers.hpp>
#include "Utilities.hpp"

namespace LEXInterop
{
    // Handles sending messages from kismet to LEX via SeqAct_SendMessageToLEX
    class LEXCommunications
    {
    public:
        // Process kismet message sending.
        // Returns false if this handler consumed the event, true otherwise.
        static bool ProcessEvent(UObject* Context, UFunction* Function, void* Parms, void* Result)
        {
            LEASI_UNUSED_2(Parms, Result);

            auto className = Context->Class->GetName();
            if (className.Equals(L"SeqAct_SendMessageToLEX", true))
            {
                if (Function->GetFullName().Equals(L"Function Engine.SequenceOp.Activated", true))
                {
                    auto op = static_cast<USequenceOp*>(Context);
                    SendMessageToLEX(op);
                    return false; // Event consumed
                }
            }
            return true; // Allow other handlers
        }

    private:
        static void SendMessageToLEX(USequenceOp* op)
        {
            std::wstringstream wss;

            for (auto&& varLink : op->VariableLinks)
            {
                for (auto&& seqVar : varLink.LinkedVariables)
                {
                    if (!seqVar) continue;

                    // MessageName - the primary message identifier
                    if (varLink.LinkDesc.StartsWith(L"MessageName", true) &&
                        seqVar->IsA(USeqVar_String::StaticClass()))
                    {
                        const auto strVar = static_cast<USeqVar_String*>(seqVar);
                        wss << strVar->StrValue.Chars();
                    }
                    // String parameter
                    else if (varLink.LinkDesc.StartsWith(L"String", true) &&
                             seqVar->IsA(USeqVar_String::StaticClass()))
                    {
                        const auto strVar = static_cast<USeqVar_String*>(seqVar);
                        wss << L" string " << strVar->StrValue.Chars();
                    }
                    // Vector parameter
                    else if (varLink.LinkDesc.StartsWith(L"Vector", true) &&
                             seqVar->IsA(USeqVar_Vector::StaticClass()))
                    {
                        const auto vectorVar = static_cast<USeqVar_Vector*>(seqVar);
                        wss << L" vector " << vectorVar->VectValue.X << L" "
                            << vectorVar->VectValue.Y << L" " << vectorVar->VectValue.Z;
                    }
                    // Float parameter
                    else if (varLink.LinkDesc.StartsWith(L"Float", true) &&
                             seqVar->IsA(USeqVar_Float::StaticClass()))
                    {
                        const auto floatVar = static_cast<USeqVar_Float*>(seqVar);
                        wss << L" float " << floatVar->FloatValue;
                    }
                    // Int parameter
                    else if (varLink.LinkDesc.StartsWith(L"Int", true) &&
                             seqVar->IsA(USeqVar_Int::StaticClass()))
                    {
                        const auto intVar = static_cast<USeqVar_Int*>(seqVar);
                        wss << L" int " << intVar->IntValue;
                    }
                    // Bool parameter
                    else if (varLink.LinkDesc.StartsWith(L"Bool", true) &&
                             seqVar->IsA(USeqVar_Bool::StaticClass()))
                    {
                        const auto boolVar = static_cast<USeqVar_Bool*>(seqVar);
                        wss << L" bool " << boolVar->bValue;
                    }
                }
            }

            const std::wstring msg = wss.str();
            SendStringToLEX(msg);
        }
    };
}
