#pragma once

#include <QByteArray>
#include <QString>

namespace arena::tes3json {

class Cp1251 final {
public:
    static QString decode(QByteArrayView bytes, bool *ok = nullptr);
    static QByteArray encode(QStringView text, bool *ok = nullptr);
    static bool isReversibleText(QByteArrayView bytes,
                                 QString *decodedText = nullptr,
                                 int *trailingNuls = nullptr);
};

} // namespace arena::tes3json
