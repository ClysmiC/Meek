#pragma once

#include "als.h"

#include "token.h"

struct AstExpr;
struct AstStmt;
struct AstNode;

enum ASTNODEK : u8
{
	ASTNODEK_Expr,
	ASTNODEK_Stmt,
};

enum EXPRK : u8
{
	EXPRK_Assignment,

	EXPRK_Unop,
	EXPRK_Binop,

	EXPRK_Literal,
	EXPRK_Group,
};

enum STMTK : u8
{
	STMTK_Expr,

	STMTK_VarDecl,
	STMTK_FunDecl
};



// IMPORTANT: All AstNodes in the tree are the exact same struct (AstNode). Switch on the ASTNODEK to
//	get the kind of node, and then switch on the exprk or stmtk to get further details.
//	It is important that these unions are the first member in each struct, so that if you have
//	a pointer to a more detailed type, you can just cast it to an AstNode to access things like
//	its id
//
// If a specific kind of node is very large in memory, then EVERY node will be that large. Keep the nodes small!


// Expressions

struct AstAssignmentExpr
{
	// TODO
};

struct AstUnopExpr
{
	Token *			pOp;
	AstExpr *		pExpr;
};

struct AstBinopExpr
{
	Token *			pOp;
	AstExpr *		pLhs;
	AstExpr *		pRhs;
};

struct AstLiteralExpr
{
	Token *			pToken;
};

struct AstGroupExpr
{
	AstExpr *		pExpr;
};



// Statements

struct AstExprStmt
{
	AstExpr *		pExpr;
};

struct AstVarDeclStmt
{
	// TODO
};

struct AstFunDeclStmt
{
	// TODO
};



// Node Definition

struct AstExpr
{
	union
	{
		AstAssignmentExpr	assignmentExpr;
		AstUnopExpr			unopExpr;
		AstBinopExpr		binopExpr;
		AstLiteralExpr		literalExpr;
		AstGroupExpr		groupExpr;
	};

	EXPRK					exprk;
};

struct AstStmt
{
	union
	{
	};

	STMTK				stmtk;
};

struct AstNode
{
	// First union trick is to force this struct to 32 bytes

	union
	{
		struct
		{
			// Second union trick is to make AstNode of every type really be the exact
			//	same struct under the hood. See note at top of file.

			union
			{
				AstExpr			expr;
				AstStmt			stmt;
			};

			ASTNODEK			astnodek;

			int					id;
			int					startLine;
		};

		char _padding[32];
	};
};

StaticAssert(sizeof(AstNode) == 32);		// Goal: Make it so 2 AstNodes fit in a cache line. This might be hard/impossible but it's my goal!
											//	If 32 bytes is too restrictive, relax this to 64 bytes
