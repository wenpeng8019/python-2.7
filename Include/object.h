#ifndef Py_OBJECT_H
#define Py_OBJECT_H
#ifdef __cplusplus
extern "C" {
#endif


/* Object and type object interface */

/*
Objects are structures allocated on the heap.  Special rules apply to
the use of objects to ensure they are properly garbage-collected.
Objects are never allocated statically or on the stack; they must be
accessed through special macros and functions only.  (Type objects are
exceptions to the first rule; the standard types are represented by
statically initialized type objects, although work on type/class unification
for Python 2.2 made it possible to have heap-allocated type objects too).

An object has a 'reference count' that is increased or decreased when a
pointer to the object is copied or deleted; when the reference count
reaches zero there are no references to the object left and it can be
removed from the heap.

An object has a 'type' that determines what it represents and what kind
of data it contains.  An object's type is fixed when it is created.
Types themselves are represented as objects; an object contains a
pointer to the corresponding type object.  The type itself has a type
pointer pointing to the object representing the type 'type', which
contains a pointer to itself!).

Objects do not float around in memory; once allocated an object keeps
the same size and address.  Objects that must hold variable-size data
can contain pointers to variable-size parts of the object.  Not all
objects of the same type have the same size; but the size cannot change
after allocation.  (These restrictions are made so a reference to an
object can be simply a pointer -- moving an object would require
updating all the pointers, and changing an object's size would require
moving it if there was another object right next to it.)

Objects are always accessed through pointers of the type 'PyObject *'.
The type 'PyObject' is a structure that only contains the reference count
and the type pointer.  The actual memory allocated for an object
contains other data that can only be accessed after casting the pointer
to a pointer to a longer structure type.  This longer type must start
with the reference count and type fields; the macro PyObject_HEAD should be
used for this (to accommodate for future changes).  The implementation
of a particular object type can cast the object pointer to the proper
type and back.

A standard interface exists for objects that contain an array of items
whose size is determined when the object is allocated.
*/

/* Py_DEBUG implies Py_TRACE_REFS. */
#if defined(Py_DEBUG) && !defined(Py_TRACE_REFS)
#define Py_TRACE_REFS
#endif

/* Py_TRACE_REFS implies Py_REF_DEBUG. */
#if defined(Py_TRACE_REFS) && !defined(Py_REF_DEBUG)
#define Py_REF_DEBUG
#endif

#ifdef Py_TRACE_REFS
/* Define pointers to support a doubly-linked list of all live heap objects. */
#define _PyObject_HEAD_EXTRA            \
    struct _object *_ob_next;           \
    struct _object *_ob_prev;

#define _PyObject_EXTRA_INIT 0, 0,

#else
#define _PyObject_HEAD_EXTRA
#define _PyObject_EXTRA_INIT
#endif

/* PyObject_HEAD defines the initial segment of every PyObject. */
#define PyObject_HEAD                   \
    _PyObject_HEAD_EXTRA                \
    Py_ssize_t ob_refcnt;               \
    struct _typeobject *ob_type;

#define PyObject_HEAD_INIT(type)        \
    _PyObject_EXTRA_INIT                \
    1, type,

#define PyVarObject_HEAD_INIT(type, size)       \
    PyObject_HEAD_INIT(type) size,

/* PyObject_VAR_HEAD defines the initial segment of all variable-size
 * container objects.  These end with a declaration of an array with 1
 * element, but enough space is malloc'ed so that the array actually
 * has room for ob_size elements.  Note that ob_size is an element count,
 * not necessarily a byte count.
 */
#define PyObject_VAR_HEAD               \
    PyObject_HEAD                       \
    Py_ssize_t ob_size; /* Number of items in variable part */
#define Py_INVALID_SIZE (Py_ssize_t)-1

/* Nothing is actually declared to be a PyObject, but every pointer to
 * a Python object can be cast to a PyObject*.  This is inheritance built
 * by hand.  Similarly every pointer to a variable-size Python object can,
 * in addition, be cast to PyVarObject*.
 */
typedef struct _object {
    PyObject_HEAD
} PyObject;

typedef struct {
    PyObject_VAR_HEAD
} PyVarObject;

#define Py_REFCNT(ob)           (((PyObject*)(ob))->ob_refcnt)
#define Py_TYPE(ob)             (((PyObject*)(ob))->ob_type)
#define Py_SIZE(ob)             (((PyVarObject*)(ob))->ob_size)

/*
Type objects contain a string containing the type name (to help somewhat
in debugging), the allocation parameters (see PyObject_New() and
PyObject_NewVar()),
and methods for accessing objects of the type.  Methods are optional, a
nil pointer meaning that particular kind of access is not available for
this type.  The Py_DECREF() macro uses the tp_dealloc method without
checking for a nil pointer; it should always be implemented except if
the implementation can guarantee that the reference count will never
reach zero (e.g., for statically allocated type objects).

NB: the methods for certain type groups are now contained in separate
method blocks.
*/

typedef PyObject * (*unaryfunc)(PyObject *);
typedef PyObject * (*binaryfunc)(PyObject *, PyObject *);
typedef PyObject * (*ternaryfunc)(PyObject *, PyObject *, PyObject *);
typedef int (*inquiry)(PyObject *);
typedef Py_ssize_t (*lenfunc)(PyObject *);
typedef int (*coercion)(PyObject **, PyObject **);
typedef PyObject *(*intargfunc)(PyObject *, int) Py_DEPRECATED(2.5);
typedef PyObject *(*intintargfunc)(PyObject *, int, int) Py_DEPRECATED(2.5);
typedef PyObject *(*ssizeargfunc)(PyObject *, Py_ssize_t);
typedef PyObject *(*ssizessizeargfunc)(PyObject *, Py_ssize_t, Py_ssize_t);
typedef int(*intobjargproc)(PyObject *, int, PyObject *);
typedef int(*intintobjargproc)(PyObject *, int, int, PyObject *);
typedef int(*ssizeobjargproc)(PyObject *, Py_ssize_t, PyObject *);
typedef int(*ssizessizeobjargproc)(PyObject *, Py_ssize_t, Py_ssize_t, PyObject *);
typedef int(*objobjargproc)(PyObject *, PyObject *, PyObject *);



/* int-based buffer interface */
typedef int (*getreadbufferproc)(PyObject *, int, void **);
typedef int (*getwritebufferproc)(PyObject *, int, void **);
typedef int (*getsegcountproc)(PyObject *, int *);
typedef int (*getcharbufferproc)(PyObject *, int, char **);
/* ssize_t-based buffer interface */
typedef Py_ssize_t (*readbufferproc)(PyObject *, Py_ssize_t, void **);
typedef Py_ssize_t (*writebufferproc)(PyObject *, Py_ssize_t, void **);
typedef Py_ssize_t (*segcountproc)(PyObject *, Py_ssize_t *);
typedef Py_ssize_t (*charbufferproc)(PyObject *, Py_ssize_t, char **);


/* Py3k buffer interface */
typedef struct bufferinfo {
    void *buf;
    PyObject *obj;        /* owned reference */
    Py_ssize_t len;
    Py_ssize_t itemsize;  /* This is Py_ssize_t so it can be
                             pointed to by strides in simple case.*/
    int readonly;
    int ndim;
    char *format;
    Py_ssize_t *shape;
    Py_ssize_t *strides;
    Py_ssize_t *suboffsets;
    Py_ssize_t smalltable[2];  /* static store for shape and strides of
                                  mono-dimensional buffers. */
    void *internal;
} Py_buffer;

typedef int (*getbufferproc)(PyObject *, Py_buffer *, int);
typedef void (*releasebufferproc)(PyObject *, Py_buffer *);

    /* Flags for getting buffers */
#define PyBUF_SIMPLE 0
#define PyBUF_WRITABLE 0x0001
/*  we used to include an E, backwards compatible alias  */
#define PyBUF_WRITEABLE PyBUF_WRITABLE
#define PyBUF_FORMAT 0x0004
#define PyBUF_ND 0x0008
#define PyBUF_STRIDES (0x0010 | PyBUF_ND)
#define PyBUF_C_CONTIGUOUS (0x0020 | PyBUF_STRIDES)
#define PyBUF_F_CONTIGUOUS (0x0040 | PyBUF_STRIDES)
#define PyBUF_ANY_CONTIGUOUS (0x0080 | PyBUF_STRIDES)
#define PyBUF_INDIRECT (0x0100 | PyBUF_STRIDES)

#define PyBUF_CONTIG (PyBUF_ND | PyBUF_WRITABLE)
#define PyBUF_CONTIG_RO (PyBUF_ND)

#define PyBUF_STRIDED (PyBUF_STRIDES | PyBUF_WRITABLE)
#define PyBUF_STRIDED_RO (PyBUF_STRIDES)

#define PyBUF_RECORDS (PyBUF_STRIDES | PyBUF_WRITABLE | PyBUF_FORMAT)
#define PyBUF_RECORDS_RO (PyBUF_STRIDES | PyBUF_FORMAT)

#define PyBUF_FULL (PyBUF_INDIRECT | PyBUF_WRITABLE | PyBUF_FORMAT)
#define PyBUF_FULL_RO (PyBUF_INDIRECT | PyBUF_FORMAT)


#define PyBUF_READ  0x100
#define PyBUF_WRITE 0x200
#define PyBUF_SHADOW 0x400
/* end Py3k buffer interface */

typedef int (*objobjproc)(PyObject *, PyObject *);
typedef int (*visitproc)(PyObject *, void *);
typedef int (*traverseproc)(PyObject *, visitproc, void *);

typedef struct {
    /* For numbers without flag bit Py_TPFLAGS_CHECKTYPES set, all
       arguments are guaranteed to be of the object's type (modulo
       coercion hacks -- i.e. if the type's coercion function
       returns other types, then these are allowed as well).  Numbers that
       have the Py_TPFLAGS_CHECKTYPES flag bit set should check *both*
       arguments for proper type and implement the necessary conversions
       in the slot functions themselves. */

    binaryfunc nb_add;
    binaryfunc nb_subtract;
    binaryfunc nb_multiply;
    binaryfunc nb_divide;
    binaryfunc nb_remainder;
    binaryfunc nb_divmod;
    ternaryfunc nb_power;
    unaryfunc nb_negative;
    unaryfunc nb_positive;
    unaryfunc nb_absolute;
    inquiry nb_nonzero;
    unaryfunc nb_invert;
    binaryfunc nb_lshift;
    binaryfunc nb_rshift;
    binaryfunc nb_and;
    binaryfunc nb_xor;
    binaryfunc nb_or;
    coercion nb_coerce;
    unaryfunc nb_int;
    unaryfunc nb_long;
    unaryfunc nb_float;
    unaryfunc nb_oct;
    unaryfunc nb_hex;
    /* Added in release 2.0 */
    binaryfunc nb_inplace_add;
    binaryfunc nb_inplace_subtract;
    binaryfunc nb_inplace_multiply;
    binaryfunc nb_inplace_divide;
    binaryfunc nb_inplace_remainder;
    ternaryfunc nb_inplace_power;
    binaryfunc nb_inplace_lshift;
    binaryfunc nb_inplace_rshift;
    binaryfunc nb_inplace_and;
    binaryfunc nb_inplace_xor;
    binaryfunc nb_inplace_or;

    /* Added in release 2.2 */
    /* The following require the Py_TPFLAGS_HAVE_CLASS flag */
    binaryfunc nb_floor_divide;
    binaryfunc nb_true_divide;
    binaryfunc nb_inplace_floor_divide;
    binaryfunc nb_inplace_true_divide;

    /* Added in release 2.5 */
    unaryfunc nb_index;
} PyNumberMethods;

typedef struct {
    lenfunc sq_length;
    binaryfunc sq_concat;
    ssizeargfunc sq_repeat;
    ssizeargfunc sq_item;
    ssizessizeargfunc sq_slice;
    ssizeobjargproc sq_ass_item;
    ssizessizeobjargproc sq_ass_slice;
    objobjproc sq_contains;
    /* Added in release 2.0 */
    binaryfunc sq_inplace_concat;
    ssizeargfunc sq_inplace_repeat;
} PySequenceMethods;

typedef struct {
    lenfunc mp_length;
    binaryfunc mp_subscript;
    objobjargproc mp_ass_subscript;
} PyMappingMethods;

typedef struct {
    readbufferproc bf_getreadbuffer;
    writebufferproc bf_getwritebuffer;
    segcountproc bf_getsegcount;
    charbufferproc bf_getcharbuffer;
    getbufferproc bf_getbuffer;
    releasebufferproc bf_releasebuffer;
} PyBufferProcs;

/* 数据类型（对象）的 slot（操作）接口原型定义
 */
typedef void (*freefunc)(void *);
typedef void (*destructor)(PyObject *);
typedef int (*printfunc)(PyObject *, FILE *, int);
typedef PyObject *(*getattrfunc)(PyObject *, char *);
typedef PyObject *(*getattrofunc)(PyObject *, PyObject *);
typedef int (*setattrfunc)(PyObject *, char *, PyObject *);
typedef int (*setattrofunc)(PyObject *, PyObject *, PyObject *);
typedef int (*cmpfunc)(PyObject *, PyObject *);
typedef PyObject *(*reprfunc)(PyObject *);
typedef long (*hashfunc)(PyObject *);
typedef PyObject *(*richcmpfunc) (PyObject *, PyObject *, int);
typedef PyObject *(*getiterfunc) (PyObject *);
typedef PyObject *(*iternextfunc) (PyObject *);
typedef PyObject *(*descrgetfunc) (PyObject *, PyObject *, PyObject *);
typedef int (*descrsetfunc) (PyObject *, PyObject *, PyObject *);
typedef int (*initproc)(PyObject *, PyObject *, PyObject *);
typedef PyObject *(*newfunc)(struct _typeobject *, PyObject *, PyObject *);
typedef PyObject *(*allocfunc)(struct _typeobject *, Py_ssize_t);

typedef struct _typeobject {
    PyObject_VAR_HEAD

    const char *tp_name; /* For printing, in format "<module>.<name>"（该数据类型的命名） */

    Py_ssize_t tp_basicsize, tp_itemsize; /* For allocation
                                           * - tp_basicsize
                                           *   该数据类型（对象）的实例（对象）的结构体定义的结构大小
                                           *   也就是由结构体直接定义的，实例（对象）的静态数据大小
                                           *   对应的，我们可以在创建该实例（对象）时，在静态数据大小的基础上，动态增加一个动态数据部分。
                                           *   而这个静态数据的大小，就是动态数据部分的地址偏移量
                                           * - tp_itemsize
                                           *   对于 PyVarObject 类型对象，它的内存占用，包括静态定义的结构体大小，和动态分配时指定的大小两个部分
                                           *   动态指定的大小为 PyVarObject.ob_size * PyTypeObject.tp_itemsize
                                           *   这里的 tp_itemsize 就是用来定义每个 item 的字节大小。
                                           */

    /* Methods to implement standard operations（数据类型的标准操作接口） */

    destructor          tp_dealloc;
    printfunc           tp_print;
    getattrfunc         tp_getattr;
    setattrfunc         tp_setattr;
    cmpfunc             tp_compare;
    reprfunc            tp_repr;

    /* Method suites for standard classes（标准类的抽象数据类型接口） */

    PyNumberMethods     *tp_as_number;
    PySequenceMethods   *tp_as_sequence;
    PyMappingMethods    *tp_as_mapping;

    /* More standard operations (here for binary compatibility) */

    hashfunc            tp_hash;
    ternaryfunc         tp_call;                        /* 对该数据类型（对象）的实例（对象）的调用处理接口
                                                         * 此时说明该实例（对象）是可被调用的。
                                                         * - 例如，函数/方法类型（对象）的实例（对象），也就是一个函数/方法实例
                                                         *   此时，调用函数/方法实例，就是执行它
                                                         * - 再如，数据类型（对象）`type` 的实例（对象），也就是一个数据类型（对象）自身
                                                         *   ! 数据类型（对象）`type` 是个特殊的数据类型（对象）
                                                         *     逻辑上它是数据类型（对象）的数据类型（对象），即是个类厂
                                                         *     对应的它的实例（对象），也是数据类型（对象）
                                                         *   + 数据类型（对象）自身的调用，可用来创建该数据类型（对象）的实例对象
                                                         *     如：int()、float()、str()、...
                                                         * - 还有，数据类型（对象）`class` 的实例（对象），也就是一个自定义类（对象）
                                                         *   和数据类型（对象）一样，对类（对象）的调用，也被用来创建自定义类（对象）的实例（对象）
                                                         * - ...
                                                         * + 该接口方法对于该数据类型（对象）说，相当于一个 {类的成员函数}
                                                         *   也就是说，它处理的目标是该数据类型（对象）的实例（对象）
                                                         * + 关于该接口的参数定义
                                                         *   - inst-obj(this): PyObject
                                                         *     + 被调用的（该数据类型的）实例（对象），相当于 {类的成员函数} 的 this 指针
                                                         *   - arg: PyObject
                                                         *     通用 args 入参
                                                         *   - kw: PyObject
                                                         *     通用 kw 类型入参
                                                         * + 调用该接口的主要场景
                                                         *   - @ PyObject_Call() $ [abstract.c]
                                                         */
    reprfunc            tp_str;
    getattrofunc        tp_getattro;                    
    setattrofunc        tp_setattro;                    /* tp_getattro/tp_setattro 应该和 tp_getattr/tp_setattr 功能等价
                                                         * 区别在于参数 attr_name 的数据类型，
                                                         * - tp_xxxattr 的数据类型为 const char*
                                                         * - 而 tp_xxxattro 的数据类型为 PyObject
                                                         * 这两个接口会自动转换 name 的数据类型，然后互相调用。
                                                         */

    /* Functions to access object as input/output buffer */
    PyBufferProcs       *tp_as_buffer;

    /* Flags to define presence of optional/expanded features */
    long                tp_flags;

    const char          *tp_doc; /* Documentation string（该数据类型的文档说明） */

    /* Assigned meaning in release 2.0 */
    /* call function for all accessible objects */
    traverseproc        tp_traverse;

    /* delete references to contained objects */
    inquiry             tp_clear;

    /* Assigned meaning in release 2.1 */
    /* rich comparisons */
    richcmpfunc         tp_richcompare;

    /* weak reference enabler */
    Py_ssize_t          tp_weaklistoffset;

    /* Added in release 2.2 */
    /* Iterators */
    getiterfunc         tp_iter;
    iternextfunc        tp_iternext;

    /* Attribute descriptor and subclassing stuff */
    struct PyMethodDef *tp_methods;                 // 该类型（对象）的方法定义
    struct PyMemberDef *tp_members;                 // 该类型（对象）的成员定义
    struct PyGetSetDef *tp_getset;                  // 该类型（对象）的 getter/setter 属性访问接口
    struct _typeobject *tp_base;                    /* 该类型（对象）的（直接）基类型
                                                     * 注意，这是类型（对象）的基类，不是（type 类型为 class 的）自定义类（对象）的基类
                                                     * - 首先，type 类型为 class 的自定义类（对象），并不是一个类型（对象），只有 type 类型为 type 的对象，才是一个类型（对象）
                                                     *   实际上，自定义类（对象）在本质上是一个类型（对象）的实例对象，而这个类型（对象），则是内置的 class 类型（对象）
                                                     * - 此外，对于自定义类（对象）来说，其对象的结构体定义为 PyClassObject，而非当前的 PyTypeObject
                                                     *   所有它根本就没有 tp_base 成员的概念
                                                     */
    PyObject        *tp_dict;                       /* 该类型（对象）的数据 dict 对象 */

    descrgetfunc    tp_descr_get;                   /* 访问该数据类型（对象）的实例（对象）的值
                                                     * 此时，该实例（对象）往往就代表一个值。例如：一个数值型实例（对象）
                                                     * 而这里在访问这个实例（对象）时，往往就是访问这个值。
                                                     * ! 和 tp_getattr 接口的区分在于
                                                     *   tp_getattr 访问的是这个实例（对象）的某个属性，而非它的值
                                                     */
    descrsetfunc    tp_descr_set;
    Py_ssize_t      tp_dictoffset;                  /* 该数据类型（对象）的实例（对象）是否存在数据 dict（对象）
                                                     * - 如果存在，则该属性的值表示的是，实例（对象）的数据 dict（对象）在实例（对象）的结构体定义中的偏移地址
                                                     * - 如果该值为 0，则说明实例（对象）没有定义数据 dict（对象）
                                                     * + 数据 dict（对象）的核心作用是给（实例）对象提供动态属性机制的，
                                                     *   该数据 dict（对象）会被 tp_getattro/tp_setattro 接口的默认实现访问
                                                     * + 事实上，大多数数据类型的实例（对象）都没有定义数据 dict（对象），例如：int、float、str、...
                                                     *   而定义了数据 dict 的数据类型（对象）包括：
                                                     *   PyTypeObject、PyModule_Type、PyFunction_Type
                                                     *   PyBufferedXXX_Type、_PyExc_BaseException、PyIOBase_Type
                                                     * + 此外，该属性的值，或者说 "实例（对象）存在数据 dict（对象）" 的特性，是具有继承性的
                                                     *   也就是说，如果该数据类型（对象）的实例（对象）存在数据 dict（对象），那么它的派生类型（的实例）也具有数据 dict 特性
                                                     */
    initproc        tp_init;                        // 该数据类型（对象）的实例（对象）构造函数
    allocfunc       tp_alloc;                       /* 该数据类型（对象）分配一个新的实例（对象）
                                                     * 该接口是个类的静态方法，也就是它处理的对象，是该数据类型（对象）自身，而非它的实例（对象）
                                                     * 该接口的主要触发场景，是被该数据类型（对象）的 tp_new 接口的实现调用。
                                                     */ 
    newfunc         tp_new;                         /* 该数据类型（对象）创建新实例（对象）
                                                     * 该接口是个类的静态方法，也就是它处理的对象，是该数据类型（对象）自身，而非它的实例（对象）
                                                     * 该接口的执行入口有三个
                                                     * - 由数据类型（对象）的自调用接口 tp_call 触发
                                                     * - 由数据类型（对象）的 __new__ 操作符触发
                                                     * - type(.., .., ..) 在创建动态数据类型（对象）时，会触发 matetype 的 tp_new 接口
                                                     */
    freefunc        tp_free;                        /* Low-level free-memory routine */
    inquiry         tp_is_gc;                       /* For PyObject_IS_GC（该类型对象是否是动态创建的。内置类型对象、None、True、...等对象都是静态全局对象） */

    PyObject        *tp_bases;                      /* 该类型（对象）的基类集合（允许多基类继承，集合中的基类没有顺序的概念） */
    PyObject        *tp_mro;                        /* method resolution order (类方法解析顺序。也就是将 tp_bases 按类的继承关系进行排序的结果) */
    PyObject        *tp_cache;
    PyObject        *tp_subclasses;                 /* 该类型（对象）下的有所（sub）派生类的集合 */

    PyObject        *tp_weaklist;
    destructor      tp_del;

    /* Type attribute cache version tag. Added in version 2.6 */
    unsigned int tp_version_tag;

#ifdef COUNT_ALLOCS
    /* these must be last and never explicitly initialized */
    Py_ssize_t tp_allocs;
    Py_ssize_t tp_frees;
    Py_ssize_t tp_maxalloc;
    struct _typeobject *tp_prev;
    struct _typeobject *tp_next;
#endif
} PyTypeObject;


/* The *real* layout of a type object when allocated on the heap */
typedef struct _heaptypeobject {
    /* Note: there's a dependency on the order of these members
       in slotptr() in typeobject.c . */
    PyTypeObject ht_type;
    PyNumberMethods as_number;
    PyMappingMethods as_mapping;
    PySequenceMethods as_sequence; /* as_sequence comes after as_mapping,
                                      so that the mapping wins when both
                                      the mapping and the sequence define
                                      a given operator (e.g. __getitem__).
                                      see add_operators() in typeobject.c . */
    PyBufferProcs as_buffer;
    PyObject *ht_name, *ht_slots;
    /* here are optional user slots, followed by the members. */
} PyHeapTypeObject;

/* access macro to the members which are floating "behind" the object */
#define PyHeapType_GET_MEMBERS(etype) \
    ((PyMemberDef *)(((char *)etype) + Py_TYPE(etype)->tp_basicsize))


/* Generic type check */
PyAPI_FUNC(int) PyType_IsSubtype(PyTypeObject *, PyTypeObject *);
#define PyObject_TypeCheck(ob, tp) \
    (Py_TYPE(ob) == (tp) || PyType_IsSubtype(Py_TYPE(ob), (tp)))

PyAPI_DATA(PyTypeObject) PyType_Type; /* built-in 'type' */
PyAPI_DATA(PyTypeObject) PyBaseObject_Type; /* built-in 'object' */
PyAPI_DATA(PyTypeObject) PySuper_Type; /* built-in 'super' */

// 判定某个对象的 type 类型，是否是 type 类型、或其派生类型
#define PyType_Check(op) \
    PyType_FastSubclass(Py_TYPE(op), Py_TPFLAGS_TYPE_SUBCLASS)
// 判定某个对象的 type 类型，是否是 type 类型
#define PyType_CheckExact(op) (Py_TYPE(op) == &PyType_Type)

PyAPI_FUNC(int) PyType_Ready(PyTypeObject *);
PyAPI_FUNC(PyObject *) PyType_GenericAlloc(PyTypeObject *, Py_ssize_t);
PyAPI_FUNC(PyObject *) PyType_GenericNew(PyTypeObject *,
                                               PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) _PyType_Lookup(PyTypeObject *, PyObject *);
PyAPI_FUNC(PyObject *) _PyObject_LookupSpecial(PyObject *, char *, PyObject **);
PyAPI_FUNC(unsigned int) PyType_ClearCache(void);
PyAPI_FUNC(void) PyType_Modified(PyTypeObject *);

/* Generic operations on objects */
PyAPI_FUNC(int) PyObject_Print(PyObject *, FILE *, int);
PyAPI_FUNC(void) _PyObject_Dump(PyObject *);
PyAPI_FUNC(PyObject *) PyObject_Repr(PyObject *);
PyAPI_FUNC(PyObject *) _PyObject_Str(PyObject *);
PyAPI_FUNC(PyObject *) PyObject_Str(PyObject *);
#define PyObject_Bytes PyObject_Str
#ifdef Py_USING_UNICODE
PyAPI_FUNC(PyObject *) PyObject_Unicode(PyObject *);
#endif
PyAPI_FUNC(int) PyObject_Compare(PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) PyObject_RichCompare(PyObject *, PyObject *, int);
PyAPI_FUNC(int) PyObject_RichCompareBool(PyObject *, PyObject *, int);
PyAPI_FUNC(PyObject *) PyObject_GetAttrString(PyObject *, const char *);
PyAPI_FUNC(int) PyObject_SetAttrString(PyObject *, const char *, PyObject *);
PyAPI_FUNC(int) PyObject_HasAttrString(PyObject *, const char *);
PyAPI_FUNC(PyObject *) PyObject_GetAttr(PyObject *, PyObject *);
PyAPI_FUNC(int) PyObject_SetAttr(PyObject *, PyObject *, PyObject *);
PyAPI_FUNC(int) PyObject_HasAttr(PyObject *, PyObject *);
PyAPI_FUNC(PyObject **) _PyObject_GetDictPtr(PyObject *);
PyAPI_FUNC(PyObject *) PyObject_SelfIter(PyObject *);
PyAPI_FUNC(PyObject *) _PyObject_NextNotImplemented(PyObject *);
PyAPI_FUNC(PyObject *) PyObject_GenericGetAttr(PyObject *, PyObject *);
PyAPI_FUNC(int) PyObject_GenericSetAttr(PyObject *,
                                              PyObject *, PyObject *);
PyAPI_FUNC(long) PyObject_Hash(PyObject *);
PyAPI_FUNC(long) PyObject_HashNotImplemented(PyObject *);
PyAPI_FUNC(int) PyObject_IsTrue(PyObject *);
PyAPI_FUNC(int) PyObject_Not(PyObject *);
PyAPI_FUNC(int) PyCallable_Check(PyObject *);
PyAPI_FUNC(int) PyNumber_Coerce(PyObject **, PyObject **);
PyAPI_FUNC(int) PyNumber_CoerceEx(PyObject **, PyObject **);

PyAPI_FUNC(void) PyObject_ClearWeakRefs(PyObject *);

/* A slot function whose address we need to compare */
extern int _PyObject_SlotCompare(PyObject *, PyObject *);
/* Same as PyObject_Generic{Get,Set}Attr, but passing the attributes
   dict as the last parameter. */
PyAPI_FUNC(PyObject *)
_PyObject_GenericGetAttrWithDict(PyObject *, PyObject *, PyObject *);
PyAPI_FUNC(int)
_PyObject_GenericSetAttrWithDict(PyObject *, PyObject *,
                                 PyObject *, PyObject *);


/* PyObject_Dir(obj) acts like Python __builtin__.dir(obj), returning a
   list of strings.  PyObject_Dir(NULL) is like __builtin__.dir(),
   returning the names of the current locals.  In this case, if there are
   no current locals, NULL is returned, and PyErr_Occurred() is false.
*/
PyAPI_FUNC(PyObject *) PyObject_Dir(PyObject *);


/* Helpers for printing recursive container types */
PyAPI_FUNC(int) Py_ReprEnter(PyObject *);
PyAPI_FUNC(void) Py_ReprLeave(PyObject *);

/* Helpers for hash functions */
PyAPI_FUNC(long) _Py_HashDouble(double);
PyAPI_FUNC(long) _Py_HashPointer(void*);

typedef struct {
    long prefix;
    long suffix;
} _Py_HashSecret_t;
PyAPI_DATA(_Py_HashSecret_t) _Py_HashSecret;

#ifdef Py_DEBUG
PyAPI_DATA(int) _Py_HashSecret_Initialized;
#endif

/* Helper for passing objects to printf and the like */
#define PyObject_REPR(obj) PyString_AS_STRING(PyObject_Repr(obj))

/* Flag bits for printing: */
#define Py_PRINT_RAW    1       /* No string quotes etc. */

/*
`Type flags (tp_flags)

These flags are used to extend the type structure in a backwards-compatible
fashion. Extensions can use the flags to indicate (and test) when a given
type structure contains a new feature. The Python core will use these when
introducing new functionality between major revisions (to avoid mid-version
changes in the PYTHON_API_VERSION).

Arbitration of the flag bit positions will need to be coordinated among
all extension writers who publically release their extensions (this will
be fewer than you might expect!)..

Python 1.5.2 introduced the bf_getcharbuffer slot into PyBufferProcs.

Type definitions should use Py_TPFLAGS_DEFAULT for their tp_flags value.

Code can use PyType_HasFeature(type_ob, flag_value) to test whether the
given type object has a specified feature.

NOTE: when building the core, Py_TPFLAGS_DEFAULT includes
Py_TPFLAGS_HAVE_VERSION_TAG; outside the core, it doesn't.  This is so
that extensions that modify tp_dict of their own types directly don't
break, since this was allowed in 2.5.  In 3.0 they will have to
manually remove this flag though!

 ! 这里定义的特征，只针对于 type 类型（对象），也就是 PyTypeObject 对象的 tp_flags 成员项
   其他类型对象，不存在该特征。尤其需要注意和区分的是
   - 自定义类（对象），即 PyClassObject 对象。
     它的 type 类型为 class 类型（对象），也就是 PyClass_Type
   - 自定义类（对象）的实例（对象），即 PyInstanceObject 对象。
     它的 type 类型为 instance 类型（对象），也就是 PyInstance_Type
*/

/* PyBufferProcs contains bf_getcharbuffer */
#define Py_TPFLAGS_HAVE_GETCHARBUFFER  (1L<<0)

/* PySequenceMethods contains sq_contains */
#define Py_TPFLAGS_HAVE_SEQUENCE_IN (1L<<1)

/* This is here for backwards compatibility.  Extensions that use the old GC
 * API will still compile but the objects will not be tracked by the GC. */
#define Py_TPFLAGS_GC 0 /* used to be (1L<<2) */

/* PySequenceMethods and PyNumberMethods contain in-place operators */
#define Py_TPFLAGS_HAVE_INPLACEOPS (1L<<3)

/* PyNumberMethods do their own coercion */
#define Py_TPFLAGS_CHECKTYPES (1L<<4)

/* tp_richcompare is defined */
#define Py_TPFLAGS_HAVE_RICHCOMPARE (1L<<5)

/* Objects which are weakly referencable if their tp_weaklistoffset is >0 */
#define Py_TPFLAGS_HAVE_WEAKREFS (1L<<6)

/* tp_iter is defined */
#define Py_TPFLAGS_HAVE_ITER (1L<<7)

/* New members introduced by Python 2.2 exist */
// 该类型是否是一个具有 class 特性的类型
// + 也就是是否支持构造、析构、类属性、基类、继承等特性
//   事实上，基本上所有的类型都支持该特性，它被作为下面 Py_TPFLAGS_DEFAULT 的一个默认项
#define Py_TPFLAGS_HAVE_CLASS (1L<<8)

/* Set if the type object is dynamically allocated */
// 该类型是否是动态创建的类型（对象）
// + 目前，只有通过对 type 类型（对象）的自调用的方式，才能动态创建一个类型（对象）
#define Py_TPFLAGS_HEAPTYPE (1L<<9)

/* Set if the type allows subclassing */
// 该类型是否可以作为基类被派生。一般来说，
// - 用于具象的描述某个数据结构的内置类型（对象）都是可作为基类。例如：int、float、string、dict、...
// - 而用于抽象的描述某类组件、设备的内置类型（对象）是不可作为基类的。例如：PyClass_Type(class)、PyInstance_Type(instance)、...
// ! 由 type 类型（对象）的自调用的方式动态创建的类型（对象），默认是具有该特征的
#define Py_TPFLAGS_BASETYPE (1L<<10)

/* Set if the type is 'ready' -- fully initialized */
#define Py_TPFLAGS_READY (1L<<12)

/* Set while the type is being 'readied', to prevent recursive ready calls */
#define Py_TPFLAGS_READYING (1L<<13)

/* Objects support garbage collection (see objimp.h) */
#define Py_TPFLAGS_HAVE_GC (1L<<14)

/* These two bits are preserved for Stackless Python, next after this is 17 */
#ifdef STACKLESS
#define Py_TPFLAGS_HAVE_STACKLESS_EXTENSION (3L<<15)
#else
#define Py_TPFLAGS_HAVE_STACKLESS_EXTENSION 0
#endif

/* Objects support nb_index in PyNumberMethods */
#define Py_TPFLAGS_HAVE_INDEX (1L<<17)

/* Objects support type attribute cache */
#define Py_TPFLAGS_HAVE_VERSION_TAG   (1L<<18)
#define Py_TPFLAGS_VALID_VERSION_TAG  (1L<<19)

/* Type is abstract and cannot be instantiated */
#define Py_TPFLAGS_IS_ABSTRACT (1L<<20)

/* Has the new buffer protocol */
#define Py_TPFLAGS_HAVE_NEWBUFFER (1L<<21)

/* These flags are used to determine if a type is a subclass. */
/// 用于标记某个类型（对象）是否是（某个）系统内置类型的派生类型
#define Py_TPFLAGS_INT_SUBCLASS         (1L<<23)
#define Py_TPFLAGS_LONG_SUBCLASS        (1L<<24)
#define Py_TPFLAGS_LIST_SUBCLASS        (1L<<25)
#define Py_TPFLAGS_TUPLE_SUBCLASS       (1L<<26)
#define Py_TPFLAGS_STRING_SUBCLASS      (1L<<27)
#define Py_TPFLAGS_UNICODE_SUBCLASS     (1L<<28)
#define Py_TPFLAGS_DICT_SUBCLASS        (1L<<29)
// 是否是 BaseException 类型的派生类
#define Py_TPFLAGS_BASE_EXC_SUBCLASS    (1L<<30)
// 是否是 type 类型的派生类
#define Py_TPFLAGS_TYPE_SUBCLASS        (1L<<31)

#define Py_TPFLAGS_DEFAULT_EXTERNAL ( \
                 Py_TPFLAGS_HAVE_GETCHARBUFFER | \
                 Py_TPFLAGS_HAVE_SEQUENCE_IN | \
                 Py_TPFLAGS_HAVE_INPLACEOPS | \
                 Py_TPFLAGS_HAVE_RICHCOMPARE | \
                 Py_TPFLAGS_HAVE_WEAKREFS | \
                 Py_TPFLAGS_HAVE_ITER | \
                 Py_TPFLAGS_HAVE_CLASS | \
                 Py_TPFLAGS_HAVE_STACKLESS_EXTENSION | \
                 Py_TPFLAGS_HAVE_INDEX | \
                 0)
