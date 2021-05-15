
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
//  - add "#line" directive
//  - idk, various other ones. 

// Okay, actually scratch that for now we're doing a semantically correct generator for just
// the proprocessor because I can't make up my mind

#include <random>
#include <vector>
#include <string>

#include <Windows.h>

#define MAX_SOURCE_FILE_SIZE (128*1024)

using SourceBuffer = StringStackBuffer<MAX_SOURCE_FILE_SIZE>;
using IdentiferBuffer = StringStackBuffer<8>;

struct PreprocGenCtx {
	SourceBuffer* OutSource = nullptr;

	std::mt19937_64 RNGState{ 0 };

	std::vector<IdentiferBuffer> Defines;
	std::vector<std::pair<IdentiferBuffer, int32>> Macros;
	std::vector<IdentiferBuffer> MacroParamsInScope;

	int32 IfDepth = 0;

	int32 GetIntInRange(int32 Min, int32 Max) {
		std::uniform_int_distribution<int32> Dist(Min, Max);
		return Dist(RNGState);
	}

	void ResetState() {
		OutSource->Clear();

		Defines.clear();
		Macros.clear();
		MacroParamsInScope.clear();

		IfDepth = 0;
	}
};

IdentiferBuffer GeneratePreProcessorIdentifier(PreprocGenCtx* Ctx) {
	IdentiferBuffer Buff;
	int32 Len = Ctx->GetIntInRange(1, 3);
	for (int32 i = 0; i < Len; i++) {
		Buff.buffer[i] = 'a' + Ctx->GetIntInRange(0, 25);
	}

	Buff.buffer[Len] = '\0';
	Buff.length = Len;

	return Buff;
}

bool GetRandomIdentifierInScope(PreprocGenCtx* Ctx, IdentiferBuffer* OutIdent) {
	int32 Decider = Ctx->GetIntInRange(0, 2);
	if (Ctx->MacroParamsInScope.size() > 0 && Decider <= 0) {
		*OutIdent = Ctx->MacroParamsInScope[Ctx->GetIntInRange(0, Ctx->MacroParamsInScope.size() - 1)].buffer;
		return true;
	}
	else if (Ctx->Macros.size() > 0 && Decider <= 1) {
		*OutIdent = Ctx->Macros[Ctx->GetIntInRange(0, Ctx->Macros.size() - 1)].first.buffer;
		return true;
	}
	else if (Ctx->Defines.size() > 0 && Decider <= 2) {
		*OutIdent = Ctx->Defines[Ctx->GetIntInRange(0, Ctx->Defines.size() - 1)].buffer;
		return true;
	}
	else {
		return false;
	}
}

void GeneratePreProcessorExpr_Internal(PreprocGenCtx* Ctx, int RecursionDepth) {
	// One of:
	// Bound params (if there are any)
	// Other defines
	// macro calls
	// ( <expr> )
	// <expr> && <expr>
	// <expr> || <expr>
	// <expr> <=> <expr>
	// <number>
	// bool
	// defined(<ident>)

	bool bFallthroughToNonRecursive = false;

	if (RecursionDepth <= 4) {
		int32 Decider = Ctx->GetIntInRange(0, 100);
		
		if (Ctx->Macros.size() > 0 && Decider <= 30) {
			int32 MacroIndex = Ctx->GetIntInRange(0, Ctx->Macros.size() - 1);
			auto MacroInfo = Ctx->Macros[MacroIndex];
			Ctx->OutSource->AppendFormat("%s(", MacroInfo.first.buffer);
			for (int32 ParamIndex = 0; ParamIndex < MacroInfo.second; ParamIndex++) {
				if (ParamIndex > 0) {
					Ctx->OutSource->Append(", ");
				}

				GeneratePreProcessorExpr_Internal(Ctx, RecursionDepth + 1);
			}
			Ctx->OutSource->Append(")");
		}
		else if (Decider <= 75) {
			static const char* const BinOps[] = { "||", "&&", "+", "*", "/", "-", "<", ">", "==", "!=", "<=", ">=" };
			static const int32 NumBinOps = sizeof(BinOps) / sizeof(BinOps[0]);

			const bool bShouldGenerateParens = Ctx->GetIntInRange(0, 2) != 0;

			if (bShouldGenerateParens) { Ctx->OutSource->Append("("); }

			GeneratePreProcessorExpr_Internal(Ctx, RecursionDepth + 1);
			Ctx->OutSource->AppendFormat(" %s ", BinOps[Ctx->GetIntInRange(0, NumBinOps - 1)]);
			GeneratePreProcessorExpr_Internal(Ctx, RecursionDepth + 1);
			
			if (bShouldGenerateParens) { Ctx->OutSource->Append(")"); }
		}
		else if (Decider <= 80) {
			Ctx->OutSource->Append("(");
			GeneratePreProcessorExpr_Internal(Ctx, RecursionDepth + 1);
			Ctx->OutSource->Append(")");
		}
		else {
			// Fall through to non-recursive cases
			bFallthroughToNonRecursive = true;
		}
	}
	else {
		bFallthroughToNonRecursive = true;
	}

	if (bFallthroughToNonRecursive) {
		int32 Decider = Ctx->GetIntInRange(0, 100);
		IdentiferBuffer RandomIdentifier;
		if (Decider < 60 && GetRandomIdentifierInScope(Ctx, &RandomIdentifier)) {
			Ctx->OutSource->Append(RandomIdentifier.buffer);
		}
		else if (Decider < 70) {
			if (Ctx->GetIntInRange(0, 3) != 0 && GetRandomIdentifierInScope(Ctx, &RandomIdentifier)) {
				Ctx->OutSource->AppendFormat("defined(%s)", RandomIdentifier.buffer);
			}
			else {
				Ctx->OutSource->AppendFormat("defined(%s)", GeneratePreProcessorIdentifier(Ctx).buffer);
			}
		}
		else if (Decider < 80) {
			Ctx->OutSource->AppendFormat("%d", Ctx->GetIntInRange(-3, 100));
		}
		else if (Decider <= 100) {
			Ctx->OutSource->AppendFormat("%s", (Ctx->GetIntInRange(0, 1) == 0) ? "true" : "false");
		}
		else {
			assert(false && "unreachable");
		}
	}


	//Ctx->OutSource->Append("<expr>");
}

