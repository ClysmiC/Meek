#pragma once

#include "als.h"

#include <stdarg.h>
#include <stdio.h>

struct AstNode;
struct Parser;
struct Scanner;
struct Token;

void reportScanError(const Scanner & scanner, const Token & tokenErr, const char * errFormat, ...);
void reportParseError(const Parser & parser, const AstNode & node, const char * errFormat, ...);

void reportIceAndExit(const char * errFormat, ...);
