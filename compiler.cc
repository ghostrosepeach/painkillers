// compiler.cc

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#define GET (ch = fgetc(input))
#define NUM (ch & 15)
#define ALNUM ((ch | 32) - 87)

#define NAN32 0x7fc00000U
#define NAN64 0x7ff800000000000UL

#define AND      0x0000000000646e61UL
#define BREAK    0x0000006b61657262UL
#define CASE     0x0000000065736163UL
#define CLASS    0x0000005353616c63UL
#define CONST    0x00000074736e6f63UL
#define CONTINUE 0x65756e69746e6f63UL
#define DO       0x0000000000006f64UL
#define ELSE     0x0000000065736c65UL
#define ENUM     0x000000006d756e65UL
#define FALSE    0x00000065736c6166UL
#define FOR      0x0000000000726f66UL
#define FUNC     0x00000000636e7566UL
#define GLOBAL   0x00006c61626f6c67UL
#define IF       0x0000000000006669UL
#define INF      0x0000000000666e69UL
#define INLINE   0x0000656e696c6e69UL
#define LET      0x000000000073656cUL
#define NAN      0x00000000006e616eUL
#define NEW      0x000000000077656eUL
#define NOT      0x0000000000736f6eUL
#define OBJECT   0x000073636570626fUL
#define OF       0x000000000000666fUL
#define OR       0x000000000000726fUL
#define RETURN   0x00006e7275746572UL
#define STATIC   0x0000636974617473UL
#define STRUCT   0x0000746375727473UL
#define SWITCH   0x0000686374697773UL
#define TRUE     0x0000000065757274UL
#define UNION    0x0000006e6f696e75UL
#define VIRTUAL  0x006c617574726976UL
#define VOLATILE 0x656c6974616c6f76UL
#define WHILE    0x000000656c696877UL

/*
enum Keyword {
  And = 6581857UL, Break = 461195539042UL, Case = 1702060387UL,
  Class = 357881179235UL, Const = 500152823651UL, Continue = 7310870969309884259UL,
  Do = 28516UL, Else = 1702063205UL, Enum = 1836412517UL,
  False = 435728179558UL, For = 7499622UL, Func = 1668183398UL,
  Global = 119165519096935UL, If = 26217UL, Inf = 6712937UL,
  Inline = 111524889521769UL, Let = 7562604UL, Nan = 7233902UL,
  New = 7824750UL, Not = 7565166UL, Object = 126870740820591UL,
  Of = 26223UL, Or = 29295UL, Return = 121437875889522UL,
  Static = 109304575259763UL, Struct = 127970521019507UL, Switch = 114776364119923UL,
  True = 1702195828UL, Union = 474315583093UL, Virtual = 30506454577473910UL,
  Volatile = 7308332243887091574UL, While = 435610544247UL
};
*/

#define NODBUFSZ 65536

#define STRBUFSZ 65536
#define SYMBUFSZ 65536
#define STRUCTBUFSZ 65536
#define SYMSZ 32

#define TYPETBLSZ 8192
#define PTRTBLSZ 8192
#define ARRAYTBLSZ 8192
#define STRUCTTBLSZ 8192
#define UNIONTBLSZ 8192
#define ENUMTBLSZ 8192
#define FUNCTBLSZ 8192
#define VARTBLSZ 8192

// #define MAXDIG 15

enum OperType {
  Add, Sub, Mul, Div, Rem, And, Or, Xor, Set, Eq, Not, NotEq,
  Lss, LssEq, Grt, GrtEq, AndBrn, OrBrn, Arr, Qst, Addr, Inc, Dec,
  Let, Func, FuncName, FuncArgList, FuncArgName, FuncArgType, Poison
};

struct Value {
  struct Type { };
  
  Type type;
  union {
    long intgr;
    double fltpt;
  };
};

struct Expr {
  enum class Prec { Non, Obj, Inc, Mul, Add, Set };
  enum class Type { Non, Obj, UnOp, BinOp };

  Prec prec;
  Type type;
  Expr* parent;

  void add(Expr* expr) {};

  Expr(Prec prec, Type type) : prec(prec), type(type), parent(0) {}
};

struct ExprBinOp : Expr {
  Expr *lhe, *rhe;  
  ExprBinOp(Prec prec) : Expr(prec, Type::BinOp) {}
};

