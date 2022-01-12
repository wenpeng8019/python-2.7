
/* Parser-tokenizer link implementation */

#include "pgenheaders.h"
#include "tokenizer.h"
#include "node.h"
#include "grammar.h"
#include "parser.h"
#include "parsetok.h"
#include "errcode.h"
#include "graminit.h"

int Py_TabcheckFlag;


/* Forward */
static node *parsetok(struct tok_state *, grammar *, int, perrdetail *, int *);
static void initerr(perrdetail *err_ret, const char* filename);

/* Parse input coming from a string.  Return error code, print some errors. */
node *
PyParser_ParseString(const char *s, grammar *g, int start, perrdetail *err_ret)
{
    return PyParser_ParseStringFlagsFilename(s, NULL, g, start, err_ret, 0);
}

node *
PyParser_ParseStringFlags(const char *s, grammar *g, int start,
                          perrdetail *err_ret, int flags)
{
    return PyParser_ParseStringFlagsFilename(s, NULL,
                                             g, start, err_ret, flags);
}

node *
PyParser_ParseStringFlagsFilename(const char *s, const char *filename,
                          grammar *g, int start,
                          perrdetail *err_ret, int flags)
{
    int iflags = flags;
    return PyParser_ParseStringFlagsFilenameEx(s, filename, g, start,
                                               err_ret, &iflags);
}

node *
PyParser_ParseStringFlagsFilenameEx(const char *s, const char *filename,
                          grammar *g, int start,
                          perrdetail *err_ret, int *flags)
{
    struct tok_state *tok;

    initerr(err_ret, filename);

    // 创建分词器
    // + 如果错误，直接返回
    if ((tok = PyTokenizer_FromString(s, start == file_input)) == NULL) {
        err_ret->error = PyErr_Occurred() ? E_DECODE : E_NOMEM;
        return NULL;
    }

    // 初始化分词器相关信息
    tok->filename = filename ? filename : "<string>";
    if (Py_TabcheckFlag || Py_VerboseFlag) {
        tok->altwarning = (tok->filename != NULL);
        if (Py_TabcheckFlag >= 2)
            tok->alterror++;
    }

    // 执行词法分析
    return parsetok(tok, g, start, err_ret, flags);
}

/* Parse input coming from a file.  Return error code, print some errors. */

node *
PyParser_ParseFile(FILE *fp, const char *filename, grammar *g, int start,
                   char *ps1, char *ps2, perrdetail *err_ret)
{
    return PyParser_ParseFileFlags(fp, filename, g, start, ps1, ps2,
                                   err_ret, 0);
}

node *
PyParser_ParseFileFlags(FILE *fp, const char *filename, grammar *g, int start,
                        char *ps1, char *ps2, perrdetail *err_ret, int flags)
{
    int iflags = flags;
    return PyParser_ParseFileFlagsEx(fp, filename, g, start, ps1, ps2, err_ret, &iflags);
}

node *
PyParser_ParseFileFlagsEx(FILE *fp, const char *filename, grammar *g, int start,
                          char *ps1, char *ps2, perrdetail *err_ret, int *flags)
{
    struct tok_state *tok;

    // 初始化构造错误状态信息
    initerr(err_ret, filename);

    // 创建分词器
    // + 如果错误，直接返回
    if ((tok = PyTokenizer_FromFile(fp, ps1, ps2)) == NULL) {
        err_ret->error = E_NOMEM;
        return NULL;
    }
    
    // 初始化分词器相关信息
    tok->filename = filename;
    if (Py_TabcheckFlag || Py_VerboseFlag) {
        tok->altwarning = (filename != NULL);
        if (Py_TabcheckFlag >= 2)
            tok->alterror++;
    }

    // 执行词法分析
    return parsetok(tok, g, start, err_ret, flags);
}

#if 0
static char with_msg[] =
"%s:%d: Warning: 'with' will become a reserved keyword in Python 2.6\n";

static char as_msg[] =
"%s:%d: Warning: 'as' will become a reserved keyword in Python 2.6\n";

static void
warn(const char *msg, const char *filename, int lineno)
{
    if (filename == NULL)
        filename = "<string>";
    PySys_WriteStderr(msg, filename, lineno);
}
#endif

/* Parse input coming from the given tokenizer structure.
   Return error code. */

// 执行词法/语法分析
// + 根据分词结构定义，解析输入的数据

