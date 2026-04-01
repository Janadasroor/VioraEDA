#include "schematic_url_encoder.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QBuffer>
#include <QByteArray>
#include <zlib.h>

QByteArray SchematicUrlEncoder::serializeToCompact(const QString& schematicPath) {
    QFile file(schematicPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    
    QByteArray jsonData = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);
    if (error.error != QJsonParseError::NoError) {
        return QByteArray();
    }
    
    QJsonObject root = doc.object();
    
    QJsonObject metadata = root.value("metadata").toObject();
    metadata["sharedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["metadata"] = metadata;
    
    QJsonDocument compactDoc(root);
    return compactDoc.toJson(QJsonDocument::Compact);
}

QString SchematicUrlEncoder::encodeForUrl(const QByteArray& data) {
    QByteArray compressed = qCompress(data, 9);
    return QString::fromLatin1(compressed.toBase64());
}

QByteArray SchematicUrlEncoder::decodeFromUrl(const QString& urlData) {
    QByteArray base64Data = urlData.toLatin1();
    QByteArray compressed = QByteArray::fromBase64(base64Data);
    return qUncompress(compressed);
}

QByteArray SchematicUrlEncoder::decompress(const QByteArray& data) {
    return qUncompress(data);
}

bool SchematicUrlEncoder::fitsInUrl(const QByteArray& data) {
    QString encoded = encodeForUrl(data);
    return encoded.length() <= MAX_URL_LENGTH;
}

bool SchematicUrlEncoder::fitsInUrl(const QString& schematicPath) {
    QByteArray data = serializeToCompact(schematicPath);
    if (data.isEmpty()) return false;
    return fitsInUrl(data);
}

QString SchematicUrlEncoder::compressAndEncode(const QString& schematicPath) {
    QByteArray data = serializeToCompact(schematicPath);
    if (data.isEmpty()) return QString();
    return encodeForUrl(data);
}

QJsonObject SchematicUrlEncoder::decodeFromShareUrl(const QString& urlData) {
    QByteArray decoded = decodeFromUrl(urlData);
    if (decoded.isEmpty()) return QJsonObject();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(decoded, &error);
    if (error.error != QJsonParseError::NoError) {
        return QJsonObject();
    }
    
    return doc.object();
}