#define Py_TPFLAGS_DEFAULT_CORE (Py_TPFLAGS_DEFAULT_EXTERNAL | \
                 Py_TPFLAGS_HAVE_VERSION_TAG)

#ifdef Py_BUILD_CORE
#define Py_TPFLAGS_DEFAULT Py_TPFLAGS_DEFAULT_CORE
#else
#define Py_TPFLAGS_DEFAULT Py_TPFLAGS_DEFAULT_EXTERNAL
#endif

#define PyType_HasFeature(t,f)  (((t)->tp_flags & (f)) != 0)
#define PyType_FastSubclass(t,f)  PyType_HasFeature(t,f)


/*
The macros Py_INCREF(op) and Py_DECREF(op) are used to increment or decrement
reference counts.  Py_DECREF calls the object's deallocator function when
the refcount falls to 0; for
objects that don't contain references to other objects or heap memory
this can be the standard function free().  Both macros can be used
wherever a void expression is allowed.  The argument must not be a
NULL pointer.  If it may be NULL, use Py_XINCREF/Py_XDECREF instead.
The macro _Py_NewReference(op) initialize reference counts to 1, and
in special builds (Py_REF_DEBUG, Py_TRACE_REFS) performs additional
bookkeeping appropriate to the special build.

We assume that the reference count field can never overflow; this can
be proven when the size of the field is the same as the pointer size, so
we ignore the possibility.  Provided a C int is at least 32 bits (which
is implicitly assumed in many parts of this code), that's enough for
about 2**31 references to an object.

XXX The following became out of date in Python 2.2, but I'm not sure
XXX what the full truth is now.  Certainly, heap-allocated type objects
XXX can and should be deallocated.
Type objects should never be deallocated; the type pointer in an object
is not considered to be a reference to the type object, to save
complications in the deallocation function.  (This is actually a
decision that's up to the implementer of each new type so if you want,
you can count such references to the type object.)

*** WARNING*** The Py_DECREF macro must have a side-effect-free argument
since it may evaluate its argument multiple times.  (The alternative
would be to mace it a proper function or assign it to a global temporary
variable first, both of which are slower; and in a multi-threaded
environment the global variable trick is not safe.)
*/

