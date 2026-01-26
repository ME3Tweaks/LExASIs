#pragma once
#include <sstream>
#include <string>
#include <LESDK/Headers.hpp>

namespace DebugLogger {

	class PropertyLogger
	{
		std::wstringstream logFile;

		int numSpacesIndent = 0;

		void IncreaseIndent();
		void DecreaseIndent();
		std::wstringstream&& indent();
		std::wstringstream&& out();

	public:

		std::wstring GetString() const;
		void PrintPropertyValues(BYTE* propsOffset, UStruct* const node, FFrame::FOutParmRec* outParmInfo = nullptr);
	};

}
