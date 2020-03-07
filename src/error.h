#pragma once

#include "als.h"

struct AstNode;
struct Parser;
struct Scanner;
struct Token;

// TODO: move these into print.h/.cpp

void reportScanError(const Scanner & scanner, const Token & tokenErr, const char * errFormat, ...);
void reportParseError(const Parser & parser, const AstNode & node, const char * errFormat, ...);

void reportIceAndExit(const char * errFormat, ...);
