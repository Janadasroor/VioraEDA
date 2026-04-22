#include "sim_manager.h"
// ... (rest of imports)

QString normalizeFluxSmartBlockSource(QString source, const QStringList& inputPins) {
    // Force the JIT function to use raw vector indexing 'vec[i]'
    // instead of the high-level 'inputs["pin"]' dictionary.
    // This matches the ABI we use in cfunc.mod.
    for (int i = 0; i < inputPins.size(); ++i) {
        const QString pin = inputPins.at(i).trimmed();
        if (pin.isEmpty()) continue;

        const QString escapedPin = QRegularExpression::escape(pin);
        const QString replacement = QString("vec[%1]").arg(i);

        source.replace(QRegularExpression(QString(R"(V\s*\(\s*\"%1\"\s*\))").arg(escapedPin), QRegularExpression::CaseInsensitiveOption), replacement);
        source.replace(QRegularExpression(QString(R"(inputs\s*\[\s*\"%1\"\s*\])").arg(escapedPin), QRegularExpression::CaseInsensitiveOption), replacement);
    }
    
    // Rewrite the function signature to use the raw vector name 'vec'
    source.replace("def update(t, inputs)", "def update(t, vec)");
    return source;
}
