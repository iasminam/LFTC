#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "lexer.h"
#include "parser.h"

int main() {
    char* parsebuf = loadFile("tests/testparser.c");
    puts(parsebuf);
    Token* parselist = tokenize(parsebuf);
    showTokens(parselist);
    pushDomain();
    parse(parselist);
    showDomain(symTable,"global");
    dropDomain();
    return 0;
}
