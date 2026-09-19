#include "cp1251.h"

#include <array>

namespace arena::tes3json {
namespace {

// Windows-1251 mapping for bytes 0x80..0xFF. 0x98 is undefined.
constexpr std::array<char16_t, 128> kDecode = {
    0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,
    0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
    0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
    0x0000,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
    0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7,
    0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
    0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7,
    0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457,
    0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417,
    0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F,
    0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427,
    0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F,
    0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437,
    0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F,
    0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447,
    0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F
};

bool printable(char16_t c)
{
    return c == u'\t' || c == u'\r' || c == u'\n' || c >= 0x20;
}

} // namespace

QString Cp1251::decode(QByteArrayView bytes, bool *ok)
{
    QString out;
    out.reserve(bytes.size());
    bool good = true;
    for (const char raw : bytes) {
        const auto b = static_cast<unsigned char>(raw);
        if (b < 0x80) {
            out.append(QChar(b));
        } else {
            const char16_t mapped = kDecode[b - 0x80];
            if (mapped == 0) {
                good = false;
                out.append(QChar(0xFFFD));
            } else {
                out.append(QChar(mapped));
            }
        }
    }
    if (ok) *ok = good;
    return out;
}

QByteArray Cp1251::encode(QStringView text, bool *ok)
{
    QByteArray out;
    out.reserve(text.size());
    bool good = true;

    for (const QChar qc : text) {
        const char16_t c = qc.unicode();
        if (c < 0x80) {
            out.append(static_cast<char>(c));
            continue;
        }

        int found = -1;
        for (int i = 0; i < static_cast<int>(kDecode.size()); ++i) {
            if (kDecode[static_cast<size_t>(i)] == c) {
                found = 0x80 + i;
                break;
            }
        }
        if (found < 0) {
            good = false;
            out.append('?');
        } else {
            out.append(static_cast<char>(found));
        }
    }

    if (ok) *ok = good;
    return out;
}

bool Cp1251::isReversibleText(QByteArrayView bytes, QString *decodedText, int *trailingNuls)
{
    if (bytes.isEmpty()) return false;

    int nul = -1;
    for (qsizetype i = 0; i < bytes.size(); ++i) {
        if (bytes.at(i) == '\0') {
            nul = static_cast<int>(i);
            break;
        }
    }

    int zeros = 0;
    QByteArrayView body = bytes;
    if (nul >= 0) {
        for (qsizetype i = nul; i < bytes.size(); ++i) {
            if (bytes.at(i) != '\0') return false;
        }
        zeros = static_cast<int>(bytes.size()) - nul;
        body = bytes.first(nul);
    }

    if (body.isEmpty()) return false;
    bool ok = false;
    const QString text = decode(body, &ok);
    if (!ok) return false;

    int printableCount = 0;
    for (const QChar c : text) {
        if (!printable(c.unicode())) return false;
        if (!c.isSpace()) ++printableCount;
    }
    if (printableCount == 0) return false;

    bool encOk = false;
    if (encode(text, &encOk) != body || !encOk) return false;

    if (decodedText) *decodedText = text;
    if (trailingNuls) *trailingNuls = zeros;
    return true;
}

} // namespace arena::tes3json