static node *
parsetok(struct tok_state *tok, 
        grammar *g, int start,              // 语法定义
        perrdetail *err_ret,                // 词法分析错误状态信息
        int *flags)
{
    parser_state *ps;
    node *n;
    int started = 0, handling_import = 0, handling_with = 0;

    // 创建（临时） parser（运行状态）对象
    // + 用于解析处理
    if ((ps = PyParser_New(g, start)) == NULL) {
        fprintf(stderr, "no mem for new parser\n");
        err_ret->error = E_NOMEM;
        PyTokenizer_Free(tok);
        return NULL;
    }
#ifdef PY_PARSER_REQUIRES_FUTURE_KEYWORD
    if (*flags & PyPARSE_PRINT_IS_FUNCTION) {
        ps->p_flags |= CO_FUTURE_PRINT_FUNCTION;
    }
    if (*flags & PyPARSE_UNICODE_LITERALS) {
        ps->p_flags |= CO_FUTURE_UNICODE_LITERALS;
    }
#endif

    // 根据语法（DFA）定义，执行语法分析
    // + 构建语法树。
    //   也就是根据语法规则，将线性的源码 token 列表，构建成基于语法逻辑结构的树
    for (;;) {
        char *a, *b;
        int type;
        size_t len;
        char *str;
        int col_offset;

        // 解析下一个分词（切词处理，词法分析）
        type = PyTokenizer_Get(tok, &a, &b);
        if (type == ERRORTOKEN) {
            err_ret->error = tok->done;
            break;
        }

        // 如果源码读取完成
        if (type == ENDMARKER && started) {

            type = NEWLINE; /* 按新行处理 Add an extra newline */
            handling_with = handling_import = 0;
            started = 0;

            /* Add the right number of dedent tokens,
               except if a certain flag is given --
               codeop.py uses this. */
            // 如果此刻存在缩进
            // + 自动模拟缩进回退
            if (tok->indent && !(*flags & PyPARSE_DONT_IMPLY_DEDENT))
            {
                tok->pendin = -tok->indent;
                tok->indent = 0;
            }
        }
        else started = 1;

        // 获取（创建）分词 token 字符串
        len = b - a; /* XXX this may compute NULL - NULL */
        str = (char *) PyObject_MALLOC(len + 1);
        if (str == NULL) {
            fprintf(stderr, "no mem for next token\n");
            err_ret->error = E_NOMEM;
            break;
        }
        if (len > 0) strncpy(str, a, len);
        str[len] = '\0';

#ifdef PY_PARSER_REQUIRES_FUTURE_KEYWORD
#endif

        // 计算该分词在行中的偏移位置
        if (a >= tok->line_start)
            col_offset = a - tok->line_start;
        else
            col_offset = -1;

        // 添加分词到 parser（运行状态）对象
        // + 对 token 进行语法分析
        //   并在 token 的基础上，构建新的语境
        if ((err_ret->error =
             PyParser_AddToken(ps, (int)type, str, tok->lineno, col_offset,
                               &(err_ret->expected))) != E_OK) {

            if (err_ret->error != E_DONE) {
                PyObject_FREE(str);
                err_ret->token = type;
            }
            break;
        }
    }

    // 解析完成
    // + 获取根据语法规则构建的，基于语法结构，源码 token 结构树
    if (err_ret->error == E_DONE) {
        n = ps->p_tree;
        ps->p_tree = NULL;
    }
    else
        n = NULL;

#ifdef PY_PARSER_REQUIRES_FUTURE_KEYWORD
    *flags = ps->p_flags;
#endif

    // 删除 parser（临时）对象
    PyParser_Delete(ps);

    // 如果解析失败
    if (n == NULL) {

        if (tok->lineno <= 1 && tok->done == E_EOF)
            err_ret->error = E_EOF;

        err_ret->lineno = tok->lineno;

        // 如果解析的是文本（不是文件）
        if (tok->buf != NULL) {

            char *text = NULL;
            size_t len;
            assert(tok->cur - tok->buf < INT_MAX);
            err_ret->offset = (int)(tok->cur - tok->buf);
            len = tok->inp - tok->buf;
#ifdef Py_USING_UNICODE
            text = PyTokenizer_RestoreEncoding(tok, len, &err_ret->offset);

#endif
            if (text == NULL) {
                text = (char *) PyObject_MALLOC(len + 1);
                if (text != NULL) {
                    if (len > 0)
                        strncpy(text, tok->buf, len);
                    text[len] = '\0';
                }
            }
            err_ret->text = text;
        }
    }
    // 解析成功
    // 如果源码中指了定源码字符集
    else if (tok->encoding != NULL) {

        /* 'nodes->n_str' uses PyObject_*, while 'tok->encoding' was
         * allocated using PyMem_
         */
        // 创建一个新的语法 token 节点
        // + 该节点作为 "字符集设定" 的语法项
        node* r = PyNode_New(encoding_decl);
        if (r) r->n_str = PyObject_MALLOC(strlen(tok->encoding)+1);
        if (!r || !r->n_str) {
            err_ret->error = E_NOMEM;
            if (r) PyObject_FREE(r);
            n = NULL;
            goto done;
        }
        
        strcpy(r->n_str, tok->encoding);
        PyMem_FREE(tok->encoding);
        tok->encoding = NULL;
        
        // 将原语法树作为子节点
        r->n_nchildren = 1;
        r->n_child = n;

        n = r;
    }

done:
    PyTokenizer_Free(tok);

    return n;
}

static void
initerr(perrdetail *err_ret, const char *filename)
{
    err_ret->error = E_OK;
    err_ret->filename = filename;
    err_ret->lineno = 0;
    err_ret->offset = 0;
    err_ret->text = NULL;
    err_ret->token = -1;
    err_ret->expected = -1;
}
