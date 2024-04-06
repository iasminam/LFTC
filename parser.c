#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "parser.h"
#include "ad.h"
#include "utils.h"
#include "at.h"

Token *iTk;		// the iterator in the tokens list
Token *consumedTk;		// the last consumed token
Symbol *owner = NULL;

void tkerr(const char *fmt,...){
	fprintf(stderr,"error in line %d: ",iTk->line);
	va_list va;
	va_start(va,fmt);
	vfprintf(stderr,fmt,va);
	va_end(va);
	fprintf(stderr,"\n");
	exit(EXIT_FAILURE);
	}

char *tkname(int code) {
    return ids[code];
}

bool consume(int code){
    printf("consume(%s)", tkname(code));
	if(iTk->code==code){
		consumedTk=iTk;
		iTk=iTk->next;
        printf(" => consumed\n");
		return true;
		}
    printf(" => found %s\n", tkname(iTk->code));
	return false;
	}


// unit: ( structDef | fnDef | varDef )* END
bool unit(){
    printf("#unit: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    for(;;){
        if(structDef()){}
        else if(fnDef()){}
        else if(varDef()){}
        else break;
    }
    if(consume(END)){
        return true;
    }
    iTk=start;
    return false;
}

//STRUCT ID LACC varDef* RACC SEMICOLON
bool structDef() {
    printf("#structDef: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(STRUCT)) {
        if(consume(ID)) {
            Token* tkName = consumedTk;
            if(consume(LACC)) {
                Symbol *s=findSymbolInDomain(symTable,tkName->text);
                if(s)tkerr("symbol redefinition: %s",tkName->text);
                s=addSymbolToDomain(symTable,newSymbol(tkName->text,SK_STRUCT));
                s->type.tb=TB_STRUCT;
                s->type.s=s;
                s->type.n=-1;
                pushDomain();
                owner=s;
                for(;;) {
                    if(varDef()) {}
                    else break;
                }
                if(consume(RACC)) {
                    if(consume(SEMICOLON)) {
                        owner=NULL;
                        dropDomain();
                        printf("#structDef: ended at %s\nLine: %d\n",tkname(iTk->code), iTk->line);
                        return true;
                    }else tkerr("Lipseste ; din declaratia de structura.");
                }else tkerr("Lipseste } din declaratia de structura");
            } else {
                iTk = start;
                return false;
            }
        }else tkerr("Lipseste numele declaratiei de structura.");
    }
    iTk=start;
    return false;
}

