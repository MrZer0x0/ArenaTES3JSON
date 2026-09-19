#pragma once

#include <QString>
#include <QStringList>

namespace arena::tes3json {

enum class TextEncodingMode {
    Windows1251,
    Raw
};

struct ConversionResult {
    QString outputPath;
    QString sidecarPath;
    QString mode;
    QString message;
    QString inputSha256;
    QString outputSha256;
    qint64 inputSize = 0;
    qint64 outputSize = 0;
    bool byteIdenticalToSource = false;
};

class Converter final {
public:
    static ConversionResult pluginToJson(const QString &inputPath,
                                         const QString &outputPath,
                                         bool compact = false,
                                         TextEncodingMode encoding = TextEncodingMode::Windows1251,
                                         bool lossless = true);
    static ConversionResult jsonToPlugin(const QString &inputPath,
                                         const QString &outputPath,
                                         TextEncodingMode encoding = TextEncodingMode::Windows1251,
                                         bool lossless = true);
    static bool verifyRoundTrip(const QString &pluginPath,
                                QString *details = nullptr,
                                TextEncodingMode encoding = TextEncodingMode::Windows1251);
    static QString defaultOutputFor(const QString &inputPath);
    static QString backendPath();

private:
    static ConversionResult runBackend(const QStringList &arguments);
};

} // namespace arena::tes3json
