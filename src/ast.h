#pragma once

#include "als.h"

#include "literal.h"
#include "symbol.h"
#include "token.h"
#include "type.h"

// Forward declarations

struct AstNode;
struct AstVarDeclStmt;
struct Type;

// TOOD: Assert versions of these? I.e., do an assert and then cast. Maybe use the , operator? LOL

// The downside to using a union instead of inheritance is that I can't implicitly upcast. Since there is quite
//	a bit of pointer casting required, these helpers make it slightly more succinct

#define Up(pNode) reinterpret_cast<AstNode *>(pNode)
#define UpConst(pNode) reinterpret_cast<const AstNode *>(pNode)
#define Down(pNode, astk) reinterpret_cast<Ast##astk *>(pNode)
#define DownConst(pNode, astk) reinterpret_cast<const Ast##astk *>(pNode)

// NOTE: Since everything is type punned, there is really no difference between up-casting and
//  down-casting to Err, Expr, etc. It is helpful, however to think of it in terms of up/down casts.

#define UpErr(pNode) reinterpret_cast<AstErr *>(pNode)
#define UpErrConst(pNode) reinterpret_cast<const AstErr *>(pNode)
#define DownErr(pNode) reinterpret_cast<AstErr *>(pNode)
#define DownErrConst(pNode) reinterpret_cast<const AstErr *>(pNode)

#define UpExpr(pNode) reinterpret_cast<AstExpr *>(pNode)
#define UpExprConst(pNode) reinterpret_cast<const AstExpr *>(pNode)
#define DownExpr(pNode) reinterpret_cast<AstExpr *>(pNode)
#define DownExprConst(pNode) reinterpret_cast<const AstExpr *>(pNode)

enum ASTCATK
{
	ASTCATK_Error,
	ASTCATK_Expr,
	ASTCATK_Stmt,
	ASTCATK_Grp,
	ASTCATK_Program
};

enum ASTK : u8
{
	// ERR

	// TODO: probably factor these into the kinds of errors that we attach extra information e.g. (unexpected tokenk or expected tokenk) to and
	//	the kinds that we don't. For the kinds that we don't, we could probably just make them all the same error kind and have an enum that
	//	that corresponds to which one it is?

	ASTK_ScanErr,
	ASTK_BubbleErr,
	ASTK_UnexpectedTokenkErr,
	ASTK_ExpectedTokenkErr,
	ASTK_InitUnnamedVarErr,
	ASTK_ChainedAssignErr,
	ASTK_IllegalDoPseudoStmtErr,
	ASTK_IllegalTopLevelStmtErr,
	ASTK_InvokeFuncLiteralErr,
	ASTK_ArrowAfterFuncSymbolExpr,

	ASTK_ErrMax,



	// EXPR

	ASTK_UnopExpr,
	ASTK_BinopExpr,
	ASTK_LiteralExpr,
	ASTK_GroupExpr,
	ASTK_SymbolExpr,
	ASTK_PointerDereferenceExpr,
	ASTK_ArrayAccessExpr,
	ASTK_FuncCallExpr,
	ASTK_FuncLiteralExpr,

	ASTK_ExprMax,		// Illegal value, used to determine ASTCATK



	// STMT

	ASTK_ExprStmt,
	ASTK_AssignStmt,	// This is a statement unless I find a good reason to make it an expression
	ASTK_VarDeclStmt,
	ASTK_FuncDefnStmt,
	ASTK_StructDefnStmt,
	ASTK_IfStmt,
	ASTK_WhileStmt,
	// TODO: for statement... once I figure out what I want it to look like
	ASTK_BlockStmt,
	ASTK_ReturnStmt,
	ASTK_BreakStmt,
	ASTK_ContinueStmt,

	ASTK_StmtMax,		// Illegal value, used to determine ASTCATK


	// GRP

	ASTK_ParamsReturnsGrp,

	ASTK_GrpMax,

	ASTK_Program,
};

// IMPORTANT: All AstNodes in the tree are the exact same struct (AstNode). Switch on the ASTK to
//	get the kind of node, and then switch on the exprk or stmtk to get further details.
//	It is important that these unions are the first member in each struct, so that if you have
//	a pointer to a more detailed type, you can just cast it to an AstNode to access things like
//	its id
//
// If a specific kind of node is very large in memory, then EVERY node will be that large. Keep the nodes small!

// Groups

struct AstParamsReturnsGrp
{
	DynamicArray<AstNode *> apParamVarDecls;
	DynamicArray<AstNode *> apReturnVarDecls;
};



// Errors

struct AstInternalCompilerErr
{
};

struct AstScanErr
{
	Token * pErrToken;
};

struct AstBubbleErr
{
};

struct AstUnexpectedTokenkErr
{
	// NOTE: This could just be a TOKENK but by the time you realize you have
	//	an unexpected token, you have often consumed it already (which means it
	//	is allocated!) so we might as well just attach the entire token.

	Token * pErrToken;
};

struct AstExpectedTokenkErr
{
	FixedArray<TOKENK, 4> aTokenkValid;
};