/* First define a pile of simple helper macros, one set per special
 * build symbol.  These either expand to the obvious things, or to
 * nothing at all when the special mode isn't in effect.  The main
 * macros can later be defined just once then, yet expand to different
 * things depending on which special build options are and aren't in effect.
 * Trust me <wink>:  while painful, this is 20x easier to understand than,
 * e.g, defining _Py_NewReference five different times in a maze of nested
 * #ifdefs (we used to do that -- it was impenetrable).
 */
#ifdef Py_REF_DEBUG
PyAPI_DATA(Py_ssize_t) _Py_RefTotal;
PyAPI_FUNC(void) _Py_NegativeRefcount(const char *fname,
                                            int lineno, PyObject *op);
PyAPI_FUNC(PyObject *) _PyDict_Dummy(void);
PyAPI_FUNC(PyObject *) _PySet_Dummy(void);
PyAPI_FUNC(Py_ssize_t) _Py_GetRefTotal(void);
#define _Py_INC_REFTOTAL        _Py_RefTotal++
#define _Py_DEC_REFTOTAL        _Py_RefTotal--
#define _Py_REF_DEBUG_COMMA     ,
#define _Py_CHECK_REFCNT(OP)                                    \
{       if (((PyObject*)OP)->ob_refcnt < 0)                             \
                _Py_NegativeRefcount(__FILE__, __LINE__,        \
                                     (PyObject *)(OP));         \
}
#else
#define _Py_INC_REFTOTAL
#define _Py_DEC_REFTOTAL
#define _Py_REF_DEBUG_COMMA
#define _Py_CHECK_REFCNT(OP)    /* a semicolon */;
#endif /* Py_REF_DEBUG */

