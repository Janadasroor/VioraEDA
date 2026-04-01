#ifndef SCHEMATIC_URL_ENCODER_H
#define SCHEMATIC_URL_ENCODER_H

#include <QString>
#include <QByteArray>
#include <QJsonObject>

class SchematicUrlEncoder {
public:
    static QByteArray serializeToCompact(const QString& schematicPath);
    
    static QString encodeForUrl(const QByteArray& data);
    
    static QByteArray decodeFromUrl(const QString& urlData);
    
    static QByteArray decompress(const QByteArray& data);
    
    static bool fitsInUrl(const QByteArray& data);
    
    static bool fitsInUrl(const QString& schematicPath);
    
    static QString compressAndEncode(const QString& schematicPath);
    
    static QJsonObject decodeFromShareUrl(const QString& urlData);
    
    static constexpr int MAX_URL_LENGTH = 8000;
    static constexpr int SAFE_URL_LENGTH = 2000;
};

#endif // SCHEMATIC_URL_ENCODER_H