#include "core/converter.h"
#include "core/localization.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>

#include <exception>

using arena::tes3json::Converter;
using arena::tes3json::UiLanguage;
using arena::tes3json::l10n;
using arena::tes3json::systemUiLanguage;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ArenaTES3JSON-cli"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.4.1"));

    const UiLanguage language = systemUiLanguage();

    QCommandLineParser parser;
    parser.setApplicationDescription(l10n(
        language,
        "TES3 ESM/ESP <-> tes3conv-compatible semantic JSON converter",
        "Конвертер TES3 ESM/ESP <-> semantic JSON, совместимый с tes3conv"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{QStringLiteral("c"), QStringLiteral("compact")},
                      l10n(language, "Write compact JSON", "Записать компактный JSON")});
    parser.addOption(QCommandLineOption(
        {QStringLiteral("e"), QStringLiteral("encoding")},
        l10n(language,
             "Text encoding: auto or any supported encoding label",
             "Кодировка текста: auto или любое поддерживаемое имя кодировки"),
        QStringLiteral("encoding"), QStringLiteral("auto")));
    parser.addOption(QCommandLineOption(
        QStringLiteral("repair-scripts"),
        l10n(language,
             "Repair changed script SCHD/SCVR and discard stale SCDT",
             "Исправлять SCHD/SCVR изменённых скриптов и удалять устаревший SCDT")));
    parser.addOption(QCommandLineOption(
        QStringLiteral("file-date"),
        l10n(language, "Output plugin mtime: original, now, or an RFC3339 date/time",
                       "Дата изменения плагина: original, now или дата/время RFC3339"),
        QStringLiteral("date"), QStringLiteral("original")));
    parser.addOption(QCommandLineOption(
        QStringLiteral("file-type"),
        l10n(language, "Output header type: original, esp, or esm",
                       "Тип заголовка результата: original, esp или esm"),
        QStringLiteral("type"), QStringLiteral("original")));
    parser.addPositionalArgument(QStringLiteral("input"),
                                 l10n(language, "Input .esm/.esp/.json", "Входной .esm/.esp/.json"));
    parser.addPositionalArgument(QStringLiteral("output"),
                                 l10n(language, "Optional output path", "Необязательный путь результата"),
                                 QStringLiteral("[output]"));
    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) parser.showHelp(2);

    const QString input = positional.at(0);
    const QString encoding = parser.value(QStringLiteral("encoding")).trimmed().isEmpty()
        ? QStringLiteral("auto")
        : parser.value(QStringLiteral("encoding")).trimmed();
    QTextStream out(stdout);

    try {
        const QString ext = QFileInfo(input).suffix().toLower();
        QString output = positional.size() >= 2 ? positional.at(1) : Converter::defaultOutputFor(input);
        const QString requestedType = parser.value(QStringLiteral("file-type")).toLower();
        if (ext == QStringLiteral("json") && positional.size() < 2
            && (requestedType == QStringLiteral("esp") || requestedType == QStringLiteral("esm"))) {
            QFileInfo autoOutput(output);
            output = autoOutput.dir().filePath(autoOutput.completeBaseName() + QLatin1Char('.') + requestedType);
        }
        if (ext == QStringLiteral("esm") || ext == QStringLiteral("esp")) {
            const auto result = Converter::pluginToJson(
                input, output, parser.isSet(QStringLiteral("compact")), encoding);
            out << "JSON: " << result.outputPath << Qt::endl;
            out << result.inputSize << " -> " << result.outputSize << " "
                << l10n(language, "bytes", "байт") << " • " << result.encoding << Qt::endl;
        } else if (ext == QStringLiteral("json")) {
            const auto result = Converter::jsonToPlugin(
                input, output, encoding, parser.isSet(QStringLiteral("repair-scripts")),
                parser.value(QStringLiteral("file-date")), requestedType);
            out << l10n(language, "Plugin", "Плагин") << ": " << result.outputPath << Qt::endl;
            out << result.inputSize << " -> " << result.outputSize << " "
                << l10n(language, "bytes", "байт") << " • " << result.encoding << Qt::endl;
            if (result.repairedScripts > 0) {
                out << l10n(language, "Scripts repaired", "Скриптов исправлено") << ": "
                    << result.repairedScripts << Qt::endl;
            }
        } else {
            out << l10n(language,
                        "Unsupported extension. Use .esm, .esp or .json.",
                        "Неподдерживаемое расширение. Используйте .esm, .esp или .json.")
                << Qt::endl;
            return 2;
        }
    } catch (const std::exception &error) {
        QTextStream err(stderr);
        err << l10n(language, "Error", "Ошибка") << ": " << QString::fromUtf8(error.what()) << Qt::endl;
        return 1;
    }
    return 0;
}
