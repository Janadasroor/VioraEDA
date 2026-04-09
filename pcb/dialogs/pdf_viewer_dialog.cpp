#include "pdf_viewer_dialog.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QResizeEvent>
#include <QScrollArea>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#ifdef VIOSPICE_HAS_QT_PDF
#include <QtPdf/QPdfDocument>
#include <QtPdf/QPdfDocumentRenderOptions>
#endif

PdfViewerDialog::PdfViewerDialog(const QString& filePath, QWidget* parent)
    : QDialog(parent)
    , m_pdfPath(filePath) {
    setWindowTitle("PDF Viewer");
    resize(1000, 760);
    setupUI();
    loadPdf(filePath);
}

PdfViewerDialog::~PdfViewerDialog() {
#ifdef VIOSPICE_HAS_QT_PDF
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
        m_document->close();
        delete m_document;
        m_document = nullptr;
    }
#endif
}

void PdfViewerDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    QToolBar* toolbar = new QToolBar(this);
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

#ifdef VIOSPICE_HAS_QT_PDF
    m_document = new QPdfDocument();

    QAction* prevBtn = toolbar->addAction("◀ Prev");
    connect(prevBtn, &QAction::triggered, this, [this]() {
        if (!m_document) {
            return;
        }
        if (m_currentPage > 0) {
            --m_currentPage;
            renderCurrentPage();
        }
    });

    QAction* nextBtn = toolbar->addAction("Next ▶");
    connect(nextBtn, &QAction::triggered, this, [this]() {
        if (!m_document) {
            return;
        }
        if (m_currentPage + 1 < m_document->pageCount()) {
            ++m_currentPage;
            renderCurrentPage();
        }
    });

    m_pageInfoLabel = new QLabel("Page 0 of 0", this);
    toolbar->addWidget(m_pageInfoLabel);
    toolbar->addSeparator();

    QAction* zoomOutBtn = toolbar->addAction("−");
    connect(zoomOutBtn, &QAction::triggered, this, &PdfViewerDialog::onZoomOut);

    QAction* zoomInBtn = toolbar->addAction("+");
    connect(zoomInBtn, &QAction::triggered, this, &PdfViewerDialog::onZoomIn);

    QAction* fitBtn = toolbar->addAction("Fit Page");
    connect(fitBtn, &QAction::triggered, this, &PdfViewerDialog::onZoomFit);

    QAction* actualBtn = toolbar->addAction("100%");
    connect(actualBtn, &QAction::triggered, this, &PdfViewerDialog::onZoomActualSize);

    m_zoomLabel = new QLabel("Fit", this);
    m_zoomLabel->setFixedWidth(60);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    toolbar->addWidget(m_zoomLabel);

    toolbar->addSeparator();
    QAction* openExtBtn = toolbar->addAction("Open External");
    connect(openExtBtn, &QAction::triggered, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_pdfPath));
    });

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setBackgroundRole(QPalette::Dark);

    m_imageLabel = new QLabel(m_scrollArea);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setBackgroundRole(QPalette::Base);
    m_imageLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_scrollArea->setWidget(m_imageLabel);

    connect(m_document, &QPdfDocument::pageCountChanged, this, [this]() {
        if (m_currentPage >= m_document->pageCount()) {
            m_currentPage = qMax(0, m_document->pageCount() - 1);
        }
        updatePageInfo();
        renderCurrentPage();
    });

    mainLayout->addWidget(toolbar);
    mainLayout->addWidget(m_scrollArea, 1);
#else
    QAction* openExtBtn = toolbar->addAction("Open External");
    connect(openExtBtn, &QAction::triggered, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_pdfPath));
        accept();
    });
    mainLayout->addWidget(toolbar);

    m_fallbackLabel = new QLabel("QtPdf is not available in this build.\nUse Open External to view the PDF.", this);
    m_fallbackLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_fallbackLabel, 1);
#endif
}

void PdfViewerDialog::loadPdf(const QString& filePath) {
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        QMessageBox::warning(this, "PDF Viewer", "File not found:\n" + filePath);
        reject();
        return;
    }

