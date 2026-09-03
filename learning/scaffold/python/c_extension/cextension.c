/* ============================================================================
 * c_extension 演示 - Python C 扩展
 * 演示编写 Python C 扩展模块，编译和调用
 * ========================================================================== */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * 1. 简单的加法函数
 * ========================================================================== */
static PyObject* c_add(PyObject* self, PyObject* args) {
    double a, b;

    if (!PyArg_ParseTuple(args, "dd", &a, &b)) {
        return NULL;
    }

    return PyFloat_FromDouble(a + b);
}

/* ============================================================================
 * 2. 字符串反转
 * ========================================================================== */
static PyObject* c_reverse(PyObject* self, PyObject* args) {
    const char* input;
    Py_ssize_t len;

    if (!PyArg_ParseTuple(args, "s#", &input, &len)) {
        return NULL;
    }

    /* 分配反转后的字符串 */
    char* reversed = (char*)malloc(len + 1);
    if (!reversed) {
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < len; i++) {
        reversed[i] = input[len - 1 - i];
    }
    reversed[len] = '\0';

    PyObject* result = PyUnicode_FromString(reversed);
    free(reversed);
    return result;
}

/* ============================================================================
 * 3. 计算数组平均数
 * ========================================================================== */
static PyObject* c_average(PyObject* self, PyObject* args) {
    PyObject* list_obj;

    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &list_obj)) {
        return NULL;
    }

    Py_ssize_t size = PyList_Size(list_obj);
    if (size == 0) {
        PyErr_SetString(PyExc_ValueError, "列表不能为空");
        return NULL;
    }

    double sum = 0.0;
    for (Py_ssize_t i = 0; i < size; i++) {
        PyObject* item = PyList_GetItem(list_obj, i);
        if (!PyNumber_Check(item)) {
            PyErr_SetString(PyExc_TypeError, "列表元素必须是数字");
            return NULL;
        }
        sum += PyFloat_AsDouble(item);
    }

    return PyFloat_FromDouble(sum / size);
}

/* ============================================================================
 * 4. 斐波那契数列（性能演示）
 * ========================================================================== */
static PyObject* c_fibonacci(PyObject* self, PyObject* args) {
    int n;

    if (!PyArg_ParseTuple(args, "i", &n)) {
        return NULL;
    }
    if (n < 0) {
        PyErr_SetString(PyExc_ValueError, "n 不能为负数");
        return NULL;
    }

    PyObject* list = PyList_New(n);
    if (!list) return NULL;

    for (int i = 0; i < n; i++) {
        long val;
        if (i <= 1) {
            val = i;
        } else {
            PyObject* prev1 = PyList_GetItem(list, i - 1);
            PyObject* prev2 = PyList_GetItem(list, i - 2);
            val = PyLong_AsLong(prev1) + PyLong_AsLong(prev2);
        }
        PyList_SetItem(list, i, PyLong_FromLong(val));
    }

    return list;
}

/* ============================================================================
 * 方法表
 * ========================================================================== */
static PyMethodDef CMethods[] = {
    {"add",       c_add,       METH_VARARGS, "加法：c_add(a, b) -> float"},
    {"reverse",   c_reverse,   METH_VARARGS, "字符串反转：c_reverse(s) -> str"},
    {"average",   c_average,   METH_VARARGS, "数组平均数：c_average(list) -> float"},
    {"fibonacci", c_fibonacci, METH_VARARGS, "斐波那契数列：c_fibonacci(n) -> list"},
    {NULL, NULL, 0, NULL}
};

/* ============================================================================
 * 模块定义
 * ========================================================================== */
static struct PyModuleDef cextmodule = {
    PyModuleDef_HEAD_INIT,
    "cextension",                    /* 模块名 */
    "Python C 扩展示例模块",         /* 文档 */
    -1,                              /* 模块状态（-1 = 全局） */
    CMethods
};

PyMODINIT_FUNC PyInit_cextension(void) {
    return PyModule_Create(&cextmodule);
}