#ifdef COUNT_ALLOCS
PyAPI_FUNC(void) inc_count(PyTypeObject *);
PyAPI_FUNC(void) dec_count(PyTypeObject *);
#define _Py_INC_TPALLOCS(OP)    inc_count(Py_TYPE(OP))
#define _Py_INC_TPFREES(OP)     dec_count(Py_TYPE(OP))
#define _Py_DEC_TPFREES(OP)     Py_TYPE(OP)->tp_frees--
#define _Py_COUNT_ALLOCS_COMMA  ,
#else
#define _Py_INC_TPALLOCS(OP)
#define _Py_INC_TPFREES(OP)
#define _Py_DEC_TPFREES(OP)
#define _Py_COUNT_ALLOCS_COMMA
#endif /* COUNT_ALLOCS */

#ifdef Py_TRACE_REFS
/* Py_TRACE_REFS is such major surgery that we call external routines. */
PyAPI_FUNC(void) _Py_NewReference(PyObject *);
PyAPI_FUNC(void) _Py_ForgetReference(PyObject *);
PyAPI_FUNC(void) _Py_Dealloc(PyObject *);
PyAPI_FUNC(void) _Py_PrintReferences(FILE *);
PyAPI_FUNC(void) _Py_PrintReferenceAddresses(FILE *);
PyAPI_FUNC(void) _Py_AddToAllObjects(PyObject *, int force);

