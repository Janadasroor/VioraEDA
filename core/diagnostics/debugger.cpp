#include "debugger.h"

Debugger& Debugger::instance() {
    static Debugger inst;
    return inst;
}

Debugger::Debugger(QObject* parent) : QObject(parent) {}

void Debugger::setBreakpoint(int line) {
    if (!m_breakpoints.contains(line)) {
        m_breakpoints.insert(line);
        Q_EMIT breakpointsChanged();
    }
}

void Debugger::removeBreakpoint(int line) {
    if (m_breakpoints.remove(line)) {
        Q_EMIT breakpointsChanged();
    }
}

bool Debugger::hasBreakpoint(int line) const {
    return m_breakpoints.contains(line);
}

void Debugger::clearBreakpoints() {
    m_breakpoints.clear();
    Q_EMIT breakpointsChanged();
}

void Debugger::setState(DebugState s) {
    if (m_state != s) {
        m_state = s;
        Q_EMIT stateChanged(m_state);
    }
}

void Debugger::setActiveLine(int line) {
    if (m_activeLine != line) {
        m_activeLine = line;
        Q_EMIT activeLineChanged(m_activeLine);
    }
}

void Debugger::step() {
    Q_EMIT stepRequested();
}

void Debugger::resume() {
    setState(DebugState::Running);
}

void Debugger::stop() {
        setState(DebugState::Stopped);
        setActiveLine(-1);
    }
    
