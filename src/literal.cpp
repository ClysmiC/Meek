#include "literal.h"

#include "error.h"

LITERALK literalkFromTokenk(TOKENK tokenk)
{
    switch (tokenk)
    {
        case TOKENK_IntLiteral:     return LITERALK_Int;
        case TOKENK_FloatLiteral:   return LITERALK_Float;
        case TOKENK_BoolLiteral:    return LITERALK_Bool;
        case TOKENK_StringLiteral:  return LITERALK_String;
        default:
            reportIceAndExit("Tokenk %d is not a literal kind", tokenk);
            return LITERALK_Nil;
    }
}