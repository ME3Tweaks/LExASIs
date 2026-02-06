#include "PropertyLogger.hpp"
#include <functional>

namespace DebugLogger {

#define SCRIPTOBJECTFULLPATH(objPtr) (objPtr ? objPtr->GetFullPath().Chars() : L"None")
#define PRINTALLELEMENTS(type, printStmnt) auto value = *reinterpret_cast<type*>(propAddr); \
											if (prop->ArrayDim > 1) \
											{ \
												logFile << "[" << prop->ArrayDim << "] : [" << printStmnt; \
												for (int i = prop->ArrayDim - 1; i > 0; --i) \
												{ \
													propAddr += prop->ElementSize; \
													value = *reinterpret_cast<type*>(propAddr); \
													logFile << ", " << printStmnt; \
												} \
												logFile << "]\n"; \
											} \
											else \
											{ \
												logFile << ": " << printStmnt << "\n"; \
											} \

	void PropertyLogger::IncreaseIndent()
	{
		numSpacesIndent += 4;
	}

	void PropertyLogger::DecreaseIndent()
	{
		numSpacesIndent -= 4;
		if (numSpacesIndent < 0)
		{
			numSpacesIndent = 0;
		}
	}

	void PropertyLogger::indent()
	{
		for (int i = 0; i < numSpacesIndent; ++i)
		{
			logFile << ' ';
		}
	}

	std::wstring PropertyLogger::GetString() const
	{
		return logFile.str();
	}

