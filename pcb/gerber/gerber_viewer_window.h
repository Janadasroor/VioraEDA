#ifndef GERBER_VIEWER_WINDOW_H
#define GERBER_VIEWER_WINDOW_H

#include <QMainWindow>
#include <QColor>
#include <QWidget>
#include <QListWidget>
#include <QTabWidget>
#include "gerber_view.h"
#include "gerber_3d_view.h"

/**
 * @brief Main window for the Gerber Viewer application
 */
class GerberViewerWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit GerberViewerWindow(QWidget* parent = nullptr);
    ~GerberViewerWindow();

    void openFiles(const QStringList& paths);

private slots:
    void onOpenFiles();
    void onClearAll();
    void onLayerToggled(QListWidgetItem* item);
    void onSelectBackgroundColor();
    void onTabChanged(int index);

private:
    void setupUI();
    void addGerberFile(const QString& path);
    void applyBackgroundColor(const QColor& color);
    void ensure3DView();
    void refreshViews();

    GerberView* m_view;
    Gerber3DView* m_3dView;
    QWidget* m_3dPage;
    QTabWidget* m_tabWidget;
    QListWidget* m_layerList;
    QList<GerberLayer*> m_loadedLayers;
    QColor m_backgroundColor;
    bool m_isBulkLoading = false;
};

#endif // GERBER_VIEWER_WINDOW_H
