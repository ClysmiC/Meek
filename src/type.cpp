#include "type.h"

#include <string.h>

bool isTypeInferred(const Type & type)
{
	bool result = (strcmp(type.ident.pToken->lexeme, "var") == 0);
    return result;
}

bool isUnmodifiedType(const Type & type)
{
	return type.aTypemods.cItem == 0;
}
