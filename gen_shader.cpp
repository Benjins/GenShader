#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

#include <assert.h>

#include <random>

template<int capacity>
struct StringStackBuffer{
	int length;
	char buffer[capacity];
	
	StringStackBuffer(){
		buffer[0] = '\0';
		length = 0;
	}
	
	StringStackBuffer(const char* format, ...){
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
	
	void Append(const char* str){
		length += snprintf(&buffer[length], capacity - length, "%s", str);
		if (length >= capacity - 1)
		{
			length = capacity - 1;
		}
	}
	
	void AppendFormat(const char* format, ...){
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


#define MAX_SHADER_SOURCE_LEN (128*1024)

using SourceBuffer = StringStackBuffer<MAX_SHADER_SOURCE_LEN>;

using TypeID = int32_t;
using DataTransformID = int32_t;

using int32 = int32_t;
using int64 = int64_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

enum BuiltinTypes
{
	BT_Bool,
	BT_Int,
	BT_Float,
	BT_Vec2,
	BT_Vec3,
	BT_Vec4,
	//BT_Mat2,
	//BT_Mat3,
	//BT_Mat4,
	BT_Count
};

struct VariableInfo
{
	StringStackBuffer<32> Name;
	TypeID Type;
};

struct TypeInfo
{
	StringStackBuffer<32> Name;
	std::vector<VariableInfo> Fields;
};

enum struct ShaderType
{
	Vert,
	Frag, // For now, just frag really
	Compute
};

/*
struct TypeHandle
{
	TypeID ID;
	int32 ArrayCount = 0; // 0 for scalars, >0 for array types, and it
};
*/

enum DataTransformationType
{
	DTT_Func,
	DTT_Op,
	DTT_FieldAccess
};

// Also limits user-defined func arity
#define MAX_DTT_ARITY 8

struct DataTransformation
{
	DataTransformationType TransformType = DTT_Func;
	TypeID DstType = BT_Bool;
	int32 NumSrcTypes = 0;
	TypeID SrcTypes[MAX_DTT_ARITY];
	StringStackBuffer<32> Name;
};

// Each expression writes out a basic stack bytecode,
// which consists of variable accesses and data transformations (both by id/index)
// this is because we might get partways through generating an expression and decide the types don't work out,
// and so in that case we discard it
//struct ExpressionStackBytecodeOp
//{
//	enum Type_t {
//		Transform,
//		VarAccess
//	} Type;
//
//	int32 Index;
//};

struct ProgramState
{
	std::vector<TypeInfo> ProgramTypes;
	std::vector<DataTransformation> DataTransforms;
	
	// TODO: Index DataTransforms by DstType, and make it easy to select a random one of those if it exists
	std::vector<std::pair<int32, int32>> DataTransformIndexByDstType;
	
	std::vector<VariableInfo> VarsInScope;
	
	// 0th element is the index of the first non-global variable in this context
	std::vector<int32> VarScopeCountStack;
	
	// TODO: For vert shaders
	//std::vector<VariableInfo> OutVars;
	
	std::mt19937_64 RNGState;
	
	// NOTE: It's inclusive
	int GetIntInRange(int min, int max)
	{
		std::uniform_int_distribution<int> Dist(min, max);
		return Dist(RNGState);
	}

	float GetFloatInRange(float min, float max)
	{
		std::uniform_real_distribution<float> Dist(min, max);
		return Dist(RNGState);
	}

	float GetFloat01()
	{
		return GetFloatInRange(0.0f, 1.0f);
	}

	void SetSeed(uint64_t Seed)
	{
		RNGState.seed(Seed);
	}
	
	std::vector<StringStackBuffer<32>> ScratchExpressionList;
};


void InitProgramState(ProgramState* PS)
{
	{
		TypeInfo Info1 = {"float", {}};
		TypeInfo Info2 = {"int", {}};
		TypeInfo Info3 = {"bool", {}};
		// Don't both with fields I guess? idk, or maybe do them later
		TypeInfo Info4 = {"vec2", {}};
		TypeInfo Info5 = {"vec3", {}};
		TypeInfo Info6 = {"vec4", {}};
		//TypeInfo Info7 = {"mat2", {}};
		//TypeInfo Info8 = {"mat3", {}};
		//TypeInfo Info9 = {"mat4", {}};

		{
			Info4.Fields.resize(2);
			Info4.Fields[0].Type = BT_Float;
			Info4.Fields[0].Name.Append("x");
			Info4.Fields[1].Type = BT_Float;
			Info4.Fields[1].Name.Append("y");
		}

		{
			Info5.Fields.resize(3);
			Info5.Fields[0].Type = BT_Float;
			Info5.Fields[0].Name.Append("x");
			Info5.Fields[1].Type = BT_Float;
			Info5.Fields[1].Name.Append("y");
			Info5.Fields[2].Type = BT_Float;
			Info5.Fields[2].Name.Append("z");
		}

		{
			Info6.Fields.resize(4);
			Info6.Fields[0].Type = BT_Float;
			Info6.Fields[0].Name.Append("x");
			Info6.Fields[1].Type = BT_Float;
			Info6.Fields[1].Name.Append("y");
			Info6.Fields[2].Type = BT_Float;
			Info6.Fields[2].Name.Append("z");
			Info6.Fields[3].Type = BT_Float;
			Info6.Fields[3].Name.Append("z");
		}
		
		PS->ProgramTypes.push_back(Info1);
		PS->ProgramTypes.push_back(Info2);
		PS->ProgramTypes.push_back(Info3);
		PS->ProgramTypes.push_back(Info4);
		PS->ProgramTypes.push_back(Info5);
		PS->ProgramTypes.push_back(Info6);
		//PS->ProgramTypes.push_back(Info7);
		//PS->ProgramTypes.push_back(Info8);
		//PS->ProgramTypes.push_back(Info9);
	}
	
	auto AddBuiltinFieldAccess = [PS](TypeID StructType, TypeID FieldType, const char* FieldName)
	{
		DataTransformation DataTrans;
		DataTrans.TransformType = DTT_FieldAccess;
		DataTrans.DstType = FieldType;
		DataTrans.NumSrcTypes = 1;
		DataTrans.SrcTypes[0] = StructType;
		DataTrans.Name.AppendFormat("%s", FieldName);
		
		PS->DataTransforms.push_back(DataTrans);
	};
	
	// TODO: Not use vector? initializer list? idk
	auto AddBuiltinFunc = [PS](TypeID OutputType, const char* FuncName, const std::vector<TypeID>& InputTypes)
	{
		DataTransformation DataTrans;
		DataTrans.TransformType = DTT_Func;
		DataTrans.DstType = OutputType;
		DataTrans.NumSrcTypes = 0;
		for (auto ID : InputTypes)
		{
			DataTrans.SrcTypes[DataTrans.NumSrcTypes] = ID;
			DataTrans.NumSrcTypes++;
		}
		DataTrans.Name.AppendFormat("%s", FuncName);
		
		PS->DataTransforms.push_back(DataTrans);
	};
	
	auto AddBuiltinBinOp = [PS](TypeID OutputType, TypeID LHSType, TypeID RHSType, const char* OpName)
	{
		DataTransformation DataTrans;
		DataTrans.TransformType = DTT_Op;
		DataTrans.DstType = OutputType;
		DataTrans.NumSrcTypes = 2;
		DataTrans.SrcTypes[0] = LHSType;
		DataTrans.SrcTypes[1] = RHSType;
		DataTrans.Name.AppendFormat("%s", OpName);
		
		PS->DataTransforms.push_back(DataTrans);
	};
	
	{
		auto AddTransformationsForVecType = [PS, AddBuiltinFieldAccess](TypeID InputType, TypeID OutputType)
		{
			int32 NumComponents = OutputType - BT_Float + 1;
			uint32 NumIters = 1U << (2 * NumComponents);
			char Buff[16] = {};
			for (uint32 i = 0; i < NumIters; i++)
			{
				uint32 Scratch = i;
				for (int32 c = 0; c < NumComponents; c++)
				{
					Buff[c] = 'w' + Scratch & 0x03;
					Scratch >>= 2;
				}

				AddBuiltinFieldAccess(InputType, OutputType, Buff);
			}
		};

		// Built-in data transformations
		for (int32 InType = BT_Vec2; InType <= BT_Vec4; InType++)
		{
			for (int32 OutType = BT_Float; OutType < InType; OutType++)
			{
				AddTransformationsForVecType(InType, OutType);
			}
		}
	}

	{
		for (int32 InType = BT_Int; InType <= BT_Vec4; InType++)
		{
			AddBuiltinBinOp(InType, InType, InType, "+");
			AddBuiltinBinOp(InType, InType, InType, "-");
			AddBuiltinBinOp(InType, InType, InType, "*");
			//AddBuiltinBinOp(InType, InType, InType, "/");
		}

		for (int32 InType = BT_Vec2; InType <= BT_Vec4; InType++)
		{
			AddBuiltinBinOp(InType, InType, BT_Float, "*");
		}

		for (int32 InType = BT_Int; InType <= BT_Float; InType++)
		{
			AddBuiltinBinOp(BT_Bool, InType, InType, "==");
			AddBuiltinBinOp(BT_Bool, InType, InType, "<=");
			AddBuiltinBinOp(BT_Bool, InType, InType, ">=");
			AddBuiltinBinOp(BT_Bool, InType, InType, "<");
			AddBuiltinBinOp(BT_Bool, InType, InType, ">");
		}
	}

	{
		for (int32 InType = BT_Vec2; InType <= BT_Vec4; InType++)
		{
			AddBuiltinFunc(BT_Float, "dot", { InType, InType });
		}

		for (int32 InType = BT_Float; InType <= BT_Vec4; InType++)
		{
			AddBuiltinFunc(InType, "abs", { InType });
			AddBuiltinFunc(InType, "sin", { InType });
			AddBuiltinFunc(InType, "cos", { InType });
			AddBuiltinFunc(InType, "sqrt", { InType });
			AddBuiltinFunc(InType, "pow", { InType, InType });
			AddBuiltinFunc(InType, "clamp", { InType, InType, InType });
		}

		{
			AddBuiltinFunc(BT_Vec3, "cross", { BT_Vec3, BT_Vec3 });
		}

		// ctors
		{
			AddBuiltinFunc(BT_Vec2, "vec2", { BT_Float, BT_Float });
			AddBuiltinFunc(BT_Vec3, "vec3", { BT_Float, BT_Float, BT_Float });
			AddBuiltinFunc(BT_Vec4, "vec4", { BT_Float, BT_Float, BT_Float, BT_Float });
		}

		//for (int32 InType = BT_Vec2; InType <= BT_Vec4; InType++)
		{
			//AddBuiltinFunc(BT_Bool, "any", { InType });
			//AddBuiltinFunc(BT_Bool, "all", { InType });
		}
	}
};

void GenerateUserDefinedStructs(ProgramState* PS, SourceBuffer* SrcBuff)
{
	int32 NumStructs = PS->GetIntInRange(0, 4);

	for (int32 i = 0; i < NumStructs; i++)
	{
		int32 NumFields = PS->GetIntInRange(1, 4);

		TypeInfo StructTypeInfo;
		StructTypeInfo.Name.AppendFormat("my_struct_%d", i);
		StructTypeInfo.Fields.reserve(NumFields);

		SrcBuff->AppendFormat("struct %s {\n", StructTypeInfo.Name.buffer);

		for (int32 f = 0; f < NumFields; f++)
		{
			TypeID FieldType = PS->GetIntInRange(0, PS->ProgramTypes.size() - 1);

			StructTypeInfo.Fields.emplace_back();
			StructTypeInfo.Fields.back().Type = FieldType;
			StructTypeInfo.Fields.back().Name.AppendFormat("field_%d", f);

			SrcBuff->AppendFormat("\t%s %s;\n", PS->ProgramTypes[FieldType].Name.buffer, StructTypeInfo.Fields.back().Name.buffer);
		}

		SrcBuff->Append("};\n\n");

		PS->ProgramTypes.push_back(StructTypeInfo);
		TypeID StructTypeID = (TypeID)(PS->ProgramTypes.size() - 1);

		for (const auto& Field : StructTypeInfo.Fields)
		{
			DataTransformation Trans;
			Trans.TransformType = DTT_FieldAccess;
			Trans.NumSrcTypes = 1;
			Trans.SrcTypes[0] = StructTypeID;
			Trans.DstType = Field.Type;
			Trans.Name.Append(Field.Name.buffer);

			PS->DataTransforms.push_back(Trans);
		}
	}
}

void IndexProgramDataTransformations(ProgramState* PS)
{
	PS->DataTransformIndexByDstType.clear();
	PS->DataTransformIndexByDstType.resize(PS->ProgramTypes.size());

	std::sort(PS->DataTransforms.begin(), PS->DataTransforms.end(), [](const DataTransformation& lhs, const DataTransformation& rhs)
	{
		return lhs.DstType < rhs.DstType;
	});


	int32 IndexBeginCursor = 0;
	TypeID CurrentType = 0;
	for (int32 IndexCursor = 0; IndexCursor < PS->DataTransforms.size(); IndexCursor++)
	{
		if (PS->DataTransforms[IndexCursor].DstType != CurrentType)
		{
			PS->DataTransformIndexByDstType[CurrentType] = std::make_pair(IndexBeginCursor, IndexCursor);
			IndexBeginCursor = IndexCursor;
			CurrentType++;
			while (CurrentType != PS->DataTransforms[IndexCursor].DstType)
			{
				// Basically, an empty interval since it's lower-inclusive, upper-exclusive
				PS->DataTransformIndexByDstType[CurrentType] = std::make_pair(IndexCursor, IndexCursor);
				CurrentType++;
			}
		}
	}
}

void GenerateGlobalVariables(ProgramState* PS, SourceBuffer* SrcBuff, ShaderType InShaderType)
{
	int32 NumAttributes = 0;
	int32 NumVarying = 0;
	int32 NumUniforms = 0;
	if (InShaderType == ShaderType::Frag)
	{
		// Generate varying, uniform, not attribute
		NumVarying = PS->GetIntInRange(0, 5);
		NumUniforms = PS->GetIntInRange(0, 5);
	}
	else
	{
		assert(false && "TODO");
	}
	
	auto DeclareNumGlobalVars = [&](const char* DeclType, int NumVars)
	{
		for (int32 i = 0; i < NumVars; i++)
		{
			PS->VarsInScope.emplace_back();
			auto& Var = PS->VarsInScope.back();
			Var.Type = (TypeID)PS->GetIntInRange(0, (int32)PS->ProgramTypes.size() - 1);
			Var.Name.AppendFormat("glob_%4s_%d", DeclType, i);
			SrcBuff->AppendFormat("%s %s %s;\n", DeclType, PS->ProgramTypes[Var.Type].Name.buffer, Var.Name.buffer);
		}
	};
	
	DeclareNumGlobalVars("attribute", NumAttributes);
	DeclareNumGlobalVars("varying", NumVarying);
	DeclareNumGlobalVars("uniform", NumUniforms);
}

void GenerateLiteralExpression(ProgramState* PS, TypeID DstType)
{
	switch (DstType)
	{
	case BT_Bool: {
		PS->ScratchExpressionList.push_back(StringStackBuffer<32>("%s", (PS->GetIntInRange(0,1) != 0) ? "true" : "false"));
	} break;
	case BT_Int: {
		PS->ScratchExpressionList.push_back(StringStackBuffer<32>("%d", PS->GetIntInRange(-20, 30)));
	} break;
	case BT_Float: {
		PS->ScratchExpressionList.push_back(StringStackBuffer<32>("%f", PS->GetFloatInRange(-2.0f, 2.0f)));
	} break;
	case BT_Vec2: {
		PS->ScratchExpressionList.push_back(StringStackBuffer<32>("vec2(%f, %f)", PS->GetFloatInRange(-2.0f, 2.0f), PS->GetFloatInRange(-2.0f, 2.0f)));
	} break;
	case BT_Vec3: {
		PS->ScratchExpressionList.push_back(StringStackBuffer<32>("vec3(%f, %f, %f)", PS->GetFloatInRange(-2.0f, 2.0f), PS->GetFloatInRange(-2.0f, 2.0f), PS->GetFloatInRange(-2.0f, 2.0f)));
	} break;
	case BT_Vec4: {
		PS->ScratchExpressionList.push_back(StringStackBuffer<32>("vec3(%f, %f, %f, %f)", PS->GetFloatInRange(-2.0f, 2.0f), PS->GetFloatInRange(-2.0f, 2.0f), PS->GetFloatInRange(-2.0f, 2.0f), PS->GetFloatInRange(-2.0f, 2.0f)));
	} break;
	default: {
		assert(false && "bad enum");
	} break;
	}
}

// TODO: How to deal with knowing which variables, and which fields have been init'd
bool GenerateExpression(ProgramState* PS, TypeID DstType, int ExprStackDepth = 0)
{
	const float Decider = PS->GetFloat01();

	// Basically, start out allowing recursion half the time, and then after a certain depth only recur 10% of the time to finish up in a reasonable time
	if (Decider < 0.5f || (Decider < 0.9f && ExprStackDepth > 3))
	{
		// Read a variable of that type if it exists
		// If it doesn't exist and it's a builtin type, issue a literal
		// If we don't have a variable and it's not a builtin type, bail out

		// TODO: Also index variables by type?
		int32 SearchStartOffset = PS->GetIntInRange(0, PS->VarsInScope.size() - 1);
		for (int32 i = 0; i < PS->VarsInScope.size(); i++)
		{
			const auto& VarInfo = PS->VarsInScope[(SearchStartOffset + i) % PS->VarsInScope.size()];
			if (VarInfo.Type == DstType)
			{
				PS->ScratchExpressionList.push_back(VarInfo.Name);
				return true;
			}
		}

		if (DstType < BT_Count)
		{
			GenerateLiteralExpression(PS, DstType);
			return true;
		}
		else
		{
			// We aren't recurring anymore, but we can't construct a value of this type with what we have
			return false;
		}
	}
	else
	{
		// Look for a data transformation that has the right destination type
		// Pick a random one to start with, but try them all until one works
		// If none works, bail

		auto StartAndEndDTTIndices = PS->DataTransformIndexByDstType[DstType];
		if (StartAndEndDTTIndices.first == StartAndEndDTTIndices.second)
		{
			return false;
		}

		int32 NumTransforms = StartAndEndDTTIndices.second - StartAndEndDTTIndices.first;
		int32 SearchStartOffset = PS->GetIntInRange(0, NumTransforms - 1);
		for (int32 i = 0; i < NumTransforms; i++)
		{
			const auto& CurrentTransform = PS->DataTransforms[StartAndEndDTTIndices.first + ((SearchStartOffset + i) % NumTransforms)];
			int32 CurrentSubExprStackSize = PS->ScratchExpressionList.size();

			bool Success = true;
			if (CurrentTransform.TransformType == DTT_FieldAccess)
			{
				assert(CurrentTransform.NumSrcTypes == 1);

				Success = GenerateExpression(PS, CurrentTransform.SrcTypes[0], ExprStackDepth + 1);
			}
			else if (CurrentTransform.TransformType == DTT_Func)
			{
				assert(CurrentTransform.NumSrcTypes >= 1);
				PS->ScratchExpressionList.push_back(StringStackBuffer<32>("%s", CurrentTransform.Name));
				PS->ScratchExpressionList.push_back(StringStackBuffer<32>("("));

				for (int32 i = 0; i < CurrentTransform.NumSrcTypes; i++)
				{
					if (i > 0)
					{
						PS->ScratchExpressionList.push_back(StringStackBuffer<32>(", "));
					}

					Success &= GenerateExpression(PS, CurrentTransform.SrcTypes[i], ExprStackDepth + 1);
					if (!Success)
					{
						break;
					}
				}

				PS->ScratchExpressionList.push_back(StringStackBuffer<32>(")"));
			}
			else if (CurrentTransform.TransformType == DTT_Op)
			{
				assert(CurrentTransform.NumSrcTypes == 2);

				PS->ScratchExpressionList.push_back(StringStackBuffer<32>("("));

				Success = GenerateExpression(PS, CurrentTransform.SrcTypes[0], ExprStackDepth + 1);

				if (Success)
				{
					PS->ScratchExpressionList.push_back(StringStackBuffer<32>("%s", CurrentTransform.Name));

					Success &= GenerateExpression(PS, CurrentTransform.SrcTypes[1], ExprStackDepth + 1);

					PS->ScratchExpressionList.push_back(StringStackBuffer<32>(")"));
				}
			}
			else
			{
				assert(false && "bad enum");
			}


			if (Success)
			{
				return true;
			}
			else
			{
				PS->ScratchExpressionList.resize(CurrentSubExprStackSize);
			}
		}

		return false;
	}
}

void WriteOutExpressionStackAsSourceString(ProgramState* PS, SourceBuffer* SrcBuff)
{
	for (const auto& ScratchSubexpr : PS->ScratchExpressionList)
	{
		SrcBuff->Append(ScratchSubexpr.buffer);
	}
}

void GenerateAssignmentStatement(ProgramState* PS, SourceBuffer* SrcBuff, const VariableInfo& VarInfo)
{
	bool Success = false;

	const int32 NumRetries = 4;
	for (int32 i = 0; i < NumRetries; i++)
	{
		Success = GenerateExpression(PS, VarInfo.Type);
		if (Success)
		{
			break;
		}
		else
		{
			PS->ScratchExpressionList.clear();
		}
	}

	if (Success)
	{
		SrcBuff->AppendFormat("\t%s = ", VarInfo.Name.buffer);
		WriteOutExpressionStackAsSourceString(PS, SrcBuff);
		SrcBuff->Append(";\n");

		PS->ScratchExpressionList.clear();
	}
	else
	{
		// Can't be one of the builtins, we should have fallbacks or something for those
		assert(VarInfo.Type >= BT_Count);
		const TypeInfo& VarTypeInfo = PS->ProgramTypes[VarInfo.Type];

		for (const auto& Field : VarTypeInfo.Fields)
		{
			VariableInfo VarFieldInfo;
			VarFieldInfo.Type = Field.Type;
			VarFieldInfo.Name.AppendFormat("%s.%s", VarInfo.Name, Field.Name);
			GenerateAssignmentStatement(PS, SrcBuff, VarFieldInfo);
		}
	}
}

void GenerateStatement(ProgramState* PS, SourceBuffer* SrcBuff)
{
	// Assignment or variable declaration
	// TODO: If statements, while loops, etc.

	const float Decider = PS->GetFloat01();

	if (PS->VarScopeCountStack.front() < PS->VarsInScope.size() && Decider < 0.6f)
	{
		int32 VarAssignIndex = PS->GetIntInRange(PS->VarScopeCountStack.front(), PS->VarsInScope.size() - 1);
		GenerateAssignmentStatement(PS, SrcBuff, PS->VarsInScope[VarAssignIndex]);
	}
	else
	{
		VariableInfo NewVarInfo;
		NewVarInfo.Type = PS->GetIntInRange(0, PS->ProgramTypes.size() - 1);
		NewVarInfo.Name.AppendFormat("temp_var_%d", (int32)PS->VarsInScope.size());

		SrcBuff->AppendFormat("\t%s %s;\n", PS->ProgramTypes[NewVarInfo.Type].Name.buffer, NewVarInfo.Name.buffer);

		GenerateAssignmentStatement(PS, SrcBuff, NewVarInfo);

		PS->VarsInScope.push_back(NewVarInfo);
	}

}

void GenerateFunctionBody(ProgramState* PS, SourceBuffer* SrcBuff)
{
	int32 NumStatements = PS->GetIntInRange(1, 10);

	for (int32 i = 0; i < NumStatements; i++)
	{
		GenerateStatement(PS, SrcBuff);
	}
}

void GenerateReturnStatement(ProgramState* PS, SourceBuffer* SrcBuff, TypeID RetType)
{
	VariableInfo RetValInfo;
	RetValInfo.Name.Append("_retval");
	RetValInfo.Type = RetType;

	GenerateAssignmentStatement(PS, SrcBuff, RetValInfo);

	SrcBuff->AppendFormat("\treturn %s;\n", RetValInfo.Name.buffer);
}

void GenerateUserDefinedFuncs(ProgramState* PS, SourceBuffer* SrcBuff)
{
	// TODO: Remember to add the data transformation as well
	// and IndexProgramDataTransformations()

	int32 NumUserFuncs = PS->GetIntInRange(0, 5);
	for (int32 i = 0; i < NumUserFuncs; i++)
	{
		PS->VarScopeCountStack.push_back(PS->VarsInScope.size());

		TypeID RetType = PS->GetIntInRange(0, PS->ProgramTypes.size() - 1);
		const auto& RetTypeInfo = PS->ProgramTypes[RetType];

		int32 NumParams = PS->GetIntInRange(1, 4);

		SrcBuff->AppendFormat("%s user_func_%d(", RetTypeInfo.Name.buffer);

		for (int32 p = 0; p < NumParams; p++)
		{
			if (p > 0)
			{
				SrcBuff->Append(", ");
			}

			TypeID ParamType = PS->GetIntInRange(0, PS->ProgramTypes.size() - 1);
			SrcBuff->AppendFormat("%s param_%d", PS->ProgramTypes[ParamType].Name.buffer, p);

			VariableInfo ParamVarInfo;
			ParamVarInfo.Type = ParamType;
			ParamVarInfo.Name.AppendFormat("param_%d", p);
			PS->VarsInScope.push_back(ParamVarInfo);
		}
		SrcBuff->AppendFormat(") {\n");

		GenerateFunctionBody(PS, SrcBuff);

		GenerateReturnStatement(PS, SrcBuff, RetType);

		SrcBuff->AppendFormat("}\n\n");

		PS->VarsInScope.resize(PS->VarScopeCountStack.back());
		PS->VarScopeCountStack.pop_back();
		assert(PS->VarScopeCountStack.size() == 0);
	}
}

void GenerateMainFunction(ProgramState* PS, SourceBuffer* SrcBuff)
{
	int32 CurrentNumVars = PS->VarsInScope.size();

	PS->VarsInScope.resize(CurrentNumVars);
}

void GenerateShaderSource(ProgramState* PS, SourceBuffer* SrcBuff, ShaderType InShaderType)
{
	// TODO: Init program outside generation loop once, and restore to it?
	InitProgramState(PS);

	GenerateUserDefinedStructs(PS, SrcBuff);
	IndexProgramDataTransformations(PS);

	GenerateGlobalVariables(PS, SrcBuff, InShaderType);

	GenerateUserDefinedFuncs(PS, SrcBuff);

	GenerateMainFunction(PS, SrcBuff);
}




#include <Windows.h>


int main(int argc, char** argv)
{
	printf("Sup.\n");

	ProgramState PS;
	PS.SetSeed(0x111);

	// Heap allocation just cause it's pretty big
	// Also: typedef as ctor name isn't portable afaik
	SourceBuffer* SrcBuff = new StringStackBuffer<MAX_SHADER_SOURCE_LEN>;
	GenerateShaderSource(&PS, SrcBuff, ShaderType::Frag);

	OutputDebugStringA("-----------\n");
	OutputDebugStringA(SrcBuff->buffer);
	OutputDebugStringA("\n-----------\n");

	delete SrcBuff;

	return 0;
}











