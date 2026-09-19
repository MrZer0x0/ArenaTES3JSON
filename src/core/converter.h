#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace arena::tes3json {

struct ConversionResult {
    QString outputPath;
    QString mode;
    QString message;
    QString encoding;
    qint64 inputSize = 0;
    qint64 outputSize = 0;
    qint64 repairedScripts = 0;
};

struct InspectionResult {
    QString kind;
    QString encoding;
    qint64 size = 0;
    qint64 objects = 0;
};

class Converter final {
public:
    static ConversionResult pluginToJson(const QString &inputPath,
                                         const QString &outputPath,
                                         bool compact = false,
                                         const QString &encoding = QStringLiteral("auto"));
    static ConversionResult jsonToPlugin(const QString &inputPath,
                                         const QString &outputPath,
                                         const QString &encoding = QStringLiteral("auto"),
                                         bool repairChangedScripts = false);
    static InspectionResult inspect(const QString &inputPath,
                                    const QString &encoding = QStringLiteral("auto"));
    static QString defaultOutputFor(const QString &inputPath);
    static QString backendPath();
    static ConversionResult parseBackendStatus(const QByteArray &stdoutData);
    static InspectionResult parseInspectionStatus(const QByteArray &stdoutData);

private:
    static QByteArray runBackendRaw(const QStringList &arguments);
    static ConversionResult runBackend(const QStringList &arguments);
};

} // namespace arena::tes3json
