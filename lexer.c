#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "lexer.h"
#include "utils.h"

Token *tokens;	// single linked list of tokens
Token *lastTk;		// the last token in list

int line=1;		// the current line in the input file

char ids[38][15] = { "ID",
                     "TYPE_CHAR","TYPE_DOUBLE","ELSE", "IF","TYPE_INT",
                     "RETURN", "STRUCT", "VOID", "WHILE",
                     "INT", "DOUBLE", "CHAR", "STRING",
                     "COMMA", "END", "SEMICOLON", "LPAR", "RPAR", "LBRACKET",
                     "RBRACKET", "LACC", "RACC",
                     "ASSIGN","EQUAL","ADD","SUB","MUL","DIV","DOT",
                     "AND","OR","NOT","NOTEQ","LESS","LESSEQ","GREATER","GREATEREQ"
};

// adds a token to the end of the tokens list and returns it
// sets its code and line
Token *addTk(int code){
	Token *tk=safeAlloc(sizeof(Token));  //aloca memorie pentru un nou token
	tk->code=code;  //Setarea codului token-ului
	tk->line=line;  //Setarea liniei curente
	tk->next=NULL;  //initializare next cu null
    //verificam daca lista de token-uri nu e goala
	if(lastTk){  
		lastTk->next=tk;  //se adauga noul token la sfarsit
	}else{
		tokens=tk;  //setam tokens sa pointeze catre noul token
    }
	lastTk=tk;  //Setează variabila globală lastTk cu adresa token-ului curent
	return tk;
}

char *extract(const char *begin,const char *end){
    char* res = safeAlloc(end-begin+1); //aloca dinamic memorie
    int i=0;
    while(begin!=end) {  
        res[i++]=*begin; //se copiaza fiecare caracter in res
        begin++;  //avanseaza la urmatorul caracter
    }
    res[i]='\0';     //adauga la sfarsit \0 pt a fi sir valid
    return res;    //return rezultat
}

