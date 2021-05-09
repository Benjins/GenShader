
#if defined(_WIN32)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

#include <assert.h>

#include <random>
#include <algorithm>

#include "stack_string.h"


using  int32 =  int32_t;
using uint32 = uint32_t;
using  int64 =  int64_t;
using uint64 = uint64_t;

// Something like mutational/differential testing
// We take a source file (probably once that's valid),
// and add a bunch of C-preprocessor commands to the file that should leave things unchanged
// i.e.
//  - "#if 1"..."#endif"
//  - "#define FOO_MACRO(x) x" and then changing a token "word" to be "FOO_MACRO(word)"
//  - "#define VEC2 vec" and then replace some "vec2" tokens with "VEC2"
//  - replace token "my_token" with "my_ ## token"
//  - replace "my_token" with "FOO_REPLACE(ken)" and add "#define FOO_REPLACE(x) my_to ## x"
//  - idk, various other ones. 

#include <Windows.h>

#define MAX_SOURCE_FILE_SIZE (128*1024)

using SourceBuffer = StringStackBuffer<MAX_SOURCE_FILE_SIZE>;

void InsertNonChangingPreprocessorTransformations(const SourceBuffer* InSrc, SourceBuffer* OutSrc, uint64 Seed)
{

}

int main()
{
	OutputDebugStringA("sdf");



	return 0;
}