struct ExprUnOp : Expr {
  Expr *lhe;
  ExprUnOp() : Expr(Prec::Inc, Type::UnOp) {}
};

struct ExprAdd : ExprBinOp { ExprAdd() : ExprBinOp(Prec::Add) {} };
struct ExprSub : ExprBinOp { ExprSub() : ExprBinOp(Prec::Add) {} };
struct ExprMul : ExprBinOp { ExprMul() : ExprBinOp(Prec::Mul) {} };
struct ExprDiv : ExprBinOp { ExprDiv() : ExprBinOp(Prec::Mul) {} };

struct ExprInc : ExprUnOp { ExprInc() : ExprUnOp() {} };
struct ExprDec : ExprUnOp { ExprDec() : ExprUnOp() {} }; 

struct ExprSet : ExprBinOp { ExprSet() : ExprBinOp(Prec::Set) {} };

struct ExprAddSet : ExprSet { ExprAddSet() : ExprSet() {} }; 

struct ExprNum : Expr {
  Value val;
  ExprNum(Value val) : Expr(Prec::Obj, Type::Obj), val(val) {}
};

struct {
  unsigned int sym;
  unsigned int arg_type;
  unsigned int ret_type;
} func_tbl[FUNCTBLSZ];

struct {
  unsigned int sym;
  unsigned int type;
} var_tbl[VARTBLSZ];

char struct_def_buf[STRUCTBUFSZ];
char str_buf[STRBUFSZ];
char sym_buf[SYMBUFSZ];
char* curr_struct = struct_def_buf;
char* curr_str = str_buf;
char* curr_sym = sym_buf + 16;

long exp_count;
long exp_buf;
long exp_base;
long int_buf;
double float_buf;
double fract_buf;

int ch;
int line_count;
int func_count;
int var_count;
int node_count;
int sym_count;
int dec_count;
int prec;
int oper;

long math_abs(long x) {
  long y = x >> 63;
  return (x ^ y) - y;
}

