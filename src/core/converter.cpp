#include "converter.h"
#include "localization.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <stdexcept>

namespace arena::tes3json {
namespace {

[[noreturn]] void fail(const QString &message)
{
    throw std::runtime_error(message.toUtf8().constData());
}

QString detectJsonFileType(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QStringLiteral("esp");
    const QByteArray prefix = file.read(256 * 1024);
    const qsizetype marker = prefix.indexOf("\"file_type\"");
    if (marker < 0) return QStringLiteral("esp");
    const QByteArray nearby = prefix.mid(marker, 256).toLower();
    return nearby.contains("\"esm\"") ? QStringLiteral("esm") : QStringLiteral("esp");
}

} // namespace

QString Converter::backendPath()
{
    const QString besideApp = QDir(QCoreApplication::applicationDirPath())
                                  .filePath(QStringLiteral(ARENATES3JSON_RUST_CORE_NAME));
    if (QFileInfo::exists(besideApp)) return besideApp;

    const QString cwd = QDir::current().filePath(QStringLiteral(ARENATES3JSON_RUST_CORE_NAME));
    if (QFileInfo::exists(cwd)) return cwd;

    return besideApp;
}

QString Converter::defaultOutputFor(const QString &inputPath)
{
    const QFileInfo info(inputPath);
    const QString ext = info.suffix().toLower();
    if (ext == QStringLiteral("esm") || ext == QStringLiteral("esp")) {
        return info.dir().filePath(info.completeBaseName() + QStringLiteral(".json"));
    }
    if (ext == QStringLiteral("json")) {
        return info.dir().filePath(info.completeBaseName() + QLatin1Char('.') + detectJsonFileType(inputPath));
    }
    return info.dir().filePath(info.completeBaseName() + QStringLiteral(".json"));
}

ConversionResult Converter::parseBackendStatus(const QByteArray &stdoutData)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(stdoutData.trimmed(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        fail(l10n(systemUiLanguage(),
                  "Backend returned invalid status JSON: %1\n%2",
                  "Backend вернул некорректный статус JSON: %1\n%2")
             .arg(parseError.errorString(), QString::fromUtf8(stdoutData)));
    }

    const QJsonObject obj = doc.object();
    ConversionResult result;
    result.outputPath = obj.value(QStringLiteral("output")).toString();
    result.mode = obj.value(QStringLiteral("mode")).toString();
    result.message = obj.value(QStringLiteral("message")).toString();
    result.encoding = obj.value(QStringLiteral("encoding")).toString();
    result.repairedScripts = static_cast<qint64>(obj.value(QStringLiteral("repaired_scripts")).toDouble());
    result.inputSize = static_cast<qint64>(obj.value(QStringLiteral("input_size")).toDouble());
    result.outputSize = static_cast<qint64>(obj.value(QStringLiteral("output_size")).toDouble());
    return result;
}

QByteArray Converter::runBackendRaw(const QStringList &arguments)
{
    QProcess process;
    process.setProgram(backendPath());
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForStarted()) {
        fail(l10n(systemUiLanguage(),
                  "Could not start backend: %1\n%2",
                  "Не удалось запустить backend: %1\n%2")
             .arg(backendPath(), process.errorString()));
    }
    if (!process.waitForFinished(-1)) {
        process.kill();
        fail(l10n(systemUiLanguage(),
                  "Backend did not finish the operation: %1",
                  "Backend не завершил операцию: %1")
             .arg(process.errorString()));
    }

    const QByteArray stdoutData = process.readAllStandardOutput().trimmed();
    const QByteArray stderrData = process.readAllStandardError().trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QString error = QString::fromUtf8(stderrData);
        if (error.isEmpty()) error = QString::fromUtf8(stdoutData);
        fail(error.isEmpty()
                 ? l10n(systemUiLanguage(),
                        "ArenaTES3JSON-core backend error",
                        "Ошибка backend ArenaTES3JSON-core")
                 : error);
    }
    return stdoutData;
}

ConversionResult Converter::runBackend(const QStringList &arguments)
{
    return parseBackendStatus(runBackendRaw(arguments));
}

InspectionResult Converter::parseInspectionStatus(const QByteArray &stdoutData)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(stdoutData.trimmed(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        fail(l10n(systemUiLanguage(),
                  "Backend returned invalid inspection JSON: %1\n%2",
                  "Backend вернул некорректный JSON проверки: %1\n%2")
             .arg(parseError.errorString(), QString::fromUtf8(stdoutData)));
    }

    const QJsonObject obj = doc.object();
    InspectionResult result;
    result.kind = obj.value(QStringLiteral("kind")).toString();
    result.encoding = obj.value(QStringLiteral("encoding")).toString();
    result.size = static_cast<qint64>(obj.value(QStringLiteral("size")).toDouble());
    result.objects = static_cast<qint64>(obj.value(QStringLiteral("objects")).toDouble());
    result.fileMtimeUtc = obj.value(QStringLiteral("file_mtime_utc")).toString();
    result.pluginType = obj.value(QStringLiteral("plugin_type")).toString();
    return result;
}

InspectionResult Converter::inspect(const QString &inputPath, const QString &encoding)
{
    return parseInspectionStatus(runBackendRaw({QStringLiteral("inspect"), inputPath,
                                                QStringLiteral("--encoding"), encoding}));
}

ConversionResult Converter::pluginToJson(const QString &inputPath,
                                         const QString &outputPath,
                                         bool compact,
                                         const QString &encoding)
{
    QStringList args{QStringLiteral("to-json"), inputPath, outputPath,
                     QStringLiteral("--encoding"), encoding};
    if (compact) args << QStringLiteral("--compact");
    return runBackend(args);
}

ConversionResult Converter::jsonToPlugin(const QString &inputPath,
                                         const QString &outputPath,
                                         const QString &encoding,
                                         bool repairChangedScripts,
                                         const QString &fileDate,
                                         const QString &fileType)
{
    QStringList args{QStringLiteral("to-plugin"), inputPath, outputPath,
                     QStringLiteral("--encoding"), encoding,
                     QStringLiteral("--file-date"), fileDate,
                     QStringLiteral("--file-type"), fileType};
    if (repairChangedScripts)
        args << QStringLiteral("--repair-scripts") << QStringLiteral("changed");
    return runBackend(args);
}

} // namespace arena::tes3json
