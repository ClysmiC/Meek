#ifdef _WIN32
	#define _CRT_SECURE_NO_DEPRECATE
#endif

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

	char * lexemeBuffer = new char[bytesRead];
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
	if (!init(&scanner, buffer, bytesRead, lexemeBuffer, bytesRead))
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

    parse(&parser);

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