Token *tokenize(const char *pch){
	const char *start;
	Token *tk;
	for(;;){      //bucla infinita care parcurge sirul de caractere panas la intalnirea \0
		switch(*pch){
			case ' ':case '\t':pch++;break;
			case '\r':		//trateaza diferite tipuri de linii noi \r \n
				if(pch[1]=='\n')pch++;
			case '\n':
				line++;
				pch++;
				break;

            //caractere
            case '\'':
                if(pch[1]!='\'') {   //verifica daca urm caracter nu e '
                    if(pch[2]=='\'') {
                        tk= addTk(CHAR);
                        tk->c=pch[1];  //seteaza c cu pch[1]
                        pch+=3;  //avanseaza cu 3 pozitii (peste cele 3 caractere)
                    }
                    else{
                        err("invalid char: %c",*pch);
                    }
                }
                else{
                    err("invalid char: %c",*pch);
                }
                break;

            //siruri de caractere
            case '"':
                pch++;
                for(start=pch;*pch!='"';pch++){}  //parcurge sir pana gaseste "
                if(*pch=='\0')
                    err("found invalid character: %c",*pch);
                char *text=extract(start,pch);
                tk= addTk(STRING);
                tk->text=text;
                pch++;
                break;

            //delimitators
            case ',':addTk(COMMA);pch++;break;
            case ';':addTk(SEMICOLON);pch++;break;
            case '(':addTk(LPAR);pch++;break;
            case ')':addTk(RPAR);pch++;break;
            case '[':addTk(LBRACKET);pch++;break;
            case ']':addTk(RBRACKET);pch++;break;
            case '{':addTk(LACC);pch++;break;
            case '}':addTk(RACC);pch++;break;
            case '\0':addTk(END);return tokens;

            //operators
            case '+':addTk(ADD);pch++;break;
            case '-':addTk(SUB);pch++;break;
            case '*':addTk(MUL);pch++;break;
            case '.':addTk(DOT);pch++;break;
            case '/':
                if(pch[1]=='/'){  //verifica daca urm caracter e /, adica avem un comentariu //
                    for(start=pch++;*pch!='\n';pch++){}  //parcurge si pana gaseste \n
                }else{  //daca nu e /
                    addTk(DIV);  
                    pch++;
                }
                break;
            case '&':
                if(pch[1]=='&'){  //daca urm caracter e & inseamna ca avem si logic
                    addTk(AND);
                    pch+=2;  //avanseaza 2 pozitii
                }else{
                    err("lipsa sec &: %c",*pch);
                }
                break;
            case '|':
                if(pch[1]=='|'){   //daca urm caracter e | inseamna ca avem sau logic
                    addTk(OR);
                    pch+=2;
                }else{
                    err("lipsa sec |: %c",*pch);
                }
                break;
            case '!':
                if(pch[1]=='='){ //daca urm caracter e = inseamna ca avem inegalitate
                    addTk(NOTEQ);
                    pch+=2;
                }else{
                    addTk(NOT);
                    pch++;
                }
                break;
            case '=':
                if(pch[1]=='='){    //daca urm caracter e = inseamna ca avem egalitate
                    addTk(EQUAL);
                    pch+=2;
                }else{
                    addTk(ASSIGN);
                    pch++;
                }
                break;
            case '<':
                if(pch[1]=='='){  //daca urm caracter e = inseamna ca avem mai mic sau egal
                    addTk(LESSEQ);
                    pch+=2;
                }else{
                    addTk(LESS);
                    pch++;
                }
                break;
            case '>':
                if(pch[1]=='='){  //daca urm caracter e = inseamna ca avem mai mare sau egal
                    addTk(GREATEREQ);
                    pch+=2;
                }else{
                    addTk(GREATER);
                    pch++;
                }
                break;

			default:
                start = pch;
				if(isalpha(*pch)||*pch=='_'){  //verifica daca primul carac e litera sau _
					for(start=pch++;isalnum(*pch)||*pch=='_';pch++){}
					char *text=extract(start,pch);
                    //keywords
					if(strcmp(text,"char")==0)addTk(TYPE_CHAR);
                    else if(strcmp(text,"double")==0)addTk(TYPE_DOUBLE);
                    else if(strcmp(text,"else")==0)addTk(ELSE);
                    else if(strcmp(text,"if")==0)addTk(IF);
                    else if(strcmp(text,"int")==0)addTk(TYPE_INT);
                    else if(strcmp(text,"return")==0)addTk(RETURN);
                    else if(strcmp(text,"struct")==0)addTk(STRUCT);
                    else if(strcmp(text,"void")==0)addTk(VOID);
                    else if(strcmp(text,"while")==0)addTk(WHILE);
                    //ID
					else{
						tk=addTk(ID);
						tk->text=text;
					}
                }
                else if(isdigit(*pch)) {   //verifica daca primul carac e cifra
                    int isInt = 1;   //1 int, 0 double
                    while(isdigit(*pch)) pch++;
                    if(*pch=='.') {   //urm carac e . ,urmeaza partea zecimala
                        isInt = 0;
                        pch++;
                        int ok=0;
                        while (isdigit(*pch)) {
                            pch++;
                            ok=1;
                        }
                       if(ok==0) err("invalid double: %c, line %d\n",*pch, line);
                    }
                    if('E'==*pch || 'e'==*pch) {  //verifica daca urm carac e E sau e
                        isInt = 0;
                        pch++;
                        if('+'==*pch || '-'==*pch) pch++;
                        if(!isdigit(*pch)) //verifica daca dupa +- nu este digit
                            err("Invalid double: %s, line %d\n", extract(start,pch),line);
                        while (isdigit(*pch)) pch++;
                    }
                    char* number = extract(start,pch);
                    if(isInt) {
                        tk = addTk(INT);
                        tk->i = atoi(number);
                    } else {
                        tk = addTk(DOUBLE);
                        tk->d = atof(number);
                    }
                }
				else err("invalid char: %c",*pch);
		}
	}
}

void showTokens(const Token *tokens){
	for(const Token *tk=tokens;tk;tk=tk->next){  //parcurge lista de token-uri
		printf("%d\t%s",tk->line,ids[tk->code]); //afișeaza nr liniei și tipul token-ului.
        if(tk->code == ID || tk->code == STRING){  //pt id, string
            printf(":%s",tk->text);
        }
        if(tk->code == INT){   //pt int
            printf(":%d",tk->i);
        }
        if(tk->code == CHAR){   //pt char
            printf(":%c", tk->c);
        }
        if(tk->code == DOUBLE){   //pt double
            printf(":%.2f",tk->d);
        }
        printf("\n");
	}
}