#ifdef VIOSPICE_HAS_QT_PDF
    if (!m_document) {
        reject();
        return;
    }

    const QPdfDocument::Error err = m_document->load(filePath);
    if (err != QPdfDocument::Error::None) {
        QMessageBox::warning(this, "PDF Viewer", "Failed to load PDF:\n" + filePath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        reject();
        return;
    }

    m_currentPage = 0;
    setWindowTitle("PDF Viewer — " + fi.fileName());
    updatePageInfo();
    updateZoomLabel();
    renderCurrentPage();
#else
    setWindowTitle("PDF Viewer — " + fi.fileName());
#endif
}

void PdfViewerDialog::onZoomIn() {
#ifdef VIOSPICE_HAS_QT_PDF
    m_fitToWindow = false;
    m_zoomFactor = qMin(m_zoomFactor * 1.2, 8.0);
    updateZoomLabel();
    renderCurrentPage();
#endif
}

void PdfViewerDialog::onZoomOut() {
#ifdef VIOSPICE_HAS_QT_PDF
    m_fitToWindow = false;
    m_zoomFactor = qMax(m_zoomFactor / 1.2, 0.1);
    updateZoomLabel();
    renderCurrentPage();
#endif
}

void PdfViewerDialog::onZoomFit() {
#ifdef VIOSPICE_HAS_QT_PDF
    m_fitToWindow = true;
    updateZoomLabel();
    renderCurrentPage();
#endif
}

void PdfViewerDialog::onZoomActualSize() {
#ifdef VIOSPICE_HAS_QT_PDF
    m_fitToWindow = false;
    m_zoomFactor = 1.0;
    updateZoomLabel();
    renderCurrentPage();
#endif
}

void PdfViewerDialog::updatePageInfo() {
#ifdef VIOSPICE_HAS_QT_PDF
    if (m_pageInfoLabel && m_document) {
        const int total = m_document->pageCount();
        const int current = total > 0 ? (m_currentPage + 1) : 0;
        m_pageInfoLabel->setText(QString("Page %1 of %2").arg(current).arg(total));
    }
#endif
}

void PdfViewerDialog::updateZoomLabel() {
#ifdef VIOSPICE_HAS_QT_PDF
    if (!m_zoomLabel) {
        return;
    }
    if (m_fitToWindow) {
        m_zoomLabel->setText("Fit");
    } else {
        m_zoomLabel->setText(QString("%1%").arg(qRound(m_zoomFactor * 100.0)));
    }
#endif
}

void PdfViewerDialog::renderCurrentPage() {
#ifdef VIOSPICE_HAS_QT_PDF
    if (!m_document || !m_imageLabel || !m_scrollArea || m_document->pageCount() <= 0) {
        return;
    }

    const QSizeF pagePoints = m_document->pagePointSize(m_currentPage);
    if (pagePoints.isEmpty()) {
        return;
    }

    QSize targetSize;
    if (m_fitToWindow) {
        const QSize viewportSize = m_scrollArea->viewport()->size() - QSize(16, 16);
        const qreal sx = viewportSize.width() / qMax(1.0, pagePoints.width());
        const qreal sy = viewportSize.height() / qMax(1.0, pagePoints.height());
        const qreal fitScale = qMax(0.1, qMin(sx, sy));
        targetSize = QSize(qMax(1, qRound(pagePoints.width() * fitScale)),
                           qMax(1, qRound(pagePoints.height() * fitScale)));
    } else {
        targetSize = QSize(qMax(1, qRound(pagePoints.width() * m_zoomFactor)),
                           qMax(1, qRound(pagePoints.height() * m_zoomFactor)));
    }

    QPdfDocumentRenderOptions options;
    const QImage image = m_document->render(m_currentPage, targetSize, options);
    if (image.isNull()) {
        return;
    }

    m_imageLabel->setPixmap(QPixmap::fromImage(image));
    m_imageLabel->resize(image.size());
#endif
}

void PdfViewerDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
#ifdef VIOSPICE_HAS_QT_PDF
    if (m_fitToWindow) {
        renderCurrentPage();
    }
#endif
}
