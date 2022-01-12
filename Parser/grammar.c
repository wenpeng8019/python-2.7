
/* Grammar implementation */

#include "Python.h"
#include "pgenheaders.h"

#include <ctype.h>

#include "token.h"
#include "grammar.h"

#ifdef RISCOS
#include <unixlib.h>
#endif

extern int Py_DebugFlag;

// 创建一个（基于 dft 算法的）语义树
grammar *
newgrammar(int start)
{
    grammar *g;

    g = (grammar *)PyObject_MALLOC(sizeof(grammar));
    if (g == NULL)
        Py_FatalError("no mem for new grammar");

    g->g_ndfas = 0;
    g->g_dfa = NULL;
    g->g_start = start;
    g->g_ll.ll_nlabels = 0;
    g->g_ll.ll_label = NULL;
    g->g_accel = 0;
    return g;
}

// 添加一个（新的）dfa 对象到语义树
dfa *
adddfa(grammar *g, int type, char *name)
{
    dfa *d;

    g->g_dfa = (dfa *)PyObject_REALLOC(g->g_dfa, sizeof(dfa) * (g->g_ndfas + 1));
    if (g->g_dfa == NULL)
        Py_FatalError("no mem to resize dfa in adddfa");

    d = &g->g_dfa[g->g_ndfas++];
    d->d_type = type;
    d->d_name = strdup(name);
    d->d_nstates = 0;
    d->d_state = NULL;
    d->d_initial = -1;
    d->d_first = NULL;
    return d; /* Only use while fresh! */
}

// 添加一个（新的）state 到 dfa 对象
int
addstate(dfa *d)
{
    state *s;

    d->d_state = (state *)PyObject_REALLOC(d->d_state, sizeof(state) * (d->d_nstates + 1));
    if (d->d_state == NULL)
        Py_FatalError("no mem to resize state in addstate");

    s = &d->d_state[d->d_nstates++];
    s->s_narcs = 0;
    s->s_arc = NULL;
    s->s_lower = 0;
    s->s_upper = 0;
    s->s_accel = NULL;
    s->s_accept = 0;
    return s - d->d_state;
}

// 添加一个（新的）arc 到 dfa 对象
void
addarc(dfa *d, int from, int to, int lbl)
{
    state *s;
    arc *a;

    assert(0 <= from && from < d->d_nstates);
    assert(0 <= to && to < d->d_nstates);

    s = &d->d_state[from];
    s->s_arc = (arc *)PyObject_REALLOC(s->s_arc, sizeof(arc) * (s->s_narcs + 1));
    if (s->s_arc == NULL)
        Py_FatalError("no mem to resize arc list in addarc");

    a = &s->s_arc[s->s_narcs++];
    a->a_lbl = lbl;
    a->a_arrow = to;
}

// 添加一个（新的）语法关键字（label）到 dfa 对象的 labellist
int
addlabel(labellist *ll, int type, char *str)
{
    int i;
    label *lb;

    // 如果已经存在
    for (i = 0; i < ll->ll_nlabels; i++) {
        if (ll->ll_label[i].lb_type == type &&
            strcmp(ll->ll_label[i].lb_str, str) == 0)
            return i;
    }

    ll->ll_label = (label *)PyObject_REALLOC(ll->ll_label,
                                    sizeof(label) * (ll->ll_nlabels + 1));
    if (ll->ll_label == NULL)
        Py_FatalError("no mem to resize labellist in addlabel");

    lb = &ll->ll_label[ll->ll_nlabels++];
    lb->lb_type = type;
    lb->lb_str = strdup(str);
    
    if (Py_DebugFlag)
        printf("Label @ %8p, %d: %s\n", ll, ll->ll_nlabels,
               PyGrammar_LabelRepr(lb));

    return lb - ll->ll_label;
}

///////////////////////////////////////////////////////////////////////////////

/* Same, but rather dies than adds */

// 查找一个语法关键字（label）
int
findlabel(labellist *ll, int type, char *str)
{
    int i;

    for (i = 0; i < ll->ll_nlabels; i++) {
        if (ll->ll_label[i].lb_type == type /*&&
            strcmp(ll->ll_label[i].lb_str, str) == 0*/)
            return i;
    }

    fprintf(stderr, "Label %d/'%s' not found\n", type, str);
    Py_FatalError("grammar.c:findlabel()");
    return 0; /* Make gcc -Wall happy */
}

