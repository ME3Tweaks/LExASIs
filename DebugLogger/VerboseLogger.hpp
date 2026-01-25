#include <LESDK/Common/Frame.hpp>
#include <LESDK/Init.hpp>

namespace DebugLogger
{

	using tStackAddrUndef = void(const FFrame* stack, void* addressOfThisFunction, void* undefined);
	using tArrayOOBWithObjectName = void(const FFrame* stack, void* addressOfThisFunction, wchar_t* formatString, wchar_t* objName, wchar_t* arrayPropName, const int index, const int arrayLen);
	using tArrayOOBLocalVerbose = void(const FFrame* stack, void* addressOfThisFunction, wchar_t* formatString, wchar_t* arrayPropName, const int index, const int arrayLen);
	void InstallVerboseLoggerHooks(::LESDK::Initializer& Init);
}