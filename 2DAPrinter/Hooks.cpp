#include "Hooks.hpp"
#include "Common/Objects.hpp"
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

			char* buffer = new char[512];
			sprintf(buffer, "Printout for Bio2DA %hs\n", twoDA->GetFullName());
			logger.writeToLog(buffer, false, false);
			for (int j = 0; j < rowNames.Count; j++)
			{
				for (int k = 0; k < colNames.Count; k++)
				{
					char* buffer = new char[512];
					if (FName nameVal; twoDA->GetNameEntryII(j, k, &nameVal))
					{
						sprintf(buffer, "2DA[%hs][%hs] = %hs NAME\n", rowNames.Data[j].GetName(), colNames.Data[k].GetName(), nameVal.Instanced());
					}
					else if (FString strVal; twoDA->GetStringEntryII(j, k, &strVal))
					{
						sprintf(buffer, "2DA[%hs][%hs] = %ls STRING\n", rowNames.Data[j].GetName(), colNames.Data[k].GetName(), strVal.Data);
					}
					else if (float floatVal = -123.35; twoDA->GetFloatEntryII(j, k, &floatVal))
					{
						sprintf(buffer, "2DA[%hs][%hs] = %f FLOAT\n", rowNames.Data[j].GetName(), colNames.Data[k].GetName(), floatVal);
					}
					else if (int intVal = -895; twoDA->GetIntEntryII(j, k, &intVal))
					{
						sprintf(buffer, "2DA[%hs][%hs] = %d INT\n", rowNames.Data[j].GetName(), colNames.Data[k].GetName(), intVal);
					}
					else
					{
						sprintf(buffer, "2DA[%hs][%hs] = NULL\n", rowNames.Data[j].GetName(), colNames.Data[k].GetName());
					}

					logger.writeToLog(buffer, false, false);
				}
			}
		}
	}

	void Parse2DANR(UBio2DANumberedRows* twoDA)
	{
		if (twoDA)
		{
			auto colNames = twoDA->GetColumnNames();
			auto rowCount = twoDA->GetNumRows();

			char* buffer = new char[512];
			sprintf(buffer, "Printout for Bio2DANumberedRows %hs\n", twoDA->GetFullName());
			logger.writeToLog(buffer, false, false);
			for (int j = 0; j < rowCount; j++)
			{
				auto rowIndex = twoDA->GetRowNumber(j);
				for (int k = 0; k < colNames.Count; k++)
				{
					buffer = new char[512];
					if (FName nameVal; twoDA->GetNameEntryII(j, k, &nameVal))
					{
						sprintf(buffer, "2DANR[%d][%hs] = %hs NAME\n", rowIndex, colNames.Data[k].GetName(), nameVal.Instanced());
					}
					else if (FString strVal; twoDA->GetStringEntryII(j, k, &strVal))
					{
						sprintf(buffer, "2DANR[%d][%hs] = %ls STRING\n", rowIndex, colNames.Data[k].GetName(), strVal.Data);
					}
					else if (float floatVal = -123.35; twoDA->GetFloatEntryII(j, k, &floatVal))
					{
						sprintf(buffer, "2DANR[%d][%hs] = %f FLOAT\n", rowIndex, colNames.Data[k].GetName(), floatVal);
					}
					else if (int intVal = -895; twoDA->GetIntEntryII(j, k, &intVal))
					{
						sprintf(buffer, "2DANR[%d][%hs] = %d INT\n", rowIndex, colNames.Data[k].GetName(), intVal);
					}
					else
					{
						sprintf(buffer, "2DANR[%d][%hs] = NULL\n", rowIndex, colNames.Data[k].GetName());
					}

					logger.writeToLog(buffer, false, false);
				}
			}
		}
	}


	void Print2DAs() {
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
