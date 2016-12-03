ttest_parser:
	gcc -g -Wall -o test_parser test/test_parser.c src/lexer.c src/dict.c src/buffer.c src/util.c src/vector.c src/parser.c

est_lexer:
	gcc -g -Wall -o test_lexer test/test_lexer.c src/lexer.c src/dict.c src/buffer.c src/util.c

make clean:
	rm test_lexer test_parser
