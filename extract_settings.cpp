#include <QSettings>
#include <QByteArray>
#include <QDebug>
#include <iostream>

int main(int argc, char *argv[]) {
    QSettings settings("viospice", "Settings");
    QStringList keys = settings.allKeys();
    for (const QString& key : keys) {
        if (key.startsWith("ui/") && (key.endsWith("/geometry") || key.endsWith("/state"))) {
            QByteArray val = settings.value(key).toByteArray();
            std::cout << key.toStdString() << " = " << val.toHex().toStdString() << std::endl;
        }
    }
    return 0;
}
