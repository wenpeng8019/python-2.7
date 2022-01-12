#ifndef Py_TOKENIZER_H
#define Py_TOKENIZER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "object.h"

/* Tokenizer interface */

#include "token.h"      /* For token types */

#define MAXINDENT 100   /* Max indentation level */

/* Tokenizer state */
struct tok_state {

    /* Input state; buf <= cur <= inp <= end */
    /* NB an entire line is held in the buffer */
    char *buf;          /* Input buffer, or NULL; malloc'ed if fp != NULL */
    char *cur;          /* Next character in buffer */
    char *inp;          /* End of data in buffer */
    char *end;          /* End of input buffer if buf != NULL */
    char *start;        /* Start of current token if not NULL */
    int done;           /* E_OK normally, E_EOF at EOF, otherwise error code */

    /* NB If done != E_OK, cur must be == inp!!! */
    FILE *fp;                   /* Rest of input; NULL if tokenizing a string */
    int tabsize;                /* 制表符对齐字符数  Tab spacing */
    int indent;                 /* 当前缩进（在缩进状态栈中的）索引  Current indentation index */
    int indstack[MAXINDENT];    /* 缩进状态栈 Stack of indents */
    int atbol;                  /* 是否处于行首 Nonzero if at begin of new line */
    int pendin;                 /* 当前未处理完成的缩进变更数量。 Pending indents (if > 0) or dedents (if < 0) */
    char *prompt, *nextprompt;  /* 基于（命令行）交互模式的输入 For interactive prompting */
    int lineno;                 /* 当前行号 Current line number */
    int level;                  /* 由 () [] {} 定义的递归 sub 子逻辑区域的深度 () [] {} Parentheses nesting level */
            /* Used to allow free continuations inside them */

    /* Stuff for checking on different tab sizes */
    // 关于缩进字符混用的情况
    // + 也就是 ' ' 符号和 '\t' 符号混合使用的情况
    //   这里的 alttab 默认指的就是 ' ' 符号，因此 alttabsize 的默认值就是 1
    //   表示一个 ' ' 符号代表 1 个字符所进
    const char *filename;       /* For error messages */
    int altwarning;             /* Issue warning if alternate tabs don't match */
    int alterror;               /* Issue error if alternate tabs don't match */
    int alttabsize;             /* Alternate tab spacing */
    int altindstack[MAXINDENT]; /* Stack of alternate indents */
    
    /* Stuff for PEP 0263 */
    int decoding_state;         /* -1:decoding, 0:init, 1:raw */
    int decoding_erred;         /* whether erred in decoding  */
    int read_coding_spec;       /* whether 'coding:...' has been read  */
    char *encoding;

    int cont_line;              /* 是否处于（由 \ 定义的）"连接行" whether we are in a continuation line. */
    const char* line_start;     /* pointer to start of current line */
#ifndef PGEN
    PyObject *decoding_readline; /* codecs.open(...).readline */
    PyObject *decoding_buffer;
#endif
    const char* enc;
    const char* str;
    const char* input; /* Tokenizer's newline translated copy of the string. */
};

extern struct tok_state *PyTokenizer_FromString(const char *, int);
extern struct tok_state *PyTokenizer_FromFile(FILE *, char *, char *);
extern void PyTokenizer_Free(struct tok_state *);
extern int PyTokenizer_Get(struct tok_state *, char **, char **);
#if defined(PGEN) || defined(Py_USING_UNICODE)
extern char * PyTokenizer_RestoreEncoding(struct tok_state* tok,
                                          int len, int *offset);
#endif

#ifdef __cplusplus
}
#endif
#endif /* !Py_TOKENIZER_H */