int main (int argc, char** argv) {  
  FILE *input, *output;
  Expr *last;

  if (!(input = argc == 2 ? fopen(argv[1], "r") : stdin)) return 1;
  output = stdout;  

  START: GET;
  CHECK:
  if (ch == EOF) { return 0; }
  switch (ch) {    
    
    case'\n':
      line_count++;
    case ' ':
      goto START;

    case '_':
    case 'A'...'Z':
    case 'a'...'z':
      curr_sym[sym_count++] = ch;  
      SYM:
      switch (GET) {
        case '_':
        case 'A'...'Z':
        case 'a'...'z':
        case '0'...'9':
          if (sym_count < SYMSZ) { curr_sym[sym_count++] = ch; }
          goto SYM;
      }
      if (sym_count <= 8) {
        switch (*((unsigned long*)curr_sym)) {
          case AND:
          case BREAK:
          case CASE:
          case CLASS:
          case CONST:
          case CONTINUE:
          case DO:
          case ELSE:
          case ENUM:
          case FALSE:
          case FOR:
          case FUNC:
          case IF:
          case INF:
          case LET:
          case NAN:
          case NEW:
          case NOT:
          case OR:
          case RETURN:
          case STATIC:
          case STRUCT:
          case SWITCH:
          case TRUE:
          case UNION:
          case VIRTUAL:
          case VOLATILE:
          case WHILE:
            break;
          default:
            goto SYMBOL;
        }
        *((unsigned long*)curr_sym) = 0; sym_count = 0;
      }
      SYMBOL:
      curr_sym += (sym_count + 15) & ~15; sym_count = 0;
      goto START;
    
    case '\"':
      STR1:
      switch (GET) {
        case ' ':
        case '!':
        case '#':
        case '%'...'[':
        case ']'...'z':
        case '|':
        case '~':
          goto STR1;
        case '\"':
          // end
          goto START;
      }
      goto START;
    
    case '\'':
      STR2:
      switch (GET) {
        case ' '...'#':
        case '%'...'&':
        case '('...'[':
        case ']'...'z':
        case '|':
        case '~':
          goto STR2;
        case '\'':
          // end
          goto START;
      }
      goto START;
    
    case '0':
      int_buf = 0;
      switch (GET) {
        case 'H':
        case 'h':
        case 'X':
        case 'x':
          HEX:
          switch (GET) {
            case '0'...'9':
              int_buf <<= 4; int_buf |= NUM;
              goto HEX;
            case 'A'...'F':
            case 'a'...'f':
              int_buf <<= 4; int_buf |= ALNUM;
            case '_':
              goto HEX;
            case '.':
              switch (GET) {
                case '0'...'9':
                  int_buf <<= 4; int_buf |= NUM; exp_count -= 4;
                  HEXFLT:
                  switch (GET) {
                    case '0'...'9':
                      int_buf <<= 4; int_buf |= NUM; exp_count -= 4;
                    case '_':
                      goto HEXFLT;
                    case 'A'...'F':
                    case 'a'...'f':
                      int_buf <<= 4; int_buf |= ALNUM; exp_count -= 4;
                      goto HEXFLT;
                    case 'P':
                    case 'p':
                      EXP2:
                      exp_buf = 0;
                      exp_base = 2;
                      DECFLTEXP2:
                      switch (GET) {
                        case '0'...'9':
                          exp_buf *= 10; exp_buf += NUM;
                        case '_':
                          goto DECFLTEXP2;
                        case 'A'...'Z':
                        case 'a'...'z':
                          return 1;
                      }
                      goto FLTFIN;
                    case 'G'...'O':
                    case 'Q'...'Z':
                    case 'g'...'o':
                    case 'q'...'z':
                      return 1;
                  }
                  goto FLTFIN;
                case 'A'...'F':
                case 'a'...'f':
                  int_buf <<= 4; int_buf |= ALNUM; exp_count -= 4;
                  goto HEXFLT;
                case '_':
                case 'G'...'Z':
                case 'g'...'z':
                  return 1;
              }
          }
          goto INTEGER;
        case 'O':
        case 'o':
        case 'Q':
        case 'q':
          OCT:
          switch (GET) {
            case '0'...'7':
              int_buf <<= 3; int_buf |= NUM;
            case '_':
              goto OCT;
            case '.':
              switch (GET) {
                case '0'...'7':
                  int_buf <<= 3; int_buf |= NUM; exp_count -= 3;
                  OCTFLT:
                  switch (GET) {
                    case '0'...'7':
                      int_buf <<= 3; int_buf |= NUM; exp_count -= 3;
                    case '_':
                      goto OCTFLT;
                    case 'P':
                    case 'p':
                      goto EXP2;
                    case '8'...'9':
                    case 'A'...'O':
                    case 'Q'...'Z':
                    case 'a'...'o':
                    case 'q'...'z':
                      return 1;
                  }
                  goto FLTFIN;
                case '_':
                case '8'...'9':
                case 'A'...'Z':
                case 'a'...'z':
                  return 1;
              }
          }
          goto INTEGER;
        case 'B':
        case 'b':
          BIN:
          switch (GET) {
            case '0'...'1':
              int_buf <<= 1; int_buf |= NUM;
            case '_':
              goto BIN;
            case '.':
              switch (GET) {
                case '0'...'1':
                  int_buf <<= 1; int_buf |= NUM; exp_count -= 1;
                  BINFLT:
                  switch (GET) {
                    case '0'...'1':
                      int_buf <<= 1; int_buf |= NUM; exp_count -= 1;
                    case '_':
                      goto BINFLT;
                    case 'P':
                    case 'p':
                      goto EXP2;
                    case '2'...'9':
                    case 'A'...'O':
                    case 'Q'...'Z':
                    case 'a'...'o':
                    case 'q'...'z':
                      return 1;
                  }
                  goto FLTFIN;
                case '_':
                case '2'...'9':
                case 'A'...'Z':
                case 'a'...'z':
                  return 1;
              }
          }
          goto INTEGER;
        case '0'...'9':
          int_buf = NUM;
        case 'D':
        case 'd':
        case '_':
          goto DEC;
        case '.':
          goto DECDOT;
        case 'A':
        case 'C':
        case 'E'...'G':
        case 'I'...'N':
        case 'P':
        case 'R'...'W':
        case 'Y'...'Z':
        case 'a':
        case 'c':
        case 'e'...'g':
        case 'i'...'n':
        case 'p':
        case 'r'...'w':
        case 'y'...'z':
          return 1;
        default:
          goto INTEGER;
      }

    case '1'...'9':
      int_buf = NUM;
      DEC:
      switch (GET) {
        case '0'...'9':
          int_buf *= 10; int_buf += NUM;
        case '_':
          goto DEC;
        case '.':
          exp_base = 10;
          DECDOT:
          switch (GET) {
            case '0'...'9':
              int_buf *= 10; int_buf += NUM; exp_count -= 1;
              DECFLT:
              switch (GET) {
                case '0'...'9':
                  int_buf *= 10; int_buf += NUM; exp_count -= 1;
                case '_':
                  goto DECFLT;
                case 'E':
                case 'e':
                  EXP10:
                  exp_buf = 0;
                  exp_base = 10;
                  DECFLTEXP10:
                  switch (GET) {
                    case '0'...'9':
                      exp_buf *= 10; exp_buf += NUM;
                    case '_':
                      goto DECFLTEXP10;
                    case 'A'...'Z':
                    case 'a'...'z':
                      return 1;
                  }
                  goto FLTFIN;
                case 'A'...'D':
                case 'F'...'Z':
                case 'a'...'d':
                case 'f'...'z':
                  return 1;
              }
              FLTFIN:
              {
                exp_count += exp_buf; exp_buf = 1;
                int is_neg = (exp_count < 0); exp_count = math_abs(exp_count);
                while (exp_count--) { exp_buf *= exp_base; }
                float_buf = is_neg ? (double)int_buf / exp_buf : (double)int_buf * exp_buf;
                exp_count = 0; exp_buf = 0;
              }
              printf("float: %f\n", float_buf);
              goto START;
            case '_':
            case 'A'...'Z':
            case 'a'...'z':
              return 1;
          }
      }
      INTEGER:
      Value val;
      val.intgr = int_buf;
      if (last) {
        switch(last->type) {
          case Expr::Type::BinOp:
            ((ExprBinOp*)last)->rhe = new ExprNum(val);
            break;
        }
      } else {
        last = new ExprNum(val);
      }
      goto START;
    
    case '&': oper = And; prec = 5; if (GET == '=') { goto ASSIGNMENT; } goto OPERATOR;
    case '~': oper = Xor; prec = 6; if (GET == '=') { goto ASSIGNMENT; } goto OPERATOR;
    case '|': oper = Or;  prec = 7; if (GET == '=') { goto ASSIGNMENT; } goto OPERATOR;
    case '*': oper = Mul; prec = 8; if (GET == '=') { goto ASSIGNMENT; } goto OPERATOR;
    case '/': oper = Div; prec = 8; if (GET == '=') { goto ASSIGNMENT; } goto OPERATOR;
    case '%': oper = Rem; prec = 8; if (GET == '=') { goto ASSIGNMENT; } goto OPERATOR;
    
    case '?': oper = Qst; if ((ch = fgetc(input)) == '=') { goto ASSIGNMENT; } goto OPERATOR;
     
    case '+': GET;
      if (ch == '+') { last->add(new ExprInc); goto START; } else
      if (ch == '=') { last->add(new ExprAddSet); goto START; } else
                     { last->add(new ExprAdd); goto CHECK; }
    
    case '-':
      if (GET == '-') { oper = Dec; goto OPERATOR; }
      if (ch == '>') { oper = Arr; prec = 7; goto OPERATOR; }
      oper = Sub; prec = 9;
      if (ch == '=') { goto ASSIGNMENT; } goto OPERATOR;
    
    case '=':
      if (GET == '=') { oper = Eq; prec = 12; goto OPERATOR; }
      oper = Set; goto ASSIGNMENT;
    
    case '<': oper = (GET == '=') ? LssEq : Lss; prec = 11; goto OPERATOR;    
    case '>': oper = (GET == '=') ? GrtEq : Grt; prec = 11; goto OPERATOR;    
    case '!': oper = Not; if (GET == '=') { oper = NotEq; prec = 12; } goto OPERATOR;
    
    case '^': oper = Addr; goto OPERATOR;     
    case ';': goto START;
    
    case '(':
      goto START;
    
    case ')': goto START;
    
    default:
      return 1;
  }

  ASSIGNMENT:  
  OPERATOR:
    goto START;
}
