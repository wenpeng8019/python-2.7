
/* Generic object operations; and implementation of None (NoObject) */
// 所有数据对象的通用操作和处理、以及对 None 对象的定义

#include "Python.h"
#include "frameobject.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////
// 针对所有数据对象的内部统计、跟踪和调试
///////////////////////////////////////////////////////////////////////////////

#ifdef Py_REF_DEBUG
Py_ssize_t _Py_RefTotal;

Py_ssize_t
_Py_GetRefTotal(void)
{
    PyObject *o;
    Py_ssize_t total = _Py_RefTotal;
    /* ignore the references to the dummy object of the dicts and sets
       because they are not reliable and not useful (now that the
       hash table code is well-tested) */
    o = _PyDict_Dummy();
    if (o != NULL)
        total -= o->ob_refcnt;
    o = _PySet_Dummy();
    if (o != NULL)
        total -= o->ob_refcnt;
    return total;
}
#endif /* Py_REF_DEBUG */

int Py_DivisionWarningFlag;
int Py_Py3kWarningFlag;

/* Object allocation routines used by NEWOBJ and NEWVAROBJ macros.
   These are used by the individual routines for object creation.
   Do not call them otherwise, they do not initialize the object! */

#ifdef Py_TRACE_REFS
/* Head of circular doubly-linked list of all objects.  These are linked
 * together via the _ob_prev and _ob_next members of a PyObject, which
 * exist only in a Py_TRACE_REFS build.
 */
static PyObject refchain = {&refchain, &refchain};

/* Insert op at the front of the list of all objects.  If force is true,
 * op is added even if _ob_prev and _ob_next are non-NULL already.  If
 * force is false amd _ob_prev or _ob_next are non-NULL, do nothing.
 * force should be true if and only if op points to freshly allocated,
 * uninitialized memory, or you've unlinked op from the list and are
 * relinking it into the front.
 * Note that objects are normally added to the list via _Py_NewReference,
 * which is called by PyObject_Init.  Not all objects are initialized that
 * way, though; exceptions include statically allocated type objects, and
 * statically allocated singletons (like Py_True and Py_None).
 */
void
_Py_AddToAllObjects(PyObject *op, int force)
{
#ifdef  Py_DEBUG
    if (!force) {
        /* If it's initialized memory, op must be in or out of
         * the list unambiguously.
         */
        assert((op->_ob_prev == NULL) == (op->_ob_next == NULL));
    }
#endif
    if (force || op->_ob_prev == NULL) {
        op->_ob_next = refchain._ob_next;
        op->_ob_prev = &refchain;
        refchain._ob_next->_ob_prev = op;
        refchain._ob_next = op;
    }
}
#endif  /* Py_TRACE_REFS */

#ifdef COUNT_ALLOCS
static PyTypeObject *type_list;
/* All types are added to type_list, at least when
   they get one object created. That makes them
   immortal, which unfortunately contributes to
   garbage itself. If unlist_types_without_objects
   is set, they will be removed from the type_list
   once the last object is deallocated. */
static int unlist_types_without_objects;
extern Py_ssize_t tuple_zero_allocs, fast_tuple_allocs;
extern Py_ssize_t quick_int_allocs, quick_neg_int_allocs;
extern Py_ssize_t null_strings, one_strings;
void
dump_counts(FILE* f)
{
    PyTypeObject *tp;

    for (tp = type_list; tp; tp = tp->tp_next)
        fprintf(f, "%s alloc'd: %" PY_FORMAT_SIZE_T "d, "
            "freed: %" PY_FORMAT_SIZE_T "d, "
            "max in use: %" PY_FORMAT_SIZE_T "d\n",
            tp->tp_name, tp->tp_allocs, tp->tp_frees,
            tp->tp_maxalloc);
    fprintf(f, "fast tuple allocs: %" PY_FORMAT_SIZE_T "d, "
        "empty: %" PY_FORMAT_SIZE_T "d\n",
        fast_tuple_allocs, tuple_zero_allocs);
    fprintf(f, "fast int allocs: pos: %" PY_FORMAT_SIZE_T "d, "
        "neg: %" PY_FORMAT_SIZE_T "d\n",
        quick_int_allocs, quick_neg_int_allocs);
    fprintf(f, "null strings: %" PY_FORMAT_SIZE_T "d, "
        "1-strings: %" PY_FORMAT_SIZE_T "d\n",
        null_strings, one_strings);
}

PyObject *
get_counts(void)
{
    PyTypeObject *tp;
    PyObject *result;
    PyObject *v;

    result = PyList_New(0);
    if (result == NULL)
        return NULL;
    for (tp = type_list; tp; tp = tp->tp_next) {
        v = Py_BuildValue("(snnn)", tp->tp_name, tp->tp_allocs,
                          tp->tp_frees, tp->tp_maxalloc);
        if (v == NULL) {
            Py_DECREF(result);
            return NULL;
        }
        if (PyList_Append(result, v) < 0) {
            Py_DECREF(v);
            Py_DECREF(result);
            return NULL;
        }
        Py_DECREF(v);
    }
    return result;
}

void
inc_count(PyTypeObject *tp)
{
    if (tp->tp_next == NULL && tp->tp_prev == NULL) {
        /* first time; insert in linked list */
        if (tp->tp_next != NULL) /* sanity check */
            Py_FatalError("XXX inc_count sanity check");
        if (type_list)
            type_list->tp_prev = tp;
        tp->tp_next = type_list;
        /* Note that as of Python 2.2, heap-allocated type objects
         * can go away, but this code requires that they stay alive
         * until program exit.  That's why we're careful with
         * refcounts here.  type_list gets a new reference to tp,
         * while ownership of the reference type_list used to hold
         * (if any) was transferred to tp->tp_next in the line above.
         * tp is thus effectively immortal after this.
         */
        Py_INCREF(tp);
        type_list = tp;
#ifdef Py_TRACE_REFS
        /* Also insert in the doubly-linked list of all objects,
         * if not already there.
         */
        _Py_AddToAllObjects((PyObject *)tp, 0);
#endif
    }
    tp->tp_allocs++;
    if (tp->tp_allocs - tp->tp_frees > tp->tp_maxalloc)
        tp->tp_maxalloc = tp->tp_allocs - tp->tp_frees;
}

void dec_count(PyTypeObject *tp)
{
    tp->tp_frees++;
    if (unlist_types_without_objects &&
        tp->tp_allocs == tp->tp_frees) {
        /* unlink the type from type_list */
        if (tp->tp_prev)
            tp->tp_prev->tp_next = tp->tp_next;
        else
            type_list = tp->tp_next;
        if (tp->tp_next)
            tp->tp_next->tp_prev = tp->tp_prev;
        tp->tp_next = tp->tp_prev = NULL;
        Py_DECREF(tp);
    }
}

#endif

#ifdef Py_REF_DEBUG
/* Log a fatal error; doesn't return. */
void
_Py_NegativeRefcount(const char *fname, int lineno, PyObject *op)
{
    char buf[300];

    PyOS_snprintf(buf, sizeof(buf),
                  "%s:%i object at %p has negative ref count "
                  "%" PY_FORMAT_SIZE_T "d",
                  fname, lineno, op, op->ob_refcnt);
    Py_FatalError(buf);
}

#endif /* Py_REF_DEBUG */

///////////////////////////////////////////////////////////////////////////////
// 数据对象和生命周期相关的操作
///////////////////////////////////////////////////////////////////////////////

// 增加数据对象的引用计数
void
Py_IncRef(PyObject *o)
{
    Py_XINCREF(o);
}

// 减少数据对象的引用计数
void
Py_DecRef(PyObject *o)
{
    Py_XDECREF(o);
}

// 初始化一个（固定结构体的）数据对象
// + 包括所属数据类型（对象）
PyObject *
PyObject_Init(PyObject *op, PyTypeObject *tp)
{
    if (op == NULL)
        return PyErr_NoMemory();

    /* Any changes should be reflected in PyObject_INIT (objimpl.h) */
    Py_TYPE(op) = tp;
    _Py_NewReference(op);
    return op;
}

// 初始化一个（可变结构体的）数据对象
// + 包括所属数据类型（对象）、以及可变部分到尺寸大小
PyVarObject *
PyObject_InitVar(PyVarObject *op, PyTypeObject *tp, Py_ssize_t size)
{
    if (op == NULL)
        return (PyVarObject *) PyErr_NoMemory();

    /* Any changes should be reflected in PyObject_INIT_VAR */
    op->ob_size = size;
    Py_TYPE(op) = tp;
    _Py_NewReference((PyObject *)op);

    return op;
}

// 创建一个（固定结构体的）数据对象
PyObject *
_PyObject_New(PyTypeObject *tp)
{
    PyObject *op;
    op = (PyObject *) PyObject_MALLOC(_PyObject_SIZE(tp));
    if (op == NULL)
        return PyErr_NoMemory();
    return PyObject_INIT(op, tp);
}

// 创建一个（可变结构体的）数据对象
PyVarObject *
_PyObject_NewVar(PyTypeObject *tp, Py_ssize_t nitems)
{
    PyVarObject *op;
    const size_t size = _PyObject_VAR_SIZE(tp, nitems);
    op = (PyVarObject *) PyObject_MALLOC(size);
    if (op == NULL)
        return (PyVarObject *)PyErr_NoMemory();
    return PyObject_INIT_VAR(op, tp, nitems);
}

/* for binary compatibility with 2.2 */
// 删除一个数据对象
#undef _PyObject_Del
void
_PyObject_Del(PyObject *op)
{
    PyObject_FREE(op);
}

///////////////////////////////////////////////////////////////////////////////
// 数据对象和展示、表示、输出相关的操作
///////////////////////////////////////////////////////////////////////////////

