/*
 * pythonloader.c
 * Copyright (C) 2015-2016 Adrian Perez <aperez@igalia.com>
 * Copyright (C) 2016 Nathan Hoad <nathan@getoffmalawn.com>
 *
 * Distributed under terms of the MIT license.
 */

#include <webkit2/webkit-web-extension.h>
#include <pygobject.h>


#define py_auto __attribute__((cleanup(py_object_cleanup)))

static void
py_object_cleanup(void *ptr)
{
    PyObject **py_obj_location = ptr;
    if (py_obj_location) {
        Py_DECREF (*py_obj_location);
        *py_obj_location = NULL;
    }
}


#define PY_CHECK(expr, err_fmt, ...)              \
    do {                                          \
        if (!(expr)) {                            \
            g_printerr (err_fmt, ##__VA_ARGS__);  \
            if (PyErr_Occurred ()) {              \
                g_printerr (": ");                \
                PyErr_Print ();                   \
            } else {                              \
                g_printerr (" (no error given)"); \
            }                                     \
            return;                               \
        }                                         \
    } while (0)


/* This would be "extension.py" from the source directory. */
static const char *extension_name = "extension";


G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data (WebKitWebExtension *extension,
                                                GVariant           *user_data)
{
    g_printerr ("Python loader: extension=%p\n", extension);

    Py_Initialize ();

#if PY_VERSION_HEX < 0x03000000
    const char *argv[] = { "", NULL };
#else
    wchar_t *argv[] = { L"", NULL };
#endif

    PySys_SetArgvEx (1, argv, 0);

    pygobject_init (-1, -1, -1);
    if (PyErr_Occurred ()) {
        g_printerr ("Could not initialize PyGObject");
        return;
    }

    pyg_enable_threads ();
    PyEval_InitThreads ();

    PyObject py_auto *gi_module = PyImport_ImportModule ("gi");
    PY_CHECK (gi_module, "Could not import 'gi'");
    g_printerr ("Python loader: 'gi' module=%p\n", gi_module);

    /* Call gi.require_version("WebKit2WebExtension", "4.0") */
    {
        PyObject py_auto *require_version_func =
                PyObject_GetAttrString (gi_module, "require_version");
        PY_CHECK (require_version_func,
                  "Could not obtain 'gi.require_version'");
        g_printerr ("Python loader: 'gi.require_version' object=%p\n", require_version_func);
        PY_CHECK (PyCallable_Check (require_version_func),
                  "Object 'gi.require_version' is not callable");
        g_printerr ("Python loader: 'gi.require_version' is callable (good!)\n");

        PyObject py_auto *func_args = Py_BuildValue ("(ss)", "WebKit2WebExtension", "4.0");
        PyObject py_auto *retval = PyObject_CallObject (require_version_func, func_args);
        PY_CHECK (retval, "Error calling 'gi.require_version'");
        g_printerr ("Python loader: call 'gi.require_version(\"WebKit2WebExtension\", \"4.0\")' succeeded\n");
    }

    PyObject py_auto *web_ext_module =
            PyImport_ImportModule ("gi.repository.WebKit2WebExtension");
    PY_CHECK (web_ext_module,
              "Could not import 'gi.repository.WebKit2WebExtension'");
    g_printerr ("Python loader: 'gi.repository.WebKit2WebExtension' module=%p\n",
                web_ext_module);

    /*
     * TODO: Instead of assuming that the Python import path contains the
     *       directory where the extension is, manually load and compile the
     *       extension code, then use PyImport_AddModule() to programmatically
     *       create a new module and PyImport_ExecCodeModule() to import it
     *       from a bytecode object.
     */
    PyObject py_auto *py_filename = PyUnicode_FromString (extension_name);
    PyObject py_auto *py_module = PyImport_Import (py_filename);
    PY_CHECK (py_module, "Could not import '%s'", extension_name);

    PyObject py_auto *py_func = PyObject_GetAttrString (py_module, "initialize");
    PY_CHECK (py_func, "Could not obtain '%s.initialize'", extension_name);
    g_printerr ("Python loader: '%s.initialize' object=%p\n", extension_name, py_func);
    PY_CHECK (PyCallable_Check (py_func),
              "Object '%s.initialize' is not callable", extension_name);
    g_printerr ("Python loader: '%s.initialize' os callable (good!)\n", extension_name);

    PyObject py_auto *py_extension = pygobject_new (G_OBJECT (extension));
    g_printerr ("Python loader: web extension object=%p\n", py_extension);

    PyObject py_auto *py_extra_args = pyg_boxed_new (G_TYPE_VARIANT,
                                                     g_variant_ref (user_data),
                                                     FALSE,
                                                     FALSE);
    g_printerr ("Python loader: initialization arguments object=%p\n", py_extra_args);

    PyObject py_auto py_auto *func_args = PyTuple_New (2);
    PyTuple_SetItem (func_args, 0, py_extension);
    PyTuple_SetItem (func_args, 1, py_extra_args);
    PyObject py_auto *py_retval = PyObject_CallObject (py_func, func_args);
    PY_CHECK (py_retval, "Error calling '%s.initialize'", extension_name);
}
