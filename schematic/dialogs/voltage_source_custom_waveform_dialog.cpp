#include "voltage_source_custom_waveform_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QtMath>
#include <cmath>
#include "../../simulator/core/sim_value_parser.h"

class VoltageSourceCustomWaveformDialog::WaveformDrawWidget : public QWidget {
public:
    explicit WaveformDrawWidget(QWidget* parent = nullptr)
        : QWidget(parent), m_drawing(false), m_snapToGrid(false), m_stepMode(false) {
        setMinimumSize(360, 180);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
    }

    void clearPoints() {
        m_points.clear();
        update();
    }

    void setPoints(const QVector<QPointF>& points) {
        m_points = points;
        update();
    }

    QVector<QPointF> points() const { return m_points; }

    void setSnapToGrid(bool enabled) { m_snapToGrid = enabled; }
    void setStepMode(bool enabled) { m_stepMode = enabled; }
    bool isStepMode() const { return m_stepMode; }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_drawing = true;
            m_points.clear();
            addPoint(event->pos());
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (m_drawing && (event->buttons() & Qt::LeftButton)) {
            addPoint(event->pos());
            event->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_drawing = false;
            addPoint(event->pos());
            event->accept();
        }
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#0f1115"));

        // grid
        p.setPen(QColor(255, 255, 255, 20));
        const int gridX = 20;
        const int gridY = 10;
        for (int i = 1; i < gridX; ++i) {
            int x = rect().left() + i * rect().width() / gridX;
            p.drawLine(x, rect().top(), x, rect().bottom());
        }
        for (int j = 1; j < gridY; ++j) {
            int y = rect().top() + j * rect().height() / gridY;
            p.drawLine(rect().left(), y, rect().right(), y);
        }

        // axes
        p.setPen(QColor(255, 255, 255, 80));
        p.drawLine(rect().left(), rect().center().y(), rect().right(), rect().center().y());

        // waveform
        if (m_points.size() >= 2) {
            QPen pen(QColor("#60a5fa"));
            pen.setWidthF(2.0);
            p.setPen(pen);
            QPainterPath path;
            path.moveTo(toWidget(m_points.first()));
            for (int i = 1; i < m_points.size(); ++i) {
                if (m_stepMode && i > 0) {
                    // Draw step: horizontal then vertical
                    QPointF p1 = toWidget(m_points[i-1]);
                    QPointF p2 = toWidget(m_points[i]);
                    path.lineTo(p2.x(), p1.y());
                    path.lineTo(p2.x(), p2.y());
                } else {
                    path.lineTo(toWidget(m_points[i]));
                }
            }
            p.drawPath(path);
        } else if (m_points.size() == 1) {
            QPen pen(QColor("#60a5fa"));
            pen.setWidthF(2.0);
            p.setPen(pen);
            QPointF wp = toWidget(m_points.first());
            p.drawEllipse(wp, 2.5, 2.5);
        }
        
        // Impossible wave indicator (mouse position if moving backwards)
        if (m_drawing && !m_points.isEmpty()) {
            QPointF n = toNormalized(mapFromGlobal(QCursor::pos()));
            if (n.x() < m_points.last().x()) {
                p.setPen(Qt::red);
                p.drawText(mapFromGlobal(QCursor::pos()) + QPoint(10, -10), "Invalid: Backwards");
            }
        }
    }

private:
    void addPoint(const QPoint& pos) {
        QPointF n = toNormalized(pos);
        
        if (m_snapToGrid) {
            n.setX(qRound(n.x() * 20.0) / 20.0);
            n.setY(qRound(n.y() * 10.0) / 10.0);
        }

        if (!m_points.isEmpty()) {
            QPointF last = m_points.last();
            // STRICT RULE: Monotonicity (No backwards time travel)
            if (n.x() < last.x()) {
                return;
            }
            if (qAbs(last.x() - n.x()) < 0.001 && qAbs(last.y() - n.y()) < 0.001) {
                return;
            }
        }
        m_points.append(n);
        update();
    }

    QPointF toNormalized(const QPoint& pos) const {
        if (width() <= 1 || height() <= 1) return QPointF(0.0, 0.0);
        double x = qBound(0.0, (pos.x() - rect().left()) / double(rect().width()), 1.0);
        double y = qBound(0.0, (pos.y() - rect().top()) / double(rect().height()), 1.0);
        double yn = 1.0 - 2.0 * y;
        return QPointF(x, yn);
    }

    QPointF toWidget(const QPointF& n) const {
        double x = rect().left() + n.x() * rect().width();
        double y = rect().top() + (1.0 - (n.y() + 1.0) * 0.5) * rect().height();
        return QPointF(x, y);
    }

    QVector<QPointF> m_points;
    bool m_drawing;
    bool m_snapToGrid;
    bool m_stepMode;
};

