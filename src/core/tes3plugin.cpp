#include "tes3plugin.h"
#include "cp1251.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

#include <cstring>
#include <limits>
#include <stdexcept>

namespace arena::tes3json {
namespace {

[[noreturn]] void fail(const QString &message)
{
    throw std::runtime_error(message.toStdString());
}

quint32 readU32(QByteArrayView bytes, qsizetype offset)
{
    if (offset < 0 || offset + 4 > bytes.size()) fail(QStringLiteral("Unexpected end of file"));
    const auto *p = reinterpret_cast<const unsigned char *>(bytes.data() + offset);
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}

void appendU32(QByteArray &out, quint32 value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 24) & 0xFF));
}

QString tagToString(QByteArrayView tag)
{
    return QString::fromLatin1(tag.data(), tag.size());
}

QByteArray tagFromJson(const QJsonValue &value, const char *what)
{
    const QByteArray tag = value.toString().toLatin1();
    if (tag.size() != 4) fail(QStringLiteral("%1 must be exactly 4 bytes").arg(QString::fromLatin1(what)));
    return tag;
}

bool plausibleTag(QByteArrayView tag)
{
    if (tag.size() != 4) return false;
    for (const char c : tag) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (!(u == '_' || (u >= '0' && u <= '9') || (u >= 'A' && u <= 'Z'))) return false;
    }
    return true;
}

bool shortTextTag(QByteArrayView tag)
{
    static const QByteArrayView known[] = {
        QByteArrayView("NAME", 4), QByteArrayView("FNAM", 4), QByteArrayView("DESC", 4),
        QByteArrayView("TEXT", 4), QByteArrayView("STRV", 4), QByteArrayView("MAST", 4),
        QByteArrayView("MODL", 4), QByteArrayView("SCRI", 4), QByteArrayView("ITEX", 4)
    };
    for (const auto candidate : known) if (tag == candidate) return true;
    return false;
}

QJsonObject encodeSubrecord(const Subrecord &sub)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("type"), tagToString(sub.type));
    obj.insert(QStringLiteral("size"), sub.data.size());
    if (sub.textMode) {
        obj.insert(QStringLiteral("encoding"), QStringLiteral("windows-1251"));
        obj.insert(QStringLiteral("text"), sub.text);
        if (sub.trailingNuls > 0) obj.insert(QStringLiteral("trailing_nuls"), sub.trailingNuls);
    } else {
        obj.insert(QStringLiteral("encoding"), QStringLiteral("binary"));
        obj.insert(QStringLiteral("data_b64"), QString::fromLatin1(sub.data.toBase64()));
    }
    return obj;
}

Subrecord decodeSubrecord(const QJsonObject &obj)
{
    Subrecord sub;
    sub.type = tagFromJson(obj.value(QStringLiteral("type")), "subrecord type");
    const QString encoding = obj.value(QStringLiteral("encoding")).toString(QStringLiteral("binary"));
    if (encoding == QStringLiteral("windows-1251")) {
        sub.textMode = true;
        sub.text = obj.value(QStringLiteral("text")).toString();
        sub.trailingNuls = obj.value(QStringLiteral("trailing_nuls")).toInt(0);
        if (sub.trailingNuls < 0 || sub.trailingNuls > 1024 * 1024) fail(QStringLiteral("Invalid trailing_nuls"));
        bool ok = false;
        sub.data = Cp1251::encode(sub.text, &ok);
        if (!ok) fail(QStringLiteral("Text contains characters that cannot be encoded as Windows-1251"));
        sub.data.append(QByteArray(sub.trailingNuls, '\0'));
    } else if (encoding == QStringLiteral("binary")) {
        sub.data = QByteArray::fromBase64(obj.value(QStringLiteral("data_b64")).toString().toLatin1());
    } else {
        fail(QStringLiteral("Unsupported subrecord encoding: %1").arg(encoding));
    }
    if (sub.data.size() > std::numeric_limits<quint32>::max()) fail(QStringLiteral("Subrecord is too large"));
    return sub;
}

} // namespace