/* Implementation of PyObject_Print with recursion checking */
static int
internal_print(PyObject *op, FILE *fp, int flags, int nesting)
{
    int ret = 0;
    if (nesting > 10) {
        PyErr_SetString(PyExc_RuntimeError, "print recursion");
        return -1;
    }
    if (PyErr_CheckSignals())
        return -1;
#ifdef USE_STACKCHECK
    if (PyOS_CheckStack()) {
        PyErr_SetString(PyExc_MemoryError, "stack overflow");
        return -1;
    }
#endif
    clearerr(fp); /* Clear any previous error condition */
    if (op == NULL) {
        Py_BEGIN_ALLOW_THREADS
        fprintf(fp, "<nil>");
        Py_END_ALLOW_THREADS
    }
    else {
        if (op->ob_refcnt <= 0)
            /* XXX(twouters) cast refcount to long until %zd is
               universally available */
            Py_BEGIN_ALLOW_THREADS
            fprintf(fp, "<refcnt %ld at %p>",
                (long)op->ob_refcnt, op);
            Py_END_ALLOW_THREADS
        else if (Py_TYPE(op)->tp_print == NULL) {
            PyObject *s;
            if (flags & Py_PRINT_RAW)
                s = PyObject_Str(op);
            else
                s = PyObject_Repr(op);
            if (s == NULL)
                ret = -1;
            else {
                ret = internal_print(s, fp, Py_PRINT_RAW,
                                     nesting+1);
            }
            Py_XDECREF(s);
        }
        else
            ret = (*Py_TYPE(op)->tp_print)(op, fp, flags);
    }
    if (ret == 0) {
        if (ferror(fp)) {
            PyErr_SetFromErrno(PyExc_IOError);
            clearerr(fp);
            ret = -1;
        }
    }
    return ret;
}

int
PyObject_Print(PyObject *op, FILE *fp, int flags)
{
    return internal_print(op, fp, flags, 0);
}

/* For debugging convenience.  See Misc/gdbinit for some useful gdb hooks */
void _PyObject_Dump(PyObject* op)
{
    if (op == NULL)
        fprintf(stderr, "NULL\n");
    else {
#ifdef WITH_THREAD
        PyGILState_STATE gil;
#endif
        fprintf(stderr, "object  : ");
#ifdef WITH_THREAD
        gil = PyGILState_Ensure();
#endif
        (void)PyObject_Print(op, stderr, 0);
#ifdef WITH_THREAD
        PyGILState_Release(gil);
#endif
        /* XXX(twouters) cast refcount to long until %zd is
           universally available */
        fprintf(stderr, "\n"
            "type    : %s\n"
            "refcount: %ld\n"
            "address : %p\n",
            Py_TYPE(op)==NULL ? "NULL" : Py_TYPE(op)->tp_name,
            (long)op->ob_refcnt,
            op);
    }
}

PyObject *
PyObject_Repr(PyObject *v)
{
    if (PyErr_CheckSignals())
        return NULL;

#ifdef USE_STACKCHECK
    if (PyOS_CheckStack()) {
        PyErr_SetString(PyExc_MemoryError, "stack overflow");
        return NULL;
    }
#endif

    if (v == NULL)
        return PyString_FromString("<NULL>");

    else if (Py_TYPE(v)->tp_repr == NULL)
        return PyString_FromFormat("<%s object at %p>",
                                   Py_TYPE(v)->tp_name, v);

    else {

        PyObject *res;
        res = (*Py_TYPE(v)->tp_repr)(v);
        if (res == NULL)
            return NULL;

#ifdef Py_USING_UNICODE
        if (PyUnicode_Check(res)) {
            PyObject* str;
            str = PyUnicode_AsEncodedString(res, NULL, NULL);
            Py_DECREF(res);
            if (str)
                res = str;
            else
                return NULL;
        }
#endif

        if (!PyString_Check(res)) {
            PyErr_Format(PyExc_TypeError,
                         "__repr__ returned non-string (type %.200s)",
                         Py_TYPE(res)->tp_name);
            Py_DECREF(res);
            return NULL;
        }
        
        return res;
    }
}

PyObject *
_PyObject_Str(PyObject *v)
{
    PyObject *res;
    int type_ok;
    if (v == NULL)
        return PyString_FromString("<NULL>");
    if (PyString_CheckExact(v)) {
        Py_INCREF(v);
        return v;
    }
#ifdef Py_USING_UNICODE
    if (PyUnicode_CheckExact(v)) {
        Py_INCREF(v);
        return v;
    }
#endif
    if (Py_TYPE(v)->tp_str == NULL)
        return PyObject_Repr(v);

    /* It is possible for a type to have a tp_str representation that loops
       infinitely. */
    if (Py_EnterRecursiveCall(" while getting the str of an object"))
        return NULL;
    res = (*Py_TYPE(v)->tp_str)(v);
    Py_LeaveRecursiveCall();
    if (res == NULL)
        return NULL;
    type_ok = PyString_Check(res);
#ifdef Py_USING_UNICODE
    type_ok = type_ok || PyUnicode_Check(res);
#endif
    if (!type_ok) {
        PyErr_Format(PyExc_TypeError,
                     "__str__ returned non-string (type %.200s)",
                     Py_TYPE(res)->tp_name);
        Py_DECREF(res);
        return NULL;
    }
    return res;
}

PyObject *
PyObject_Str(PyObject *v)
{
    PyObject *res = _PyObject_Str(v);
    if (res == NULL)
        return NULL;
#ifdef Py_USING_UNICODE
    if (PyUnicode_Check(res)) {
        PyObject* str;
        str = PyUnicode_AsEncodedString(res, NULL, NULL);
        Py_DECREF(res);
        if (str)
            res = str;
        else
            return NULL;
    }
#endif
    assert(PyString_Check(res));
    return res;
}

#ifdef Py_USING_UNICODE
PyObject *
PyObject_Unicode(PyObject *v)
{
    PyObject *res;
    PyObject *func;
    PyObject *str;
    int unicode_method_found = 0;
    static PyObject *unicodestr;

    if (v == NULL) {
        res = PyString_FromString("<NULL>");
        if (res == NULL)
            return NULL;
        str = PyUnicode_FromEncodedObject(res, NULL, "strict");
        Py_DECREF(res);
        return str;
    } else if (PyUnicode_CheckExact(v)) {
        Py_INCREF(v);
        return v;
    }

    if (PyInstance_Check(v)) {
        /* We're an instance of a classic class */
        /* Try __unicode__ from the instance -- alas we have no type */
        func = PyObject_GetAttr(v, unicodestr);
        if (func != NULL) {
            unicode_method_found = 1;
            res = PyObject_CallFunctionObjArgs(func, NULL);
            Py_DECREF(func);
        }
        else {
            PyErr_Clear();
        }
    }
    else {
        /* Not a classic class instance, try __unicode__. */
        func = _PyObject_LookupSpecial(v, "__unicode__", &unicodestr);
        if (func != NULL) {
            unicode_method_found = 1;
            res = PyObject_CallFunctionObjArgs(func, NULL);
            Py_DECREF(func);
        }
        else if (PyErr_Occurred())
            return NULL;
    }

    /* Didn't find __unicode__ */
    if (!unicode_method_found) {
        if (PyUnicode_Check(v)) {
            /* For a Unicode subtype that's didn't overwrite __unicode__,
               return a true Unicode object with the same data. */
            return PyUnicode_FromUnicode(PyUnicode_AS_UNICODE(v),
                                         PyUnicode_GET_SIZE(v));
        }
        if (PyString_CheckExact(v)) {
            Py_INCREF(v);
            res = v;
        }
        else {
            if (Py_TYPE(v)->tp_str != NULL)
                res = (*Py_TYPE(v)->tp_str)(v);
            else
                res = PyObject_Repr(v);
        }
    }

    if (res == NULL)
        return NULL;
    if (!PyUnicode_Check(res)) {
        str = PyUnicode_FromEncodedObject(res, NULL, "strict");
        Py_DECREF(res);
        res = str;
    }
    return res;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// 数据对象和比较、比对相关的操作
///////////////////////////////////////////////////////////////////////////////

/* Helper to warn about deprecated tp_compare return values.  Return:
   -2 for an exception;
   -1 if v <  w;
    0 if v == w;
    1 if v  > w.
   (This function cannot return 2.)
*/
static int
adjust_tp_compare(int c)
{
    if (PyErr_Occurred()) {
        if (c != -1 && c != -2) {
            PyObject *t, *v, *tb;
            PyErr_Fetch(&t, &v, &tb);
            if (PyErr_Warn(PyExc_RuntimeWarning,
                           "tp_compare didn't return -1 or -2 "
                           "for exception") < 0) {
                Py_XDECREF(t);
                Py_XDECREF(v);
                Py_XDECREF(tb);
            }
            else
                PyErr_Restore(t, v, tb);
        }
        return -2;
    }
    else if (c < -1 || c > 1) {
        if (PyErr_Warn(PyExc_RuntimeWarning,
                       "tp_compare didn't return -1, 0 or 1") < 0)
            return -2;
        else
            return c < -1 ? -1 : 1;
    }
    else {
        assert(c >= -1 && c <= 1);
        return c;
    }
}


/* Macro to get the tp_richcompare field of a type if defined */
#define RICHCOMPARE(t) (PyType_HasFeature((t), Py_TPFLAGS_HAVE_RICHCOMPARE) \
             ? (t)->tp_richcompare : NULL)

/* Map rich comparison operators to their swapped version, e.g. LT --> GT */
int _Py_SwappedOp[] = {Py_GT, Py_GE, Py_EQ, Py_NE, Py_LT, Py_LE};

/* Try a genuine rich comparison, returning an object.  Return:
   NULL for exception;
   NotImplemented if this particular rich comparison is not implemented or
     undefined;
   some object not equal to NotImplemented if it is implemented
     (this latter object may not be a Boolean).
*/
static PyObject *
try_rich_compare(PyObject *v, PyObject *w, int op)
{
    richcmpfunc f;
    PyObject *res;

    if (v->ob_type != w->ob_type &&
        PyType_IsSubtype(w->ob_type, v->ob_type) &&
        (f = RICHCOMPARE(w->ob_type)) != NULL) {
        res = (*f)(w, v, _Py_SwappedOp[op]);
        if (res != Py_NotImplemented)
            return res;
        Py_DECREF(res);
    }
    if ((f = RICHCOMPARE(v->ob_type)) != NULL) {
        res = (*f)(v, w, op);
        if (res != Py_NotImplemented)
            return res;
        Py_DECREF(res);
    }
    if ((f = RICHCOMPARE(w->ob_type)) != NULL) {
        return (*f)(w, v, _Py_SwappedOp[op]);
    }
    res = Py_NotImplemented;
    Py_INCREF(res);
    return res;
}

