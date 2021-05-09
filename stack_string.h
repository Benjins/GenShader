#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

template<int capacity>
struct StringStackBuffer {
	int length;
	char buffer[capacity];

	StringStackBuffer() {
		Clear();
	}

	StringStackBuffer(const char* format, ...) {
		buffer[0] = '\0';
		length = 0;
		va_list varArgs;
		va_start(varArgs, format);
		length += vsnprintf(buffer, capacity, format, varArgs);
		if (length >= capacity - 1)
		{
			length = capacity - 1;
		}
		va_end(varArgs);
	}

	void Clear() {
		buffer[0] = '\0';
		length = 0;
	}

	void Append(const char* str) {
		length += snprintf(&buffer[length], capacity - length, "%s", str);
		if (length >= capacity - 1)
		{
			length = capacity - 1;
		}
	}

	void AppendFormat(const char* format, ...) {
		va_list varArgs;
		va_start(varArgs, format);
		length += vsnprintf(&buffer[length], capacity - length, format, varArgs);
		va_end(varArgs);
		if (length >= capacity - 1)
		{
			length = capacity - 1;
		}
	}
};
