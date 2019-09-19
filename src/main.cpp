#include "als.h"

#include "ast.h"
#include "scan.h"
#include "parse.h"

#include <stdio.h>

int main()
{
    // printf("Size of AstNode %d", sizeof(AstNode));

	char * filename = "C:/Users/Andrew/Desktop/lang/lang/test.cly";		// TODO: Read this in from command line
	int bufferSize = 1024 * 1024;										// TODO: Support files bigger than 1 mb. Maybe have scanner return a special value when its buffer is full and it will ask you to pass it a new one

	char * buffer = new char[bufferSize];
	Defer(delete[] buffer);

	FILE * file = fopen(filename, "rb");
	Defer(fclose(file));

	int bytesRead = file ? fread(buffer, 0x1, bufferSize, file) : -1;

    // NOTE: Lexeme buffer size is multiplied by 2 to handle worst case scenario where each byte is its own lexeme,
    //  which gets written into the buffer and then null terminated.

    int lexemeBufferSize = bytesRead * 2;
	char * lexemeBuffer = new char[bytesRead * 2];
	Defer(delete[] lexemeBuffer);

	if (bytesRead < 0)
	{
		fprintf(stderr, "Error opening file\n");
		return 1;
	}
	else if (bytesRead == bufferSize)
	{
		fprintf(stderr, "Files larger than %d bytes are not yet supported", bufferSize);
		return 1;
	}

	Scanner scanner;
	if (!init(&scanner, buffer, bytesRead, lexemeBuffer, lexemeBufferSize))
	{
		// TODO: distinguish this as internal compiler error

		fprintf(stderr, "Failed to initialize scanner");
		return 1;
	}

    Parser parser;
    if (!init(&parser, &scanner))
    {
        // TODO: distinguish this as internal compiler error

        fprintf(stderr, "Failed to initialize parser");
        return 1;
    }

    bool success;
    AstNode * pAst = parseProgram(&parser, &success);

#if DEBUG
    debugPrintAst(*pAst);

    printf("\n");
    if (parser.hadError)
    {
        printf("Parse had error(s)!\n");
    }
    else
    {
        printf("No parse errors :)\n");
    }
#endif

	/*Token token;
	while (nextToken(&scanner, &token) != TOKENK_Eof)
	{
		if (token.tokenk == TOKENK_Error)
		{
			fprintf(stderr, "Error token\n");
		}
		else
		{
			fprintf(stdout, "%s : %d\n", token.lexeme, token.tokenk);
		}
	}*/

	getchar();
}
