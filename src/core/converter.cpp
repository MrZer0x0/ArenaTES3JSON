#include "converter.h"
#include "tes3plugin.h"

#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

#include <stdexcept>

namespace arena::tes3json {
namespace {

[[noreturn]] void fail(const QString &message) { throw std::runtime_error(message.toStdString()); }

QByteArray readAll(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) fail(QStringLiteral("Cannot open %1: %2").arg(path, f.errorString()));
    return f.readAll();
}

void writeAll(const QString &path, QByteArrayView bytes)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) fail(QStringLiteral("Cannot write %1: %2").arg(path, f.errorString()));
    if (f.write(bytes.data(), bytes.size()) != bytes.size()) fail(QStringLiteral("Short write: %1").arg(path));
    if (!f.commit()) fail(QStringLiteral("Cannot commit %1: %2").arg(path, f.errorString()));
}

QString sha(QByteArrayView bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes.toByteArray(), QCryptographicHash::Sha256).toHex());
}

} // namespace

QString Converter::defaultOutputFor(const QString &inputPath)
{
    const QFileInfo fi(inputPath);
    const QString ext = fi.suffix().toLower();
    if (ext == QStringLiteral("esp") || ext == QStringLiteral("esm")) {
        return fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".json"));
    }
    if (ext == QStringLiteral("json") && fi.exists()) {
        QFile f(inputPath);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &error);
            if (error.error == QJsonParseError::NoError && doc.isObject()) {
                const QString sourceExt = doc.object().value(QStringLiteral("source")).toObject()
                                              .value(QStringLiteral("extension")).toString().toLower();
                if (sourceExt == QStringLiteral("esm") || sourceExt == QStringLiteral("esp")) {
                    return fi.dir().filePath(fi.completeBaseName() + QLatin1Char('.') + sourceExt);
                }
            }
        }
    }
    return fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".esp"));
}

ConversionResult Converter::pluginToJson(const QString &inputPath, const QString &outputPath, bool compact)
{
    const QByteArray input = readAll(inputPath);
    const QFileInfo fi(inputPath);
    Tes3Plugin plugin = Tes3Plugin::parse(input, fi.fileName(), fi.suffix());
    const QJsonDocument doc(plugin.toJson());
    const QByteArray json = doc.toJson(compact ? QJsonDocument::Compact : QJsonDocument::Indented);
    writeAll(outputPath, json);

    ConversionResult result;
    result.outputPath = outputPath;
    result.inputSha256 = sha(input);
    result.outputSha256 = sha(json);
    result.inputSize = input.size();
    result.outputSize = json.size();
    return result;
}

ConversionResult Converter::jsonToPlugin(const QString &inputPath, const QString &outputPathArg)
{
    const QByteArray json = readAll(inputPath);
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        fail(QStringLiteral("JSON parse error at %1: %2").arg(parseError.offset).arg(parseError.errorString()));
    }

    Tes3Plugin plugin = Tes3Plugin::fromJson(doc.object());
    QString outputPath = outputPathArg;
    if (outputPath.isEmpty()) {
        const QFileInfo fi(inputPath);
        QString ext = plugin.sourceExtension;
        if (ext != QStringLiteral("esm") && ext != QStringLiteral("esp")) ext = QStringLiteral("esp");
        outputPath = fi.dir().filePath(fi.completeBaseName() + QLatin1Char('.') + ext);
    }

    const QByteArray output = plugin.build();
    writeAll(outputPath, output);

    ConversionResult result;
    result.outputPath = outputPath;
    result.inputSha256 = sha(json);
    result.outputSha256 = sha(output);
    result.inputSize = json.size();
    result.outputSize = output.size();
    if (!plugin.sourceSha256.isEmpty()) {
        result.byteIdenticalToSource = (plugin.sourceSha256 == QCryptographicHash::hash(output, QCryptographicHash::Sha256));
    }
    return result;
}

bool Converter::verifyRoundTrip(const QString &pluginPath, QString *details)
{
    try {
        const QByteArray input = readAll(pluginPath);
        const QFileInfo fi(pluginPath);
        const Tes3Plugin first = Tes3Plugin::parse(input, fi.fileName(), fi.suffix());
        const QJsonDocument json(first.toJson());
        const Tes3Plugin second = Tes3Plugin::fromJson(json.object());
        const QByteArray output = second.build();
        const bool same = (input == output);
        if (details) {
            *details = same
                ? QStringLiteral("OK: byte-for-byte identical, %1 bytes, SHA-256 %2")
                      .arg(input.size()).arg(sha(input))
                : QStringLiteral("FAILED: source %1 bytes / rebuilt %2 bytes, SHA-256 %3 / %4")
                      .arg(input.size()).arg(output.size()).arg(sha(input), sha(output));
        }
        return same;
    } catch (const std::exception &e) {
        if (details) *details = QString::fromUtf8(e.what());
        return false;
    }
}

} // namespace arena::tes3json
