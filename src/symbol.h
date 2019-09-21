#pragma once

#include "als.h"

struct ParseFuncType;
struct ParseType;
struct Token;

typedef s32 scopeid;
extern const scopeid gc_unresolvedScopeid;

struct Identifier
{
	Token * pToken;
	scopeid declScopeid;		// -1 means unresolved

	// Cached

	u32 hash;
};

enum SYMBOLK
{
	SYMBOLK_Var,
	SYMBOLK_Func,
	SYMBOLK_Struct,
};

enum VARK
{
	VARK_StructOrBuiltin,
	VARK_Func
};

struct SymbolInfo;

struct VarInfo
{
	SymbolInfo * pInfo;

	VARK vark;

	union
	{
		// TODO: These probably need to get replaced with more "solid" resolved types

		ParseType * pParseType;				// VARK_StructOrBuiltin
		ParseFuncType * pParseFuncType;		// VARK_Func
	};
};

struct StructInfo
{
	SymbolInfo * pInfo;
	HashMap<Identifier, VarInfo*> fields;
};

struct FuncInfo
{
	SymbolInfo * pInfo;
	DynamicArray<SymbolInfo*> aParamsIn;
	DynamicArray<SymbolInfo*> aParamsOut;
};

struct SymbolInfo
{
	Identifier * pIdent;

	SYMBOLK symbolk;

	// HMM: Could make this union the first field and use type punning to save pointers
	//	back to the SymbolInfo

	union
	{
		VarInfo varInfo;		// SYMBOLK_Var
		FuncInfo funcInfo;		// SYMBOLK_Func
		StructInfo structInfo;	// SYMBOLK_Struct
	};
};

void init(SymbolInfo * pSymbolInfo, SYMBOLK symbolk, Identifier * pIdent);

void setIdent(Identifier * pIdentifier, Token * pToken, scopeid declScopeid);
void setIdentUnresolved(Identifier * pIdentifier, Token * pToken);

u32 identHash(const Identifier & ident);
u32 identHashPrecomputed(const Identifier & i);
bool identEq(const Identifier & i0, const Identifier & i1);
