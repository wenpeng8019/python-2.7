
/* Parse tree node interface */

#ifndef Py_NODE_H
#define Py_NODE_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct _node {

    short		n_type;                 // 节点类型
    char		*n_str;                 // 该节点的 token 字符串
    int			n_lineno;               // token 所在行号
    int			n_col_offset;           // token 在行中的偏移量

    int			n_nchildren;            // 子节点数量
    struct _node	*n_child;           // 子节点列表
} node;

PyAPI_FUNC(node *) PyNode_New(int type);
PyAPI_FUNC(int) PyNode_AddChild(node *n, int type, char *str, int lineno, int col_offset);
PyAPI_FUNC(void) PyNode_Free(node *n);

/* Node access functions */
#define NCH(n)		    ((n)->n_nchildren)
	
#define CHILD(n, i)	    (&(n)->n_child[i])
#define RCHILD(n, i)    (CHILD(n, NCH(n) + i))
#define TYPE(n)		    ((n)->n_type)
#define STR(n)		    ((n)->n_str)

/* Assert that the type of a node is what we expect */
#define REQ(n, type) assert(TYPE(n) == (type))

PyAPI_FUNC(void) PyNode_ListTree(node *);

#ifdef __cplusplus
}
#endif
#endif /* !Py_NODE_H */
