#include "converter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <stdexcept>

#ifndef ARENATES3JSON_RUST_CORE_NAME
#  ifdef Q_OS_WIN
#    define ARENATES3JSON_RUST_CORE_NAME "ArenaTES3JSON-core.exe"
#  else
#    define ARENATES3JSON_RUST_CORE_NAME "ArenaTES3JSON-core"
#  endif
#endif

namespace arena::tes3json {
namespace {

[[noreturn]] void fail(const QString &message)
{
    throw std::runtime_error(message.toUtf8().constData());
}

QString encodingArgument(TextEncodingMode mode)
{
    return mode == TextEncodingMode::Windows1251 ? QStringLiteral("cp1251") : QStringLiteral("raw");
}

QString detectJsonFileType(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QStringLiteral("esp");
    const QByteArray prefix = file.read(128 * 1024);
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

ConversionResult Converter::runBackend(const QStringList &arguments)
{
    QProcess process;
    process.setProgram(backendPath());
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForStarted()) {
        fail(QStringLiteral("Не удалось запустить backend: %1\n%2")
             .arg(backendPath(), process.errorString()));
    }
    if (!process.waitForFinished(-1)) {
        process.kill();
        fail(QStringLiteral("Backend не завершил операцию: %1").arg(process.errorString()));
    }

    const QByteArray stdoutData = process.readAllStandardOutput().trimmed();
    const QByteArray stderrData = process.readAllStandardError().trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QString error = QString::fromUtf8(stderrData);
        if (error.isEmpty()) error = QString::fromUtf8(stdoutData);
        fail(error.isEmpty() ? QStringLiteral("Ошибка backend ArenaTES3JSON-core") : error);
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(stdoutData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        fail(QStringLiteral("Backend вернул некорректный статус JSON: %1\n%2")
             .arg(parseError.errorString(), QString::fromUtf8(stdoutData)));
    }

    const QJsonObject obj = doc.object();
    ConversionResult result;
    result.outputPath = obj.value(QStringLiteral("output")).toString();
    result.sidecarPath = obj.value(QStringLiteral("sidecar")).toString();
    result.mode = obj.value(QStringLiteral("mode")).toString();
    result.message = obj.value(QStringLiteral("message")).toString();
    result.inputSha256 = obj.value(QStringLiteral("source_sha256")).toString();
    result.outputSha256 = obj.value(QStringLiteral("output_sha256")).toString();
    result.inputSize = static_cast<qint64>(obj.value(QStringLiteral("input_size")).toDouble());
    result.outputSize = static_cast<qint64>(obj.value(QStringLiteral("output_size")).toDouble());
    result.byteIdenticalToSource = obj.value(QStringLiteral("byte_identical")).toBool(false);
    return result;
}

ConversionResult Converter::pluginToJson(const QString &inputPath,
                                         const QString &outputPath,
                                         bool compact,
                                         TextEncodingMode encoding,
                                         bool lossless)
{
    QStringList args{QStringLiteral("to-json"), inputPath, outputPath,
                     QStringLiteral("--encoding"), encodingArgument(encoding)};
    if (compact) args << QStringLiteral("--compact");
    if (!lossless) args << QStringLiteral("--no-lossless");
    return runBackend(args);
}

ConversionResult Converter::jsonToPlugin(const QString &inputPath,
                                         const QString &outputPath,
                                         TextEncodingMode encoding,
                                         bool lossless)
{
    QStringList args{QStringLiteral("to-plugin"), inputPath, outputPath,
                     QStringLiteral("--encoding"), encodingArgument(encoding)};
    if (!lossless) args << QStringLiteral("--no-lossless");
    return runBackend(args);
}

bool Converter::verifyRoundTrip(const QString &pluginPath, QString *details, TextEncodingMode encoding)
{
    try {
        const ConversionResult result = runBackend({QStringLiteral("verify"), pluginPath,
                                                    QStringLiteral("--encoding"), encodingArgument(encoding)});
        if (details) {
            *details = QStringLiteral("%1\nРазмер: %2 байт\nSHA-256: %3")
                           .arg(result.message)
                           .arg(result.outputSize)
                           .arg(result.outputSha256);
        }
        return result.byteIdenticalToSource;
    } catch (const std::exception &error) {
        if (details) *details = QString::fromUtf8(error.what());
        return false;
    }
}

} // namespace arena::tes3json
