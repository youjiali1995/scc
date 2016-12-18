#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vector.h"
#include "lexer.h"
#include "parser.h"
#include "gen.h"
#include "util.h"

FILE *fopen_out(const char *fname)
{
    char *out;
    FILE *fp;
    size_t len = strlen(fname);

    if (len < 3 || fname[len - 1] != 'c' || fname[len - 2] != '.')
        errorf("filename suffix is not .c: %s\n", fname);
    out = strdup(fname);
    out[len - 1] = 's';
    fp = fopen(out, "w");
    free(out);
    if (!fp)
        errorf("Can't open file %s to write\n", out);
    return fp;
}

void compile(const char *fname, FILE *in)
{
    size_t i;
    lexer_t lexer;
    parser_t parser;
    vector_t *ast;
    node_t *node;
    FILE *out;

    ast = make_vector();
    out = (in == stdin) ? stdout : fopen_out(fname);

    lexer_init(&lexer, fname, in);
    parser_init(&parser, &lexer);
    while ((node = get_node(&parser)))
        vector_append(ast, node);
    for (i = 0; i < vector_len(ast); i++)
        emit(out, vector_get(ast, i));

    if (in != stdin) {
        fclose(in);
        fclose(out);
    }
    free_vector(ast, NULL);
}

int main(int argc, char *argv[])
{
    int i;

    if (argc == 1)
        compile("stdin", stdin);
    else
        for (i = 1; i < argc; i++)
            compile(argv[i], fopen(argv[i], "r"));

    return 0;
}