#else
/* Without Py_TRACE_REFS, there's little enough to do that we expand code
 * inline.
 */
#define _Py_NewReference(op) (                          \
    _Py_INC_TPALLOCS(op) _Py_COUNT_ALLOCS_COMMA         \
    _Py_INC_REFTOTAL  _Py_REF_DEBUG_COMMA               \
    Py_REFCNT(op) = 1)

#define _Py_ForgetReference(op) _Py_INC_TPFREES(op)

#define _Py_Dealloc(op) (                               \
    _Py_INC_TPFREES(op) _Py_COUNT_ALLOCS_COMMA          \
    (*Py_TYPE(op)->tp_dealloc)((PyObject *)(op)))
#endif /* !Py_TRACE_REFS */

#define Py_INCREF(op) (                         \
    _Py_INC_REFTOTAL  _Py_REF_DEBUG_COMMA       \
    ((PyObject*)(op))->ob_refcnt++)

#define Py_DECREF(op)                                   \
    do {                                                \
        if (_Py_DEC_REFTOTAL  _Py_REF_DEBUG_COMMA       \
        --((PyObject*)(op))->ob_refcnt != 0)            \
            _Py_CHECK_REFCNT(op)                        \
        else                                            \
        _Py_Dealloc((PyObject *)(op));                  \
    } while (0)