struct AstInitUnnamedVarErr {};

struct AstChainedAssignErr {};

struct AstIllegalDoPseudoStmtErr
{
	ASTK astkStmt;
};

struct AstIllegalTopLevelStmtErr
{
	ASTK astkStmt;
};

struct AstInvokeFuncLiteralErr {};

struct AstArrowAfterFuncSymbolExpr {};

struct AstErr
{
	// NOTE: Error kind is encoded in ASTK

	union
	{
		AstInternalCompilerErr internalCompilerErr;
		AstScanErr scanErr;
		AstBubbleErr bubbleErr;
		AstUnexpectedTokenkErr unexpectedTokenErr;
		AstExpectedTokenkErr expectedTokenErr;
		AstInitUnnamedVarErr unnamedVarErr;
		AstChainedAssignErr chainedAssignErr;
		AstIllegalDoPseudoStmtErr illegalDoPseudoStmtErr;
		AstIllegalTopLevelStmtErr illegalTopLevelStmt;
		AstInvokeFuncLiteralErr invokeFuncLiteralErr;
	};

	// Errors propogate up the AST, but still hang on to their child nodes so that
	//	we can clean whatever information we can from their valid children.

	DynamicArray<AstNode *> apChildren;
};
#if 0
static constexpr uint s_nodeSizeDebug = sizeof(AstErr);
#endif



// Expressions


struct AstUnopExpr
{
	Token *	pOp;
	AstNode * pExpr;
};

struct AstBinopExpr
{
	Token *	pOp;
	AstNode * pLhsExpr;
	AstNode * pRhsExpr;
};

struct AstLiteralExpr
{
	Token *	pToken;

	LITERALK literalk;      // Kind of redundant with pToken->tokenk
	union
	{
		int intValue;
		float floatValue;
		bool boolValue;
		char * strValue;	// TODO: Where does this point into... do we strip the quotes in the lexeme buffer?
	};

	bool isValueSet = false;
	bool isValueErroneous = false;	// Things like integers that are too large, etc.

#if 0
	static constexpr uint s_nodeSizeDebug = sizeof(AstLiteralExpr);
#endif
};

struct AstGroupExpr
{
	AstNode * pExpr;
};

enum SYMBEXPRK
{
	SYMBEXPRK_Unresolved,
	SYMBEXPRK_Var,
	SYMBEXPRK_MemberVar,
	SYMBEXPRK_Func
};

struct AstSymbolExpr
{
	SYMBEXPRK symbexprk;

	Token * pTokenIdent;

	union
	{
		struct UUnresolvedData
		{
			bool ignoreVars;
			DynamicArray<SymbolInfo> aCandidates;		// Candidates set by resolve pass. Parent node is responsible for choosing the correct candidate.
		} unresolvedData;

		struct UVarData
		{
			NULLABLE AstVarDeclStmt * pDeclCached;		// Set in resolve pass. Not a child node.
		} varData;

		struct UMemberData
		{
			AstNode * pOwner;
			NULLABLE AstVarDeclStmt * pDeclCached;		// Set in resolve pass. Not a child node.
		} memberData;

		struct UFuncData
		{
			DynamicArray<TYPID> aTypidDisambig;			// Disambiguating typeid's supplied by the programmer
			NULLABLE AstFuncDefnStmt * pDefnCached;		// Set in resolve pass. Not a child node.
		} funcData;
	};
};
#if 0
static constexpr uint s_nodeSizeDebug = sizeof(AstSymbolExpr);
#endif

struct AstPointerDereferenceExpr
{
	AstNode * pPointerExpr;
};

struct AstArrayAccessExpr
{
	AstNode * pArrayExpr;
	AstNode * pSubscriptExpr;
};

struct AstFuncCallExpr
{
	AstNode * pFunc;
	DynamicArray<AstNode *> apArgs;		// Args are EXPR
};

struct AstFuncLiteralExpr
{
	AstParamsReturnsGrp * pParamsReturnsGrp;
	AstNode * pBodyStmt;

	SCOPEID scopeid;		// Scope introduced by this func defn

#if 0
	static constexpr uint s_nodeSizeDebug = sizeof(AstFuncLiteralExpr);
#endif
};

struct AstExpr
{
	// NOTE: Error kind is encoded in ASTK

	union
	{
		AstUnopExpr					unopExpr;
		AstBinopExpr				binopExpr;
		AstLiteralExpr				literalExpr;
		AstGroupExpr				groupExpr;
		AstSymbolExpr				symbolExpr;
		AstPointerDereferenceExpr	pointerDereferenceExpr;
		AstArrayAccessExpr			arrayAccessExpr;
		AstFuncCallExpr				funcCallExpr;
		AstFuncLiteralExpr			funcLiteralExpr;
	};

	TYPID typidEval = TYPID_Unresolved;
};
#if 0
static constexpr uint s_nodeSizeDebug = sizeof(AstExpr);
#endif



// Statements

struct AstExprStmt
{
	AstNode * pExpr;
};