/* Try a genuine rich comparison, returning an int.  Return:
   -1 for exception (including the case where try_rich_compare() returns an
      object that's not a Boolean);
    0 if the outcome is false;
    1 if the outcome is true;
    2 if this particular rich comparison is not implemented or undefined.
*/
static int
try_rich_compare_bool(PyObject *v, PyObject *w, int op)
{
    PyObject *res;
    int ok;

    if (RICHCOMPARE(v->ob_type) == NULL && RICHCOMPARE(w->ob_type) == NULL)
        return 2; /* Shortcut, avoid INCREF+DECREF */
    res = try_rich_compare(v, w, op);
    if (res == NULL)
        return -1;
    if (res == Py_NotImplemented) {
        Py_DECREF(res);
        return 2;
    }
    ok = PyObject_IsTrue(res);
    Py_DECREF(res);
    return ok;
}

/* Try rich comparisons to determine a 3-way comparison.  Return:
   -2 for an exception;
   -1 if v  < w;
    0 if v == w;
    1 if v  > w;
    2 if this particular rich comparison is not implemented or undefined.
*/
static int
try_rich_to_3way_compare(PyObject *v, PyObject *w)
{
    static struct { int op; int outcome; } tries[3] = {
        /* Try this operator, and if it is true, use this outcome: */
        {Py_EQ, 0},
        {Py_LT, -1},
        {Py_GT, 1},
    };
    int i;

    if (RICHCOMPARE(v->ob_type) == NULL && RICHCOMPARE(w->ob_type) == NULL)
        return 2; /* Shortcut */

    for (i = 0; i < 3; i++) {
        switch (try_rich_compare_bool(v, w, tries[i].op)) {
        case -1:
            return -2;
        case 1:
            return tries[i].outcome;
        }
    }

    return 2;
}

/* Try a 3-way comparison, returning an int.  Return:
   -2 for an exception;
   -1 if v <  w;
    0 if v == w;
    1 if v  > w;
    2 if this particular 3-way comparison is not implemented or undefined.
*/
static int
try_3way_compare(PyObject *v, PyObject *w)
{
    int c;
    cmpfunc f;

    /* Comparisons involving instances are given to instance_compare,
       which has the same return conventions as this function. */

    f = v->ob_type->tp_compare;
    if (PyInstance_Check(v))
        return (*f)(v, w);
    if (PyInstance_Check(w))
        return (*w->ob_type->tp_compare)(v, w);

    /* If both have the same (non-NULL) tp_compare, use it. */
    if (f != NULL && f == w->ob_type->tp_compare) {
        c = (*f)(v, w);
        return adjust_tp_compare(c);
    }

    /* If either tp_compare is _PyObject_SlotCompare, that's safe. */
    if (f == _PyObject_SlotCompare ||
        w->ob_type->tp_compare == _PyObject_SlotCompare)
        return _PyObject_SlotCompare(v, w);

    /* If we're here, v and w,
        a) are not instances;
        b) have different types or a type without tp_compare; and
        c) don't have a user-defined tp_compare.
       tp_compare implementations in C assume that both arguments
       have their type, so we give up if the coercion fails or if
       it yields types which are still incompatible (which can
       happen with a user-defined nb_coerce).
    */
    c = PyNumber_CoerceEx(&v, &w);
    if (c < 0)
        return -2;
    if (c > 0)
        return 2;
    f = v->ob_type->tp_compare;
    if (f != NULL && f == w->ob_type->tp_compare) {
        c = (*f)(v, w);
        Py_DECREF(v);
        Py_DECREF(w);
        return adjust_tp_compare(c);
    }

    /* No comparison defined */
    Py_DECREF(v);
    Py_DECREF(w);
    return 2;
}

/* Final fallback 3-way comparison, returning an int.  Return:
   -2 if an error occurred;
   -1 if v <  w;
    0 if v == w;
    1 if v >  w.
*/
static int
default_3way_compare(PyObject *v, PyObject *w)
{
    int c;
    const char *vname, *wname;

    if (v->ob_type == w->ob_type) {
        /* When comparing these pointers, they must be cast to
         * integer types (i.e. Py_uintptr_t, our spelling of C9X's
         * uintptr_t).  ANSI specifies that pointer compares other
         * than == and != to non-related structures are undefined.
         */
        Py_uintptr_t vv = (Py_uintptr_t)v;
        Py_uintptr_t ww = (Py_uintptr_t)w;
        return (vv < ww) ? -1 : (vv > ww) ? 1 : 0;
    }

    /* None is smaller than anything */
    if (v == Py_None)
        return -1;
    if (w == Py_None)
        return 1;

    /* different type: compare type names; numbers are smaller */
    if (PyNumber_Check(v))
        vname = "";
    else
        vname = v->ob_type->tp_name;
    if (PyNumber_Check(w))
        wname = "";
    else
        wname = w->ob_type->tp_name;
    c = strcmp(vname, wname);
    if (c < 0)
        return -1;
    if (c > 0)
        return 1;
    /* Same type name, or (more likely) incomparable numeric types */
    return ((Py_uintptr_t)(v->ob_type) < (
        Py_uintptr_t)(w->ob_type)) ? -1 : 1;
}

/* Do a 3-way comparison, by hook or by crook.  Return:
   -2 for an exception (but see below);
   -1 if v <  w;
    0 if v == w;
    1 if v >  w;
   BUT: if the object implements a tp_compare function, it returns
   whatever this function returns (whether with an exception or not).
*/
static int
do_cmp(PyObject *v, PyObject *w)
{
    int c;
    cmpfunc f;

    if (v->ob_type == w->ob_type
        && (f = v->ob_type->tp_compare) != NULL) {
        c = (*f)(v, w);
        if (PyInstance_Check(v)) {
            /* Instance tp_compare has a different signature.
               But if it returns undefined we fall through. */
            if (c != 2)
                return c;
            /* Else fall through to try_rich_to_3way_compare() */
        }
        else
            return adjust_tp_compare(c);
    }
    /* We only get here if one of the following is true:
       a) v and w have different types
       b) v and w have the same type, which doesn't have tp_compare
       c) v and w are instances, and either __cmp__ is not defined or
          __cmp__ returns NotImplemented
    */
    c = try_rich_to_3way_compare(v, w);
    if (c < 2)
        return c;
    c = try_3way_compare(v, w);
    if (c < 2)
        return c;
    return default_3way_compare(v, w);
}

/* Compare v to w.  Return
   -1 if v <  w or exception (PyErr_Occurred() true in latter case).
    0 if v == w.
    1 if v > w.
   XXX The docs (C API manual) say the return value is undefined in case
   XXX of error.
*/
int
PyObject_Compare(PyObject *v, PyObject *w)
{
    int result;

    if (v == NULL || w == NULL) {
        PyErr_BadInternalCall();
        return -1;
    }
    if (v == w)
        return 0;
    if (Py_EnterRecursiveCall(" in cmp"))
        return -1;
    result = do_cmp(v, w);
    Py_LeaveRecursiveCall();
    return result < 0 ? -1 : result;
}

/* Return (new reference to) Py_True or Py_False. */
static PyObject *
convert_3way_to_object(int op, int c)
{
    PyObject *result;
    switch (op) {
    case Py_LT: c = c <  0; break;
    case Py_LE: c = c <= 0; break;
    case Py_EQ: c = c == 0; break;
    case Py_NE: c = c != 0; break;
    case Py_GT: c = c >  0; break;
    case Py_GE: c = c >= 0; break;
    }
    result = c ? Py_True : Py_False;
    Py_INCREF(result);
    return result;
}

/* We want a rich comparison but don't have one.  Try a 3-way cmp instead.
   Return
   NULL      if error
   Py_True   if v op w
   Py_False  if not (v op w)
*/
static PyObject *
try_3way_to_rich_compare(PyObject *v, PyObject *w, int op)
{
    int c;

    c = try_3way_compare(v, w);
    if (c >= 2) {

        /* Py3K warning if types are not equal and comparison isn't == or !=  */
        if (Py_Py3kWarningFlag &&
            v->ob_type != w->ob_type && op != Py_EQ && op != Py_NE &&
            PyErr_WarnEx(PyExc_DeprecationWarning,
                       "comparing unequal types not supported "
                       "in 3.x", 1) < 0) {
            return NULL;
        }

        c = default_3way_compare(v, w);
    }
    if (c <= -2)
        return NULL;
    return convert_3way_to_object(op, c);
}

/* Do rich comparison on v and w.  Return
   NULL      if error
   Else a new reference to an object other than Py_NotImplemented, usually(?):
   Py_True   if v op w
   Py_False  if not (v op w)
*/
static PyObject *
do_richcmp(PyObject *v, PyObject *w, int op)
{
    PyObject *res;

    res = try_rich_compare(v, w, op);
    if (res != Py_NotImplemented)
        return res;
    Py_DECREF(res);

    return try_3way_to_rich_compare(v, w, op);
}