// TODO: Flags, e.g. whether define(<ident>) is allowed (
void GeneratePreProcessorExpr(PreprocGenCtx* Ctx) {
	return GeneratePreProcessorExpr_Internal(Ctx, 0);
}

void GeneratePreProcessorSource(PreprocGenCtx* Ctx) {

	// TODO: Predefined macros
	// GL_compatibility_profile
	// __FILE__
	// __VERSION__
	// __LINE__

	int32 NumLines = Ctx->GetIntInRange(3, 30);

	for (int32 Line = 0; Line < NumLines; Line++)
	{
		int32 Decider = Ctx->GetIntInRange(0, 100);

		// #define
		if (Decider < 30) {
			auto Ident = GeneratePreProcessorIdentifier(Ctx);
			Ctx->OutSource->AppendFormat("#define %s ", Ident.buffer);
			GeneratePreProcessorExpr(Ctx);
			Ctx->OutSource->Append("\n");

			Ctx->Defines.push_back(Ident);
		}
		// #define ()
		else if (Decider < 70) {
			int32 Arity = Ctx->GetIntInRange(0, 5);

			auto MacroName = GeneratePreProcessorIdentifier(Ctx);
			Ctx->OutSource->AppendFormat("#define %s(", MacroName.buffer);
			for (int32 i = 0; i < Arity; i++) {
				if (i > 0) {
					Ctx->OutSource->Append(", ");
				}
				auto ParamName = GeneratePreProcessorIdentifier(Ctx);
				Ctx->OutSource->Append(ParamName.buffer);
				Ctx->MacroParamsInScope.push_back(ParamName);
			}
			Ctx->OutSource->Append(") ");

			GeneratePreProcessorExpr(Ctx);

			Ctx->OutSource->Append("\n");
			Ctx->Macros.emplace_back(MacroName, Arity);

			Ctx->MacroParamsInScope.clear();
		}
		// #if
		else if (Decider < 75) {
			Ctx->OutSource->Append("#if ");
			GeneratePreProcessorExpr(Ctx);
			Ctx->OutSource->Append("\n");

			// TODO: Scope the macros defined w/in an #if/#endif block?

			Ctx->IfDepth++;
		}
		// #endif
		else if (Ctx->IfDepth > 0 && Decider < 80) {
			Ctx->OutSource->Append("#endif\n");
			Ctx->IfDepth--;
		}
		// #line
		else if (Decider < 90) {
			if (Ctx->GetIntInRange(0, 3) == 0) {
				Ctx->OutSource->AppendFormat("#line %d %d\n", Ctx->GetIntInRange(-1, 30), Ctx->GetIntInRange(-1, 5));
			}
			else {
				Ctx->OutSource->AppendFormat("#line %d\n", Ctx->GetIntInRange(-1, 30));
			}
		}
		// #undef
		else if (Decider <= 100) {
			int32 UndefDecider = Ctx->GetIntInRange(0, 2);
			if (Ctx->Defines.size() > 0 && UndefDecider == 0) {
				int32 IndexToErase = Ctx->GetIntInRange(0, Ctx->Defines.size() - 1);
				Ctx->OutSource->AppendFormat("#undef %s\n", Ctx->Defines[IndexToErase].buffer);
				Ctx->Defines.erase(Ctx->Defines.begin() + IndexToErase);
			}
			else if (Ctx->Macros.size() > 0 && UndefDecider == 1) {
				int32 IndexToErase = Ctx->GetIntInRange(0, Ctx->Macros.size() - 1);
				Ctx->OutSource->AppendFormat("#undef %s\n", Ctx->Macros[IndexToErase].first.buffer);
				Ctx->Macros.erase(Ctx->Macros.begin() + IndexToErase);
			}
			else {
				Ctx->OutSource->AppendFormat("#undef %s\n", GeneratePreProcessorIdentifier(Ctx).buffer);
			}
		}
		else {
			assert(false && "unreachable");
		}

	}

	// Add in any "#endif"s needed
	while (Ctx->IfDepth > 0) {
		Ctx->OutSource->Append("#endif\n");
		Ctx->IfDepth--;
	}


	// Add in an expr
	GeneratePreProcessorExpr(Ctx);
}

//void InsertNonChangingPreprocessorTransformations(const SourceBuffer* InSrc, SourceBuffer* OutSrc, uint64 Seed)
//{
//}

int main()
{
	OutputDebugStringA("sdf");


	PreprocGenCtx Ctx;
	Ctx.OutSource = new StringStackBuffer<MAX_SOURCE_FILE_SIZE>();

	for (int32 i = 0; i < 10; i++)
	{
		Ctx.ResetState();
		Ctx.RNGState.seed(i);

		GeneratePreProcessorSource(&Ctx);
		OutputDebugStringA("\n-----------\n");
		OutputDebugStringA(Ctx.OutSource->buffer);
		OutputDebugStringA("\n-----------\n");
	}

	delete Ctx.OutSource;


	return 0;
}