struct AstAssignStmt
{
	Token * pAssignToken;	// Distinguish between =, +=, -=, etc.
	AstNode * pLhsExpr;
	AstNode * pRhsExpr;
};

struct AstVarDeclStmt
{
	ScopedIdentifier ident;

	NULLABLE AstNode * pInitExpr;

	VARSEQID varseqid;
	TYPID typidDefn;
};

struct AstStructDefnStmt
{
	ScopedIdentifier ident;
	DynamicArray<AstNode *> apVarDeclStmt;
	SCOPEID scopeid;        // Scope introduced by this struct defn
	TYPID typidDefn;
};
#if 0
	static constexpr uint s_nodeSizeDebug = sizeof(AstStructDefnStmt);
#endif

struct AstFuncDefnStmt
{
	ScopedIdentifier ident;

	AstParamsReturnsGrp * pParamsReturnsGrp;

	AstNode * pBodyStmt;
	SCOPEID scopeid;        // Scope introduced by this func defn
	TYPID typidDefn;
};
#if 0
	static constexpr uint s_nodeSizeDebug = sizeof(AstFuncDefnStmt);
#endif

struct AstBlockStmt
{
	DynamicArray<AstNode *> apStmts;
	SCOPEID scopeid;
};

struct AstIfStmt
{
	AstNode * pCondExpr;
	AstNode * pThenStmt;
	AstNode * pElseStmt;
};

struct AstWhileStmt
{
	AstNode * pCondExpr;
	AstNode * pBodyStmt;
};

struct AstReturnStmt
{
	AstNode * pExpr;
};

struct AstBreakStmt {};
struct AstContinueStmt {};



// Program

struct AstProgram
{
	DynamicArray<AstNode *> apNodes;
};



// Node

struct AstNode
{
	AstNode() {}

	// First union trick is to force this struct to 64 bytes

	union
	{

		struct
		{
			// Second union trick is to make AstNode of every type really be the exact
			//	same struct under the hood. See note at top of file.

			union
			{
				// ERR

				AstErr					err;

				// EXPR

				AstExpr					expr;

				// STMT

				AstExprStmt				exprStmt;
				AstAssignStmt			assignExpr;
				AstVarDeclStmt			varDeclStmt;
				AstFuncDefnStmt			funcDefnStmt;
				AstStructDefnStmt		structDefnStmt;
				AstIfStmt				ifStmt;
				AstWhileStmt			whileStmt;
				AstBlockStmt			blockStmt;
				AstReturnStmt			returnStmt;
				AstBreakStmt			breakStmt;
				AstContinueStmt			continueStmt;

				// GRP

				AstParamsReturnsGrp		paramsReturnsGrp;

				// PROGRAM

				AstProgram				program;
			};

			// NOTE: These fields MUST be after the above union in the struct

			ASTK				astk;

			ASTID			    astid;

			// Indices into source file

			// int					iStart;
			// int                 iEnd;
		};

		char _padding[64];
	};
};

// TODO: try to make this true again:

// StaticAssert(sizeof(AstNode) <= 64);		// Goal: Make it so each AstNode fits in a cache line.
											//  For any additional per-node data that isn't "hot", use decorator tables. See ast_decorate.h/.cpp
											// TODO: Make sure we have an option for our allocator to align these to cache lines!
											//  (have dynamic allocator allocate an extra 64 bytes and point the "actual" buffer to
											//  an aligned spot?

#if 0
static constexpr uint s_nodeSizeDebug = sizeof(AstNode);
#endif



// Convenient accessors that handle casting/reaching through GRP nodes

//DynamicArray<AstNode *> * paramVarDecls(const AstFuncDefnStmt & stmt);
//DynamicArray<AstNode *> * paramVarDecls(const AstFuncLiteralExpr & expr);
//
//DynamicArray<AstNode *> * returnVarDecls(const AstFuncDefnStmt & stmt);
//DynamicArray<AstNode *> * returnVarDecls(const AstFuncLiteralExpr & expr);
//
//ScopedIdentifier * ident(const AstFuncDefnStmt & stmt);
//SYMBSEQID * symbseqid(const AstFuncDefnStmt & stmt);

// TODO: other "value" functions
// TODO: move to literal.h / literal.cpp

int intValue(AstLiteralExpr * pLiteralExpr);

bool isLValue(ASTK astk);

inline ASTCATK category(ASTK astk)
{
	if (astk < ASTK_ErrMax) return ASTCATK_Error;
	if (astk < ASTK_ExprMax) return ASTCATK_Expr;
	if (astk < ASTK_StmtMax) return ASTCATK_Stmt;
	if (astk < ASTK_GrpMax) return ASTCATK_Grp;

	Assert(astk == ASTK_Program);
	return ASTCATK_Program;
}

inline ASTCATK category(const AstNode & node)
{
	return category(node.astk);
}

inline bool isErrorNode(const AstNode & node)
{
	return category(node.astk) == ASTCATK_Error;
}

bool containsErrorNode(const DynamicArray<AstNode *> & apNodes);

const char * displayString(ASTK astk, bool capitalizeFirstLetter=false);