/* Return:
   NULL for exception;
   some object not equal to NotImplemented if it is implemented
     (this latter object may not be a Boolean).
*/
PyObject *
PyObject_RichCompare(PyObject *v, PyObject *w, int op)
{
    PyObject *res;

    assert(Py_LT <= op && op <= Py_GE);
    if (Py_EnterRecursiveCall(" in cmp"))
        return NULL;

    /* If the types are equal, and not old-style instances, try to
       get out cheap (don't bother with coercions etc.). */
    if (v->ob_type == w->ob_type && !PyInstance_Check(v)) {
        cmpfunc fcmp;
        richcmpfunc frich = RICHCOMPARE(v->ob_type);
        /* If the type has richcmp, try it first.  try_rich_compare
           tries it two-sided, which is not needed since we've a
           single type only. */
        if (frich != NULL) {
            res = (*frich)(v, w, op);
            if (res != Py_NotImplemented)
                goto Done;
            Py_DECREF(res);
        }
        /* No richcmp, or this particular richmp not implemented.
           Try 3-way cmp. */
        fcmp = v->ob_type->tp_compare;
        if (fcmp != NULL) {
            int c = (*fcmp)(v, w);
            c = adjust_tp_compare(c);
            if (c == -2) {
                res = NULL;
                goto Done;
            }
            res = convert_3way_to_object(op, c);
            goto Done;
        }
    }

    /* Fast path not taken, or couldn't deliver a useful result. */
    res = do_richcmp(v, w, op);
Done:
    Py_LeaveRecursiveCall();
    return res;
}

/* Return -1 if error; 1 if v op w; 0 if not (v op w). */
int
PyObject_RichCompareBool(PyObject *v, PyObject *w, int op)
{
    PyObject *res;
    int ok;

    /* Quick result when objects are the same.
       Guarantees that identity implies equality. */
    if (v == w) {
        if (op == Py_EQ)
            return 1;
        else if (op == Py_NE)
            return 0;
    }

    res = PyObject_RichCompare(v, w, op);
    if (res == NULL)
        return -1;
    if (PyBool_Check(res))
        ok = (res == Py_True);
    else
        ok = PyObject_IsTrue(res);
    Py_DECREF(res);
    return ok;
}

///////////////////////////////////////////////////////////////////////////////
// 数据对象和 Hash 值计算相关的操作
///////////////////////////////////////////////////////////////////////////////

/* Set of hash utility functions to help maintaining the invariant that
    if a==b then hash(a)==hash(b)

   All the utility functions (_Py_Hash*()) return "-1" to signify an error.
*/

long
_Py_HashDouble(double v)
{
    double intpart, fractpart;
    int expo;
    long hipart;
    long x;             /* the final hash value */
    /* This is designed so that Python numbers of different types
     * that compare equal hash to the same value; otherwise comparisons
     * of mapping keys will turn out weird.
     */

    if (!Py_IS_FINITE(v)) {
        if (Py_IS_INFINITY(v))
            return v < 0 ? -271828 : 314159;
        else
            return 0;
    }
    fractpart = modf(v, &intpart);
    if (fractpart == 0.0) {
        /* This must return the same hash as an equal int or long. */
        if (intpart > LONG_MAX/2 || -intpart > LONG_MAX/2) {
            /* Convert to long and use its hash. */
            PyObject *plong;                    /* converted to Python long */
            plong = PyLong_FromDouble(v);
            if (plong == NULL)
                return -1;
            x = PyObject_Hash(plong);
            Py_DECREF(plong);
            return x;
        }
        /* Fits in a C long == a Python int, so is its own hash. */
        x = (long)intpart;
        if (x == -1)
            x = -2;
        return x;
    }
    /* The fractional part is non-zero, so we don't have to worry about
     * making this match the hash of some other type.
     * Use frexp to get at the bits in the double.
     * Since the VAX D double format has 56 mantissa bits, which is the
     * most of any double format in use, each of these parts may have as
     * many as (but no more than) 56 significant bits.
     * So, assuming sizeof(long) >= 4, each part can be broken into two
     * longs; frexp and multiplication are used to do that.
     * Also, since the Cray double format has 15 exponent bits, which is
     * the most of any double format in use, shifting the exponent field
     * left by 15 won't overflow a long (again assuming sizeof(long) >= 4).
     */
    v = frexp(v, &expo);
    v *= 2147483648.0;          /* 2**31 */
    hipart = (long)v;           /* take the top 32 bits */
    v = (v - (double)hipart) * 2147483648.0; /* get the next 32 bits */
    x = hipart + (long)v + (expo << 15);
    if (x == -1)
        x = -2;
    return x;
}

long
_Py_HashPointer(void *p)
{
    long x;
    size_t y = (size_t)p;
    /* bottom 3 or 4 bits are likely to be 0; rotate y by 4 to avoid
       excessive hash collisions for dicts and sets */
    y = (y >> 4) | (y << (8 * SIZEOF_VOID_P - 4));
    x = (long)y;
    if (x == -1)
        x = -2;
    return x;
}

long
PyObject_HashNotImplemented(PyObject *self)
{
    PyErr_Format(PyExc_TypeError, "unhashable type: '%.200s'",
                 self->ob_type->tp_name);
    return -1;
}

_Py_HashSecret_t _Py_HashSecret;

long
PyObject_Hash(PyObject *v)
{
    PyTypeObject *tp = v->ob_type;
    if (tp->tp_hash != NULL)
        return (*tp->tp_hash)(v);
    /* To keep to the general practice that inheriting
     * solely from object in C code should work without
     * an explicit call to PyType_Ready, we implicitly call
     * PyType_Ready here and then check the tp_hash slot again
     */
    if (tp->tp_dict == NULL) {
        if (PyType_Ready(tp) < 0)
            return -1;
        if (tp->tp_hash != NULL)
            return (*tp->tp_hash)(v);
    }
    if (tp->tp_compare == NULL && RICHCOMPARE(tp) == NULL) {
        return _Py_HashPointer(v); /* Use address as hash value */
    }
    /* If there's a cmp but no hash defined, the object can't be hashed */
    return PyObject_HashNotImplemented(v);
}

///////////////////////////////////////////////////////////////////////////////
// 数据对象的属性访问操作
///////////////////////////////////////////////////////////////////////////////

PyObject *
PyObject_GetAttrString(PyObject *v, const char *name)
{
    PyObject *w, *res;

    if (Py_TYPE(v)->tp_getattr != NULL)
        return (*Py_TYPE(v)->tp_getattr)(v, (char*)name);

    w = PyString_InternFromString(name);
    if (w == NULL)
        return NULL;
    res = PyObject_GetAttr(v, w);

    Py_XDECREF(w);
    return res;
}

int
PyObject_HasAttrString(PyObject *v, const char *name)
{
    PyObject *res = PyObject_GetAttrString(v, name);
    if (res != NULL) {
        Py_DECREF(res);
        return 1;
    }
    PyErr_Clear();
    return 0;
}

int
PyObject_SetAttrString(PyObject *v, const char *name, PyObject *w)
{
    PyObject *s;
    int res;

    if (Py_TYPE(v)->tp_setattr != NULL)
        return (*Py_TYPE(v)->tp_setattr)(v, (char*)name, w);
    s = PyString_InternFromString(name);
    if (s == NULL)
        return -1;
    res = PyObject_SetAttr(v, s, w);
    Py_XDECREF(s);
    return res;
}

PyObject *
PyObject_GetAttr(PyObject *v, PyObject *name)
{
    PyTypeObject *tp = Py_TYPE(v);

    if (!PyString_Check(name)) {
#ifdef Py_USING_UNICODE
        /* The Unicode to string conversion is done here because the
           existing tp_getattro slots expect a string object as name
           and we wouldn't want to break those. */
        if (PyUnicode_Check(name)) {
            name = _PyUnicode_AsDefaultEncodedString(name, NULL);
            if (name == NULL)
                return NULL;
        }
        else
#endif
        {
            PyErr_Format(PyExc_TypeError,
                         "attribute name must be string, not '%.200s'",
                         Py_TYPE(name)->tp_name);
            return NULL;
        }
    }
    if (tp->tp_getattro != NULL)
        return (*tp->tp_getattro)(v, name);
        
    if (tp->tp_getattr != NULL)
        return (*tp->tp_getattr)(v, PyString_AS_STRING(name));

    PyErr_Format(PyExc_AttributeError,
                 "'%.50s' object has no attribute '%.400s'",
                 tp->tp_name, PyString_AS_STRING(name));
    return NULL;
}

int
PyObject_HasAttr(PyObject *v, PyObject *name)
{
    PyObject *res = PyObject_GetAttr(v, name);
    if (res != NULL) {
        Py_DECREF(res);
        return 1;
    }
    PyErr_Clear();
    return 0;
}

int
PyObject_SetAttr(PyObject *v, PyObject *name, PyObject *value)
{
    PyTypeObject *tp = Py_TYPE(v);
    int err;

    if (!PyString_Check(name)){
#ifdef Py_USING_UNICODE
        /* The Unicode to string conversion is done here because the
           existing tp_setattro slots expect a string object as name
           and we wouldn't want to break those. */
        if (PyUnicode_Check(name)) {
            name = PyUnicode_AsEncodedString(name, NULL, NULL);
            if (name == NULL)
                return -1;
        }
        else
#endif
        {
            PyErr_Format(PyExc_TypeError,
                         "attribute name must be string, not '%.200s'",
                         Py_TYPE(name)->tp_name);
            return -1;
        }
    }
    else
        Py_INCREF(name);

    PyString_InternInPlace(&name);
    if (tp->tp_setattro != NULL) {
        err = (*tp->tp_setattro)(v, name, value);
        Py_DECREF(name);
        return err;
    }
    if (tp->tp_setattr != NULL) {
        err = (*tp->tp_setattr)(v, PyString_AS_STRING(name), value);
        Py_DECREF(name);
        return err;
    }
    Py_DECREF(name);
    if (tp->tp_getattr == NULL && tp->tp_getattro == NULL)
        PyErr_Format(PyExc_TypeError,
                     "'%.100s' object has no attributes "
                     "(%s .%.100s)",
                     tp->tp_name,
                     value==NULL ? "del" : "assign to",
                     PyString_AS_STRING(name));
    else
        PyErr_Format(PyExc_TypeError,
                     "'%.100s' object has only read-only attributes "
                     "(%s .%.100s)",
                     tp->tp_name,
                     value==NULL ? "del" : "assign to",
                     PyString_AS_STRING(name));
    return -1;
}

