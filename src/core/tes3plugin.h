#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace arena::tes3json {

struct Subrecord {
    QByteArray type;
    QByteArray data;
    bool textMode = false;
    QString text;
    int trailingNuls = 0;
};

struct Record {
    QByteArray type;
    quint32 unknown = 0;
    quint32 flags = 0;
    QVector<Subrecord> subrecords;
    QByteArray tail;
};

class Tes3Plugin final {
public:
    QVector<Record> records;
    QByteArray fileTail;
    QString sourceExtension;
    QString sourceFileName;
    QByteArray sourceSha256;

    static Tes3Plugin parse(QByteArrayView bytes,
                            const QString &fileName = {},
                            const QString &extension = {});
    QByteArray build() const;

    QJsonObject toJson() const;
    static Tes3Plugin fromJson(const QJsonObject &root);

    static bool looksLikePlugin(QByteArrayView bytes);
};

} // namespace arena::tes3json
