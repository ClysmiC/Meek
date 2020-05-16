#pragma once

#include "als.h"

#include "id_def.h"
#include "token.h"

struct AstNode;
struct AstVarDeclStmt;
struct AstFuncDefnStmt;
struct AstStructDefnStmt;
struct Lexeme;
struct MeekCtx;
struct Token;
struct Type;

enum SYMBOLK
{
	SYMBOLK_Var,
	SYMBOLK_Func,
	SYMBOLK_Struct,

	SYMBOLK_BuiltInType,

	SYMBOLK_Nil
};

struct SymbolInfo
{
	SYMBOLK symbolk;

	union
	{
		struct UVarData
		{
			AstVarDeclStmt * pVarDeclStmt;
			uintptr byteOffset;
		} varData;

		struct UFuncData
		{
			AstFuncDefnStmt * pFuncDefnStmt;
		} funcData;

		struct UStructData
		{
			AstStructDefnStmt * pStructDefnStmt;
		} structData;

		struct UBuiltInData
		{
			TYPID typid;
		} builtInData;
	};
};

enum SCOPEK : s8
{
	SCOPEK_BuiltIn,
	SCOPEK_Global,
	SCOPEK_StructDefn,

	SCOPEK_FuncTopLevel,	// Outermost scope of func. Owns parameters.
	SCOPEK_FuncInner,		// Control flow, user-introduced scopes, etc.

	SCOPEK_Nil = -1
};

struct Scope
{
	NULLABLE Scope * pScopeParent = nullptr;
	SCOPEID id = SCOPEID_Nil;
	SCOPEK scopek = SCOPEK_Nil;

	union
	{
		struct UBuiltInData
		{

		} builtInData;

		struct UGlobalData
		{
			uintptr cByteGlobalVariable;
		} globalData;

		struct UStructDefnData
		{
			uintptr cByteMember;
		} structDefnData;

		struct UFuncTopLevelData
		{
			uintptr cByteLocalVariable;
			uintptr cByteParam;
		} funcTopLevelData;

		struct UFuncInnerData
		{
			uintptr cByteLocalVariable;
		} funcInnerData;
	};

	HashMap<Lexeme, DynamicArray<SymbolInfo>> symbolsDefined;
};

struct ScopedIdentifier
{
	Lexeme lexeme;
	SCOPEID scopeid;
};

u32 scopedIdentHash(const ScopedIdentifier & ident);
bool scopedIdentEq(const ScopedIdentifier & ident0, const ScopedIdentifier & ident1);

// Flags for SYMBol Query

enum FSYMBQ : u16
{
	FSYMBQ_IgnoreVars		= 0x1 << 0,
	FSYMBQ_IgnoreTypes		= 0x1 << 1,
	FSYMBQ_IgnoreFuncs		= 0x1 << 2,

	FSYMBQ_IgnoreParent		= 0x1 << 3,
	FSYMBQ_SortVarseqid		= 0x1 << 4,
	FSYMBQ_IgnoreParamVars	= 0x1 << 5,

	GRFSYMBQ_None			= 0
};
typedef u16 GRFSYMBQ;

void init(Scope * pScope, SCOPEID scopeid, SCOPEK scopek, Scope * pScopeParent);
void defineSymbol(Scope * pScope, const Lexeme & lexeme, const SymbolInfo & symbInfo);

bool auditSymbolsAndSetFuncids(MeekCtx * pCtx, Scope * pScope);
void computeScopedVariableOffsets(MeekCtx * pCtx, Scope * pScope);

int compareVarseqid(const SymbolInfo & s0, const SymbolInfo & s1);
SCOPEID scopeidFromSymbolInfo(const SymbolInfo & symbInfo);
SymbolInfo lookupVarSymbol(const Scope & scope, const Lexeme & lexeme, GRFSYMBQ grfsymbq = GRFSYMBQ_None);
SymbolInfo lookupTypeSymbol(const Scope & scope, const Lexeme & lexeme, GRFSYMBQ grfsymbq = GRFSYMBQ_None);
void lookupFuncSymbol(const Scope & scope, const Lexeme & lexeme, DynamicArray<SymbolInfo> * poResult, GRFSYMBQ grfsymbq = GRFSYMBQ_None);

void lookupSymbol(const Scope & scope, const Lexeme & lexeme, DynamicArray<SymbolInfo> * poResult, GRFSYMBQ grfsymbq=GRFSYMBQ_None);
void lookupAllVars(const Scope & scope, DynamicArray<SymbolInfo> * poResult, GRFSYMBQ grfsymbq = GRFSYMBQ_None);

void updateVarSymbolOffset(const Scope & scope, const Lexeme & lexeme, u32 cByteOffset);
uintptr cByteLocalVars(const Scope & scope);

bool isDeclarationOrderIndependent(SYMBOLK symbolk);
bool isDeclarationOrderIndependent(const SymbolInfo & info);
