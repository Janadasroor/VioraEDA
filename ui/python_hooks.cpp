/**
 * @file python_hooks.cpp
 * @brief Event hooks system — Python callbacks for C++ events.
 *
 * Compiled without Qt headers to avoid Python/Qt macro collisions.
 */

// Verify HAVE_PYTHON is actually defined for this translation unit
#ifndef HAVE_PYTHON
#error "python_hooks.cpp requires HAVE_PYTHON to be defined. Check CMake configuration."
#endif

#ifdef HAVE_PYTHON
#include <Python.h>
#endif

#include "python_hooks.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <mutex>

#ifdef HAVE_PYTHON

struct HookRegistry {
    std::map<std::string, std::vector<PyObject*>> handlers;
    std::mutex mutex;
};

static HookRegistry g_registry;

extern "C" {

void py_hooks_init(void) {
    // Nothing to do — registry is constructed on first use
}

int py_hooks_register(const char* event_name, void* callable) {
    if (!event_name || !callable) return -1;

    std::lock_guard<std::mutex> lock(g_registry.mutex);
    PyObject* cb = static_cast<PyObject*>(callable);
    if (!PyCallable_Check(cb)) return -1;

    Py_INCREF(cb);
    g_registry.handlers[event_name].push_back(cb);
    return 0;
}

void py_hooks_clear(const char* event_name) {
    if (!event_name) return;
    std::lock_guard<std::mutex> lock(g_registry.mutex);
    auto it = g_registry.handlers.find(event_name);
    if (it != g_registry.handlers.end()) {
        for (PyObject* cb : it->second) {
            Py_DECREF(cb);
        }
        it->second.clear();
        g_registry.handlers.erase(it);
    }
}

int py_hooks_fire(const char* event_name, const char* data) {
    if (!event_name) return -1;

    std::lock_guard<std::mutex> lock(g_registry.mutex);
    auto it = g_registry.handlers.find(event_name);
    if (it == g_registry.handlers.end() || it->second.empty()) {
        return 0;
    }

    int called = 0;
    PyObject* args = NULL;
    if (data) {
        PyObject* dataStr = PyUnicode_FromString(data);
        if (dataStr) {
            args = PyTuple_Pack(1, dataStr);
            Py_DECREF(dataStr);
        }
    } else {
        args = PyTuple_New(0);
    }

    if (!args) return -1;

    // Copy handler list to avoid issues if handlers unregister themselves
    std::vector<PyObject*> handlers = it->second;

    for (PyObject* cb : handlers) {
        PyObject* result = PyObject_CallObject(cb, args);
        if (result) {
            Py_DECREF(result);
        } else {
            PyErr_Print();
        }
        called++;
    }

    Py_DECREF(args);
    return called;
}

char* py_hooks_list_events(void) {
    std::lock_guard<std::mutex> lock(g_registry.mutex);
    std::string result;
    bool first = true;
    for (const auto& [name, handlers] : g_registry.handlers) {
        if (!handlers.empty()) {
            if (!first) result += ", ";
            result += name;
            first = false;
        }
    }
    char* out = strdup(result.c_str());
    return out;
}

int py_hooks_count(const char* event_name) {
    if (!event_name) return 0;
    std::lock_guard<std::mutex> lock(g_registry.mutex);
    auto it = g_registry.handlers.find(event_name);
    if (it != g_registry.handlers.end()) {
        return (int)it->second.size();
    }
    return 0;
}

void py_hooks_free_str(char* ptr) {
    free(ptr);
}

} // extern "C"

#else

extern "C" {
void py_hooks_init(void) {}
int py_hooks_register(const char*, void*) { return -1; }
void py_hooks_clear(const char*) {}
int py_hooks_fire(const char*, const char*) { return -1; }
char* py_hooks_list_events(void) { return strdup(""); }
int py_hooks_count(const char*) { return 0; }
void py_hooks_free_str(char* ptr) { free(ptr); }
}

#endif