/* Helper to get a pointer to an object's __dict__ slot, if any */

// 获取对象的 "__dict__" 内置成员
PyObject **
_PyObject_GetDictPtr(PyObject *obj)
{
    Py_ssize_t dictoffset;

    // 获取对象的数据类型（对象）
    PyTypeObject *tp = Py_TYPE(obj);

    // 如果该数据类型（对象）不支持类机制
    // + 此时，不支持 __dict__ 特性
    if (!(tp->tp_flags & Py_TPFLAGS_HAVE_CLASS))
        return NULL;

    // 获取该数据类型（对象）的实例（对象）的 dict 地址偏移量
    // 如果该数据类型（对象）没有定义 dict 项，则直接返回
    dictoffset = tp->tp_dictoffset;
    if (dictoffset == 0)
        return NULL;

    // 如果该数据类型（对象）的实例（对象），是个动态可变对象
    if (dictoffset < 0) {
        Py_ssize_t tsize;
        size_t size;

        // 计算动态可变部分长度
        tsize = ((PyVarObject *)obj)->ob_size;
        if (tsize < 0)
            tsize = -tsize;
        size = _PyObject_VAR_SIZE(tp, tsize);

        dictoffset += (long)size;
        assert(dictoffset > 0);
        assert(dictoffset % SIZEOF_VOID_P == 0);
    }
    
    // (根据 dict 地址偏移量) 返回 dict 对象
    return (PyObject **) ((char *)obj + dictoffset);
}

/* Generic GetAttr functions - put these in your tp_[gs]etattro slot */
// 获取指定实例（对象）的 attr 属性的通用（默认）实现
// 一般会将它作为数据类型（对象）的 tp_getattro slot（接口）的实现。
// 此外，也可以直接调用该接口，用以实现相同的功能和目的
// + 关于参数
//   - `obj`
//     + 目标实例（对象），也就是要获取它的 attr 属性
//       当作为数据类型（对象）的 tp_getattro slot（接口）时，该参数就相当于 this 指针
PyObject *
_PyObject_GenericGetAttrWithDict(PyObject *obj, PyObject *name, PyObject *dict)
{   // @ PyObject_GenericGetAttr

    // 获取该实例（对象）的数据类型（对象） 
    PyTypeObject *tp = Py_TYPE(obj);

    PyObject *descr = NULL;
    PyObject *res = NULL;
    descrgetfunc f;
    Py_ssize_t dictoffset;
    PyObject **dictptr;

    // 验证要访问的 attr 属性命名
    if (!PyString_Check(name)){

#ifdef Py_USING_UNICODE
        /* The Unicode to string conversion is done here because the
           existing tp_setattro slots expect a string object as name
           and we wouldn't want to break those. */
        // 对 Unicode 字符串进行转换，因为 tp_setattro 接口不支持 unicode
        if (PyUnicode_Check(name)) {
            name = PyUnicode_AsEncodedString(name, NULL, NULL);
            if (name == NULL)
                return NULL;
        }
        else
#endif
        {
            PyErr_Format(PyExc_TypeError,
                         "attribute name must be string, not '%.200s'",
                         Py_TYPE(name)->tp_name);
            return NULL;
        }
    }
    else
        Py_INCREF(name);

    // 确保该实例（对象）的数据类型（对象）完成 ready 初始化处理
    if (tp->tp_dict == NULL) {
        if (PyType_Ready(tp) < 0)
            goto done;
    }

#if 0 /* XXX this is not quite _PyType_Lookup anymore */
    /* Inline _PyType_Lookup */
    {
        Py_ssize_t i, n;
        PyObject *mro, *base, *dict;

        /* Look in tp_dict of types in MRO */
        mro = tp->tp_mro;
        assert(mro != NULL);
        assert(PyTuple_Check(mro));
        n = PyTuple_GET_SIZE(mro);
        for (i = 0; i < n; i++) {
            base = PyTuple_GET_ITEM(mro, i);
            if (PyClass_Check(base))
                dict = ((PyClassObject *)base)->cl_dict;
            else {
                assert(PyType_Check(base));
                dict = ((PyTypeObject *)base)->tp_dict;
            }
            assert(dict && PyDict_Check(dict));
            descr = PyDict_GetItem(dict, name);
            if (descr != NULL)
                break;
        }
    }
#else
    // 根据数据类型（对象）的继承性，获取 {数据类型（对象）的 name 定义项}
    descr = _PyType_Lookup(tp, name);
#endif
    Py_XINCREF(descr);

    f = NULL;

    // 如果得到的 {数据类型（对象）的 name 定义项}，是（某个数据类型的）实例（对象）
    // + 那么，{数据类型（对象）的 name 定义项} 就是一个 descriptor（对象）
    //   此时，该 descriptor 对象，重载并定义了对该数据类型（对象）的实例（对象）的 name 属性的访问
    if (descr != NULL &&
        PyType_HasFeature(descr->ob_type, Py_TPFLAGS_HAVE_CLASS)) {

        // 获取 descr 的 tp_descr_get 处理接口
        f = descr->ob_type->tp_descr_get;

        // 如果 descr 同时存在 tp_descr_set 处理接口，也就是 descr 是可读写的
        // + 此时直接用 descr 的 tp_descr_get 处理接口来返回相应的值
        // - 如果 descr 是可读写的，那么对 name 的读写，就都由 descr 来负责，也就是写入的数据会保存到 descr
        // - 如果 descr 只读时，那么对 name 的写入，就相当于对 descr 的（只读）值的覆盖和重载。
        //   此时，descr 的（只读）值就相当于一个默认值，而重载的数据会保存到实例（对象）自己的数据 dict 中。
        if (f != NULL && PyDescr_IsData(descr)) {

            res = f(descr, obj, (PyObject *)obj->ob_type);
            Py_DECREF(descr);

            goto done;
        }
    }

    /// 从实例（对象）的数据 dict 中获取 name 项

    // 如果参数中没有指定该实例（对象）的数据 dict
    // + 通过该实例（对象）的 GetDictPtr 接口来自动获取其数据 dict
    if (dict == NULL) {

        /* Inline _PyObject_GetDictPtr */
        // 如果该实例（对象）存在数据 dict
        dictoffset = tp->tp_dictoffset;
        if (dictoffset != 0) {

            if (dictoffset < 0) {
                Py_ssize_t tsize;
                size_t size;

                tsize = ((PyVarObject *)obj)->ob_size;
                if (tsize < 0)
                    tsize = -tsize;
                size = _PyObject_VAR_SIZE(tp, tsize);

                dictoffset += (long)size;
                assert(dictoffset > 0);
                assert(dictoffset % SIZEOF_VOID_P == 0);
            }

            // 获取该实例（对象）的数据 dict
            dictptr = (PyObject **) ((char *)obj + dictoffset);
            dict = *dictptr;
        }
    }
    // 如果该实例（对象）存在数据 dict
    if (dict != NULL) {
        Py_INCREF(dict);

        // 获取数据 dict 中的 name 项
        res = PyDict_GetItem(dict, name);
        if (res != NULL) {
            Py_INCREF(res);
            Py_XDECREF(descr);
            Py_DECREF(dict);
            goto done;
        }
        Py_DECREF(dict);
    }

    // 如果 {数据类型（对象）的 name 定义项} 是个只读的 descriptor（对象）
    // + 此时说明在实例（对象）自己的数据 dict 中，并没有对 name 项进行重载覆盖
    if (f != NULL) {
        res = f(descr, obj, (PyObject *)Py_TYPE(obj));
        Py_DECREF(descr);
        goto done;
    }

    // 如果数据类型（对象）的 name 项不是 descriptor 对象
    // + 此时该 name 项被视为是数据类型（对象）的 {静态成员}
    if (descr != NULL) {
        res = descr;
        /* descr was already increfed above */
        goto done;
    }

    PyErr_Format(PyExc_AttributeError,
                 "'%.50s' object has no attribute '%.400s'",
                 tp->tp_name, PyString_AS_STRING(name));
  done:
    Py_DECREF(name);
    return res;
}

PyObject *
PyObject_GenericGetAttr(PyObject *obj, PyObject *name)
{
    return _PyObject_GenericGetAttrWithDict(obj, name, NULL);
}

int
_PyObject_GenericSetAttrWithDict(PyObject *obj, PyObject *name,
                                 PyObject *value, PyObject *dict)
{
    PyTypeObject *tp = Py_TYPE(obj);
    PyObject *descr;
    descrsetfunc f;
    PyObject **dictptr;
    int res = -1;

    if (!PyString_Check(name)){
#ifdef Py_USING_UNICODE
        /* The Unicode to string conversion is done here because the
           existing tp_setattro slots expect a string object as name
           and we wouldn't want to break those. */
        if (PyUnicode_Check(name)) {
            name = PyUnicode_AsEncodedString(name, NULL, NULL);
            if (name == NULL)
                return -1;
        }
        else
#endif
        {
            PyErr_Format(PyExc_TypeError,
                         "attribute name must be string, not '%.200s'",
                         Py_TYPE(name)->tp_name);
            return -1;
        }
    }
    else
        Py_INCREF(name);

    if (tp->tp_dict == NULL) {
        if (PyType_Ready(tp) < 0)
            goto done;
    }

    descr = _PyType_Lookup(tp, name);
    f = NULL;
    if (descr != NULL &&
        PyType_HasFeature(descr->ob_type, Py_TPFLAGS_HAVE_CLASS)) {
        f = descr->ob_type->tp_descr_set;
        if (f != NULL && PyDescr_IsData(descr)) {
            res = f(descr, obj, value);
            goto done;
        }
    }

    if (dict == NULL) {
        dictptr = _PyObject_GetDictPtr(obj);
        if (dictptr != NULL) {
            dict = *dictptr;
            if (dict == NULL && value != NULL) {
                dict = PyDict_New();
                if (dict == NULL)
                    goto done;
                *dictptr = dict;
            }
        }
    }
    if (dict != NULL) {
        Py_INCREF(dict);
        if (value == NULL)
            res = PyDict_DelItem(dict, name);
        else
            res = PyDict_SetItem(dict, name, value);
        if (res < 0 && PyErr_ExceptionMatches(PyExc_KeyError))
            PyErr_SetObject(PyExc_AttributeError, name);
        Py_DECREF(dict);
        goto done;
    }

    if (f != NULL) {
        res = f(descr, obj, value);
        goto done;
    }

    if (descr == NULL) {
        PyErr_Format(PyExc_AttributeError,
                     "'%.100s' object has no attribute '%.200s'",
                     tp->tp_name, PyString_AS_STRING(name));
        goto done;
    }

    PyErr_Format(PyExc_AttributeError,
                 "'%.50s' object attribute '%.400s' is read-only",
                 tp->tp_name, PyString_AS_STRING(name));
  done:
    Py_DECREF(name);
    return res;
}

