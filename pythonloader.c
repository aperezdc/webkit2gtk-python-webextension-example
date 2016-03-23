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


G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data (WebKitWebExtension *extension,
                                                GVariant           *user_data)
{
    g_printerr ("Python loader, extension=%p\n", extension);

    Py_Initialize ();

#if PY_VERSION_HEX < 0x03000000
    const char *argv[] = { "", NULL };
#else
    wchar_t *argv[] = { L"", NULL };
#endif

    PySys_SetArgvEx (1, argv, 0);

    pygobject_init (-1, -1, -1);
    if (PyErr_Occurred ()) {
        g_warning ("Could not initialize PyGObject");
        return;
    }

    pyg_enable_threads ();
    PyEval_InitThreads ();

    PyObject py_auto *web_ext_module =
            PyImport_ImportModule ("gi.repository.WebKit2WebExtension");
    if (!web_ext_module) {
        if (PyErr_Occurred ()) {
            g_printerr ("Could not import gi.repository.WebKit2WebExtension: ");
            PyErr_Print ();
        } else {
            g_printerr ("Could not import gi.repository.WebKit2WebExtension"
                        " (no error given)\n");
        }
        return;
    }

    PyObject py_auto *py_filename = PyUnicode_FromString ("extension");
    PyObject py_auto *py_module = PyImport_Import (py_filename);
    if (!py_module) {
        g_warning ("Could not import '%s'", "extension");
        return;
    }

    PyObject py_auto *py_func = PyObject_GetAttrString (py_module, "initialize");
    if (!py_func) {
        g_warning ("Could not obtain '%s.initialize'", "extension");
        return;
    }
    if (!PyCallable_Check (py_func)) {
        g_warning ("Object '%s.initialize' is not callable", "extension");
        return;
    }

    PyObject py_auto *py_extension = pygobject_new (G_OBJECT (extension));
    PyObject py_auto *py_extra_args = pyg_boxed_new (G_TYPE_VARIANT,
                                                     g_variant_ref (user_data),
                                                     FALSE,
                                                     FALSE);

    PyObject py_auto *py_func_args = PyTuple_New (2);
    PyTuple_SetItem (py_func_args, 0, py_extension);
    PyTuple_SetItem (py_func_args, 1, py_extra_args);
    PyObject py_auto *py_retval = PyObject_CallObject (py_func, py_func_args);
    if (!py_retval) {
        g_printerr ("Error calling '%s.initialize':\n", "extension");
        PyErr_Print ();
    }
}
