/**
 * @file python_executor.cpp
 * @brief Python execution helper — compiled without Qt headers to avoid macro collisions.
 */

// Python.h MUST be the first include — no Qt headers before this
#ifdef HAVE_PYTHON
#include <Python.h>
#endif

#include "python_executor.h"

#include <cstdlib>
#include <cstring>
#include <cctype>

#ifdef HAVE_PYTHON

extern "C" {

int py_executor_is_initialized(void) {
    return Py_IsInitialized() ? 1 : 0;
}

char* py_executor_execute(const char* code, int* out_is_error) {
    *out_is_error = 0;

    if (!Py_IsInitialized()) {
        *out_is_error = 1;
        const char* msg = "Error: Python runtime not initialized.";
        char* result = (char*)malloc(strlen(msg) + 1);
        if (result) strcpy(result, msg);
        return result;
    }

    // Redirect stdout/stderr
    PyRun_SimpleString(
        "import sys, io\n"
        "if not hasattr(sys, '_viospice_orig_stdout'):\n"
        "    sys._viospice_orig_stdout = sys.stdout\n"
        "    sys._viospice_orig_stderr = sys.stderr\n"
        "sys.stdout = io.StringIO()\n"
        "sys.stderr = io.StringIO()\n"
    );

    PyObject* mainDict = PyModule_GetDict(PyImport_AddModule("__main__"));

    // Try eval first (for expressions)
    PyObject* result = PyRun_String(code, Py_eval_input, mainDict, mainDict);
    int hadError = 0;
    if (!result) {
        // eval failed, try exec (statements)
        PyErr_Clear();
        PyObject* result2 = PyRun_String(code, Py_file_input, mainDict, mainDict);
        if (!result2) {
            PyErr_Print();
            hadError = 1;
        }
        Py_XDECREF(result2);
    } else {
        Py_DECREF(result);
    }

    // Capture stdout/stderr
    PyRun_SimpleString(
        "import sys\n"
        "_out = sys.stdout.getvalue()\n"
        "_err = sys.stderr.getvalue()\n"
        "sys.stdout = sys._viospice_orig_stdout\n"
        "sys.stderr = sys._viospice_orig_stderr\n"
    );

    PyObject* outObj = PyObject_GetAttrString(mainDict, "_out");
    PyObject* errObj = PyObject_GetAttrString(mainDict, "_err");

    // Build combined output string
    const char* outStr = NULL;
    const char* errStr = NULL;
    if (outObj && outObj != Py_None) {
        outStr = PyUnicode_AsUTF8(outObj);
    }
    if (errObj && errObj != Py_None) {
        errStr = PyUnicode_AsUTF8(errObj);
        hadError = 1;
    }

    char* combined = NULL;
    if (outStr && errStr) {
        size_t len = strlen(outStr) + 1 + strlen(errStr) + 1;
        combined = (char*)malloc(len);
        if (combined) {
            sprintf(combined, "%s\n%s", outStr, errStr);
        }
    } else if (outStr) {
        combined = strdup(outStr);
    } else if (errStr) {
        combined = strdup(errStr);
    } else {
        combined = strdup("");
    }

    Py_XDECREF(outObj);
    Py_XDECREF(errObj);

    *out_is_error = hadError;
    return combined;
}

void py_executor_free(void* ptr) {
    free(ptr);
}

int py_executor_is_complete(const char* code) {
    if (!code) return 1;

    // Skip whitespace
    const char* p = code;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') return 1;

    // Find end (skip trailing whitespace)
    const char* end = code + strlen(code) - 1;
    while (end > code && isspace((unsigned char)*end)) end--;

    char lastChar = *end;
    if (lastChar == ':') return 0;
    if (lastChar == '\\') return 0;

    // Count brackets
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

} // extern "C"

#endif
