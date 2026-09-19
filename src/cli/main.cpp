#include "core/converter.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>

#include <exception>

using arena::tes3json::Converter;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ArenaTES3JSON-cli"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lossless TES3 ESM/ESP <-> JSON converter with native Windows-1251 support"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{QStringLiteral("c"), QStringLiteral("compact")}, QStringLiteral("Write compact JSON")});
    parser.addOption({QStringLiteral("verify"), QStringLiteral("Verify byte-identical plugin -> JSON -> plugin round-trip")});
    parser.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Input .esm/.esp/.json"));
    parser.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Optional output path"), QStringLiteral("[output]"));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) parser.showHelp(2);

    const QString input = args.at(0);
    QTextStream out(stdout);

    if (parser.isSet(QStringLiteral("verify"))) {
        QString details;
        const bool ok = Converter::verifyRoundTrip(input, &details);
        out << details << Qt::endl;
        return ok ? 0 : 3;
    }

    try {
        const QString ext = QFileInfo(input).suffix().toLower();
        if (ext == QStringLiteral("esp") || ext == QStringLiteral("esm")) {
            const QString output = args.size() >= 2 ? args.at(1) : Converter::defaultOutputFor(input);
            const auto result = Converter::pluginToJson(input, output, parser.isSet(QStringLiteral("compact")));
            out << "Written " << result.outputPath << " (" << result.outputSize << " bytes)" << Qt::endl;
        } else if (ext == QStringLiteral("json")) {
            const QString output = args.size() >= 2 ? args.at(1) : QString();
            const auto result = Converter::jsonToPlugin(input, output);
            out << "Written " << result.outputPath << " (" << result.outputSize << " bytes)" << Qt::endl;
            if (result.byteIdenticalToSource) out << "Round-trip matches source SHA-256 byte-for-byte." << Qt::endl;
        } else {
            out << "Unsupported extension. Use .esm, .esp or .json." << Qt::endl;
            return 2;
        }
    } catch (const std::exception &e) {
        QTextStream err(stderr);
        err << "Error: " << QString::fromUtf8(e.what()) << Qt::endl;
        return 1;
    }
    return 0;
}
