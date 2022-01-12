
/* 
 * meta grammar 的（基于 DFA 算法的）语义树的定义
 * 可以用来解析由 meta grammar 语言编写的源文件
 *
 * meta grammar 是 Python 语言语法的语法
 * 也就是用另一个语言，来编写 Python 语言的语法
 * 利用 meta grammar 这个语言，可以自动化的生成 Python 语言语法的（基于 DFA 算法的）语义树
 * 该语义树是用来解析由 Python 语言编写的源文件的关键
 * 这相当于用 meta grammar 语言来开发一个新的（Python）语言（的语义树）
 *
 * 当前文件，即 meta grammar 语法的语义树，则是 Python 预置的（不是动态生成的）
 * 至于是如何生成的，则不得而知。由于逻辑简单，因此大概率是手动推导的
 *
 * meta grammar 语言的语法规范（官方没有给出，这是反推出来的）
 * - MSTART: (NEWLINE | RULE)* ENDMARKER
 * - RULE: NAME ':' RHS NEWLINE
 * - RHS: ALT ('|' ALT)*
 * - ALT: ITEM+
 * - ITEM: '[' RHS ']' | ATOM ['*' | '+']
 * - ATOM: NAME | STRING | '(' RHS ')'
 *
 * 关于 meta grammar 语法规范的说明
 * - RULE 表示 Python 语言语法的一个顶级的逻辑单元
 *   整个由 Python 语言编写的源码，最终可以分解成是由多个完整的 RULE 组成的线性组合。 
 *   RULE 语句在 meta grammar 中的语法是：`NAME : RHS`，这里
 *   - NAME 是 RULE 的命名
 *   - RHS 是 RULE 的定义实现
 * - RHS 可由一个或多个目标项 ALT 构成，当存在多个目标项时，用 '|' 符号分隔
 * - 每个目标项（ALT）都是由一组连续的 ITEM 构成，而 ITEM 存在以下几种可能
 *   - `[ RHS ]`: 表示一个可有可无的项
 *   - `ATOM`: 表示一个确定的定义
 *   - `ATOM*`: 表示多个确定的定义，也可以没有
 *   - `ATOM+`: 表示至少 1 个确定的定义
 * - ATOM（确定的定义），可以是
 *   - NAME: 另一个 RULE 命名
 *   - STRING: 表示一个 Python 语言语法关键字
 *   - `(RHS)`: 表示一个 sub 子 RHS 项目
 *
 * 总结
 * - 整体逻辑类似正则表达式
 * - 逻辑上看
 *   - RULE 相当于一个函数定义，NAME 就是函数的命名，RHS 就是函数的实现（代码块）
 *   - ALT 相当于 RHS（代码块）中的 case 分支
 *   - ITEM 就是一行行的语句（表达式）的组合
 *   - ATOM 就是最后的语句（表达式）
 *     - NAME 表示调用另一个函数（RULE）
 *     - STRING 表示自身实现的功能
 *     - `(RHS)`: 表示一个 sub 子代码块
 *   所以，这相当于一个最简单的程序语言模型
 */

#include "pgenheaders.h"
#include "metagrammar.h"
#include "grammar.h"
#include "pgen.h"

// MSTART
//----------------------------

static arc arcs_0_0[3] = {
    {2, 0},
    {3, 0},
    {4, 1},
};
static arc arcs_0_1[1] = {
    {0, 1},
};
static state states_0[2] = {
    {3, arcs_0_0},
    {1, arcs_0_1},
};

// RULE
//----------------------------

static arc arcs_1_0[1] = {
    {5, 1},
};
static arc arcs_1_1[1] = {
    {6, 2},
};
static arc arcs_1_2[1] = {
    {7, 3},
};
static arc arcs_1_3[1] = {
    {3, 4},
};
static arc arcs_1_4[1] = {
    {0, 4},
};
static state states_1[5] = {
    {1, arcs_1_0},
    {1, arcs_1_1},
    {1, arcs_1_2},
    {1, arcs_1_3},
    {1, arcs_1_4},
};

// RHS
//----------------------------

static arc arcs_2_0[1] = {
    {8, 1},
};
static arc arcs_2_1[2] = {
    {9, 0},
    {0, 1},
};
static state states_2[2] = {
    {1, arcs_2_0},
    {2, arcs_2_1},
};

// ALT
//----------------------------

static arc arcs_3_0[1] = {
    {10, 1},
};
static arc arcs_3_1[2] = {
    {10, 1},
    {0, 1},
};
static state states_3[2] = {
    {1, arcs_3_0},
    {2, arcs_3_1},
};

// ITEM
//----------------------------

static arc arcs_4_0[2] = {
    {11, 1},
    {13, 2},
};
static arc arcs_4_1[1] = {
    {7, 3},
};
static arc arcs_4_2[3] = {
    {14, 4},
    {15, 4},
    {0, 2},
};
static arc arcs_4_3[1] = {
    {12, 4},
};
static arc arcs_4_4[1] = {
    {0, 4},
};
static state states_4[5] = {
    {2, arcs_4_0},
    {1, arcs_4_1},
    {3, arcs_4_2},
    {1, arcs_4_3},
    {1, arcs_4_4},
};

// ATOM
//----------------------------

static arc arcs_5_0[3] = {
    {5, 1},
    {16, 1},
    {17, 2},
};
static arc arcs_5_1[1] = {
    {0, 1},
};
static arc arcs_5_2[1] = {
    {7, 3},
};
static arc arcs_5_3[1] = {
    {18, 1},
};
static state states_5[4] = {
    {3, arcs_5_0},
    {1, arcs_5_1},
    {1, arcs_5_2},
    {1, arcs_5_3},
};

//----------------------------

static dfa dfas[6] = {
    {256, "MSTART", 0, 2, states_0, "\070\000\000"},
    {257, "RULE", 0, 5, states_1, "\040\000\000"},
    {258, "RHS", 0, 2, states_2, "\040\010\003"},
    {259, "ALT", 0, 2, states_3, "\040\010\003"},
    {260, "ITEM", 0, 5, states_4, "\040\010\003"},
    {261, "ATOM", 0, 4, states_5, "\040\000\003"},
};

// meta grammar 的语法关键字定义
static label labels[19] = {
    {0, "EMPTY"},  // ENDMARKER 表示结束标记
    {256, 0},      // MSTART
    {257, 0},      // RULE 
    {4, 0},        // NEWLINE（换行符）
    {0, 0},        // ENDMARKER 
    {1, 0},        // NAME
    {11, 0},       // COLON（冒号）
    {258, 0},      // RHS
    {259, 0},      // ALT
    {18, 0},       // VBAR（竖线）
    {260, 0},      // ITEM
    {9, 0},        // LSQB（左方括号） 
    {10, 0},       // RSQB（右方括号）
    {261, 0},      // ATOM
    {16, 0},       // STAR（星号）
    {14, 0},       // PLUS（加号）
    {3, 0},        // STRING（字符串）
    {7, 0},        // LPAR（左括号）
    {8, 0},        // RPAR（右括号）
};

static grammar _PyParser_Grammar = {

    // DFA 数组
    6, dfas,

    // 语法关键词表
    {19, labels},

    // 语义树入口 Start symbol of the grammar（MSTART）
    256
};

grammar *
meta_grammar(void)
{
    return &_PyParser_Grammar;
}

grammar *
Py_meta_grammar(void)
{
  return meta_grammar();
}
