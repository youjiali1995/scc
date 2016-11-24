#include <stdio.h>
#include <ctype.h>
#include "../src/lexer.h"

int main(void)
{
    lexer_t lexer;
    token_t *token;
    char *kw[] = {
        "",
        "void",
        "char",
        "int",
        "double",
        "for",
        "while",
        "if",
        "else",
        "return"
    };

    lexer_init(&lexer, NULL, stdin);
    for (token = get_token(&lexer); token; token = get_token(&lexer))
        switch (token->type) {
        case TK_KEYWORD:
            printf("type: keyword, val: %s\n", kw[token->ival]);
            break;

        case TK_ID:
            printf("type: identifier, val: %s\n", token->sval);
            break;

        case TK_NUMBER:
            printf("type: number, val: %s\n", token->sval);
            break;

        case TK_CHAR:
            printf("type: character, val: %c\n", token->ival);
            break;

        case TK_STRING:
            printf("type: string, val: %s\n", token->sval);
            break;

        case TK_PUNCT:
            printf("type: punctuation, val: ");
            if (isascii(token->ival))
                printf("%c\n", token->ival);
            else
                printf("%d\n", token->ival);
            break;

        default:
            fprintf(stderr, "unknown token type: %d\n", token->type);
        }

    return 0;
}