VoltageSourceCustomWaveformDialog::VoltageSourceCustomWaveformDialog(QWidget* parent)
    : QDialog(parent), m_drawWidget(nullptr), m_periodEdit(nullptr), m_amplitudeEdit(nullptr),
      m_offsetEdit(nullptr), m_samplesSpin(nullptr), m_repeatCheck(nullptr),
      m_saveToFileCheck(nullptr), m_filePathEdit(nullptr), m_browseBtn(nullptr),
      m_clearBtn(nullptr), m_repeatEnabled(false), m_saveToFileEnabled(false) {
    setWindowTitle("Custom Waveform (Draw)");
    setupUi();
}

void VoltageSourceCustomWaveformDialog::setDefaultSavePath(const QString& dirPath, const QString& baseName) {
    m_defaultDir = dirPath;
    m_defaultBaseName = baseName;
}

void VoltageSourceCustomWaveformDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    auto* header = new QLabel("Draw one waveform period (left to right) or use tools below.");
    header->setStyleSheet("color: #e5e7eb; font-weight: 600;");
    mainLayout->addWidget(header);

    // Toolbar for advanced tools
    auto* toolLayout = new QHBoxLayout();
    
    auto* sineBtn = new QPushButton("Sine");
    auto* squareBtn = new QPushButton("Square");
    auto* triBtn = new QPushButton("Triangle");
    auto* sawBtn = new QPushButton("Sawtooth");
    
    toolLayout->addWidget(sineBtn);
    toolLayout->addWidget(squareBtn);
    toolLayout->addWidget(triBtn);
    toolLayout->addWidget(sawBtn);
    toolLayout->addSpacing(10);
    
    auto* smoothBtn = new QPushButton("Smooth");
    auto* noiseBtn = new QPushButton("Noise");
    auto* invertBtn = new QPushButton("Invert");
    
    toolLayout->addWidget(smoothBtn);
    toolLayout->addWidget(noiseBtn);
    toolLayout->addWidget(invertBtn);
    toolLayout->addSpacing(20);

    auto* snapCheck = new QCheckBox("Snap");
    auto* stepCheck = new QCheckBox("Step Mode");
    toolLayout->addWidget(snapCheck);
    toolLayout->addWidget(stepCheck);
    toolLayout->addStretch();
    
    mainLayout->addLayout(toolLayout);

    m_drawWidget = new WaveformDrawWidget();
    mainLayout->addWidget(m_drawWidget, 1);

    connect(snapCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_drawWidget) m_drawWidget->setSnapToGrid(checked);
    });
    connect(stepCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_drawWidget) m_drawWidget->setStepMode(checked);
    });

    auto* formulaLayout = new QHBoxLayout();
    formulaLayout->addWidget(new QLabel("Equation f(x):"));
    m_formulaEdit = new QLineEdit();
    m_formulaEdit->setPlaceholderText("e.g. sin(2*pi*x) + 0.5*sin(6*pi*x)");
    formulaLayout->addWidget(m_formulaEdit, 1);
    auto* applyFormulaBtn = new QPushButton("Apply");
    formulaLayout->addWidget(applyFormulaBtn);
    mainLayout->addLayout(formulaLayout);

    auto* controlLayout = new QGridLayout();
    controlLayout->addWidget(new QLabel("Period [s]:"), 0, 0);
    m_periodEdit = new QLineEdit("1m");
    controlLayout->addWidget(m_periodEdit, 0, 1);
    controlLayout->addWidget(new QLabel("Amplitude [V]:"), 0, 2);
    m_amplitudeEdit = new QLineEdit("1");
    controlLayout->addWidget(m_amplitudeEdit, 0, 3);
    controlLayout->addWidget(new QLabel("Offset [V]:"), 0, 4);
    m_offsetEdit = new QLineEdit("0");
    controlLayout->addWidget(m_offsetEdit, 0, 5);
    controlLayout->addWidget(new QLabel("Samples:"), 0, 6);
    m_samplesSpin = new QSpinBox();
    m_samplesSpin->setRange(8, 512);
    m_samplesSpin->setValue(64);
    controlLayout->addWidget(m_samplesSpin, 0, 7);
    m_repeatCheck = new QCheckBox("Repeat (PWL r=0)");
    m_repeatCheck->setChecked(false);
    controlLayout->addWidget(m_repeatCheck, 1, 0, 1, 3);

    m_saveToFileCheck = new QCheckBox("Save PWL to file");
    m_saveToFileCheck->setChecked(false);
    controlLayout->addWidget(m_saveToFileCheck, 1, 3, 1, 2);
    m_filePathEdit = new QLineEdit();
    m_filePathEdit->setPlaceholderText("waveform.pwl");
    m_filePathEdit->setEnabled(false);
    controlLayout->addWidget(m_filePathEdit, 1, 5, 1, 2);
    m_browseBtn = new QPushButton("Browse");
    m_browseBtn->setEnabled(false);
    controlLayout->addWidget(m_browseBtn, 1, 7, 1, 1);
    mainLayout->addLayout(controlLayout);

    auto* btnLayout = new QHBoxLayout();
    m_clearBtn = new QPushButton("Clear");
    auto* cancelBtn = new QPushButton("Cancel");
    auto* okBtn = new QPushButton("OK");
    btnLayout->addWidget(m_clearBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(okBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_clearBtn, &QPushButton::clicked, this, &VoltageSourceCustomWaveformDialog::onClear);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, &VoltageSourceCustomWaveformDialog::onAccepted);
    connect(m_saveToFileCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_filePathEdit) m_filePathEdit->setEnabled(checked);
        if (m_browseBtn) m_browseBtn->setEnabled(checked);
    });
    connect(m_browseBtn, &QPushButton::clicked, this, [this]() {
        QString suggested = m_defaultDir.isEmpty() ? QString() : (m_defaultDir + "/" + (m_defaultBaseName.isEmpty() ? "waveform.pwl" : m_defaultBaseName));
        QString path = QFileDialog::getSaveFileName(this, "Save PWL File", suggested, "PWL Files (*.pwl *.txt *.csv *.dat);;All Files (*)");
        if (!path.isEmpty() && m_filePathEdit) m_filePathEdit->setText(path);
    });

    connect(sineBtn, &QPushButton::clicked, this, &VoltageSourceCustomWaveformDialog::onApplySine);
    connect(squareBtn, &QPushButton::clicked, this, &VoltageSourceCustomWaveformDialog::onApplySquare);
    connect(triBtn, &QPushButton::clicked, this, &VoltageSourceCustomWaveformDialog::onApplyTriangle);
    connect(sawBtn, &QPushButton::clicked, this, &VoltageSourceCustomWaveformDialog::onApplySawtooth);
    connect(smoothBtn, &QPushButton::clicked, this, &VoltageSourceCustomWaveformDialog::onApplySmooth);
    connect(noiseBtn, &QPushButton::clicked, this, &VoltageSourceCustomWaveformDialog::onApplyNoise);
    connect(invertBtn, &QPushButton::clicked, this, &VoltageSourceCustomWaveformDialog::onApplyInvert);
    connect(applyFormulaBtn, &QPushButton::clicked, this, &VoltageSourceCustomWaveformDialog::onApplyFormula);
}

