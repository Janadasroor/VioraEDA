#include "theme_manager.h"
#include "config_manager.h"
#include <QApplication>

namespace {
PCBTheme::ThemeType themeTypeFromName(const QString& name) {
    const QString normalized = name.trimmed().toLower();
    if (normalized == "light") return PCBTheme::Light;
    if (normalized == "engineering") return PCBTheme::Engineering;
    return PCBTheme::Dark;
}

QString themeNameFromType(PCBTheme::ThemeType type) {
    switch (type) {
    case PCBTheme::Light: return "Light";
    case PCBTheme::Engineering: return "Engineering";
    case PCBTheme::Dark:
    default:
        return "Dark";
    }
}
}

ThemeManager& ThemeManager::instance() {
    static ThemeManager instance;
    static bool firstCall = true;
    if (firstCall && qApp) {
        instance.m_theme->applyToApplication();
        firstCall = false;
    }
    return instance;
}

PCBTheme* ThemeManager::theme() {
    return instance().currentTheme();
}

ThemeManager::ThemeManager()
    : m_theme(new PCBTheme(themeTypeFromName(ConfigManager::instance().currentTheme()))) {
}

ThemeManager::~ThemeManager() {
    delete m_theme;
}

void ThemeManager::setTheme(PCBTheme::ThemeType type) {
    delete m_theme;
    m_theme = new PCBTheme(type);

    ConfigManager::instance().setCurrentTheme(themeNameFromType(type));
    ConfigManager::instance().save();
    
    // Apply globally
    m_theme->applyToApplication();
    
    Q_EMIT themeChanged();
}

void ThemeManager::setTheme(PCBTheme* theme) {
    if (theme != m_theme) {
        delete m_theme;
        m_theme = theme;

        if (m_theme) {
            ConfigManager::instance().setCurrentTheme(themeNameFromType(m_theme->type()));
            ConfigManager::instance().save();
        }
        
        // Apply globally
        m_theme->applyToApplication();
        
        Q_EMIT themeChanged();
    }
}
