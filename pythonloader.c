/*
 * pythonloader.c
 * Copyright (C) 2015-2016 Adrian Perez <aperez@igalia.com>
 * Copyright (C) 2016 Nathan Hoad <nathan@getoffmalawn.com>
 *
 * Distributed under terms of the MIT license.
 */

#include <webkit2/webkit-web-extension.h>
#include <pygobject.h>
#include <stdarg.h>

/*
 * XXX: Hacky workaround ahead!
 *
 * PyGObject internally uses _pygi_struct_new_from_g_type() to wrap
 * GVariant instances into a gi.Struct, but the function is not in the
 * public API. Instead, we temporarily wrap the GVariant into a GValue
 * (ugh!), and use pyg_value_as_pyobject(), which in turn calls function
 * pygi_value_to_py_structured_type() —also private—, and finally that
 * in turn calls _pygi_struct_new_from_g_type() as initially desired.
 */
static PyObject*
g_variant_to_pyobject (GVariant *variant)
{
    GValue value = { 0, };
    g_value_init (&value, G_TYPE_VARIANT);
    g_value_set_variant (&value, variant);
    return pyg_value_as_pyobject (&value, FALSE);
}

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


#define PY_CHECK_ACT(expr, act, err_fmt, ...)     \
    do {                                          \
        if (!(expr)) {                            \
            g_printerr (err_fmt, ##__VA_ARGS__);  \
            if (PyErr_Occurred ()) {              \
                g_printerr (": ");                \
                PyErr_Print ();                   \
            } else {                              \
                g_printerr (" (no error given)"); \
            }                                     \
            act;                                  \
        }                                         \
    } while (0)

#define PY_CHECK(expr, err_fmt, ...) \
        PY_CHECK_ACT (expr, return, err_fmt, ##__VA_ARGS__)


/* This would be "extension.py" from the source directory. */
static const char *extension_name = "extension";


static gboolean
pygi_require (const gchar *module, ...)
{
    PyObject py_auto *gi_module = PyImport_ImportModule ("gi");
    PY_CHECK_ACT (gi_module, return FALSE, "Could not import 'gi'");

    PyObject py_auto *func = PyObject_GetAttrString (gi_module, "require_version");
    PY_CHECK_ACT (func, return FALSE,
                  "Could not obtain 'gi.require_version'");
    PY_CHECK_ACT (PyCallable_Check (func), return FALSE,
                  "Object 'gi.require_version' is not callable");

    gboolean result = TRUE;
    va_list arglist;
    va_start (arglist, module);
    while (module) {
        /*
         * For each module and version, call: gi.require(module, version)
         */
        const gchar *version = va_arg (arglist, const gchar*);
        {
            PyObject py_auto *args = Py_BuildValue ("(ss)", module, version);
            PyObject py_auto *rval = PyObject_CallObject (func, args);
            PY_CHECK_ACT (rval, result = FALSE; break,
                          "Error calling 'gi.require_version(\"%s\", \"%s\")'",
                          module, version);
        }
        module = va_arg (arglist, const gchar*);
    }
    va_end (arglist);
    return result;
}


G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data (WebKitWebExtension *extension,
                                                GVariant           *user_data)
{
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

    if (!pygi_require ("GLib", "2.0",
                       "WebKit2WebExtension", "4.0",
                       NULL))
        return;

    PyObject py_auto *web_ext_module =
            PyImport_ImportModule ("gi.repository.WebKit2WebExtension");
    PY_CHECK (web_ext_module,
              "Could not import 'gi.repository.WebKit2WebExtension'");

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
    PY_CHECK (PyCallable_Check (py_func),
              "Object '%s.initialize' is not callable", extension_name);

    PyObject py_auto *py_extension = pygobject_new (G_OBJECT (extension));
    PyObject py_auto *py_extra_args = g_variant_to_pyobject (user_data);

    PY_CHECK (py_extra_args, "Cannot create GLib.Variant");

    PyObject py_auto py_auto *func_args = PyTuple_New (2);
    PyTuple_SetItem (func_args, 0, py_extension);
    PyTuple_SetItem (func_args, 1, py_extra_args);

    PyObject py_auto *py_retval = PyObject_CallObject (py_func, func_args);
    PY_CHECK (py_retval, "Error calling '%s.initialize'", extension_name);
}
