#include "als.h"

#include "ast.h"
#include "scan.h"
#include "parse.h"

#include <stdio.h>

uint hashFn(const uint & i)
{
    uint result = i;
    result = ((result >> 16) ^ result) * 0x45d9f3b;
    result = ((result >> 16) ^ result) * 0x45d9f3b;
    result = (result >> 16) ^ result;
    return result;
}

bool equalFn(const uint & i0, const uint & i1)
{
    return i0 == i1;
}

int main()
{
    // printf("Size of AstNode %d", sizeof(AstNode));

#if 1
    {
        HashMap<uint, uint> map;
        init(&map, hashFn, equalFn, 47);

        Assert(map.cCapacity == 64);
        
        uint cItemsTest = 100'000;   // TODO: test with 1,000,000 ... it seems to break there
        for (uint i = 0; i < cItemsTest; i++)
        {
            if (i == 257)
            {
                int x = 0;
            }

            insert(&map, i, i + 5);
        }

        for (uint i = 0; i < cItemsTest; i++)
        {
            if (i == 257)
            {
                int x = 0;
            }

            uint value;
            Verify(lookup(&map, i, &value));
            Assert(value == i + 5);
        }

        for (uint i = 0; i < cItemsTest; i += 2)
        {
            uint newValue = i + 10;
            update(&map, i, newValue);
        }

        for (uint i = 0; i < cItemsTest; i ++)
        {
            uint value;
            Verify(lookup(&map, i, &value));
            Assert(value == (i % 2) ? i + 5 : i + 10);
        }

        for (uint i = 1; i < cItemsTest; i += 2)
        {
            Verify(remove(&map, i));
        }

        for (uint i = 0; i < cItemsTest; i++)
        {
            uint value;
            bool found = lookup(&map, i, &value);

            if (i % 2)
            {
                Assert(!found);
            }
            else
            {
                Assert(found);
                Assert(value == i + 10);
            }
        }
    }
#endif

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
