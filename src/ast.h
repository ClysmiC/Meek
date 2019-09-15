#pragma once

#include "als.h"
#include "token.h"

// Forward declarations

struct AstNode;
struct ParseType;
struct ParseFuncType;



enum ASTCATK
{
	ASTCATK_Error,
	ASTCATK_Expr,
	ASTCATK_Stmt,
	ASTCATK_Info,
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
	ASTK_IllegalInitErr,
	ASTK_ChainedAssignErr,

	ASTK_ErrMax,

	// EXPR

	ASTK_UnopExpr,
	ASTK_BinopExpr,
	ASTK_LiteralExpr,
	ASTK_GroupExpr,
	ASTK_VarExpr,
	ASTK_ArrayAccessExpr,
	ASTK_FuncCallExpr,
		// TODO
		//	ASTK_FuncLiteralExpr,



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

	ASTK_StmtMax,		// Illegal value, used to determine ASTCATK

	// INFO

	// ASTK_ParamInfo,

	ASTK_InfoMax,

    ASTK_Program,

	// NOTE: If you add more misc ASTK's down here (i.e., not Stmt, Expr, Err), make sure to update
	//	the ASTCATK functions
};

// enum ASTERRK : u16
// {
// 	ASTERRK_Bubble,


// 	ASTERRK_UnexpectedTokenk,

// 	// Expected token kind

// 	ASTERRK_ExpectedExpr,
// 	ASTERRK_ExpectedCloseParen,
// 	ASTERRK_ExpectedCloseBracket,
// 	ASTERRK_ExpectedIdentifier,
// };



// IMPORTANT: All AstNodes in the tree are the exact same struct (AstNode). Switch on the ASTK to
//	get the kind of node, and then switch on the exprk or stmtk to get further details.
//	It is important that these unions are the first member in each struct, so that if you have
//	a pointer to a more detailed type, you can just cast it to an AstNode to access things like
//	its id
//
// If a specific kind of node is very large in memory, then EVERY node will be that large. Keep the nodes small!

// Errors

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
	TOKENK tokenk;
};

struct AstInitUnnamedVarErr {};

struct AstIllegalInitErr
{
	Token * pVarIdent;
};

struct AstChainedAssignErr {};

struct AstErr
{
	// NOTE: Error kind is encoded in ASTK
	// NOTE: Using same union trick for upcasting/downcasting that AstNode uses

	union
	{
		AstScanErr scanErr;
		AstBubbleErr bubbleErr;
		AstUnexpectedTokenkErr unexpectedTokenErr;
		AstExpectedTokenkErr expectedTokenErr;
		AstInitUnnamedVarErr unnamedVarErr;
		AstIllegalInitErr illegalInitErr;
		AstChainedAssignErr chainedAssignErr;
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
	// Better way to store identifier here? Should we store resolve info?

	// NOTE: pOwner is the left hand side of the '.' -- null means that it is
	//	just a plain old variable without a dot.

	AstNode * pOwner;
	Token * pIdent;
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



// Statements

struct AstExprStmt
{
	AstNode * pExpr;
};

struct AstAssignStmt
{
	AstNode * pLhsExpr;
	AstNode * pRhsExpr;
};

struct AstVarDeclStmt
{
	Token * pIdent;			// null means name is omitted (allowed in function in/out param types)
	ParseType * pType;		// null means inferred type while parsing
	AstNode * pInitExpr;	// null means default init

	// TODO: I want different values here when parsing and after typechecking.
	//	Namely, while parsing I want to store the expressions inside subscripts,
	//	and after typechecking those expressions should all be resolved to ints.
};

struct AstStructDefnStmt
{
	Token * pIdent;
	DynamicArray<AstNode *> apVarDeclStmt;
};

struct AstFuncDefnStmt
{
    Token * pIdent;
	ParseFuncType * pFuncType;

	DynamicArray<AstNode *> apStmts;
};



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

StaticAssert(sizeof(AstNode) == 32);		// Goal: Make it so 2 AstNodes fit in a cache line. This might be hard/impossible but it's my goal!
											//	If 32 bytes is too restrictive, relax this to 64 bytes

inline ASTCATK category(ASTK astk)
{
	if (astk < ASTK_ErrMax) return ASTCATK_Error;
	if (astk < ASTK_ExprMax) return ASTCATK_Expr;
	if (astk < ASTK_StmtMax) return ASTCATK_Stmt;
	if (astk < ASTK_InfoMax) return ASTCATK_Info;

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
