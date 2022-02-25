#pragma once

#include <string>
#include <iostream>
#include <chrono>
#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQ_NAME PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(MESSAGE) LogDuration UNIQ_NAME(MESSAGE)
#define LOG_DURATION_STREAM(MESSAGE, STREAM) LogDuration UNIQ_NAME(MESSAGE, STREAM)

class LogDuration
{
public:
	using clock = std::chrono::steady_clock;
	LogDuration() = default;
	LogDuration(const std::string& message, std::ostream& out = std::cerr)
		: message_(message)
	{

	}
	~LogDuration()
	{
		clock::time_point t2 = clock::now();
		std::cerr << message_ << ": " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;
	}
private:
	const std::string message_;
	const clock::time_point t1 = clock::now();
};
