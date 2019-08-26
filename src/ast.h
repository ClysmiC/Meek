#pragma once

#include "als.h"

#include "token.h"

struct AstExpr;
struct AstStmt;
struct AstNode;

enum ASTCATK
{
	ASTCATK_Error,
	ASTCATK_Expr,
	ASTCATK_Stmt
};

enum ASTK : u8
{
	ASTK_Error,

	// EXPR

	ASTK_UnopExpr,
	ASTK_BinopExpr,
	ASTK_LiteralExpr,
	ASTK_GroupExpr,

	ASTK_ExprMax,		// Illegal value, used to determine ASTCATK

	// STMT

	ASTK_ExprStmt,
	ASTK_AssignStmt,	// This is a statement unless I find a good reason to make it an expression
	ASTK_VarDeclStmt,
	ASTK_FunDeclStmt,

	ASTK_StmtMax		// Illegal value, used to determine ASTCATK
};

enum ASTERRK : u16
{
	ASTERRK_ExpectedExpr,
	ASTERRK_UnexpectedEof,
	ASTERRK_ExpectedCloseParen,
};



// IMPORTANT: All AstNodes in the tree are the exact same struct (AstNode). Switch on the ASTK to
//	get the kind of node, and then switch on the exprk or stmtk to get further details.
//	It is important that these unions are the first member in each struct, so that if you have
//	a pointer to a more detailed type, you can just cast it to an AstNode to access things like
//	its id
//
// If a specific kind of node is very large in memory, then EVERY node will be that large. Keep the nodes small!

struct AstError
{
	// Errors propogate up the AST

	AstNode * pChildren[2] = { 0 };

	ASTERRK asterrk;
};

// Expressions


struct AstUnopExpr
{
	Token *			pOp;
	AstNode *		pExpr;
};

struct AstBinopExpr
{
	Token *			pOp;
	AstNode *		pLhsExpr;
	AstNode *		pRhsExpr;
};

struct AstLiteralExpr
{
	Token *			pToken;
};

struct AstGroupExpr
{
	AstNode *		pExpr;
};



// Statements

struct AstExprStmt
{
	AstNode *		pExpr;
};

struct AstAssignStmt
{
	AstNode * pLhsExpr;
	AstNode * pRhsExpr;
};

struct AstVarDeclStmt
{
	// TODO
};

struct AstFunDeclStmt
{
	// TODO
};

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
				AstError			error;

				// EXPR

				AstUnopExpr			unopExpr;
				AstBinopExpr		binopExpr;
				AstLiteralExpr		literalExpr;
				AstGroupExpr		groupExpr;

				// STMT

				AstAssignStmt		assignExpr;
			};

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
	if (astk == ASTK_Error) return ASTCATK_Error;
	if (astk < ASTK_ExprMax) return ASTCATK_Expr;

	Assert(astk < ASTK_StmtMax);
	return ASTCATK_Stmt;
}

inline ASTCATK category(AstNode * pNode)
{
	return category(pNode->astk);
}

inline bool isErrorNode(AstNode * pNode)
{
	return pNode->astk == ASTK_Error;
}
