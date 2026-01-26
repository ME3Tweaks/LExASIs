#include "PropertyLogger.hpp"

namespace DebugLogger {

#define SCRIPTOBJECTFULLPATH(objPtr) (objPtr ? objPtr->GetFullPath().Chars() : L"None")
#define PRINTALLELEMENTS(type, printStmnt) auto value = *reinterpret_cast<type*>(propAddr); \
											if (prop->ArrayDim > 1) \
											{ \
												out() << "[" << prop->ArrayDim << "] : [" << printStmnt; \
												for (int i = prop->ArrayDim - 1; i > 0; --i) \
												{ \
													propAddr += prop->ElementSize; \
													value = *reinterpret_cast<type*>(propAddr); \
													out() << ", " << printStmnt; \
												} \
												out() << "]\n"; \
											} \
											else \
											{ \
												out() << ": " << printStmnt << "\n"; \
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

	std::wstringstream&& PropertyLogger::indent()
	{
		for (int i = 0; i < numSpacesIndent; ++i)
		{
			logFile << ' ';
		}
		return std::move(logFile);
	}

	std::wstringstream&& PropertyLogger::out()
	{
		return std::move(logFile);
	}

	std::wstring PropertyLogger::GetString() const
	{
		return logFile.str();
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
			indent() << propName.Chars();
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
				PRINTALLELEMENTS(SFXName, value.Instanced().Chars())
			}
			else if (propClass == UStrProperty::StaticClass())
			{
				PRINTALLELEMENTS(FString, "\"" << value.Chars() << "\"") // Used to have an ull check
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
				indent() << ": " << array.Count() << " Elements ";
				out() << "[";
				propAddr = array.GetData();
				if (propClass == UStructProperty::StaticClass())
				{
					auto uStruct = static_cast<UStructProperty*>(prop)->Struct;
					out() << " : ( StructType: " << uStruct->GetName().Chars() << ") [\n";
					for (UINT i = 0; i < array.Count(); ++i)
					{
						if (i > 0)
						{
							indent() << ",\n";
						}
						PrintPropertyValues(propAddr, uStruct);
						propAddr += prop->ElementSize;
					}
					indent() << "]\n";
				}
				else
				{
					for (UINT i = 0; i < array.Count(); ++i)
					{
						if (i > 0)
						{
							out() << ", ";
						}
						if (propClass == UIntProperty::StaticClass())
						{
							auto value = *reinterpret_cast<int*>(propAddr);
							out() << value;
						}
						else if (propClass == UFloatProperty::StaticClass())
						{
							auto value = *reinterpret_cast<float*>(propAddr);
							out() << value;
						}
						else if (propClass == UByteProperty::StaticClass())
						{
							auto value = *reinterpret_cast<BYTE*>(propAddr);
							out() << static_cast<int>(value);
						}
						else if (propClass == UBoolProperty::StaticClass())
						{
							auto value = *reinterpret_cast<unsigned*>(propAddr);
							out() << (value ? "True" : "False");
						}
						else if (propClass == UNameProperty::StaticClass())
						{
							auto value = *reinterpret_cast<SFXName*>(propAddr);
							out() << value.ToString(SFXName::FormatMode::k_formatInstanced).Chars();
						}
						else if (propClass == UStrProperty::StaticClass())
						{
							auto value = *reinterpret_cast<FString*>(propAddr);
							out() << "\"" << value.Chars() << "\""; // Used to have a null check here
						}
						else if (propClass == UStringRefProperty::StaticClass())
						{
							auto value = *reinterpret_cast<int*>(propAddr);
							out() << "$" << value;
						}
						else if (propClass == UDelegateProperty::StaticClass())
						{
							auto value = *reinterpret_cast<FScriptDelegate*>(propAddr);
							out() << "(Function Name:" << value.FunctionName.ToString(SFXName::FormatMode::k_formatInstanced).Chars() << ", Object: " << SCRIPTOBJECTFULLPATH(value.Object) << ")";
						}
						else if (propClass == UInterfaceProperty::StaticClass())
						{
							auto value = *reinterpret_cast<FScriptInterface*>(propAddr);
							out() << SCRIPTOBJECTFULLPATH(value.Object);
						}
						else if (prop->IsA(UObjectProperty::StaticClass()))
						{
							auto value = *reinterpret_cast<UObject**>(propAddr);
							out() << SCRIPTOBJECTFULLPATH(value);
						}
						propAddr += prop->ElementSize;
					}
					out() << "]\n";
				}
			}
			else if (propClass == UDelegateProperty::StaticClass())
			{
				PRINTALLELEMENTS(FScriptDelegate, "(Function Name : " << value.FunctionName.Instanced().Chars() << ", Object : " << SCRIPTOBJECTFULLPATH(value.Object) << ")")
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
					out() << "[" << prop->ArrayDim << "] : ( StructType: " << uStruct->GetName().Chars() << ") [\n";
					PrintPropertyValues(propAddr, uStruct);
					for (int i = prop->ArrayDim - 1; i > 0; --i)
					{
						indent() << ",\n";
						propAddr += prop->ElementSize;
						PrintPropertyValues(propAddr, uStruct);
					}
					indent() << "]\n";
				}
				else
				{
					out() << "( StructType: " << uStruct->GetName().Chars() << ")\n";
					PrintPropertyValues(propAddr, uStruct);
				}
			}
			else if (prop->IsA(UObjectProperty::StaticClass()))
			{
				PRINTALLELEMENTS(UObject*, SCRIPTOBJECTFULLPATH(value))
			}

		}
		DecreaseIndent();
	}

}