//-----------------------------------------------------------------------------

/* Forward */
static void translabel(grammar *, label *);

void
translatelabels(grammar *g)
{
    int i;

#ifdef Py_DEBUG
    printf("Translating labels ...\n");
#endif

    /* Don't translate EMPTY */
    for (i = EMPTY+1; i < g->g_ll.ll_nlabels; i++)
        translabel(g, &g->g_ll.ll_label[i]);
}

static void
translabel(grammar *g, label *lb)
{
    int i;

    if (Py_DebugFlag)
        printf("Translating label %s ...\n", PyGrammar_LabelRepr(lb));

    // 对于 `NAME` 项
    // + 归并定义项，也就是确定该命名是否存在明确的定义项
    if (lb->lb_type == NAME) {

        // 判断该命名是否是 dfa（RULE）对象
        for (i = 0; i < g->g_ndfas; i++) {

            // 如果当前语法关键字（label）是 top 顶级语句
            if (strcmp(lb->lb_str, g->g_dfa[i].d_name) == 0) {

                if (Py_DebugFlag)
                    printf(
                        "Label %s is non-terminal %d.\n",
                        lb->lb_str,
                        g->g_dfa[i].d_type);

                lb->lb_type = g->g_dfa[i].d_type;
                free(lb->lb_str);
                lb->lb_str = NULL;
                return;
            }
        }

        // 判断该命名是否是操作符
        for (i = 0; i < (int)N_TOKENS; i++) {

            // 如果当前语法关键字（label）是操作符
            if (strcmp(lb->lb_str, _PyParser_TokenNames[i]) == 0) {

                if (Py_DebugFlag)
                    printf("Label %s is terminal %d.\n",
                        lb->lb_str, i);

                lb->lb_type = i;
                free(lb->lb_str);
                lb->lb_str = NULL;
                return;
            }
        }

        printf("Can't translate NAME label '%s'\n", lb->lb_str);
        return;
    }

    // 对于 `STRING` 项
    if (lb->lb_type == STRING) {
        
        if (isalpha(Py_CHARMASK(lb->lb_str[1])) || lb->lb_str[1] == '_') {

            char *p;
            char *src;
            char *dest;
            size_t name_len;
            if (Py_DebugFlag)
                printf("Label %s is a keyword\n", lb->lb_str);

            lb->lb_type = NAME;
            src = lb->lb_str + 1;
            p = strchr(src, '\'');
            if (p)
                name_len = p - src;
            else
                name_len = strlen(src);
            dest = (char *)malloc(name_len + 1);
            if (!dest) {
                printf("Can't alloc dest '%s'\n", src);
                return;
            }
            strncpy(dest, src, name_len);
            dest[name_len] = '\0';
            free(lb->lb_str);
            lb->lb_str = dest;
        }
        else if (lb->lb_str[2] == lb->lb_str[0]) {

            int type = (int) PyToken_OneChar(lb->lb_str[1]);
            if (type != OP) {
                lb->lb_type = type;
                free(lb->lb_str);
                lb->lb_str = NULL;
            }
            else
                printf("Unknown OP label %s\n",
                    lb->lb_str);
        }
        else if (lb->lb_str[2] && lb->lb_str[3] == lb->lb_str[0]) {

            int type = (int) PyToken_TwoChars(lb->lb_str[1], lb->lb_str[2]);
            if (type != OP) {
                lb->lb_type = type;
                free(lb->lb_str);
                lb->lb_str = NULL;
            }
            else
                printf("Unknown OP label %s\n",
                    lb->lb_str);
        }
        else if (lb->lb_str[2] && lb->lb_str[3] && lb->lb_str[4] == lb->lb_str[0]) {

            int type = (int) PyToken_ThreeChars(lb->lb_str[1],
                                                lb->lb_str[2],
                                                lb->lb_str[3]);
            if (type != OP) {
                lb->lb_type = type;
                free(lb->lb_str);
                lb->lb_str = NULL;
            }
            else
                printf("Unknown OP label %s\n",
                    lb->lb_str);
        }
        else
            printf("Can't translate STRING label %s\n",
                lb->lb_str);

    } // if (lb->lb_type == STRING)

    else
        printf("Can't translate label '%s'\n",
               PyGrammar_LabelRepr(lb));
}