void VoltageSourceCustomWaveformDialog::onApplySine() {
    QVector<QPointF> points;
    const int count = 100;
    for (int i = 0; i <= count; ++i) {
        double x = double(i) / count;
        points.append(QPointF(x, std::sin(2.0 * M_PI * x)));
    }
    m_drawWidget->setPoints(points);
}

void VoltageSourceCustomWaveformDialog::onApplySquare() {
    QVector<QPointF> points;
    points.append(QPointF(0, 1));
    points.append(QPointF(0.499, 1));
    points.append(QPointF(0.5, -1));
    points.append(QPointF(0.999, -1));
    points.append(QPointF(1.0, 1));
    m_drawWidget->setPoints(points);
}

void VoltageSourceCustomWaveformDialog::onApplyTriangle() {
    QVector<QPointF> points;
    points.append(QPointF(0, 0));
    points.append(QPointF(0.25, 1));
    points.append(QPointF(0.75, -1));
    points.append(QPointF(1.0, 0));
    m_drawWidget->setPoints(points);
}

void VoltageSourceCustomWaveformDialog::onApplySawtooth() {
    QVector<QPointF> points;
    points.append(QPointF(0, -1));
    points.append(QPointF(0.999, 1));
    points.append(QPointF(1.0, -1));
    m_drawWidget->setPoints(points);
}

