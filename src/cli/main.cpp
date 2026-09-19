#include "core/converter.h"
#include "core/localization.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>

#include <exception>

using arena::tes3json::Converter;
using arena::tes3json::TextEncodingMode;
using arena::tes3json::UiLanguage;
using arena::tes3json::l10n;
using arena::tes3json::systemUiLanguage;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ArenaTES3JSON-cli"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.3.6"));

    const UiLanguage language = systemUiLanguage();

    QCommandLineParser parser;
    parser.setApplicationDescription(l10n(
        language,
        "TES3 ESM/ESP <-> tes3conv-compatible JSON converter",
        "Конвертер TES3 ESM/ESP <-> JSON, совместимый с tes3conv"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{QStringLiteral("c"), QStringLiteral("compact")},
                      l10n(language, "Write compact JSON", "Записать компактный JSON")});
    parser.addOption({QStringLiteral("raw-encoding"),
                      l10n(language,
                           "Disable Windows-1251/1C translation",
                           "Отключить преобразование Windows-1251/1C")});
    parser.addPositionalArgument(QStringLiteral("input"),
                                 l10n(language, "Input .esm/.esp/.json", "Входной .esm/.esp/.json"));
    parser.addPositionalArgument(QStringLiteral("output"),
                                 l10n(language, "Optional output path", "Необязательный путь результата"),
                                 QStringLiteral("[output]"));
    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) parser.showHelp(2);

    const QString input = positional.at(0);
    const auto encoding = parser.isSet(QStringLiteral("raw-encoding"))
        ? TextEncodingMode::Raw : TextEncodingMode::Windows1251;
    QTextStream out(stdout);

    try {
        const QString ext = QFileInfo(input).suffix().toLower();
        const QString output = positional.size() >= 2 ? positional.at(1) : Converter::defaultOutputFor(input);
        if (ext == QStringLiteral("esm") || ext == QStringLiteral("esp")) {
            const auto result = Converter::pluginToJson(
                input, output, parser.isSet(QStringLiteral("compact")), encoding);
            out << "JSON: " << result.outputPath << Qt::endl;
            out << result.inputSize << " -> " << result.outputSize << " "
                << l10n(language, "bytes", "байт") << Qt::endl;
        } else if (ext == QStringLiteral("json")) {
            const auto result = Converter::jsonToPlugin(input, output, encoding);
            out << l10n(language, "Plugin", "Плагин") << ": " << result.outputPath << Qt::endl;
            out << result.inputSize << " -> " << result.outputSize << " "
                << l10n(language, "bytes", "байт") << Qt::endl;
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
