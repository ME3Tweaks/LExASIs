namespace Common {
	// Removes the logging pattern so that data written by the LEASI_INFO/WARN/ERROR macros is not prefixed
	void RemoveLoggingPattern();
	// Restores the default logging pattern so that data written by the LEASI_INFO/WARN/ERROR macros is prefixed with timestamps and the source
	void RestoreLoggingPattern();
	// Set the pattern the logger uses when formatting log strings
	void SetLoggingPattern(std::string);
	// Sets up the default logger with console and file sinks and the default logging pattern
	void SetupDefaultLogger(const char* gameName, const char* logBaseName);
}