#ifndef PDF_VIEWER_DIALOG_H
#define PDF_VIEWER_DIALOG_H

#include <QDialog>

class QLabel;
class QScrollArea;

#ifdef VIOSPICE_HAS_QT_PDF
class QPdfDocument;
#endif

class PdfViewerDialog : public QDialog {
    Q_OBJECT

public:
    explicit PdfViewerDialog(const QString& filePath, QWidget* parent = nullptr);
    ~PdfViewerDialog() override;

private slots:
    void onZoomIn();
    void onZoomOut();
    void onZoomFit();
    void onZoomActualSize();

private:
    void setupUI();
    void loadPdf(const QString& filePath);
    void updatePageInfo();
    void updateZoomLabel();
    void renderCurrentPage();

protected:
    void resizeEvent(QResizeEvent* event) override;

    QString m_pdfPath;
    QLabel* m_pageInfoLabel = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QLabel* m_imageLabel = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    int m_currentPage = 0;
    qreal m_zoomFactor = 1.0;
    bool m_fitToWindow = true;

#ifdef VIOSPICE_HAS_QT_PDF
    QPdfDocument* m_document = nullptr;
#else
    QLabel* m_fallbackLabel = nullptr;
#endif
};

#endif // PDF_VIEWER_DIALOG_H
