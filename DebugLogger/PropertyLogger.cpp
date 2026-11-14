#include "PropertyLogger.hpp"
#include <functional>

namespace DebugLogger {

#define SCRIPTOBJECTFULLPATH(objPtr) (objPtr ? objPtr->GetFullPath().Chars() : L"None")
#define PRINTALLELEMENTS(type, printStmnt) if (prop->ArrayDim > 1) \
											{ \
												logFile << "[" << prop->ArrayDim << "] : ["; \
												for (int elemIdx = 0; elemIdx < prop->ArrayDim; ++elemIdx) \
												{ \
													const type& value = *reinterpret_cast<const type*>(propAddr + elemIdx * prop->ElementSize); \
													if (elemIdx > 0) \
													{ \
														logFile << ", "; \
													} \
													logFile << printStmnt; \
												} \
												logFile << "]\n"; \
											} \
											else \
											{ \
												const type& value = *reinterpret_cast<const type*>(propAddr); \
												logFile << ": " << printStmnt << "\n"; \
											}

	// Sanity check
	static constexpr UINT k_maxSaneArrayCount = 1u << 20;
	// Max entries to print so we don't print huge list
	static constexpr UINT k_maxPrintedArrayElements = 64;

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

			// Reset per loop
			BYTE* propAddr = nullptr;
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
					auto& array = *reinterpret_cast<TArrayView<BYTE>*>(propAddr);
					auto array_property = static_cast<UArrayProperty*>(prop);
					auto innerProp = array_property->Inner;
					auto innerClass = innerProp->Class;
					const UINT count = array.Count();
					if (count > array.Capacity() || count > k_maxSaneArrayCount || (count > 0 && array.GetData() == nullptr))
					{
						logFile << ": [invalid array header - Count " << count << ", Max " << array.Capacity() << "]\n";
						return;
					}
					const UINT printCount = count < k_maxPrintedArrayElements ? count : k_maxPrintedArrayElements;
					logFile << ": " << count << " Elements [";
					BYTE* elemAddr = array.GetData();
					if (innerClass == UStructProperty::StaticClass())
					{
						auto uStruct = static_cast<UStructProperty*>(innerProp)->Struct;
						logFile << " ( StructType: " << uStruct->GetName() << ") [\n";
						for (UINT i = 0; i < printCount; ++i)
						{
							if (i > 0)
							{
								indent();
								logFile << ",\n";
							}
							PrintPropertyValues(elemAddr, uStruct);
							elemAddr += innerProp->ElementSize;
						}
						if (printCount < count)
						{
							indent();
							logFile << "... " << (count - printCount) << " more\n";
						}
						indent();
						logFile << "]\n";
					}
					else
					{
						for (UINT i = 0; i < printCount; ++i)
						{
							if (i > 0)
							{
								logFile << ", ";
							}
							if (innerClass == UIntProperty::StaticClass())
							{
								logFile << *reinterpret_cast<const int*>(elemAddr);
							}
							else if (innerClass == UFloatProperty::StaticClass())
							{
								logFile << *reinterpret_cast<const float*>(elemAddr);
							}
							else if (innerClass == UByteProperty::StaticClass())
							{
								logFile << static_cast<int>(*reinterpret_cast<const BYTE*>(elemAddr));
							}
							else if (innerClass == UBoolProperty::StaticClass())
							{
								logFile << (*reinterpret_cast<const unsigned*>(elemAddr) ? "True" : "False");
							}
							else if (innerClass == UNameProperty::StaticClass())
							{
								logFile << reinterpret_cast<const SFXName*>(elemAddr)->Instanced();
							}
							else if (innerClass == UStrProperty::StaticClass())
							{
								logFile << "\"" << *reinterpret_cast<const FString*>(elemAddr) << "\""; // Used to have a null check here
							}
							else if (innerClass == UStringRefProperty::StaticClass())
							{
								logFile << "$" << *reinterpret_cast<const int*>(elemAddr);
							}
							else if (innerClass == UDelegateProperty::StaticClass())
							{
								const auto& value = *reinterpret_cast<const FScriptDelegate*>(elemAddr);
								logFile << "(Function Name:" << value.FunctionName.ToString(SFXName::FormatMode::k_formatInstanced) << ", Object: " << SCRIPTOBJECTFULLPATH(value.Object) << ")";
							}
							else if (innerClass == UInterfaceProperty::StaticClass())
							{
								const auto& value = *reinterpret_cast<const FScriptInterface*>(elemAddr);
								logFile << SCRIPTOBJECTFULLPATH(value.Object);
							}
							else if (innerProp->IsA(UObjectProperty::StaticClass()))
							{
								UObject* value = *reinterpret_cast<UObject* const*>(elemAddr);
								logFile << SCRIPTOBJECTFULLPATH(value);
							}
							elemAddr += innerProp->ElementSize;
						}
						if (printCount < count)
						{
							logFile << ", ... " << (count - printCount) << " more";
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
					if (prop->ArrayDim > 1)
					{
						logFile << "[" << prop->ArrayDim << "] : ( StructType: " << uStruct->GetName() << ") [\n";
						BYTE* elemAddr = propAddr;
						for (int i = 0; i < prop->ArrayDim; ++i)
						{
							if (i > 0)
							{
								indent();
								logFile << ",\n";
							}
							PrintPropertyValues(elemAddr, uStruct);
							elemAddr += prop->ElementSize;
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
		}
		DecreaseIndent();
	}
}
