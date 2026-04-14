#ifndef PYTHON_HOOKS_H
#define PYTHON_HOOKS_H

/**
 * @brief Event hooks system — allows Python scripts to register callbacks
 * for C++ events (simulation finished, schematic changed, etc.).
 *
 * Uses a pure C API to avoid Qt/Python macro collisions.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the hooks subsystem (call once after Py_Initialize). */
void py_hooks_init(void);

/**
 * Register a Python callable for a named event.
 * @param event_name Event name (e.g., "simulation_finished")
 * @param callable Python callable object (borrowed reference)
 * @return 0 on success, -1 on error
 */
int py_hooks_register(const char* event_name, void* callable);

/**
 * Unregister all handlers for a named event.
 */
void py_hooks_clear(const char* event_name);

/**
 * Fire an event — call all registered Python handlers with the given data.
 * @param event_name Event name
 * @param data JSON string with event data (NULL if no data)
 * @return Number of handlers called, or -1 on error
 */
int py_hooks_fire(const char* event_name, const char* data);

/**
 * List all registered event names (comma-separated, caller must free).
 */
char* py_hooks_list_events(void);

/**
 * Get the number of handlers registered for an event.
 */
int py_hooks_count(const char* event_name);

/** Free a string returned by py_hooks_list_events(). */
void py_hooks_free_str(char* ptr);

#ifdef __cplusplus
}
#endif

#endif // PYTHON_HOOKS_H
