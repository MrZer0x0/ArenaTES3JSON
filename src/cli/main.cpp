#include "core/converter.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>

#include <exception>

using arena::tes3json::Converter;
using arena::tes3json::TextEncodingMode;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ArenaTES3JSON-cli"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("TES3 ESM/ESP <-> tes3conv-compatible JSON converter"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{QStringLiteral("c"), QStringLiteral("compact")}, QStringLiteral("Write compact JSON")});
    parser.addOption({QStringLiteral("raw-encoding"), QStringLiteral("Disable Windows-1251/1C translation")});
    parser.addOption({QStringLiteral("no-lossless"), QStringLiteral("Do not create/use .arena-lossless sidecar")});
    parser.addOption({QStringLiteral("verify"), QStringLiteral("Verify the byte-identical lossless path")});
    parser.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Input .esm/.esp/.json"));
    parser.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Optional output path"), QStringLiteral("[output]"));
    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) parser.showHelp(2);

    const QString input = positional.at(0);
    const auto encoding = parser.isSet(QStringLiteral("raw-encoding"))
        ? TextEncodingMode::Raw : TextEncodingMode::Windows1251;
    const bool lossless = !parser.isSet(QStringLiteral("no-lossless"));
    QTextStream out(stdout);

    if (parser.isSet(QStringLiteral("verify"))) {
        QString details;
        const bool ok = Converter::verifyRoundTrip(input, &details, encoding);
        out << details << Qt::endl;
        return ok ? 0 : 3;
    }

    try {
        const QString ext = QFileInfo(input).suffix().toLower();
        const QString output = positional.size() >= 2 ? positional.at(1) : Converter::defaultOutputFor(input);
        if (ext == QStringLiteral("esm") || ext == QStringLiteral("esp")) {
            const auto result = Converter::pluginToJson(input, output,
                                                        parser.isSet(QStringLiteral("compact")), encoding, lossless);
            out << "JSON: " << result.outputPath << Qt::endl;
            if (!result.sidecarPath.isEmpty()) out << "Lossless sidecar: " << result.sidecarPath << Qt::endl;
        } else if (ext == QStringLiteral("json")) {
            const auto result = Converter::jsonToPlugin(input, output, encoding, lossless);
            out << "Plugin: " << result.outputPath << Qt::endl;
            out << result.message << Qt::endl;
            if (result.byteIdenticalToSource) out << "Byte-for-byte: YES" << Qt::endl;
        } else {
            out << "Unsupported extension. Use .esm, .esp or .json." << Qt::endl;
            return 2;
        }
    } catch (const std::exception &error) {
        QTextStream err(stderr);
        err << "Error: " << QString::fromUtf8(error.what()) << Qt::endl;
        return 1;
    }
    return 0;
}