/* Safely decref `op` and set `op` to NULL, especially useful in tp_clear
 * and tp_dealloc implementatons.
 *
 * Note that "the obvious" code can be deadly:
 *
 *     Py_XDECREF(op);
 *     op = NULL;
 *
 * Typically, `op` is something like self->containee, and `self` is done
 * using its `containee` member.  In the code sequence above, suppose
 * `containee` is non-NULL with a refcount of 1.  Its refcount falls to
 * 0 on the first line, which can trigger an arbitrary amount of code,
 * possibly including finalizers (like __del__ methods or weakref callbacks)
 * coded in Python, which in turn can release the GIL and allow other threads
 * to run, etc.  Such code may even invoke methods of `self` again, or cause
 * cyclic gc to trigger, but-- oops! --self->containee still points to the
 * object being torn down, and it may be in an insane state while being torn
 * down.  This has in fact been a rich historic source of miserable (rare &
 * hard-to-diagnose) segfaulting (and other) bugs.
 *
 * The safe way is:
 *
 *      Py_CLEAR(op);
 *
 * That arranges to set `op` to NULL _before_ decref'ing, so that any code
 * triggered as a side-effect of `op` getting torn down no longer believes
 * `op` points to a valid object.
 *
 * There are cases where it's safe to use the naive code, but they're brittle.
 * For example, if `op` points to a Python integer, you know that destroying
 * one of those can't cause problems -- but in part that relies on that
 * Python integers aren't currently weakly referencable.  Best practice is
 * to use Py_CLEAR() even if you can't think of a reason for why you need to.
 */
