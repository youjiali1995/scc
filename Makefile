test_lexer:
	gcc -g -Wall -o test_lexer test/test_lexer.c src/lexer.c src/dict.c src/buffer.c src/util.c

make clean:
	rm test_lexer
