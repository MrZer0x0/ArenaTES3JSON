#pragma once

#include <QLocale>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace arena::tes3json {

enum class UiLanguage {
    English,
    Russian,
};

inline UiLanguage uiLanguageFromTag(QStringView tag)
{
    const QString normalized = tag.trimmed().toString().replace(QLatin1Char('_'), QLatin1Char('-'));
    if (normalized.compare(QStringLiteral("ru"), Qt::CaseInsensitive) == 0
        || normalized.startsWith(QStringLiteral("ru-"), Qt::CaseInsensitive)) {
        return UiLanguage::Russian;
    }
    return UiLanguage::English;
}

inline UiLanguage systemUiLanguage()
{
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    if (!uiLanguages.isEmpty()) {
        return uiLanguageFromTag(uiLanguages.constFirst());
    }

    return QLocale::system().language() == QLocale::Language::Russian
        ? UiLanguage::Russian
        : UiLanguage::English;
}

inline QString l10n(UiLanguage language, const char *english, const char *russian)
{
    return QString::fromUtf8(language == UiLanguage::Russian ? russian : english);
}

} // namespace arena::tes3json