void VoltageSourceCustomWaveformDialog::onApplySmooth() {
    QVector<QPointF> points = m_drawWidget->points();
    if (points.size() < 3) return;
    QVector<QPointF> next;
    next.append(points.first());
    for (int i = 1; i < points.size() - 1; ++i) {
        double avgY = (points[i-1].y() + points[i].y() + points[i+1].y()) / 3.0;
        next.append(QPointF(points[i].x(), avgY));
    }
    next.append(points.last());
    m_drawWidget->setPoints(next);
}

void VoltageSourceCustomWaveformDialog::onApplyNoise() {
    QVector<QPointF> points = m_drawWidget->points();
    for (auto& p : points) {
        p.setY(p.y() + (double(std::rand()) / RAND_MAX - 0.5) * 0.1);
    }
    m_drawWidget->setPoints(points);
}

void VoltageSourceCustomWaveformDialog::onApplyInvert() {
    QVector<QPointF> points = m_drawWidget->points();
    for (auto& p : points) {
        p.setY(-p.y());
    }
    m_drawWidget->setPoints(points);
}

void VoltageSourceCustomWaveformDialog::onApplyFormula() {
    QString formula = m_formulaEdit->text().trimmed();
    if (formula.isEmpty()) return;
    QVector<QPointF> points;
    const int count = 200;
    for (int i = 0; i <= count; ++i) {
        double x = double(i) / count;
        points.append(QPointF(x, evaluateFormula(formula, x)));
    }
    m_drawWidget->setPoints(points);
}

double VoltageSourceCustomWaveformDialog::evaluateFormula(const QString& formula, double x) {
    // Basic expression evaluation.
    // Replace 'x' with current value and 'pi' with M_PI
    QString expr = formula.toLower();
    expr.replace("pi", QString::number(M_PI, 'g', 15));
    
    // For simplicity, we just handle a few common functions via string replacement 
    // and then use a very simple evaluator or just handle the common ones here.
    // In a real application, we'd use a math library.
    
    // Since we don't have a full evaluator, let's at least support basic sin/cos/x.
    // If it's just "sin(2*pi*x)", we can do it.
    
    // Actually, I'll implement a tiny recursive descent parser for: +, -, *, /, (), x, sin, cos, exp, log, sqrt
    
    struct Parser {
        QString s;
        int pos = 0;
        double x_val;

        double parseExpression() {
            double res = parseTerm();
            while (pos < s.length()) {
                if (s[pos] == '+') { pos++; res += parseTerm(); }
                else if (s[pos] == '-') { pos++; res -= parseTerm(); }
                else break;
            }
            return res;
        }

        double parseTerm() {
            double res = parseFactor();
            while (pos < s.length()) {
                if (s[pos] == '*') { pos++; res *= parseFactor(); }
                else if (s[pos] == '/') { 
                    pos++; 
                    double d = parseFactor();
                    res = (d != 0) ? res / d : 0;
                }
                else break;
            }
            return res;
        }

        double parseFactor() {
            if (pos >= s.length()) return 0;
            if (s[pos] == '(') {
                pos++;
                double res = parseExpression();
                if (pos < s.length() && s[pos] == ')') pos++;
                return res;
            }
            if (s[pos] == '-') { pos++; return -parseFactor(); }
            
            if (s[pos].isDigit() || s[pos] == '.') {
                int start = pos;
                while (pos < s.length() && (s[pos].isDigit() || s[pos] == '.' || s[pos] == 'e')) pos++;
                return s.mid(start, pos - start).toDouble();
            }

            if (s.mid(pos).startsWith("x")) { pos++; return x_val; }
            if (s.mid(pos).startsWith("sin(")) { pos += 4; double v = std::sin(parseExpression()); if (pos < s.length() && s[pos] == ')') pos++; return v; }
            if (s.mid(pos).startsWith("cos(")) { pos += 4; double v = std::cos(parseExpression()); if (pos < s.length() && s[pos] == ')') pos++; return v; }
            if (s.mid(pos).startsWith("exp(")) { pos += 4; double v = std::exp(parseExpression()); if (pos < s.length() && s[pos] == ')') pos++; return v; }
            if (s.mid(pos).startsWith("log(")) { pos += 4; double v = std::log(parseExpression()); if (pos < s.length() && s[pos] == ')') pos++; return v; }
            if (s.mid(pos).startsWith("sqrt(")) { pos += 5; double v = std::sqrt(parseExpression()); if (pos < s.length() && s[pos] == ')') pos++; return v; }
            if (s.mid(pos).startsWith("tan(")) { pos += 4; double v = std::tan(parseExpression()); if (pos < s.length() && s[pos] == ')') pos++; return v; }

            pos++;
            return 0;
        }
    };

    Parser p;
    p.s = expr.replace(" ", "");
    p.x_val = x;
    return p.parseExpression();
}

