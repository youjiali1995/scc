#include "parser.h"

void parser_init(parser_t *parser, lexer_t *lexer)
{
    assert(parser && lexer);
    parser->lexer = lexer;
}