bool Tes3Plugin::looksLikePlugin(QByteArrayView bytes)
{
    return bytes.size() >= 16 && bytes.first(4) == QByteArrayView("TES3", 4);
}

Tes3Plugin Tes3Plugin::parse(QByteArrayView bytes, const QString &fileName, const QString &extension)
{
    if (!looksLikePlugin(bytes)) fail(QStringLiteral("Not a TES3 ESM/ESP file: first record is not TES3"));

    Tes3Plugin plugin;
    plugin.sourceFileName = fileName;
    plugin.sourceExtension = extension.toLower();
    plugin.sourceSha256 = QCryptographicHash::hash(bytes.toByteArray(), QCryptographicHash::Sha256);

    qsizetype pos = 0;
    while (pos + 16 <= bytes.size()) {
        Record rec;
        rec.type = bytes.sliced(pos, 4).toByteArray();
        const quint32 size = readU32(bytes, pos + 4);
        rec.unknown = readU32(bytes, pos + 8);
        rec.flags = readU32(bytes, pos + 12);
        pos += 16;

        if (quint64(size) > quint64(bytes.size() - pos)) {
            fail(QStringLiteral("Record %1 declares %2 bytes, only %3 remain")
                 .arg(tagToString(rec.type)).arg(size).arg(bytes.size() - pos));
        }

        const QByteArrayView payload = bytes.sliced(pos, size);
        pos += size;

        qsizetype subPos = 0;
        while (subPos + 8 <= payload.size()) {
            const QByteArrayView tag = payload.sliced(subPos, 4);
            if (!plausibleTag(tag)) break;
            const quint32 subSize = readU32(payload, subPos + 4);
            if (quint64(subSize) > quint64(payload.size() - subPos - 8)) break;

            Subrecord sub;
            sub.type = tag.toByteArray();
            sub.data = payload.sliced(subPos + 8, subSize).toByteArray();

            QString text;
            int nuls = 0;
            const bool candidate = Cp1251::isReversibleText(sub.data, &text, &nuls);
            if (candidate && (sub.data.size() > 4 || shortTextTag(sub.type))) {
                sub.textMode = true;
                sub.text = text;
                sub.trailingNuls = nuls;
            }

            rec.subrecords.push_back(std::move(sub));
            subPos += 8 + subSize;
        }

        if (subPos < payload.size()) rec.tail = payload.sliced(subPos).toByteArray();
        plugin.records.push_back(std::move(rec));
    }

    if (pos < bytes.size()) plugin.fileTail = bytes.sliced(pos).toByteArray();
    return plugin;
}

QByteArray Tes3Plugin::build() const
{
    QByteArray out;
    for (const Record &rec : records) {
        if (rec.type.size() != 4) fail(QStringLiteral("Record type must be exactly 4 bytes"));
        QByteArray payload;
        for (const Subrecord &sub : rec.subrecords) {
            if (sub.type.size() != 4) fail(QStringLiteral("Subrecord type must be exactly 4 bytes"));
            if (sub.data.size() > std::numeric_limits<quint32>::max()) fail(QStringLiteral("Subrecord too large"));
            payload.append(sub.type);
            appendU32(payload, static_cast<quint32>(sub.data.size()));
            payload.append(sub.data);
        }
        payload.append(rec.tail);
        if (payload.size() > std::numeric_limits<quint32>::max()) fail(QStringLiteral("Record too large"));

        out.append(rec.type);
        appendU32(out, static_cast<quint32>(payload.size()));
        appendU32(out, rec.unknown);
        appendU32(out, rec.flags);
        out.append(payload);
    }
    out.append(fileTail);
    return out;
}

