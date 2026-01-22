#include "Hooks.hpp"
#include "Common/Objects.hpp"
#include "Common/DefaultLogger.hpp"
#include "LESDK/Headers.hpp"
#include <sstream>

namespace Bio2DAPrinter
{
	bool CanPrint2DAs = true;

	void Parse2DA(UBio2DA* twoDA)
	{
		if (twoDA)
		{
			auto colNames = twoDA->GetColumnNames();
			auto rowNames = twoDA->GetRowNames();

			LEASI_INFO(L"Printout for Bio2DA {}", twoDA->GetFullName());
			for (UINT j = 0; j < rowNames.Count(); j++)
			{
				for (UINT k = 0; k < colNames.Count(); k++)
				{
					auto rowName = rowNames.GetData()[j].ToString(SFXName::FormatMode::k_formatInstanced);
					auto colName = colNames.GetData()[k].ToString(SFXName::FormatMode::k_formatInstanced);
					if (SFXName nameVal; twoDA->GetNameEntryII(j, k, &nameVal))
					{
						LEASI_INFO(L"2DA[{}][{}] = {} NAME", rowName, colName, nameVal.ToString(SFXName::FormatMode::k_formatInstanced));
					}
					else if (FString strVal; twoDA->GetStringEntryII(j, k, &strVal))
					{
						LEASI_INFO(L"2DA[{}][{}] = {} STRING", rowName, colName, strVal);
					}
					else if (float floatVal = -123.35f; twoDA->GetFloatEntryII(j, k, &floatVal))
					{
						LEASI_INFO(L"2DA[{}][{}] = {} FLOAT", rowName, colName, floatVal);
					}
					else if (int intVal = -895; twoDA->GetIntEntryII(j, k, &intVal))
					{
						LEASI_INFO(L"2DA[{}][{}] = {} INT", rowName, colName, intVal);
					}
					else
					{
						LEASI_INFO(L"2DA[{}][{}] = NULL", rowName, colName);
					}
				}
			}
		}
	}

	void Parse2DANumberedRows(UBio2DANumberedRows* twoDA)
	{
		if (twoDA)
		{
			auto colNames = twoDA->GetColumnNames();
			auto rowCount = twoDA->GetNumRows();

			LEASI_INFO(L"Printout for Bio2DANumberedRows {}", twoDA->GetFullName());

			for (int j = 0; j < rowCount; j++)
			{
				auto rowIndex = twoDA->GetRowNumber(j);
				for (UINT k = 0; k < colNames.Count(); k++)
				{
					auto colName = colNames.GetData()[k].ToString(SFXName::FormatMode::k_formatInstanced);
					if (SFXName nameVal; twoDA->GetNameEntryII(j, k, &nameVal))
					{
						LEASI_INFO(L"2DANR[{}][{}] = {} NAME", rowIndex, colName, nameVal.ToString(SFXName::FormatMode::k_formatInstanced));
					}
					else if (FString strVal; twoDA->GetStringEntryII(j, k, &strVal))
					{
						LEASI_INFO(L"2DANR[{}][{}] = {} STRING", rowIndex, colName, strVal);
					}
					else if (float floatVal = -123.35f; twoDA->GetFloatEntryII(j, k, &floatVal))
					{
						LEASI_INFO(L"2DANR[{}][{}] = {} FLOAT", rowIndex, colName, floatVal);
					}
					else if (int intVal = -895; twoDA->GetIntEntryII(j, k, &intVal))
					{
						LEASI_INFO(L"2DANR[{}][{}] = {} INT", rowIndex, colName, intVal);
					}
					else
					{
						LEASI_INFO("2DANR[{}][{}] = NULL", rowIndex, colName);
					}
				}
			}
		}
	}


	void Print2DAs() {
		Common::RemoveLoggingPattern();
		Common::TypedObjectIterator<UBio2DA> Iterator{};
		for (; Iterator; ++Iterator)
		{
			UBio2DA* const bio2DA = *Iterator;
			if (bio2DA->Class == UBio2DA::StaticClass())
			{
				Parse2DA(bio2DA);
			}
			else
			{
				Parse2DANumberedRows(static_cast<UBio2DANumberedRows*>(bio2DA));
			}
		}
		Common::RestoreLoggingPattern();
	}

	// ! UObject::ProcessEvent hook
	// ========================================

	t_UObject_ProcessEvent* UObject_ProcessEvent_orig = nullptr;
	void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result)
	{
		// Handle BioHUD.PostRender for on-screen display
		if (CanPrint2DAs && Function->GetFullName().Equals(L"Function SFXGame.BioHUD.PostRender"))
		{
			// Toggle drawing/not drawing
			if ((GetKeyState('2') & 0x8000) && (GetKeyState(VK_CONTROL) & 0x8000)) {
				if (CanPrint2DAs) {
					Print2DAs();
					CanPrint2DAs = false; // Will not activate combo again until you re-press combo
					LEASI_INFO("Printed 2DAs to log. Can no longer print 2DAs in this session.");
				}
			}
		}

		UObject_ProcessEvent_orig(Context, Function, Parms, Result);
		return;
	}
}
