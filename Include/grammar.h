
/* Grammar interface */

#ifndef Py_GRAMMAR_H
#define Py_GRAMMAR_H
#ifdef __cplusplus
extern "C" {
#endif

#include "bitset.h" /* Sigh... */

/* A label of an arc */
// 标签

typedef struct {
    int		 lb_type;
    char	*lb_str;
} label;

#define EMPTY 0		/* Label number 0 is by definition the empty label */

/* A list of labels */

typedef struct {
    int		 ll_nlabels;
    label	*ll_label;
} labellist;

/* An arc from one state to another */
// 从一个状态到另一个状态到（向量）弧

typedef struct {
    short	a_lbl;		/* Label of this arc */
    short	a_arrow;	/* State where this arc goes to */
} arc;

/* A state in a DFA */
// DFA 的一个状态

typedef struct {
    int		 s_narcs;
    arc		*s_arc;		/* Array of arcs */
	
    /* Optional accelerators */
    int		 s_lower;	/* Lowest label index */
    int		 s_upper;	/* Highest label index */
    int		*s_accel;	/* Accelerator */
    int		 s_accept;	/* Nonzero for accepting state */
} state;

/* A DFA */
// 一个 DFA 对象

typedef struct {
    int		 d_type;	/* Non-terminal this represents */
    char	*d_name;	/* For printing */
    int		 d_initial;	/* Initial state */
    int		 d_nstates;
    state	*d_state;	/* Array of states */
    bitset	 d_first;
} dfa;

/* A grammar */
// 一个 grammar（语法）对象

typedef struct {

    // DFA 数组
    int		 g_ndfas;
    dfa		*g_dfa;		/* Array of DFAs */

    // 标签列表
    labellist	 g_ll;

    int		 g_start;	/* Start symbol of the grammar */
    int		 g_accel;	/* Set if accelerators present */

} grammar;

/* FUNCTIONS */

grammar *newgrammar(int start);
dfa *adddfa(grammar *g, int type, char *name);
int addstate(dfa *d);
void addarc(dfa *d, int from, int to, int lbl);
dfa *PyGrammar_FindDFA(grammar *g, int type);

int addlabel(labellist *ll, int type, char *str);
int findlabel(labellist *ll, int type, char *str);
char *PyGrammar_LabelRepr(label *lb);
void translatelabels(grammar *g);

void addfirstsets(grammar *g);

void PyGrammar_AddAccelerators(grammar *g);
void PyGrammar_RemoveAccelerators(grammar *);

void printgrammar(grammar *g, FILE *fp);
void printnonterminals(grammar *g, FILE *fp);

#ifdef __cplusplus
}
#endif
#endif /* !Py_GRAMMAR_H */