void VoltageSourceCustomWaveformDialog::onClear() {
    if (m_drawWidget) m_drawWidget->clearPoints();
}

void VoltageSourceCustomWaveformDialog::onAccepted() {
    m_pwlPoints = buildPwlPoints();
    m_repeatEnabled = m_repeatCheck && m_repeatCheck->isChecked();
    m_saveToFileEnabled = m_saveToFileCheck && m_saveToFileCheck->isChecked();

    if (m_saveToFileEnabled) {
        m_pwlFilePath = m_filePathEdit ? m_filePathEdit->text().trimmed() : QString();
        if (m_pwlFilePath.isEmpty() && !m_defaultDir.isEmpty()) {
            const QString base = m_defaultBaseName.isEmpty() ? "waveform.pwl" : m_defaultBaseName;
            m_pwlFilePath = m_defaultDir + "/" + base;
        }
        if (m_pwlFilePath.isEmpty()) {
            QString suggested = m_defaultDir.isEmpty() ? QString() : (m_defaultDir + "/" + (m_defaultBaseName.isEmpty() ? "waveform.pwl" : m_defaultBaseName));
            QString path = QFileDialog::getSaveFileName(this, "Save PWL File", suggested, "PWL Files (*.pwl *.txt *.csv *.dat);;All Files (*)");
            if (!path.isEmpty()) m_pwlFilePath = path;
        }

        if (m_pwlFilePath.isEmpty()) {
            QMessageBox::warning(this, "Save PWL", "Please choose a file path for the PWL data.");
            return;
        }

        QFile file(m_pwlFilePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(this, "Save Error", QString("Failed to save PWL file:\n%1").arg(file.errorString()));
            return;
        }

        QTextStream out(&file);
        QStringList tokens = m_pwlPoints.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        for (int i = 0; i + 1 < tokens.size(); i += 2) {
            out << tokens[i] << " " << tokens[i + 1] << "\n";
        }
        file.close();
    }

    accept();
}

static double parseOrDefault(const QString& text, double fallback) {
    double v = 0.0;
    if (SimValueParser::parseSpiceNumber(text.trimmed(), v)) return v;
    return fallback;
}

QString VoltageSourceCustomWaveformDialog::buildPwlPoints() const {
    QVector<QPointF> points = m_drawWidget ? m_drawWidget->points() : QVector<QPointF>();
    if (points.isEmpty()) {
        return "0 0 1 0";
    }

    std::sort(points.begin(), points.end(), [](const QPointF& a, const QPointF& b) {
        return a.x() < b.x();
    });

    if (points.first().x() > 0.0) {
        points.prepend(QPointF(0.0, points.first().y()));
    }
    if (points.last().x() < 1.0) {
        points.append(QPointF(1.0, points.last().y()));
    }

    const int samples = m_samplesSpin ? m_samplesSpin->value() : 64;
    const double period = qMax(1e-12, parseOrDefault(m_periodEdit ? m_periodEdit->text() : "1", 1.0));
    const double amplitude = parseOrDefault(m_amplitudeEdit ? m_amplitudeEdit->text() : "1", 1.0);
    const double offset = parseOrDefault(m_offsetEdit ? m_offsetEdit->text() : "0", 0.0);

    auto sampleY = [&](double x) -> double {
        for (int i = 1; i < points.size(); ++i) {
            if (x <= points[i].x()) {
                const QPointF a = points[i - 1];
                const QPointF b = points[i];
                if (m_drawWidget && m_drawWidget->isStepMode()) {
                    return a.y(); // Constant until next step (Zero-order hold)
                }
                double t = (b.x() - a.x()) <= 0.0 ? 0.0 : (x - a.x()) / (b.x() - a.x());
                return a.y() + t * (b.y() - a.y());
            }
        }
        return points.last().y();
    };

    QStringList tokens;
    tokens.reserve(samples * 2);
    for (int i = 0; i < samples; ++i) {
        double x = (samples == 1) ? 0.0 : double(i) / double(samples - 1);
        double t = x * period;
        double y = offset + amplitude * sampleY(x);
        tokens << QString::number(t, 'g', 12) << QString::number(y, 'g', 12);
    }

    return tokens.join(' ');
}
