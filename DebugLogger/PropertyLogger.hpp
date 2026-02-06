#pragma once
#include <sstream>
#include <string>
#include <LESDK/Headers.hpp>

// Stream operator overloads to eliminate .Chars() calls
// These must be in the global namespace for ADL to work correctly
inline std::wostream& operator<<(std::wostream& os, const FString& str)
{
	return os << str.Chars();
}

inline std::wostream& operator<<(std::wostream& os, const SFXName& name)
{
	return os << name.Instanced();
}

namespace DebugLogger {

	class PropertyLogger
	{
		std::wstringstream logFile;

		int numSpacesIndent = 0;

		void IncreaseIndent();
		void DecreaseIndent();
		void indent();
		// std::wstringstream&& out();

	public:

		std::wstring GetString() const;
		void PrintPropertyValues(BYTE* propsOffset, UStruct* const node, FFrame::FOutParmRec* outParmInfo = nullptr);
	};

}
