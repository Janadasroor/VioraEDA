#ifndef PYTHON_EXECUTOR_H
#define PYTHON_EXECUTOR_H

/**
 * @brief Execute Python code and capture output.
 *
 * This header is intentionally free of Qt includes to avoid macro collisions
 * between Python.h and Qt's MOC system (slots, signals, emit keywords).
 * Use plain C strings for the API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Returns 1 if Python is initialized, 0 otherwise. */
int py_executor_is_initialized(void);

/**
 * Execute Python code and return output.
 * Caller must free the returned string with py_executor_free().
 * @param code UTF-8 Python code to execute
 * @param out_is_error Set to 1 if an error occurred
 * @return malloc'd UTF-8 string (captured stdout/stderr), or NULL on failure
 */
char* py_executor_execute(const char* code, int* out_is_error);

/** Free a string returned by py_executor_execute(). */
void py_executor_free(void* ptr);

/** Returns 1 if the code looks like a complete Python statement. */
int py_executor_is_complete(const char* code);

#ifdef __cplusplus
}
#endif

#endif // PYTHON_EXECUTOR_H