#define Py_CLEAR(op)                            \
    do {                                        \
        if (op) {                               \
            PyObject *_py_tmp = (PyObject *)(op);               \
            (op) = NULL;                        \
            Py_DECREF(_py_tmp);                 \
        }                                       \
    } while (0)

/* Macros to use in case the object pointer may be NULL: */
#define Py_XINCREF(op) do { if ((op) == NULL) ; else Py_INCREF(op); } while (0)
#define Py_XDECREF(op) do { if ((op) == NULL) ; else Py_DECREF(op); } while (0)

/*
These are provided as conveniences to Python runtime embedders, so that
they can have object code that is not dependent on Python compilation flags.
*/
PyAPI_FUNC(void) Py_IncRef(PyObject *);
PyAPI_FUNC(void) Py_DecRef(PyObject *);

/*
_Py_NoneStruct is an object of undefined type which can be used in contexts
where NULL (nil) is not suitable (since NULL often means 'error').

Don't forget to apply Py_INCREF() when returning this value!!!
*/
PyAPI_DATA(PyObject) _Py_NoneStruct; /* Don't use this directly */
#define Py_None (&_Py_NoneStruct)

/* Macro for returning Py_None from a function */
#define Py_RETURN_NONE return Py_INCREF(Py_None), Py_None

