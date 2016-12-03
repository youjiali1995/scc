#include <stdio.h>
#include "../src/parser.h"

int main(void)
{
    lexer_t lexer;
    parser_t parser;
    node_t *node;
    char *node_type[] = {
        "NODE_CONSTANT",
        "NODE_STRING",
        "NODE_POSTFIX",
        "NODE_UNARY",
        "NODE_BINARY",
        "NODE_TERNARY",
        "NODE_IF",
        "NODE_FOR",
        "NODE_WHILE",
        "NODE_FUNC_DECL",
        "NODE_FUNC_DEF",
        "NODE_FUNC_CALL",
        "NODE_VAR_DECL",
        "NODE_VAR_INIT",
        "NODE_COMPOUND_STMT",
        "NODE_RETURN"
    };

    lexer_init(&lexer, "stdin", stdin);
    parser_init(&parser, &lexer);
    while ((node = get_node(&parser))) {
        printf("%s\n", node_type[node->type]);
    }

    return 0;
}
