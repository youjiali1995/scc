scc:
	gcc -g -Wall -o scc src/*.c
test_parser:
	gcc -g -Wall -o test_parser test/test_parser.c src/lexer.c src/dict.c src/buffer.c src/util.c src/vector.c src/parser.c

test_lexer:
	gcc -g -Wall -o test_lexer test/test_lexer.c src/lexer.c src/dict.c src/buffer.c src/util.c

make clean:
	rm test_parser test_lexer scc
