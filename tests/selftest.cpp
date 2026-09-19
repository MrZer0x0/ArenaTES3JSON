#include "core/cp1251.h"
#include "core/tes3plugin.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>

using namespace arena::tes3json;

static void u32(QByteArray &b, quint32 v)
{
    b.append(char(v & 0xff)); b.append(char((v >> 8) & 0xff));
    b.append(char((v >> 16) & 0xff)); b.append(char((v >> 24) & 0xff));
}

static QByteArray sub(const char tag[5], const QByteArray &data)
{
    QByteArray b(tag, 4); u32(b, quint32(data.size())); b += data; return b;
}

static QByteArray record(const char tag[5], quint32 unknown, quint32 flags, const QByteArray &payload)
{
    QByteArray b(tag, 4); u32(b, quint32(payload.size())); u32(b, unknown); u32(b, flags); b += payload; return b;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    bool encOk = false;
    const QString russian = QString::fromUtf8("Арена Ёжик — тест");
    const QByteArray cp = Cp1251::encode(russian, &encOk);
    if (!encOk || Cp1251::decode(cp) != russian) return 10;

    // Every defined Windows-1251 byte must decode and encode back to itself.
    for (int value = 0; value <= 0xFF; ++value) {
        if (value == 0x98) continue; // undefined in Windows-1251
        QByteArray one(1, static_cast<char>(value));
        bool decodeOk = false;
        const QString decoded = Cp1251::decode(one, &decodeOk);
        bool reencodeOk = false;
        const QByteArray reencoded = Cp1251::encode(decoded, &reencodeOk);
        if (!decodeOk || !reencodeOk || reencoded != one) return 11;
    }

    QByteArray textData = Cp1251::encode(QString::fromUtf8("Привет, Морровинд!"), &encOk);
    textData.append('\0');
    QByteArray binary; binary.append(char(0x01)); binary.append(char(0x00)); binary.append(char(0xFE)); binary.append(char(0x7F));

    QByteArray source;
    source += record("TES3", 0x12345678u, 0xAABBCCDDu,
                     sub("NAME", textData) + sub("DATA", binary));
    source += record("GMST", 7u, 0x400u, sub("NAME", QByteArray("sArenaTest\0", 11)));

    const Tes3Plugin plugin = Tes3Plugin::parse(source, QStringLiteral("test.esp"), QStringLiteral("esp"));
    const QJsonObject json = plugin.toJson();
    const Tes3Plugin round = Tes3Plugin::fromJson(json);
    const QByteArray rebuilt = round.build();
    if (rebuilt != source) {
        out << "round-trip mismatch" << Qt::endl;
        return 20;
    }

    QJsonObject edited = json;
    QJsonArray records = edited.value(QStringLiteral("records")).toArray();
    QJsonObject r0 = records.at(0).toObject();
    QJsonArray subs = r0.value(QStringLiteral("subrecords")).toArray();
    QJsonObject s0 = subs.at(0).toObject();
    s0.insert(QStringLiteral("text"), QString::fromUtf8("Новый текст"));
    subs[0] = s0; r0.insert(QStringLiteral("subrecords"), subs); records[0] = r0;
    edited.insert(QStringLiteral("records"), records);

    const Tes3Plugin changed = Tes3Plugin::fromJson(edited);
    const QByteArray changedBytes = changed.build();
    const Tes3Plugin changedAgain = Tes3Plugin::parse(changedBytes);
    if (changedAgain.records.at(0).subrecords.at(0).text != QString::fromUtf8("Новый текст")) return 30;

    // If raw Base64 is changed while data_sha256 is left as exported, the raw
    // bytes are an intentional edit and must take precedence over text.
    QJsonObject rawEdited = json;
    QJsonArray rawRecords = rawEdited.value(QStringLiteral("records")).toArray();
    QJsonObject rawR0 = rawRecords.at(0).toObject();
    QJsonArray rawSubs = rawR0.value(QStringLiteral("subrecords")).toArray();
    QJsonObject rawS0 = rawSubs.at(0).toObject();
    const QByteArray rawReplacement = QByteArray::fromHex("0102ff00");
    rawS0.insert(QStringLiteral("data_b64"), QString::fromLatin1(rawReplacement.toBase64()));
    rawSubs[0] = rawS0; rawR0.insert(QStringLiteral("subrecords"), rawSubs); rawRecords[0] = rawR0;
    rawEdited.insert(QStringLiteral("records"), rawRecords);
    const Tes3Plugin rawChanged = Tes3Plugin::fromJson(rawEdited);
    if (rawChanged.records.at(0).subrecords.at(0).data != rawReplacement) return 40;

    out << "ArenaTES3JSON self-test OK" << Qt::endl;
    return 0;
}
