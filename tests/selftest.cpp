#include "core/converter.h"
#include "core/localization.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

using namespace arena::tes3json;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    if (!QFile::exists(Converter::backendPath())) {
        out << "Rust backend not staged: " << Converter::backendPath() << Qt::endl;
        return 10;
    }

    QTemporaryDir temp;
    if (!temp.isValid()) return 20;
    const QString jsonPath = temp.filePath(QStringLiteral("sample.json"));
    QFile file(jsonPath);
    if (!file.open(QIODevice::WriteOnly)) return 21;
    file.write("[\n  {\n    \"type\": \"Header\",\n    \"flags\": \"\",\n    \"version\": 1.3,\n    \"file_type\": \"Esm\",\n    \"author\": \"Arena\",\n    \"description\": \"test\",\n    \"num_objects\": 0,\n    \"masters\": []\n  }\n]\n");
    file.close();

    const QString output = Converter::defaultOutputFor(jsonPath);
    if (!output.endsWith(QStringLiteral(".esm"))) {
        out << "Header file_type detection failed: " << output << Qt::endl;
        return 30;
    }

    if (uiLanguageFromTag(QStringLiteral("ru-RU")) != UiLanguage::Russian
        || uiLanguageFromTag(QStringLiteral("ru")) != UiLanguage::Russian
        || uiLanguageFromTag(QStringLiteral("en-US")) != UiLanguage::English
        || uiLanguageFromTag(QStringLiteral("de-DE")) != UiLanguage::English) {
        out << "UI language selection failed" << Qt::endl;
        return 31;
    }

    out << "ArenaTES3JSON frontend self-test OK" << Qt::endl;
    return 0;
}