//varDef: typeBase ID arrayDecl? SEMICOLON
bool varDef() {
    Type t;
    printf("#varDef: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(typeBase(&t)) {
        if(consume(ID)) {
            Token *tkName = consumedTk;
            if(arrayDecl(&t)) {
                if(t.n==0)tkerr("a vector variable must have a specified dimension");
            }
            if(consume(SEMICOLON)) {
                Symbol *var=findSymbolInDomain(symTable,tkName->text);
                if(var)tkerr("symbol redefinition: %s",tkName->text);
                var=newSymbol(tkName->text,SK_VAR);
                var->type=t;
                var->owner=owner;
                addSymbolToDomain(symTable,var);
                if(owner){
                    switch(owner->kind){
                        case SK_FN:
                            var->varIdx=symbolsLen(owner->fn.locals);
                            addSymbolToList(&owner->fn.locals,dupSymbol(var));
                            break;
                        case SK_STRUCT:
                            var->varIdx=typeSize(&owner->type);
                            addSymbolToList(&owner->structMembers,dupSymbol(var));
                            break;
                    }
                }else{
                    var->varMem=safeAlloc(typeSize(&t));
                }
                printf("#varDef: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
                return true;
            }
        }
        else tkerr("Lipseste numele din declaratia de variabila.");
    }
    iTk=start;
    return false;
}

// typeBase: TYPE_INT | TYPE_DOUBLE | TYPE_CHAR | STRUCT ID
bool typeBase(Type *t){
    t->n=-1;
    printf("#typeBase: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(TYPE_INT)){
        t->tb=TB_INT;
        printf("#typeBase: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    if(consume(TYPE_DOUBLE)){
        t->tb=TB_DOUBLE;
        printf("#typeBase: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    if(consume(TYPE_CHAR)){
        t->tb=TB_CHAR;
        printf("#typeBase: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    if(consume(STRUCT)){
        if(consume(ID)){
            Token *tkName = consumedTk;
            t->tb=TB_STRUCT;
            t->s=findSymbol(tkName->text);
            if(!t->s)tkerr("structura nedefinita: %s",tkName->text);
            printf("#typeBase: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }else tkerr("Lipseste numele instantei de structura.");
    }
    iTk = start;
    return false;
}

//arrayDecl: LBRACKET INT? RBRACKET
bool arrayDecl(Type *t) {
    printf("#arrayDecl: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(LBRACKET)) {
        if(consume(INT)) {
            Token* tkSize = consumedTk;
            t->n = tkSize->i;
        } else {
            t->n=0;
        }
        if(consume(RBRACKET)) {
            printf("#arrayDecl: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        } else tkerr("Lipseste ] din definitia array-ului");
    }
    iTk=start;
    return false;
}

//fnDef: ( typeBase | VOID ) ID
//LPAR ( fnParam ( COMMA fnParam )* )? RPAR
//stmCompound
bool fnDef() {
    Type t;
    printf("#fnDef: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(typeBase(&t)) {}
    else if(consume(VOID)) {
        t.tb=TB_VOID;
    } else return false;
    if(consume(ID)) {
        Token *tkName = consumedTk;
        if(consume(LPAR)) {
            Symbol *fn=findSymbolInDomain(symTable,tkName->text);
            if(fn)tkerr("symbol redefinition: %s",tkName->text);
            fn=newSymbol(tkName->text,SK_FN);
            fn->type=t;
            addSymbolToDomain(symTable,fn);
            owner=fn;
            pushDomain();
            if(fnParam()) {
                while(consume(COMMA)) {
                    if(fnParam()) {}
                    else tkerr("Virgula nu precede niciun parametru");
                }
            }
            if(consume(RPAR)) {
                if(stmCompound(false)) {
                    dropDomain();
                    owner=NULL;
                    printf("#fnDef: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
                    return true;
                }else tkerr("Lipseste definitia functiei");
            }else tkerr("Lipseste ) din declaratia functiei");
        }else {
            iTk = start;
            return false;
        }
    }else tkerr("Lipseste numele functiei");
    iTk = start;
    return false;
}

//fnParam: typeBase ID arrayDecl?
bool fnParam() {
    Type t;
    printf("#fnParam: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(typeBase(&t)) {
        if(consume(ID)) {
            Token *tkName = consumedTk;
            if(arrayDecl(&t)) {
                t.n=0;
            }
            Symbol *param=findSymbolInDomain(symTable,tkName->text);
            if(param)tkerr("symbol redefinition: %s",tkName->text);
            param=newSymbol(tkName->text,SK_PARAM);
            param->type=t;
            param->owner=owner;
            param->paramIdx=symbolsLen(owner->fn.params);
//          parametrul este adaugat atat la domeniul curent, cat si la parametrii fn
            addSymbolToDomain(symTable,param);
            addSymbolToList(&owner->fn.params,dupSymbol(param));
            printf("#fnParam: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }else tkerr("Lipseste numele parametrului de functie");
    }
    iTk = start;
    return false;
}

//stm: stmCompound
//| IF LPAR expr RPAR stm ( ELSE stm )?
//| WHILE LPAR expr RPAR stm
//| RETURN expr? SEMICOLON
//| expr? SEMICOLON
bool stm() {
    printf("#stm: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    Ret rCond,rExpr;
    if(stmCompound(true)) { return true; }
    if(consume(IF)) {
        if(consume(LPAR)) {
            if(expr(&rCond)) {
                if(!canBeScalar(&rCond))
                    tkerr("the if condition must be a scalar value");
                if(consume(RPAR)) {
                    if(stm()) {
                        if(consume(ELSE)) {
                            if(stm()) {
                                printf("#stm: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
                                return true;
                            }else tkerr("Lipseste corpul else-ului");
                        }
                        printf("#stm: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
                        return true;
                    }else tkerr("Lipseste corpul if-ului");
                }else tkerr("Lipseste ) din definitia if-ului");
            }else tkerr("Lipseste expresia de evaluat din if");
        }else tkerr("Lipseste ( din definitia if-ului");
    }
    if(consume(WHILE)) {
        if(consume(LPAR)) {
            if(expr(&rCond)) {
                if(!canBeScalar(&rCond))
                    tkerr("the while condition must be a scalar value");
                if(consume(RPAR)) {
                    if(stm()) {
                        printf("#stm: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
                        return true;
                    }else tkerr("Lipseste corpul while-ului");
                }else tkerr("Lipseste ) din definitia while-ului");
            }else tkerr("Lipseste expresia de evaluat din while");
        }else tkerr("Lipseste ( din definitia while-ului");
    }
    if(consume(RETURN)) {
        if(expr(&rExpr)) {
            if(owner->type.tb==TB_VOID)
                tkerr("a void function cannot return a value");
            if(!canBeScalar(&rExpr))
                tkerr("the return value must be a scalar value");
            if(!convTo(&rExpr.type,&owner->type))
                tkerr("cannot convert the return expression type to the function return type");
        } else {
            if(owner->type.tb!=TB_VOID)
                tkerr("a non-void function must return a value");
        }
        if(consume(SEMICOLON)) {
            printf("#stm: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }else tkerr("Lipseste ; la finalul return-ului");
    }
    if(expr(&rExpr)) {
        if(consume(SEMICOLON)) {
            return true;
        }else tkerr("Lipseste ; la sfarsitul expresiei");
    }
    if(consume(SEMICOLON)) {
        return true;
    }
    iTk = start;
    return false;
}

//stmCompound: LACC ( varDef | stm )* RACC
bool stmCompound(bool newDomain) {
    printf("#stmCompound: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(LACC)) {
        if(newDomain)pushDomain();
        for(;;) {
            if(varDef() || stm()){}
            else break;
        }
        if(consume(RACC)) {
            if(newDomain)dropDomain();
            printf("#stmCompound: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }else tkerr("Lipseste } din definitia statement-ului");
    }
    iTk = start;
    return false;
}

//expr: exprAssign
bool expr(Ret *r) {
    printf("#expr: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(exprAssign(r)) {
        printf("#expr: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    iTk = start;
    return false;
}

//exprAssign: exprUnary ASSIGN exprAssign | exprOr
bool exprAssign(Ret *r) {
    printf("#exprAssign: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    Ret rDst;
    if(exprUnary(&rDst)) {
        if (consume(ASSIGN)) {
            if (exprAssign(r)) {
                if(!rDst.lval)tkerr("the assign destination must be a left-value");
                if(rDst.ct)tkerr("the assign destination cannot be constant");
                if(!canBeScalar(&rDst))tkerr("the assign destination must be scalar");
                if(!canBeScalar(r))tkerr("the assign source must be scalar");
                if(!convTo(&r->type,&rDst.type))tkerr("the assign source cannot be converted to destination");
                r->lval=false;
                r->ct=true;
                printf("#exprAssign: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
                return true;
            }else tkerr("Operand drept invalid dupa operator de atribuire");
        }
    }
    iTk=start;
    if(exprOr(r)) {
        printf("#exprAssign: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    iTk=start;
    return false;
}

/*
//exprOr: exprOr OR exprAnd | exprAnd
bool exprOrPrim(Ret *r) {
    printf("#exprOrPrim: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(OR)) {
        Ret right;
        if(exprAnd(&right)) {
            Type tDst;
            if(!arithTypeTo(&r->type,&right.type,&tDst))
                tkerr(iTk,"invalid operand type for ||");
            *r=(Ret){{TB_INT,NULL,-1},false,true};
            exprOrPrim(r);
            printf("#exprOrPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }else tkerr(iTk,"invalid expression after ||");
    } else {
        printf("#exprOrPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    iTk = start;
    return false;
}
*/

bool exprOrPrim(Ret *r) {
    printf("#exprOrPrim: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(OR)) {
        Ret right;
        if(exprAnd(&right)) {
            Type tDst;
            if(!arithTypeTo(&r->type,&right.type,&tDst)) {
                char errorMsg[100]; // Assuming a maximum error message length of 100 characters
                sprintf(errorMsg, "invalid operand type for || at line %d", iTk->line);
                tkerr(errorMsg);
            }
            *r=(Ret){{TB_INT,NULL,-1},false,true};
            exprOrPrim(r);
            printf("#exprOrPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        } else {
            char errorMsg[100]; // Assuming a maximum error message length of 100 characters
            sprintf(errorMsg, "invalid expression after || at line %d", iTk->line);
            tkerr(errorMsg);
        }
    } else {
        printf("#exprOrPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    iTk = start;
    return false;
}

bool exprOr(Ret *r) {
    printf("#exprOr: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(exprAnd(r)) {
        if(exprOrPrim(r)) {
            printf("#exprOr: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }
    }
    iTk=start;
    return false;
}


//exprAnd: exprAnd AND exprEq | exprEq
bool exprAndPrim(Ret *r) {
    printf("#exprAndPrim: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(AND)) {
        Ret right;
        if(exprEq(&right)) {
            Type tDst;
            if (!arithTypeTo(&r->type, &right.type, &tDst))
                tkerr("invalid operand type for &&");
            *r = (Ret) {{TB_INT, NULL, -1}, false, true};
            printf("#exprAndPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            exprAndPrim(r);
            return true;
        } else tkerr("Operand drept invalid dupa operator AND logic");
    }else {
        printf("#exprPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    iTk = start;
    return false;
}

bool exprAnd(Ret *r) {
    printf("#exprAnd: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(exprEq(r)) {
        if(exprAndPrim(r)) {
            printf("#exprAnd: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }
    }
    iTk = start;
    return false;
}

//exprEq: exprEq ( EQUAL | NOTEQ ) exprRel | exprRel
bool exprEqPrim(Ret *r) {
    printf("#exprEqPrim: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(EQUAL) || consume(NOTEQ)) {
        Ret right;
        if(exprRel(&right)) {
            Type tDst;
            if(!arithTypeTo(&r->type,&right.type,&tDst))
                tkerr("invalid operand type for == or!=");
            *r=(Ret){{TB_INT,NULL,-1},false,true};
            printf("#exprEqPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            exprEqPrim(r);
            return true;
            } else tkerr("Operand drept invalid dupa operator de egalitate");
        }else {
        printf("#exprEqPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    iTk = start;
    return false;
}

bool exprEq(Ret *r) {
    printf("#exprEq: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(exprRel(r)) {
        if(exprEqPrim(r)) {
            printf("#exprEq: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }
    }
    iTk = start;
    return false;
}

//exprRel: exprRel ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd | exprAdd
bool exprRelPrim(Ret *r) {
    printf("#exprRelPrim: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ)) {
        Ret right;
        if(exprAdd(&right)) {
            Type tDst;
            if(!arithTypeTo(&r->type,&right.type,&tDst))
                tkerr("invalid operand type for <, <=, >,>=");
            *r=(Ret){{TB_INT,NULL,-1},false,true};
            exprRelPrim(r);
            printf("#exprRelPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }else tkerr("Operand drept invalid dupa operator relational");
    }else {
        printf("#exprRelPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    iTk = start;
    return false;
}

bool exprRel(Ret *r) {
    printf("#exprRel: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(exprAdd(r)) {
        if(exprRelPrim(r)) {
            printf("#exprRel: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }
    }
    iTk = start;
    return false;
}

//exprAdd: exprAdd ( ADD | SUB ) exprMul | exprMul
bool exprAddPrim(Ret *r) {
    printf("#exprAddPrim: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(ADD) || consume(SUB)) {
        Ret right;
        if(exprMul(&right)) {
            Type tDst;
            if(!arithTypeTo(&r->type,&right.type,&tDst))tkerr("invalid operand type for + or -");
            *r=(Ret){tDst,false,true};
            exprAddPrim(r);
            printf("#exprAddPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }else tkerr("Operand drept invalid dupa operator de adaugare");
    }else {
        printf("#exprAddPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    iTk = start;
    return false;
}


bool exprAdd(Ret *r) {
    printf("#exprAdd: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(exprMul(r)) {
        if(exprAddPrim(r)) {
            printf("#exprAdd: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }
    }
    iTk = start;
    return false;
}

//exprMul: exprMul ( MUL | DIV ) exprCast | exprCast
bool exprMulPrim(Ret *r) {
    printf("#exprMulPrim: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(MUL) || consume(DIV)) {
        Ret right;
        if(exprCast(&right)) {
            Type tDst;
            if(!arithTypeTo(&r->type,&right.type,&tDst))tkerr("invalid operand type for * or /");
            *r=(Ret){tDst,false,true};
            exprMulPrim(r);
            printf("#exprMulPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }else tkerr("Operand drept invalid dupa operator de multiplicare");
    }else {
        printf("#exprMulPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    iTk = start;
    return false;
}

bool exprMul(Ret *r) {
    printf("#exprMul: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(exprCast(r)) {
        if(exprMulPrim(r)) {
            printf("#exprMul: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }
    }
    iTk = start;
    return false;
}

//exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
bool exprCast(Ret *r) {
    printf("#exprCast: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(LPAR)) {
        Type t;
        Ret op;
        if(typeBase(&t)) {
            if(arrayDecl(&t)) {}
            if(consume(RPAR)) {
                if(exprCast(&op)) {
                    if(t.tb==TB_STRUCT)tkerr("cannot convert to a struct type");
                    if(op.type.tb==TB_STRUCT)tkerr("cannot convert a struct");
                    if(op.type.n>=0&&t.n<0)tkerr("an array can be converted only to another array");
                    if(op.type.n<0&&t.n>=0)tkerr("a scalar can be converted only to another scalar");
                    *r=(Ret){t,false,true};
                    printf("#exprCast: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
                    return true;
                }
            }else tkerr("Lipseste ) din expresia de cast");
        }
    }
    iTk = start;
    if(exprUnary(r)) {
        printf("#exprCast: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    iTk = start;
    return false;
}

//exprUnary: ( SUB | NOT ) exprUnary | exprPostfix
bool exprUnary(Ret *r) {
    printf("#exprUnary: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(SUB) || consume(NOT)) {
        if(exprUnary(r)) {
            if(!canBeScalar(r))tkerr("unary - or ! must have a scalar operand");
            r->lval=false;
            r->ct=true;
            printf("#exprUnary: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }else tkerr("Operand drept invalid dupa operator unar");
    }
    iTk=start;
    if(exprPostfix(r)) {
        printf("#exprUnary: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    iTk = start;
    return false;
}

//exprPostfix: exprPostfix LBRACKET expr RBRACKET
//| exprPostfix DOT ID
//| exprPrimary

bool exprPostfixPrim(Ret *r) {
    printf("#exprPostfixPrim: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(LBRACKET)) {
        Ret idx;
        if(expr(&idx)) {
            if(consume(RBRACKET)) {
                if(r->type.n<0)
                    tkerr("only an array can be indexed");
                Type tInt={TB_INT,NULL,-1};
                if(!convTo(&idx.type,&tInt))tkerr("the index is not convertible to int");
                r->type.n=-1;
                r->lval=true;
                r->ct=false;
                exprPostfixPrim(r);
                printf("#exprPostfixPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
                return true;
            }else tkerr("Lipsest ] din indexare");
        }else tkerr("Expresie index eronata");
    }
    iTk=start;
    if(consume(DOT)) {
            if(consume(ID)) {
                Token *tkName = consumedTk;
                if(r->type.tb!=TB_STRUCT)tkerr("a field can only be selected from a struct");
                Symbol *s=findSymbolInList(r->type.s->structMembers,tkName->text);
                if(!s)
                    tkerr("the structure %s does not have a field%s",r->type.s->name,tkName->text);
                *r=(Ret){s->type,true,s->type.n>=0};
                exprPostfixPrim(r);
                printf("#exprPostfixPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
                return true;
            }else tkerr("Lipseste numele campului");
    }else {
        printf("#exprPostfixPrim: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    iTk = start;
    return false;
}


bool exprPostfix(Ret *r) {
    printf("#exprPostfix: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(exprPrimary(r)) {
        if(exprPostfixPrim(r)) {
            printf("#exprPostfix: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
            return true;
        }
    }
    iTk = start;
    return false;
}

//exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
//| INT | DOUBLE | CHAR | STRING | LPAR expr RPAR
bool exprPrimary(Ret *r) {
    printf("#exprPrimary: %s\nLine: %d\n", tkname(iTk->code), iTk->line);
    Token *start = iTk;
    if(consume(ID)) {
        Token *tkName = consumedTk;
        Symbol *s=findSymbol(tkName->text);
        if(!s)tkerr("undefined id: %s",tkName->text);
        if(consume(LPAR)) {
            if(s->kind!=SK_FN)tkerr("only a function can be called");
            Ret rArg;
            Symbol *param=s->fn.params;
            if(expr(&rArg)) {
                if(!param)tkerr("too many arguments in function call");
                if(!convTo(&rArg.type,&param->type))
                    tkerr("in call, cannot convert the argument type to the parameter type");
                param=param->next;
                for(;;) {
                    if(consume(COMMA)) {
                        if(consume(expr(&rArg))) {
                            if(!param)tkerr("too many arguments in function call");
                            if(!convTo(&rArg.type,&param->type))
                                tkerr("in call, cannot convert the argument type to the parameter type");
                            param=param->next;
                        }else tkerr("Argument invalid dupa virgula");
                    }else break;
                }
            }
            if(consume(RPAR)) {
                if(param)tkerr("too few arguments in function call");
                *r=(Ret){s->type,false,true};
                printf("#exprPrimary: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
                return true;
            }else {
                tkerr("Lipseste ) dupa apel de functie");
            }
        }
        if(s->kind==SK_FN)tkerr("a function can only be called");
        *r=(Ret){s->type,true,s->type.n>=0};
        printf("#exprPrimary: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    iTk = start;
    if(consume(INT)) {
        *r=(Ret){{TB_INT,NULL,-1},false,true};
        printf("#exprPrimary: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    if(consume(DOUBLE)) {
        *r=(Ret){{TB_DOUBLE,NULL,-1},false,true};
        printf("#exprPrimary: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    if(consume(CHAR)) {
        *r=(Ret){{TB_CHAR,NULL,-1},false,true};
        printf("#exprPrimary: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    if(consume(STRING)) {
        *r=(Ret){{TB_CHAR,NULL,0},false,true};
        printf("#exprPrimary: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
        return true;
    }
    if(consume(LPAR)) {
        if(expr(r)) {
            if(consume(RPAR)){
                printf("#exprPrimary: ended at %s\nLine: %d\n", tkname(iTk->code), iTk->line);
                return true;
            }else tkerr("Lipseste ) dupa expresie");
        }
    }
    iTk = start;
    return false;
}

void parse(Token *tokens){
	iTk=tokens;
	if(!unit())tkerr("syntax error");
}