QJsonObject Tes3Plugin::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("ArenaTES3JSON"));
    root.insert(QStringLiteral("format_version"), 1);

    QJsonObject source;
    source.insert(QStringLiteral("file_name"), sourceFileName);
    source.insert(QStringLiteral("extension"), sourceExtension);
    source.insert(QStringLiteral("sha256"), QString::fromLatin1(sourceSha256.toHex()));
    source.insert(QStringLiteral("string_encoding"), QStringLiteral("windows-1251"));
    root.insert(QStringLiteral("source"), source);

    QJsonArray recordsJson;
    for (const Record &rec : records) {
        QJsonObject obj;
        obj.insert(QStringLiteral("type"), tagToString(rec.type));
        obj.insert(QStringLiteral("unknown"), static_cast<qint64>(rec.unknown));
        obj.insert(QStringLiteral("flags"), static_cast<qint64>(rec.flags));

        QJsonArray subs;
        for (const Subrecord &sub : rec.subrecords) subs.append(encodeSubrecord(sub));
        obj.insert(QStringLiteral("subrecords"), subs);
        if (!rec.tail.isEmpty()) obj.insert(QStringLiteral("tail_b64"), QString::fromLatin1(rec.tail.toBase64()));
        recordsJson.append(obj);
    }
    root.insert(QStringLiteral("records"), recordsJson);
    if (!fileTail.isEmpty()) root.insert(QStringLiteral("file_tail_b64"), QString::fromLatin1(fileTail.toBase64()));
    return root;
}

Tes3Plugin Tes3Plugin::fromJson(const QJsonObject &root)
{
    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("ArenaTES3JSON")) {
        fail(QStringLiteral("Unsupported JSON: missing ArenaTES3JSON format marker"));
    }
    if (root.value(QStringLiteral("format_version")).toInt() != 1) {
        fail(QStringLiteral("Unsupported ArenaTES3JSON format_version"));
    }

    Tes3Plugin plugin;
    const QJsonObject source = root.value(QStringLiteral("source")).toObject();
    plugin.sourceFileName = source.value(QStringLiteral("file_name")).toString();
    plugin.sourceExtension = source.value(QStringLiteral("extension")).toString().toLower();
    plugin.sourceSha256 = QByteArray::fromHex(source.value(QStringLiteral("sha256")).toString().toLatin1());

    const QJsonArray recs = root.value(QStringLiteral("records")).toArray();
    plugin.records.reserve(recs.size());
    for (const QJsonValue &rv : recs) {
        if (!rv.isObject()) fail(QStringLiteral("Record entry must be an object"));
        const QJsonObject obj = rv.toObject();
        Record rec;
        rec.type = tagFromJson(obj.value(QStringLiteral("type")), "record type");
        const qint64 unknown = obj.value(QStringLiteral("unknown")).toInteger(0);
        const qint64 flags = obj.value(QStringLiteral("flags")).toInteger(0);
        if (unknown < 0 || unknown > std::numeric_limits<quint32>::max() ||
            flags < 0 || flags > std::numeric_limits<quint32>::max()) {
            fail(QStringLiteral("Record header values must fit uint32"));
        }
        rec.unknown = static_cast<quint32>(unknown);
        rec.flags = static_cast<quint32>(flags);

        const QJsonArray subs = obj.value(QStringLiteral("subrecords")).toArray();
        rec.subrecords.reserve(subs.size());
        for (const QJsonValue &sv : subs) {
            if (!sv.isObject()) fail(QStringLiteral("Subrecord entry must be an object"));
            rec.subrecords.push_back(decodeSubrecord(sv.toObject()));
        }
        rec.tail = QByteArray::fromBase64(obj.value(QStringLiteral("tail_b64")).toString().toLatin1());
        plugin.records.push_back(std::move(rec));
    }
    plugin.fileTail = QByteArray::fromBase64(root.value(QStringLiteral("file_tail_b64")).toString().toLatin1());
    return plugin;
}

} // namespace arena::tes3json
