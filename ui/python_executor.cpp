/**
 * @file python_executor.cpp
 * @brief Python execution helper — compiled without Qt headers.
 *
 * Redirects sys.stdout at the C level using PySys_SetObject for reliable capture.
 */

#ifdef HAVE_PYTHON
#include <Python.h>
#endif

#include "python_executor.h"

#include <cstdlib>
#include <cstring>
#include <cctype>

#ifdef HAVE_PYTHON

extern "C" {

char* py_executor_execute(const char* code, int* out_is_error) {
    *out_is_error = 0;

    if (!Py_IsInitialized()) {
        *out_is_error = 1;
        return strdup("Error: Python runtime not initialized.");
    }

    PyGILState_STATE gstate = PyGILState_Ensure();

    // Auto-import vspice as v on first execution
    static bool autoImportDone = false;
    if (!autoImportDone) {
        PyObject* mainDict = PyModule_GetDict(PyImport_AddModule("__main__"));
        // Try to import vspice as v — ignore errors if vspice isn't available
        PyRun_String(
            "try:\n"
            "    import vspice\n"
            "    import vspice.v as v\n"
            "    v.handlers = vspice.handlers\n"
            "    # Expose gemini terminal at module level\n"
            "    if hasattr(vspice.v, 'gemini') and vspice.v.gemini is not None:\n"
            "        gemini = vspice.v.gemini\n"
            "        term = gemini\n"
            "except Exception as _v_err:\n"
            "    pass\n",
            Py_file_input, mainDict, mainDict
        );
        PyErr_Clear();
        autoImportDone = true;
    }

    // Create StringIO for capture
    PyObject* ioModule = PyImport_ImportModule("io");
    if (!ioModule) {
        PyGILState_Release(gstate);
        *out_is_error = 1;
        return strdup("Error: Cannot import io.");
    }
    
    PyObject* stringIOClass = PyObject_GetAttrString(ioModule, "StringIO");
    Py_DECREF(ioModule);
    if (!stringIOClass) {
        PyGILState_Release(gstate);
        *out_is_error = 1;
        return strdup("Error: Cannot get StringIO.");
    }
    
    PyObject* capture = PyObject_CallObject(stringIOClass, NULL);
    Py_DECREF(stringIOClass);
    if (!capture) {
        PyGILState_Release(gstate);
        *out_is_error = 1;
        return strdup("Error: Cannot create StringIO.");
    }
    
    // Save original stdout
    PyObject* origStdout = PySys_GetObject("stdout");
    Py_XINCREF(origStdout);
    
    // Redirect stdout at system level
    PySys_SetObject("stdout", capture);
    
    // Execute user code
    PyObject* mainDict = PyModule_GetDict(PyImport_AddModule("__main__"));
    
    // Try eval first (expressions like 2+2)
    PyObject* result = PyRun_String(code, Py_eval_input, mainDict, mainDict);
    int hadError = 0;
    if (!result) {
        PyErr_Clear();
        // Try exec (statements like print("hi"))
        result = PyRun_String(code, Py_file_input, mainDict, mainDict);
        if (!result) {
            PyErr_Print();
            hadError = 1;
        }
    } else {
        // Expression succeeded — print repr to our captured stdout
        // But skip None (e.g., print() calls, assignments)
        if (result != Py_None) {
            PyObject* repr = PyObject_Repr(result);
            if (repr) {
                PyObject* writeMethod = PyObject_GetAttrString(capture, "write");
                if (writeMethod) {
                    PyObject* arg = PyUnicode_FromFormat("%s\n", PyUnicode_AsUTF8(repr));
                    if (arg) {
                        PyObject_CallFunctionObjArgs(writeMethod, arg, NULL);
                        Py_DECREF(arg);
                    }
                    Py_DECREF(writeMethod);
                }
                Py_DECREF(repr);
            }
        }
    }
    Py_XDECREF(result);
    
    // Get captured output
    char* output = NULL;
    PyObject* getvalue = PyObject_GetAttrString(capture, "getvalue");
    if (getvalue) {
        PyObject* outStr = PyObject_CallObject(getvalue, NULL);
        if (outStr) {
            const char* cstr = PyUnicode_AsUTF8(outStr);
            if (cstr && strlen(cstr) > 0) {
                output = strdup(cstr);
            }
            Py_DECREF(outStr);
        }
        Py_DECREF(getvalue);
    }
    
    // Restore original stdout
    PySys_SetObject("stdout", origStdout);
    Py_XDECREF(origStdout);
    Py_DECREF(capture);
    
    if (!output) {
        output = strdup("");
    }
    
    PyGILState_Release(gstate);
    
    *out_is_error = hadError;
    return output;
}

int py_executor_is_initialized(void) {
    return Py_IsInitialized() ? 1 : 0;
}

int py_executor_is_complete(const char* code) {
    if (!code) return 1;
    const char* p = code;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') return 1;
    const char* end = code + strlen(code) - 1;
    while (end > code && isspace((unsigned char)*end)) end--;
    char lastChar = *end;
    if (lastChar == ':') return 0;
    if (lastChar == '\\') return 0;
    int parens = 0, brackets = 0, braces = 0;
    for (const char* c = code; c <= end; c++) {
        if (*c == '(') parens++;
        else if (*c == ')') parens--;
        else if (*c == '[') brackets++;
        else if (*c == ']') brackets--;
        else if (*c == '{') braces++;
        else if (*c == '}') braces--;
    }
    if (parens > 0 || brackets > 0 || braces > 0) return 0;
    return 1;
}

void py_executor_free(void* ptr) { free(ptr); }

} // extern "C"

#else

extern "C" {
int py_executor_is_initialized(void) { return 0; }
char* py_executor_execute(const char*, int* out_is_error) {
    *out_is_error = 1;
    return strdup("Python support not compiled in.");
}
void py_executor_free(void* ptr) { free(ptr); }
int py_executor_is_complete(const char*) { return 1; }
}

#endif
