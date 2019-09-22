#pragma once

#include "als.h"
#include "token.h"
#include "symbol.h"

// Forward declarations

struct AstNode;
struct Type;
struct FuncType;



enum ASTCATK
{
	ASTCATK_Error,
	ASTCATK_Expr,
	ASTCATK_Stmt,
	ASTCATK_Program
};

enum ASTK : u8
{
	// ERR

	// TODO: probably factor these into the kinds of errors that we attach extra information e.g. (unexpected tokenk or expected tokenk) to and
	//	the kinds that we don't. For the kinds that we don't, we could probably just make them all the same error kind and have an enum that
	//	that corresponds to which one it is?

	ASTK_InternalCompilerErr,	// TODO: add error codes!
	ASTK_ScanErr,
	ASTK_BubbleErr,
	ASTK_UnexpectedTokenkErr,
	ASTK_ExpectedTokenkErr,
	ASTK_InitUnnamedVarErr,
	ASTK_IllegalInitErr,
	ASTK_ChainedAssignErr,
	ASTK_IllegalDoStmtErr,
	ASTK_InvokeFuncLiteralErr,
	ASTK_SymbolRedefinitionErr,

	ASTK_ErrMax,



	// EXPR

	ASTK_UnopExpr,
	ASTK_BinopExpr,
	ASTK_LiteralExpr,
	ASTK_GroupExpr,
	ASTK_VarExpr,
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



    ASTK_Program,

	// NOTE: If you add more misc ASTK's down here (i.e., not Stmt, Expr, Err), make sure to update
	//	the ASTCATK functions
};



// IMPORTANT: All AstNodes in the tree are the exact same struct (AstNode). Switch on the ASTK to
//	get the kind of node, and then switch on the exprk or stmtk to get further details.
//	It is important that these unions are the first member in each struct, so that if you have
//	a pointer to a more detailed type, you can just cast it to an AstNode to access things like
//	its id
//
// If a specific kind of node is very large in memory, then EVERY node will be that large. Keep the nodes small!

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

struct AstIllegalInitErr
{
	Token * pVarIdent;
};

struct AstChainedAssignErr {};

struct AstIllegalDoStmtErr
{
	ASTK astkStmt;
};

struct AstInvokeFuncLiteralErr {};

struct AstSymbolRedefinitionErr
{
	Token * pDefnToken;
	Token * pRedefnToken;
};

struct AstErr
{
	// NOTE: Error kind is encoded in ASTK
	// NOTE: Using same union trick for upcasting/downcasting that AstNode uses

	union
	{
		AstInternalCompilerErr internalCompilerErr;
		AstScanErr scanErr;
		AstBubbleErr bubbleErr;
		AstUnexpectedTokenkErr unexpectedTokenErr;
		AstExpectedTokenkErr expectedTokenErr;
		AstInitUnnamedVarErr unnamedVarErr;
		AstIllegalInitErr illegalInitErr;
		AstChainedAssignErr chainedAssignErr;
        AstIllegalDoStmtErr illegalDoStmtErr;
		AstInvokeFuncLiteralErr invokeFuncLiteralErr;
		AstSymbolRedefinitionErr symbolRedefinitionErr;
	};

	// Errors propogate up the AST, but still hang on to their child nodes so that
	//	we can clean whatever information we can from their valid children.

	DynamicArray<AstNode *> apChildren;
};



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
};

struct AstGroupExpr
{
	AstNode * pExpr;
};

struct AstVarExpr
{
	// NOTE: pOwner is the left hand side of the '.' -- null means that it is
	//	just a plain old variable without a dot.

	AstNode * pOwner;
	ResolvedIdentifier ident;
};

struct AstArrayAccessExpr
{
	AstNode * pArray;
	AstNode * pSubscript;
};

struct AstFuncCallExpr
{
	AstNode * pFunc;
	DynamicArray<AstNode *> apArgs;		// Args are EXPR
};

struct AstFuncLiteralExpr
{
	FuncType * pFuncType;
	AstNode * pBodyStmt;
};



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
	ResolvedIdentifier ident;

	Type * pType;
	AstNode * pInitExpr;	// null means default init


	// TODO: I want different values here when parsing and after typechecking.
	//	Namely, while parsing I want to store the expressions inside subscripts,
	//	and after typechecking those expressions should all be resolved to ints.
};

struct AstStructDefnStmt
{
	ResolvedIdentifier ident;
	DynamicArray<AstNode *> apVarDeclStmt;
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

struct AstFuncDefnStmt
{
	ResolvedIdentifier ident;
	FuncType * pFuncType;
	AstNode * pBodyStmt;
};

struct AstBlockStmt
{
	DynamicArray<AstNode *> apStmts;
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
    // Compiler implicitly deleted this constructor because of the union trickery it seems. Lame.

    AstNode() {}

	// First union trick is to force this struct to 32 bytes

	union
	{

		struct
		{
			// Second union trick is to make AstNode of every type really be the exact
			//	same struct under the hood. See note at top of file.

			union
			{
				// ERR

				AstErr  			err;

				// EXPR

				AstUnopExpr			unopExpr;
				AstBinopExpr		binopExpr;
				AstLiteralExpr		literalExpr;
				AstGroupExpr		groupExpr;
				AstVarExpr			varExpr;
				AstArrayAccessExpr	arrayAccessExpr;
				AstFuncCallExpr		funcCallExpr;

				// STMT

                AstExprStmt         exprStmt;
				AstAssignStmt		assignExpr;
                AstVarDeclStmt      varDeclStmt;
                AstFuncDefnStmt     funcDefnStmt;
                AstStructDefnStmt   structDefnStmt;
				AstIfStmt			ifStmt;
				AstWhileStmt		whileStmt;
				AstBlockStmt		blockStmt;
				AstReturnStmt		returnStmt;
				AstBreakStmt		breakStmt;
				AstContinueStmt		continueStmt;

				// PROGRAM

				AstProgram			program;
			};

            // NOTE: These fields MUST be after the above union in the struct

			ASTK				astk;

			int					id;
			int					startLine;
		};

        char _padding[32];
	};
};

StaticAssert(sizeof(AstNode) <= 64);		// Goal: Make it so each AstNode fits in a cache line
                                            // TODO: Maybe try to work this down to 32 bytes so 2 fit in a cache line!
                                            // TODO: Make sure we have an option for our allocator to align these to cache lines!
                                            //  (have dynamic allocator allocate an extra 64 bytes and point the "actual" buffer to
                                            //  an aligned spot?

inline ASTCATK category(ASTK astk)
{
	if (astk < ASTK_ErrMax) return ASTCATK_Error;
	if (astk < ASTK_ExprMax) return ASTCATK_Expr;
	if (astk < ASTK_StmtMax) return ASTCATK_Stmt;

	Assert(astk == ASTK_Program);
	return ASTCATK_Program;
}

inline ASTCATK category(const AstNode & node)
{
	return category(node.astk);
}

inline bool isErrorNode(const AstNode & node)
{
	return node.astk < ASTK_ErrMax;
}

bool containsErrorNode(const DynamicArray<AstNode *> & apNodes);

const char * displayString(ASTK astk);

#if 0
// Convenient place to hover the mouse and get size info for different kinds of nodes!

constexpr uint convenientSizeDebugger = sizeof(AstErr);
#endif
