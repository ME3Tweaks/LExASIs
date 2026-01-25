#include "DebugLogger/VerboseLogger.hpp"

#if defined(BLORBO)

namespace DebugLogger {
	INIT_FIND_PATTERN(AccessedNone_location, "48 8b 0d 5f b9 6e 01 48 85 c9 74 06 48 8b 01 ff 50 20")
		AttachAccessedNoneVerboseLogger();

#define ATTACH_VERBOSE_LOGGER(TYPE, LOGFUNCTION, PATTERN) \
	{void* LOGFUNCTION##_location = nullptr; \
	INIT_FIND_PATTERN(LOGFUNCTION##_location, PATTERN) \
	Attach##TYPE##VerboseLogger(LOGFUNCTION, LOGFUNCTION##_location, #LOGFUNCTION " at " PATTERN);} \

	//dynamic array, local variable
	ATTACH_VERBOSE_LOGGER(ArrayOOB, ArrayOOBLocalVerboseLogger, "48 8b 15 fd 0c 6c 01 49 8b cd e8 0d c9 02 00 90")

		//dynamic array, instance variable
		ATTACH_VERBOSE_LOGGER(ArrayOOB, ArrayOOBWithObjectNameVerboseLogger, "48 8b 15 8c 0b 6c 01 49 8b cd e8 9c c7 02 00 90")

		//static array
		ATTACH_VERBOSE_LOGGER(ArrayOOB, ArrayOOBWithObjectNameVerboseLogger, "48 8b 15 04 10 6c 01 49 8b cd e8 14 cc 02 00 90")

		// float / float
		ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 c4 fa fa 00 48 8b cb e8 ec 7a 03 00")

		// float /= float
		ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 4b f1 fa 00 48 8b cb e8 73 71 03 00")

		// rotator /= float
		ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 48 df f9 00 48 8b cb e8 70 5f 02 00")

		// rotator / float
		ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 f9 dc f9 00 48 8b cb e8 21 5d 02 00")

		// vector /= float
		ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 e8 c9 f9 00 48 8b cb e8 10 4a 02 00")

		// vector / float
		ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 c9 be f9 00 48 8b cb e8 f1 3e 02 00")

		// int / int
		ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 8f 91 f9 00 48 8b cb e8 b7 11 02 00")

		// float % float
		ATTACH_VERBOSE_LOGGER(Math, ModuloBy0VerboseLogger, "4c 8d 05 04 fa fa 00 48 8b cb e8 0c 7a 03 00")

		// int % int
		ATTACH_VERBOSE_LOGGER(Math, ModuloBy0VerboseLogger, "4c 8d 05 cf 90 f9 00 48 8b cb e8 d7 10 02 00")

		ATTACH_VERBOSE_LOGGER(Math, SqrtOfNegativeNumberVerboseLogger, "4c 8d 05 05 e9 fa 00 48 8b cb e8 dd 68 03 00")


		return true;

#undef ATTACH_VERBOSE_LOGGER
#undef nameof
}
#endif