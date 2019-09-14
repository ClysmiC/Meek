#include "type.h"

#include <string.h>

bool isTypeInferred(const ParseType & parseType)
{
	bool result = (strcmp(parseType.pType->lexeme, "var") == 0);
    return result;
}

bool isUnmodifiedType(const ParseType & parseType)
{
	return parseType.aTypemods.cItem == 0;
}
