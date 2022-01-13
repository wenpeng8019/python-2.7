
/* List a node on a file */

#include "pgenheaders.h"
#include "token.h"
#include "node.h"

/* Forward */
static void list1node(FILE *, node *);
static void listnode(FILE *, node *);

void
PyNode_ListTree(node *n)
{
    listnode(stdout, n);
}

static int level, atbol;

// 输出语义树（节点）到指定文件
static void
listnode(FILE *fp, node *n)
{
    level = 0;
    atbol = 1;
    list1node(fp, n);
}

static void
list1node(FILE *fp, node *n)
{
    if (n == 0)
        return;

    // 语义节点不是叶节点
    if (ISNONTERMINAL(TYPE(n))) {

        // 递归遍历子语义节点
        int i;
        for (i = 0; i < NCH(n); i++)
            list1node(fp, CHILD(n, i));
    }
    // 语义节点是叶节点
    else if (ISTERMINAL(TYPE(n))) {

        switch (TYPE(n)) {

        // 对于缩进语义项
        case INDENT:
            ++level;
            break;
        case DEDENT:
            --level;
            break;

        default:
        
            // 如果处于行首
            if (atbol) {
                int i;
                for (i = 0; i < level; ++i)
                    fprintf(fp, "\t");
                atbol = 0;
            }

            // 对于新行
            if (TYPE(n) == NEWLINE) {

                if (STR(n) != NULL)
                    fprintf(fp, "%s", STR(n));
                fprintf(fp, "\n");
                atbol = 1;
            }
            else
                fprintf(fp, "%s ", STR(n));

            break;
        }
    }
    else
        fprintf(fp, "? ");
}
