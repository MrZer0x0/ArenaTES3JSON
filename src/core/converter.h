#pragma once

#include <QString>

namespace arena::tes3json {

struct ConversionResult {
    QString outputPath;
    QString inputSha256;
    QString outputSha256;
    qsizetype inputSize = 0;
    qsizetype outputSize = 0;
    bool byteIdenticalToSource = false;
};

class Converter final {
public:
    static ConversionResult pluginToJson(const QString &inputPath,
                                         const QString &outputPath,
                                         bool compact = false);
    static ConversionResult jsonToPlugin(const QString &inputPath,
                                         const QString &outputPath = {});
    static bool verifyRoundTrip(const QString &pluginPath, QString *details = nullptr);
    static QString defaultOutputFor(const QString &inputPath);
};

} // namespace arena::tes3json