/*
Py_NotImplemented is a singleton used to signal that an operation is
not implemented for a given type combination.
*/
PyAPI_DATA(PyObject) _Py_NotImplementedStruct; /* Don't use this directly */
#define Py_NotImplemented (&_Py_NotImplementedStruct)

/* Rich comparison opcodes */
#define Py_LT 0
#define Py_LE 1
#define Py_EQ 2
#define Py_NE 3
#define Py_GT 4
#define Py_GE 5

/* Maps Py_LT to Py_GT, ..., Py_GE to Py_LE.
 * Defined in object.c.
 */
PyAPI_DATA(int) _Py_SwappedOp[];

/*
Define staticforward and statichere for source compatibility with old
C extensions.

The staticforward define was needed to support certain broken C
compilers (notably SCO ODT 3.0, perhaps early AIX as well) botched the
static keyword when it was used with a forward declaration of a static
initialized structure.  Standard C allows the forward declaration with
static, and we've decided to stop catering to broken C compilers.
(In fact, we expect that the compilers are all fixed eight years later.)
*/

#define staticforward static
#define statichere static


/*
More conventions
================

Argument Checking
-----------------

Functions that take objects as arguments normally don't check for nil
arguments, but they do check the type of the argument, and return an
error if the function doesn't apply to the type.

Failure Modes
-------------

Functions may fail for a variety of reasons, including running out of
memory.  This is communicated to the caller in two ways: an error string
is set (see errors.h), and the function result differs: functions that
normally return a pointer return NULL for failure, functions returning
an integer return -1 (which could be a legal return value too!), and
other functions return 0 for success and -1 for failure.
Callers should always check for errors before using the result.  If
an error was set, the caller must either explicitly clear it, or pass
the error on to its caller.

Reference Counts
----------------

It takes a while to get used to the proper usage of reference counts.

Functions that create an object set the reference count to 1; such new
objects must be stored somewhere or destroyed again with Py_DECREF().
Some functions that 'store' objects, such as PyTuple_SetItem() and
PyList_SetItem(),
don't increment the reference count of the object, since the most
frequent use is to store a fresh object.  Functions that 'retrieve'
objects, such as PyTuple_GetItem() and PyDict_GetItemString(), also
don't increment
the reference count, since most frequently the object is only looked at
quickly.  Thus, to retrieve an object and store it again, the caller
must call Py_INCREF() explicitly.

NOTE: functions that 'consume' a reference count, like
PyList_SetItem(), consume the reference even if the object wasn't
successfully stored, to simplify error handling.

It seems attractive to make other functions that take an object as
argument consume a reference count; however, this may quickly get
confusing (even the current practice is already confusing).  Consider
it carefully, it may save lots of calls to Py_INCREF() and Py_DECREF() at
times.
*/


/* Trashcan mechanism, thanks to Christian Tismer.

When deallocating a container object, it's possible to trigger an unbounded
chain of deallocations, as each Py_DECREF in turn drops the refcount on "the
next" object in the chain to 0.  This can easily lead to stack faults, and
especially in threads (which typically have less stack space to work with).

A container object that participates in cyclic gc can avoid this by
bracketing the body of its tp_dealloc function with a pair of macros:

static void
mytype_dealloc(mytype *p)
{
    ... declarations go here ...

    PyObject_GC_UnTrack(p);        // must untrack first
    Py_TRASHCAN_SAFE_BEGIN(p)
    ... The body of the deallocator goes here, including all calls ...
    ... to Py_DECREF on contained objects.                         ...
    Py_TRASHCAN_SAFE_END(p)
}

CAUTION:  Never return from the middle of the body!  If the body needs to
"get out early", put a label immediately before the Py_TRASHCAN_SAFE_END
call, and goto it.  Else the call-depth counter (see below) will stay
above 0 forever, and the trashcan will never get emptied.

How it works:  The BEGIN macro increments a call-depth counter.  So long
as this counter is small, the body of the deallocator is run directly without
further ado.  But if the counter gets large, it instead adds p to a list of
objects to be deallocated later, skips the body of the deallocator, and
resumes execution after the END macro.  The tp_dealloc routine then returns
without deallocating anything (and so unbounded call-stack depth is avoided).

When the call stack finishes unwinding again, code generated by the END macro
notices this, and calls another routine to deallocate all the objects that
may have been added to the list of deferred deallocations.  In effect, a
chain of N deallocations is broken into N / PyTrash_UNWIND_LEVEL pieces,
with the call stack never exceeding a depth of PyTrash_UNWIND_LEVEL.
*/

PyAPI_FUNC(void) _PyTrash_deposit_object(PyObject*);
PyAPI_FUNC(void) _PyTrash_destroy_chain(void);
PyAPI_DATA(int) _PyTrash_delete_nesting;
PyAPI_DATA(PyObject *) _PyTrash_delete_later;

#define PyTrash_UNWIND_LEVEL 50

#define Py_TRASHCAN_SAFE_BEGIN(op) \
    if (_PyTrash_delete_nesting < PyTrash_UNWIND_LEVEL) { \
        ++_PyTrash_delete_nesting;
        /* The body of the deallocator is here. */
#define Py_TRASHCAN_SAFE_END(op) \
        --_PyTrash_delete_nesting; \
        if (_PyTrash_delete_later && _PyTrash_delete_nesting <= 0) \
            _PyTrash_destroy_chain(); \
    } \
    else \
        _PyTrash_deposit_object((PyObject*)op);

#ifdef __cplusplus
}
#endif
#endif /* !Py_OBJECT_H */
