#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace arena::tes3json {

enum class TextEncodingMode {
    Windows1251,
    Raw
};

struct ConversionResult {
    QString outputPath;
    QString mode;
    QString message;
    qint64 inputSize = 0;
    qint64 outputSize = 0;
};

class Converter final {
public:
    static ConversionResult pluginToJson(const QString &inputPath,
                                         const QString &outputPath,
                                         bool compact = false,
                                         TextEncodingMode encoding = TextEncodingMode::Windows1251);
    static ConversionResult jsonToPlugin(const QString &inputPath,
                                         const QString &outputPath,
                                         TextEncodingMode encoding = TextEncodingMode::Windows1251);
    static QString defaultOutputFor(const QString &inputPath);
    static QString backendPath();
    static ConversionResult parseBackendStatus(const QByteArray &stdoutData);

private:
    static ConversionResult runBackend(const QStringList &arguments);
};

} // namespace arena::tes3json
