#include <stdio.h>
#include "file.h"
#include "lexer.h"

int main(int argc, char *argv[])
{
    int i;
    lexer_t lexer;

    for (i = 1; i < argc; i++) {
        lexer_init(&lexer, argv[i], NULL);

    }
}
