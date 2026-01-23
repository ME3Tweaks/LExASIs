#if defined(SDK_TARGET_LE3)
#include "Common/Base.hpp"

namespace DebugLogger {
	void InstallGameSpecificHooks(::LESDK::Initializer& Init) {
		LEASI_UNUSED(Init);
	}
}

#endif