int
PyObject_GenericSetAttr(PyObject *obj, PyObject *name, PyObject *value)
{
    return _PyObject_GenericSetAttrWithDict(obj, name, value, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// 数据对象和迭代、遍历相关的操作
///////////////////////////////////////////////////////////////////////////////

PyObject *
PyObject_SelfIter(PyObject *obj)
{
    Py_INCREF(obj);
    return obj;
}

/* Helper used when the __next__ method is removed from a type:
   tp_iternext is never NULL and can be safely called without checking
   on every iteration.
 */

PyObject *
_PyObject_NextNotImplemented(PyObject *self)
{
    PyErr_Format(PyExc_TypeError,
                 "'%.200s' object is not iterable",
                 Py_TYPE(self)->tp_name);
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// 数据对象的逻辑值判定
///////////////////////////////////////////////////////////////////////////////

/* Test a value used as condition, e.g., in a for or if statement.
   Return -1 if an error occurred */

int
PyObject_IsTrue(PyObject *v)
{
    Py_ssize_t res;
    if (v == Py_True)
        return 1;
    if (v == Py_False)
        return 0;
    if (v == Py_None)
        return 0;
    else if (v->ob_type->tp_as_number != NULL &&
             v->ob_type->tp_as_number->nb_nonzero != NULL)
        res = (*v->ob_type->tp_as_number->nb_nonzero)(v);
    else if (v->ob_type->tp_as_mapping != NULL &&
             v->ob_type->tp_as_mapping->mp_length != NULL)
        res = (*v->ob_type->tp_as_mapping->mp_length)(v);
    else if (v->ob_type->tp_as_sequence != NULL &&
             v->ob_type->tp_as_sequence->sq_length != NULL)
        res = (*v->ob_type->tp_as_sequence->sq_length)(v);
    else
        return 1;
    /* if it is negative, it should be either -1 or -2 */
    return (res > 0) ? 1 : Py_SAFE_DOWNCAST(res, Py_ssize_t, int);
}

/* equivalent of 'not v'
   Return -1 if an error occurred */

int
PyObject_Not(PyObject *v)
{
    int res;
    res = PyObject_IsTrue(v);
    if (res < 0)
        return res;
    return res == 0;
}

/* Coerce two numeric types to the "larger" one.
   Increment the reference count on each argument.
   Return value:
   -1 if an error occurred;
   0 if the coercion succeeded (and then the reference counts are increased);
   1 if no coercion is possible (and no error is raised).
*/
int
PyNumber_CoerceEx(PyObject **pv, PyObject **pw)
{
    register PyObject *v = *pv;
    register PyObject *w = *pw;
    int res;

    /* Shortcut only for old-style types */
    if (v->ob_type == w->ob_type &&
        !PyType_HasFeature(v->ob_type, Py_TPFLAGS_CHECKTYPES))
    {
        Py_INCREF(v);
        Py_INCREF(w);
        return 0;
    }
    if (v->ob_type->tp_as_number && v->ob_type->tp_as_number->nb_coerce) {
        res = (*v->ob_type->tp_as_number->nb_coerce)(pv, pw);
        if (res <= 0)
            return res;
    }
    if (w->ob_type->tp_as_number && w->ob_type->tp_as_number->nb_coerce) {
        res = (*w->ob_type->tp_as_number->nb_coerce)(pw, pv);
        if (res <= 0)
            return res;
    }
    return 1;
}

/* Coerce two numeric types to the "larger" one.
   Increment the reference count on each argument.
   Return -1 and raise an exception if no coercion is possible
   (and then no reference count is incremented).
*/
int
PyNumber_Coerce(PyObject **pv, PyObject **pw)
{
    int err = PyNumber_CoerceEx(pv, pw);
    if (err <= 0)
        return err;
    PyErr_SetString(PyExc_TypeError, "number coercion failed");
    return -1;
}


/* Test whether an object can be called */

int
PyCallable_Check(PyObject *x)
{
    if (x == NULL)
        return 0;
    if (PyInstance_Check(x)) {
        PyObject *call = PyObject_GetAttrString(x, "__call__");
        if (call == NULL) {
            PyErr_Clear();
            return 0;
        }
        /* Could test recursively but don't, for fear of endless
           recursion if some joker sets self.__call__ = self */
        Py_DECREF(call);
        return 1;
    }
    else {
        return x->ob_type->tp_call != NULL;
    }
}

///////////////////////////////////////////////////////////////////////////////
// 数据对象所属目录（域）的计算
///////////////////////////////////////////////////////////////////////////////

/* ------------------------- PyObject_Dir() helpers ------------------------- */

/* Helper for PyObject_Dir.
   Merge the __dict__ of aclass into dict, and recursively also all
   the __dict__s of aclass's base classes.  The order of merging isn't
   defined, as it's expected that only the final set of dict keys is
   interesting.
   Return 0 on success, -1 on error.
*/

static int
merge_class_dict(PyObject* dict, PyObject* aclass)
{
    PyObject *classdict;
    PyObject *bases;

    assert(PyDict_Check(dict));
    assert(aclass);

    /* Merge in the type's dict (if any). */
    classdict = PyObject_GetAttrString(aclass, "__dict__");
    if (classdict == NULL)
        PyErr_Clear();
    else {
        int status = PyDict_Update(dict, classdict);
        Py_DECREF(classdict);
        if (status < 0)
            return -1;
    }

    /* Recursively merge in the base types' (if any) dicts. */
    bases = PyObject_GetAttrString(aclass, "__bases__");
    if (bases == NULL)
        PyErr_Clear();
    else {
        /* We have no guarantee that bases is a real tuple */
        Py_ssize_t i, n;
        n = PySequence_Size(bases); /* This better be right */
        if (n < 0)
            PyErr_Clear();
        else {
            for (i = 0; i < n; i++) {
                int status;
                PyObject *base = PySequence_GetItem(bases, i);
                if (base == NULL) {
                    Py_DECREF(bases);
                    return -1;
                }
                status = merge_class_dict(dict, base);
                Py_DECREF(base);
                if (status < 0) {
                    Py_DECREF(bases);
                    return -1;
                }
            }
        }
        Py_DECREF(bases);
    }
    return 0;
}

/* Helper for PyObject_Dir.
   If obj has an attr named attrname that's a list, merge its string
   elements into keys of dict.
   Return 0 on success, -1 on error.  Errors due to not finding the attr,
   or the attr not being a list, are suppressed.
*/

static int
merge_list_attr(PyObject* dict, PyObject* obj, const char *attrname)
{
    PyObject *list;
    int result = 0;

    assert(PyDict_Check(dict));
    assert(obj);
    assert(attrname);

    list = PyObject_GetAttrString(obj, attrname);
    if (list == NULL)
        PyErr_Clear();

    else if (PyList_Check(list)) {
        int i;
        for (i = 0; i < PyList_GET_SIZE(list); ++i) {
            PyObject *item = PyList_GET_ITEM(list, i);
            if (PyString_Check(item)) {
                result = PyDict_SetItem(dict, item, Py_None);
                if (result < 0)
                    break;
            }
        }
        if (Py_Py3kWarningFlag &&
            (strcmp(attrname, "__members__") == 0 ||
             strcmp(attrname, "__methods__") == 0)) {
            if (PyErr_WarnEx(PyExc_DeprecationWarning,
                           "__members__ and __methods__ not "
                           "supported in 3.x", 1) < 0) {
                Py_XDECREF(list);
                return -1;
            }
        }
    }

    Py_XDECREF(list);
    return result;
}

/* Helper for PyObject_Dir without arguments: returns the local scope. */
static PyObject *
_dir_locals(void)
{
    PyObject *names;
    PyObject *locals = PyEval_GetLocals();

    if (locals == NULL) {
        PyErr_SetString(PyExc_SystemError, "frame does not exist");
        return NULL;
    }

    names = PyMapping_Keys(locals);
    if (!names)
        return NULL;
    if (!PyList_Check(names)) {
        PyErr_Format(PyExc_TypeError,
            "dir(): expected keys() of locals to be a list, "
            "not '%.200s'", Py_TYPE(names)->tp_name);
        Py_DECREF(names);
        return NULL;
    }
    /* the locals don't need to be DECREF'd */
    return names;
}

/* Helper for PyObject_Dir of type objects: returns __dict__ and __bases__.
   We deliberately don't suck up its __class__, as methods belonging to the
   metaclass would probably be more confusing than helpful.
*/
static PyObject *
_specialized_dir_type(PyObject *obj)
{
    PyObject *result = NULL;
    PyObject *dict = PyDict_New();

    if (dict != NULL && merge_class_dict(dict, obj) == 0)
        result = PyDict_Keys(dict);

    Py_XDECREF(dict);
    return result;
}

/* Helper for PyObject_Dir of module objects: returns the module's __dict__. */
static PyObject *
_specialized_dir_module(PyObject *obj)
{
    PyObject *result = NULL;
    PyObject *dict = PyObject_GetAttrString(obj, "__dict__");

    if (dict != NULL) {
        if (PyDict_Check(dict))
            result = PyDict_Keys(dict);
        else {
            char *name = PyModule_GetName(obj);
            if (name)
                PyErr_Format(PyExc_TypeError,
                             "%.200s.__dict__ is not a dictionary",
                             name);
        }
    }

    Py_XDECREF(dict);
    return result;
}

/* Helper for PyObject_Dir of generic objects: returns __dict__, __class__,
   and recursively up the __class__.__bases__ chain.
*/
static PyObject *
_generic_dir(PyObject *obj)
{
    PyObject *result = NULL;
    PyObject *dict = NULL;
    PyObject *itsclass = NULL;

    /* Get __dict__ (which may or may not be a real dict...) */
    dict = PyObject_GetAttrString(obj, "__dict__");
    if (dict == NULL) {
        PyErr_Clear();
        dict = PyDict_New();
    }
    else if (!PyDict_Check(dict)) {
        Py_DECREF(dict);
        dict = PyDict_New();
    }
    else {
        /* Copy __dict__ to avoid mutating it. */
        PyObject *temp = PyDict_Copy(dict);
        Py_DECREF(dict);
        dict = temp;
    }

    if (dict == NULL)
        goto error;

    /* Merge in __members__ and __methods__ (if any).
     * This is removed in Python 3000. */
    if (merge_list_attr(dict, obj, "__members__") < 0)
        goto error;
    if (merge_list_attr(dict, obj, "__methods__") < 0)
        goto error;

    /* Merge in attrs reachable from its class. */
    itsclass = PyObject_GetAttrString(obj, "__class__");
    if (itsclass == NULL)
        /* XXX(tomer): Perhaps fall back to obj->ob_type if no
                       __class__ exists? */
        PyErr_Clear();
    else {
        if (merge_class_dict(dict, itsclass) != 0)
            goto error;
    }

    result = PyDict_Keys(dict);
    /* fall through */
error:
    Py_XDECREF(itsclass);
    Py_XDECREF(dict);
    return result;
}

/* Helper for PyObject_Dir: object introspection.
   This calls one of the above specialized versions if no __dir__ method
   exists. */
static PyObject *
_dir_object(PyObject *obj)
{
    PyObject *result = NULL;
    static PyObject *dir_str = NULL;
    PyObject *dirfunc;

    assert(obj);
    if (PyInstance_Check(obj)) {
        dirfunc = PyObject_GetAttrString(obj, "__dir__");
        if (dirfunc == NULL) {
            if (PyErr_ExceptionMatches(PyExc_AttributeError))
                PyErr_Clear();
            else
                return NULL;
        }
    }
    else {
        dirfunc = _PyObject_LookupSpecial(obj, "__dir__", &dir_str);
        if (PyErr_Occurred())
            return NULL;
    }
    if (dirfunc == NULL) {
        /* use default implementation */
        if (PyModule_Check(obj))
            result = _specialized_dir_module(obj);
        else if (PyType_Check(obj) || PyClass_Check(obj))
            result = _specialized_dir_type(obj);
        else
            result = _generic_dir(obj);
    }
    else {
        /* use __dir__ */
        result = PyObject_CallFunctionObjArgs(dirfunc, NULL);
        Py_DECREF(dirfunc);
        if (result == NULL)
            return NULL;

        /* result must be a list */
        /* XXX(gbrandl): could also check if all items are strings */
        if (!PyList_Check(result)) {
            PyErr_Format(PyExc_TypeError,
                         "__dir__() must return a list, not %.200s",
                         Py_TYPE(result)->tp_name);
            Py_DECREF(result);
            result = NULL;
        }
    }

    return result;
}

/* Implementation of dir() -- if obj is NULL, returns the names in the current
   (local) scope.  Otherwise, performs introspection of the object: returns a
   sorted list of attribute names (supposedly) accessible from the object
*/
PyObject *
PyObject_Dir(PyObject *obj)
{
    PyObject * result;

    if (obj == NULL)
        /* no object -- introspect the locals */
        result = _dir_locals();
    else
        /* object -- introspect the object */
        result = _dir_object(obj);

    assert(result == NULL || PyList_Check(result));

    if (result != NULL && PyList_Sort(result) != 0) {
        /* sorting the list failed */
        Py_DECREF(result);
        result = NULL;
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
// 数据对象 None、及其数据类型（对象）PyNone_Type 的定义
///////////////////////////////////////////////////////////////////////////////

/*
NoObject is usable as a non-NULL undefined value, used by the macro None.
There is (and should be!) no way to create other objects of this type,
so there is exactly one (which is indestructible, by the way).
(XXX This type and the type of NotImplemented below should be unified.)
*/

/* ARGSUSED */
static PyObject *
none_repr(PyObject *op)
{
    return PyString_FromString("None");
}

/* ARGUSED */
static void
none_dealloc(PyObject* ignore)
{
    /* This should never get called, but we also don't want to SEGV if
     * we accidentally decref None out of existence.
     */
    Py_FatalError("deallocating None");
}


static PyTypeObject PyNone_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "NoneType",
    0,
    0,
    none_dealloc,       /*tp_dealloc*/ /*never called*/
    0,                  /*tp_print*/
    0,                  /*tp_getattr*/
    0,                  /*tp_setattr*/
    0,                  /*tp_compare*/
    none_repr,          /*tp_repr*/
    0,                  /*tp_as_number*/
    0,                  /*tp_as_sequence*/
    0,                  /*tp_as_mapping*/
    (hashfunc)_Py_HashPointer, /*tp_hash */
};

PyObject _Py_NoneStruct = {
  _PyObject_EXTRA_INIT
  1, &PyNone_Type
};

///////////////////////////////////////////////////////////////////////////////
// 数据对象 NotImplemented、及其数据类型（对象）PyNotImplemented_Type 的定义
///////////////////////////////////////////////////////////////////////////////

/* NotImplemented is an object that can be used to signal that an
   operation is not implemented for the given type combination. */

static PyObject *
NotImplemented_repr(PyObject *op)
{
    return PyString_FromString("NotImplemented");
}

static PyTypeObject PyNotImplemented_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "NotImplementedType",
    0,
    0,
    none_dealloc,       /*tp_dealloc*/ /*never called*/
    0,                  /*tp_print*/
    0,                  /*tp_getattr*/
    0,                  /*tp_setattr*/
    0,                  /*tp_compare*/
    NotImplemented_repr, /*tp_repr*/
    0,                  /*tp_as_number*/
    0,                  /*tp_as_sequence*/
    0,                  /*tp_as_mapping*/
    0,                  /*tp_hash */
};

PyObject _Py_NotImplementedStruct = {
    _PyObject_EXTRA_INIT
    1, &PyNotImplemented_Type
};

///////////////////////////////////////////////////////////////////////////////
// 所有内置数据类型的 ready 初始化
///////////////////////////////////////////////////////////////////////////////

void
_Py_ReadyTypes(void)
{
    if (PyType_Ready(&PyType_Type) < 0)
        Py_FatalError("Can't initialize type type");

    if (PyType_Ready(&_PyWeakref_RefType) < 0)
        Py_FatalError("Can't initialize weakref type");

    if (PyType_Ready(&_PyWeakref_CallableProxyType) < 0)
        Py_FatalError("Can't initialize callable weakref proxy type");

    if (PyType_Ready(&_PyWeakref_ProxyType) < 0)
        Py_FatalError("Can't initialize weakref proxy type");

    if (PyType_Ready(&PyBool_Type) < 0)
        Py_FatalError("Can't initialize bool type");

    if (PyType_Ready(&PyString_Type) < 0)
        Py_FatalError("Can't initialize str type");

    if (PyType_Ready(&PyByteArray_Type) < 0)
        Py_FatalError("Can't initialize bytearray type");

    if (PyType_Ready(&PyList_Type) < 0)
        Py_FatalError("Can't initialize list type");

    if (PyType_Ready(&PyNone_Type) < 0)
        Py_FatalError("Can't initialize None type");

    if (PyType_Ready(&PyNotImplemented_Type) < 0)
        Py_FatalError("Can't initialize NotImplemented type");

    if (PyType_Ready(&PyTraceBack_Type) < 0)
        Py_FatalError("Can't initialize traceback type");

    if (PyType_Ready(&PySuper_Type) < 0)
        Py_FatalError("Can't initialize super type");

    if (PyType_Ready(&PyBaseObject_Type) < 0)
        Py_FatalError("Can't initialize object type");

    if (PyType_Ready(&PyRange_Type) < 0)
        Py_FatalError("Can't initialize xrange type");

    if (PyType_Ready(&PyDict_Type) < 0)
        Py_FatalError("Can't initialize dict type");

    if (PyType_Ready(&PySet_Type) < 0)
        Py_FatalError("Can't initialize set type");

    if (PyType_Ready(&PyUnicode_Type) < 0)
        Py_FatalError("Can't initialize unicode type");

    if (PyType_Ready(&PySlice_Type) < 0)
        Py_FatalError("Can't initialize slice type");

    if (PyType_Ready(&PyStaticMethod_Type) < 0)
        Py_FatalError("Can't initialize static method type");

#ifndef WITHOUT_COMPLEX
    if (PyType_Ready(&PyComplex_Type) < 0)
        Py_FatalError("Can't initialize complex type");
#endif

    if (PyType_Ready(&PyFloat_Type) < 0)
        Py_FatalError("Can't initialize float type");

    if (PyType_Ready(&PyBuffer_Type) < 0)
        Py_FatalError("Can't initialize buffer type");

    if (PyType_Ready(&PyLong_Type) < 0)
        Py_FatalError("Can't initialize long type");

    if (PyType_Ready(&PyInt_Type) < 0)
        Py_FatalError("Can't initialize int type");

    if (PyType_Ready(&PyFrozenSet_Type) < 0)
        Py_FatalError("Can't initialize frozenset type");

    if (PyType_Ready(&PyProperty_Type) < 0)
        Py_FatalError("Can't initialize property type");

    if (PyType_Ready(&PyMemoryView_Type) < 0)
        Py_FatalError("Can't initialize memoryview type");

    if (PyType_Ready(&PyTuple_Type) < 0)
        Py_FatalError("Can't initialize tuple type");

    if (PyType_Ready(&PyEnum_Type) < 0)
        Py_FatalError("Can't initialize enumerate type");

    if (PyType_Ready(&PyReversed_Type) < 0)
        Py_FatalError("Can't initialize reversed type");

    if (PyType_Ready(&PyCode_Type) < 0)
        Py_FatalError("Can't initialize code type");

    if (PyType_Ready(&PyFrame_Type) < 0)
        Py_FatalError("Can't initialize frame type");

    if (PyType_Ready(&PyCFunction_Type) < 0)
        Py_FatalError("Can't initialize builtin function type");

    if (PyType_Ready(&PyMethod_Type) < 0)
        Py_FatalError("Can't initialize method type");

    if (PyType_Ready(&PyFunction_Type) < 0)
        Py_FatalError("Can't initialize function type");

    if (PyType_Ready(&PyClass_Type) < 0)
        Py_FatalError("Can't initialize class type");

    if (PyType_Ready(&PyDictProxy_Type) < 0)
        Py_FatalError("Can't initialize dict proxy type");

    if (PyType_Ready(&PyGen_Type) < 0)
        Py_FatalError("Can't initialize generator type");

    if (PyType_Ready(&PyGetSetDescr_Type) < 0)
        Py_FatalError("Can't initialize get-set descriptor type");

    if (PyType_Ready(&PyWrapperDescr_Type) < 0)
        Py_FatalError("Can't initialize wrapper type");

    if (PyType_Ready(&PyInstance_Type) < 0)
        Py_FatalError("Can't initialize instance type");

    if (PyType_Ready(&PyEllipsis_Type) < 0)
        Py_FatalError("Can't initialize ellipsis type");

    if (PyType_Ready(&PyMemberDescr_Type) < 0)
        Py_FatalError("Can't initialize member descriptor type");

    if (PyType_Ready(&PyFile_Type) < 0)
        Py_FatalError("Can't initialize file type");
}

///////////////////////////////////////////////////////////////////////////////
// 其他系统内部相关处理
///////////////////////////////////////////////////////////////////////////////

/// 和内部调试统计和输出相关的处理

#ifdef Py_TRACE_REFS

void
_Py_NewReference(PyObject *op)
{
    _Py_INC_REFTOTAL;
    op->ob_refcnt = 1;
    _Py_AddToAllObjects(op, 1);
    _Py_INC_TPALLOCS(op);
}

void
_Py_ForgetReference(register PyObject *op)
{
#ifdef SLOW_UNREF_CHECK
    register PyObject *p;
#endif
    if (op->ob_refcnt < 0)
        Py_FatalError("UNREF negative refcnt");
    if (op == &refchain ||
        op->_ob_prev->_ob_next != op || op->_ob_next->_ob_prev != op)
        Py_FatalError("UNREF invalid object");
#ifdef SLOW_UNREF_CHECK
    for (p = refchain._ob_next; p != &refchain; p = p->_ob_next) {
        if (p == op)
            break;
    }
    if (p == &refchain) /* Not found */
        Py_FatalError("UNREF unknown object");
#endif
    op->_ob_next->_ob_prev = op->_ob_prev;
    op->_ob_prev->_ob_next = op->_ob_next;
    op->_ob_next = op->_ob_prev = NULL;
    _Py_INC_TPFREES(op);
}

void
_Py_Dealloc(PyObject *op)
{
    destructor dealloc = Py_TYPE(op)->tp_dealloc;
    _Py_ForgetReference(op);
    (*dealloc)(op);
}

/* Print all live objects.  Because PyObject_Print is called, the
 * interpreter must be in a healthy state.
 */
void
_Py_PrintReferences(FILE *fp)
{
    PyObject *op;
    fprintf(fp, "Remaining objects:\n");
    for (op = refchain._ob_next; op != &refchain; op = op->_ob_next) {
        fprintf(fp, "%p [%" PY_FORMAT_SIZE_T "d] ", op, op->ob_refcnt);
        if (PyObject_Print(op, fp, 0) != 0)
            PyErr_Clear();
        putc('\n', fp);
    }
}

/* Print the addresses of all live objects.  Unlike _Py_PrintReferences, this
 * doesn't make any calls to the Python C API, so is always safe to call.
 */
void
_Py_PrintReferenceAddresses(FILE *fp)
{
    PyObject *op;
    fprintf(fp, "Remaining object addresses:\n");
    for (op = refchain._ob_next; op != &refchain; op = op->_ob_next)
        fprintf(fp, "%p [%" PY_FORMAT_SIZE_T "d] %s\n", op,
            op->ob_refcnt, Py_TYPE(op)->tp_name);
}

PyObject *
_Py_GetObjects(PyObject *self, PyObject *args)
{
    int i, n;
    PyObject *t = NULL;
    PyObject *res, *op;

    if (!PyArg_ParseTuple(args, "i|O", &n, &t))
        return NULL;
    op = refchain._ob_next;
    res = PyList_New(0);
    if (res == NULL)
        return NULL;
    for (i = 0; (n == 0 || i < n) && op != &refchain; i++) {
        while (op == self || op == args || op == res || op == t ||
               (t != NULL && Py_TYPE(op) != (PyTypeObject *) t)) {
            op = op->_ob_next;
            if (op == &refchain)
                return res;
        }
        if (PyList_Append(res, op) < 0) {
            Py_DECREF(res);
            return NULL;
        }
        op = op->_ob_next;
    }
    return res;
}

#endif

/// 一些内部 hack 处理

/* Hack to force loading of capsule.o */
PyTypeObject *_Py_capsule_hack = &PyCapsule_Type;


/* Hack to force loading of cobject.o */
PyTypeObject *_Py_cobject_hack = &PyCObject_Type;


/* Hack to force loading of abstract.o */
Py_ssize_t (*_Py_abstract_hack)(PyObject *) = PyObject_Size;


/// 对外内存管理接口的实现

/* Python's malloc wrappers (see pymem.h) */

void *
PyMem_Malloc(size_t nbytes)
{
    return PyMem_MALLOC(nbytes);
}

void *
PyMem_Realloc(void *p, size_t nbytes)
{
    return PyMem_REALLOC(p, nbytes);
}

void
PyMem_Free(void *p)
{
    PyMem_FREE(p);
}

/// 和 repr session（会话）相关的处理

/* These methods are used to control infinite recursion in repr, str, print,
   etc.  Container objects that may recursively contain themselves,
   e.g. builtin dictionaries and lists, should used Py_ReprEnter() and
   Py_ReprLeave() to avoid infinite recursion.

   Py_ReprEnter() returns 0 the first time it is called for a particular
   object and 1 every time thereafter.  It returns -1 if an exception
   occurred.  Py_ReprLeave() has no return value.

   See dictobject.c and listobject.c for examples of use.
*/

#define KEY "Py_Repr"

int
Py_ReprEnter(PyObject *obj)
{
    PyObject *dict;
    PyObject *list;
    Py_ssize_t i;

    dict = PyThreadState_GetDict();
    if (dict == NULL)
        return 0;
    list = PyDict_GetItemString(dict, KEY);
    if (list == NULL) {
        list = PyList_New(0);
        if (list == NULL)
            return -1;
        if (PyDict_SetItemString(dict, KEY, list) < 0)
            return -1;
        Py_DECREF(list);
    }
    i = PyList_GET_SIZE(list);
    while (--i >= 0) {
        if (PyList_GET_ITEM(list, i) == obj)
            return 1;
    }
    PyList_Append(list, obj);
    return 0;
}

void
Py_ReprLeave(PyObject *obj)
{
    PyObject *dict;
    PyObject *list;
    Py_ssize_t i;

    dict = PyThreadState_GetDict();
    if (dict == NULL)
        return;
    list = PyDict_GetItemString(dict, KEY);
    if (list == NULL || !PyList_Check(list))
        return;
    i = PyList_GET_SIZE(list);
    /* Count backwards because we always expect obj to be list[-1] */
    while (--i >= 0) {
        if (PyList_GET_ITEM(list, i) == obj) {
            PyList_SetSlice(list, i, i + 1, NULL);
            break;
        }
    }
}

/// 和 Trash 相关的处理

/* Trashcan support. */

/* Current call-stack depth of tp_dealloc calls. */
int _PyTrash_delete_nesting = 0;

/* List of objects that still need to be cleaned up, singly linked via their
 * gc headers' gc_prev pointers.
 */
PyObject *_PyTrash_delete_later = NULL;

/* Add op to the _PyTrash_delete_later list.  Called when the current
 * call-stack depth gets large.  op must be a currently untracked gc'ed
 * object, with refcount 0.  Py_DECREF must already have been called on it.
 */
void
_PyTrash_deposit_object(PyObject *op)
{
    assert(PyObject_IS_GC(op));
    assert(_Py_AS_GC(op)->gc.gc_refs == _PyGC_REFS_UNTRACKED);
    assert(op->ob_refcnt == 0);
    _Py_AS_GC(op)->gc.gc_prev = (PyGC_Head *)_PyTrash_delete_later;
    _PyTrash_delete_later = op;
}

/* Dealloccate all the objects in the _PyTrash_delete_later list.  Called when
 * the call-stack unwinds again.
 */
void
_PyTrash_destroy_chain(void)
{
    while (_PyTrash_delete_later) {
        PyObject *op = _PyTrash_delete_later;
        destructor dealloc = Py_TYPE(op)->tp_dealloc;

        _PyTrash_delete_later =
            (PyObject*) _Py_AS_GC(op)->gc.gc_prev;

        /* Call the deallocator directly.  This used to try to
         * fool Py_DECREF into calling it indirectly, but
         * Py_DECREF was already called on this object, and in
         * assorted non-release builds calling Py_DECREF again ends
         * up distorting allocation statistics.
         */
        assert(op->ob_refcnt == 0);
        ++_PyTrash_delete_nesting;
        (*dealloc)(op);
        --_PyTrash_delete_nesting;
    }
}

#ifdef __cplusplus
}
#endif
