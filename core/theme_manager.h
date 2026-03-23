#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include "theme.h"
#include <QObject>

class ThemeManager : public QObject {
    Q_OBJECT

public:
    virtual ~ThemeManager();
    static ThemeManager& instance();
    static PCBTheme* theme();

    void setTheme(PCBTheme::ThemeType type);
    void setTheme(PCBTheme* theme);
    PCBTheme* currentTheme() const { return m_theme; }

signals:
    void themeChanged();

private:
    ThemeManager();

    PCBTheme* m_theme;
};

#endif // THEME_MANAGER_H
