#define __LOG_ON

#include <Python.h>
#include "log.h"

void plugin_upload(const char *path){
    char *url;

    PyObject *py_module;
    PyObject *py_func;
    PyObject *py_path;
    PyObject *py_args;
    PyObject *py_url;

    Py_Initialize();

    /* load current dirrectory *SHOULD BE REMOVED IN RELEASE* */
    PyRun_SimpleString("import sys; sys.path.append('.')");

    /* import */
    py_module = PyImport_Import(PyUnicode_FromString("upload"));
    if (!py_module) {
        LOG_FR("plugin `upload` no found");
        return;
    }

    py_func = PyObject_GetAttrString(py_module, "upload");
    if (!py_func) {
        LOG_FR("function `upload()` no found");
        return;
    }

    /* build args */
    py_args = PyTuple_New(1);
    PyTuple_SetItem(py_args, 0, PyUnicode_FromString(path));

    /* call */
    py_url = PyObject_CallObject(py_func, py_args);
    url = PyUnicode_AsUTF8(py_url);

    LOG_FR("%s", url);
    Py_Finalize();
}

void plugin_avatar(const char *nick, const char *user, const char *host){
    char *path;
    PyObject *py_module;
    PyObject *py_func;
    PyObject *py_args;
    PyObject *py_path;

    Py_Initialize();

    /* load current dirrectory *SHOULD BE REMOVED IN RELEASE* */
    PyRun_SimpleString("import sys; sys.path.append('.')");

    /* import */
    py_module = PyImport_Import(PyUnicode_FromString("avatar"));
    if (!py_module) {
        LOG_FR("plugin `avatar` no found");
        return;
    }

    py_func = PyObject_GetAttrString(py_module, "avatar");
    if (!py_func) {
        LOG_FR("function `avatar()` no found");
        return;
    }

    /* build args */
    py_args = PyTuple_New(3);
    PyTuple_SetItem(py_args, 0, PyUnicode_FromString(nick));
    PyTuple_SetItem(py_args, 1, PyUnicode_FromString(user));
    PyTuple_SetItem(py_args, 2, PyUnicode_FromString(host));

    /* call */
    py_path = PyObject_CallObject(py_func, py_args);
    path = PyUnicode_AsUTF8(py_path);

    LOG_FR("%s", path);
    Py_Finalize();
}
int plugin_init(){
    plugin_upload("/home/la/Pictures/Wallpapers/bg.jpg");
    plugin_avatar("1", "2", "3");
    return 0;
}