	// Helper function to safely execute property logging with SEH
	static bool TryExecutePropertyLogging(const std::function<void()>& func)
	{
		__try
		{
			func();
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	void PropertyLogger::PrintPropertyValues(BYTE* propsOffset, UStruct* const node, FFrame::FOutParmRec* outParmInfo)
	{
		BYTE* propAddr = nullptr;
		IncreaseIndent();
		for (auto curChild = node->Children; curChild; curChild = curChild->Next)
		{
			if (!curChild->IsA(UProperty::StaticClass()))
			{
				continue;
			}

			auto prop = static_cast<UProperty*>(curChild);
			if (prop->PropertyFlags & 0x400) //ReturnValue flag
			{
				continue;
			}
			if (prop->PropertyFlags & 0x100) //OutParm flag
			{
				for (auto curOutParm = outParmInfo; curOutParm; curOutParm = curOutParm->NextOutParm)
				{
					if (curOutParm->Property == prop)
					{
						propAddr = curOutParm->PropAddr;
						break;
					}
				}
			}
			else
			{
				propAddr = propsOffset + prop->Offset;
			}

			if (propAddr == nullptr)
			{
				continue;
			}

			auto propClass = prop->Class;

			auto propName = prop->GetName(); // Should be instanced?
			indent();
			logFile << propName;
			// Wrap property reading in SEH to handle corrupt data
			bool success = TryExecutePropertyLogging([&]() {
				if (propClass == UIntProperty::StaticClass())
				{
					PRINTALLELEMENTS(int, value)
				}
				else if (propClass == UFloatProperty::StaticClass())
				{
					PRINTALLELEMENTS(float, value)
				}
				else if (propClass == UByteProperty::StaticClass())
				{
					PRINTALLELEMENTS(BYTE, static_cast<int>(value))
				}
				else if (propClass == UBoolProperty::StaticClass())
				{
					auto boolProp = static_cast<UBoolProperty*>(prop);
					PRINTALLELEMENTS(DWORD, (value & boolProp->BitMask ? "True" : "False"))
				}
				else if (propClass == UNameProperty::StaticClass())
				{
					PRINTALLELEMENTS(SFXName, value.Instanced())
				}
				else if (propClass == UStrProperty::StaticClass())
				{
					PRINTALLELEMENTS(FString, "\"" << value << "\"") // Used to have an ull check
				}
				else if (propClass == UStringRefProperty::StaticClass())
				{
					PRINTALLELEMENTS(int, "$" << value)
				}
				else if (propClass == UArrayProperty::StaticClass())
				{
					auto array = *reinterpret_cast<TArray<BYTE>*>(propAddr);
					auto array_property = static_cast<UArrayProperty*>(prop);
					prop = array_property->Inner;
					propClass = prop->Class;
					indent();
					logFile << ": " << array.Count() << " Elements ";
					logFile << "[";
					propAddr = array.GetData();
					if (propClass == UStructProperty::StaticClass())
					{
						auto uStruct = static_cast<UStructProperty*>(prop)->Struct;
						logFile << " : ( StructType: " << uStruct->GetName() << ") [\n";
						for (UINT i = 0; i < array.Count(); ++i)
						{
							if (i > 0)
							{
								indent();
								logFile << ",\n";
							}
							PrintPropertyValues(propAddr, uStruct);
							propAddr += prop->ElementSize;
						}
						indent();
						logFile << "]\n";
					}
					else
					{
						for (UINT i = 0; i < array.Count(); ++i)
						{
							if (i > 0)
							{
								logFile << ", ";
							}
							if (propClass == UIntProperty::StaticClass())
							{
								auto value = *reinterpret_cast<int*>(propAddr);
								logFile << value;
							}
							else if (propClass == UFloatProperty::StaticClass())
							{
								auto value = *reinterpret_cast<float*>(propAddr);
								logFile << value;
							}
							else if (propClass == UByteProperty::StaticClass())
							{
								auto value = *reinterpret_cast<BYTE*>(propAddr);
								logFile << static_cast<int>(value);
							}
							else if (propClass == UBoolProperty::StaticClass())
							{
								auto value = *reinterpret_cast<unsigned*>(propAddr);
								logFile << (value ? "True" : "False");
							}
							else if (propClass == UNameProperty::StaticClass())
							{
								auto value = *reinterpret_cast<SFXName*>(propAddr);
								logFile << value.Instanced();
							}
							else if (propClass == UStrProperty::StaticClass())
							{
								auto value = *reinterpret_cast<FString*>(propAddr);
								logFile << "\"" << value << "\""; // Used to have a null check here
							}
							else if (propClass == UStringRefProperty::StaticClass())
							{
								auto value = *reinterpret_cast<int*>(propAddr);
								logFile << "$" << value;
							}
							else if (propClass == UDelegateProperty::StaticClass())
							{
								auto value = *reinterpret_cast<FScriptDelegate*>(propAddr);
								logFile << "(Function Name:" << value.FunctionName.ToString(SFXName::FormatMode::k_formatInstanced) << ", Object: " << SCRIPTOBJECTFULLPATH(value.Object) << ")";
							}
							else if (propClass == UInterfaceProperty::StaticClass())
							{
								auto value = *reinterpret_cast<FScriptInterface*>(propAddr);
								logFile << SCRIPTOBJECTFULLPATH(value.Object);
							}
							else if (prop->IsA(UObjectProperty::StaticClass()))
							{
								auto value = *reinterpret_cast<UObject**>(propAddr);
								logFile << SCRIPTOBJECTFULLPATH(value);
							}
							propAddr += prop->ElementSize;
						}
						logFile << "]\n";
					}
				}
				else if (propClass == UDelegateProperty::StaticClass())
				{
					PRINTALLELEMENTS(FScriptDelegate, "(Function Name : " << value.FunctionName.Instanced() << ", Object : " << SCRIPTOBJECTFULLPATH(value.Object) << ")")
				}
				else if (propClass == UInterfaceProperty::StaticClass())
				{
					PRINTALLELEMENTS(FScriptInterface, SCRIPTOBJECTFULLPATH(value.Object))
				}
				else if (propClass == UStructProperty::StaticClass())
				{
					auto uStruct = static_cast<UStructProperty*>(prop)->Struct;
					// ``auto value = reinterpret_cast<UStructProperty*>(propAddr);
					if (prop->ArrayDim > 1)
					{
						logFile << "[" << prop->ArrayDim << "] : ( StructType: " << uStruct->GetName() << ") [\n";
						PrintPropertyValues(propAddr, uStruct);
						for (int i = prop->ArrayDim - 1; i > 0; --i)
						{
							indent();
							logFile << ",\n";
							propAddr += prop->ElementSize;
							PrintPropertyValues(propAddr, uStruct);
						}
						indent();
						logFile << "]\n";
					}
					else
					{
						logFile << "( StructType: " << uStruct->GetName() << ")\n";
						PrintPropertyValues(propAddr, uStruct);
					}
				}
				else if (prop->IsA(UObjectProperty::StaticClass()))
				{
					PRINTALLELEMENTS(UObject*, SCRIPTOBJECTFULLPATH(value))
				}
				});
			if (!success)
			{
				logFile << " [ERROR: Access violation - corrupt or invalid memory, may be issue in ASI and not the game]\n";
			}
			DecreaseIndent();
		}
	}
}