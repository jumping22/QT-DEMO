#include <QCoreApplication>
#include <QDebug>
#include <QTextCodec>

#include <stdint.h>
#include <string.h>

#include "KvSerializer.h"

/**
 * 测试用例原始数据（十六进制）：
 * 64 61 74 61 54 79 70 65 3A 30 3B 6E 61 6D 65 3A E5 BC A0 E6 98 8E E5 8D 8E
 * 3B 49 64 3A 50 32 30 32 36 30 37 30 30 31 3B 65 78 61 6D 3A E8 83 B8 E9 83 A8
 * 43 54 41 3B 77 65 69 67 68 74 3A 37 30
 *
 * 对应明文：
 * dataType:0;name:张明华;Id:P202607001;exam:胸部CTA;weight:70
 */
static const unsigned char kTestPayload[] = {
    0x64, 0x61, 0x74, 0x61, 0x54, 0x79, 0x70, 0x65, 0x3A, 0x30, 0x3B,
    0x6E, 0x61, 0x6D, 0x65, 0x3A, 0xE5, 0xBC, 0xA0, 0xE6, 0x98, 0x8E,
    0xE5, 0x8D, 0x8E, 0x3B, 0x49, 0x64, 0x3A, 0x50, 0x32, 0x30, 0x32,
    0x36, 0x30, 0x37, 0x30, 0x30, 0x31, 0x3B, 0x65, 0x78, 0x61, 0x6D,
    0x3A, 0xE8, 0x83, 0xB8, 0xE9, 0x83, 0xA8, 0x43, 0x54, 0x41, 0x3B,
    0x77, 0x65, 0x69, 0x67, 0x68, 0x74, 0x3A, 0x37, 0x30
};

static const int kTestPayloadLen = static_cast<int>(sizeof(kTestPayload));

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // QT4 控制台中文输出：按本地编码转换（RK3568 上常见为 UTF-8）
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#if QT_VERSION < 0x050000
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
#endif

    KvSerializer serializer;
    if (!serializer.deserialize(kTestPayload, kTestPayloadLen)) {
        qWarning() << "deserialize failed";
        return 1;
    }

    qDebug() << "==== deserialize result ====";
    const QStringList keys = serializer.keys();
    for (int i = 0; i < keys.size(); ++i) {
        const QString &key = keys.at(i);
        qDebug().nospace()
            << qPrintable(key) << " = "
            << qPrintable(serializer.value(key));
    }

    qDebug() << "---- field access ----";
    qDebug() << "dataType:" << serializer.value(QLatin1String("dataType"));
    qDebug() << "name    :" << serializer.value(QLatin1String("name"));
    qDebug() << "Id      :" << serializer.value(QLatin1String("Id"));
    qDebug() << "exam    :" << serializer.value(QLatin1String("exam"));
    qDebug() << "weight  :" << serializer.value(QLatin1String("weight"));

    // 再序列化一轮，验证往返一致性
    const QByteArray again = serializer.serialize();
    qDebug() << "==== serialize again ====";
    qDebug() << again;
    qDebug() << "round-trip ok:"
             << (again == QByteArray(reinterpret_cast<const char *>(kTestPayload),
                                     kTestPayloadLen));

    // QByteArray -> 字符串数组（按字段拆分）
    const QStringList fields = KvSerializer::toStringList(again);
    qDebug() << "==== toStringList ====";
    for (int i = 0; i < fields.size(); ++i) {
        qDebug().nospace() << "[" << i << "] " << qPrintable(fields.at(i));
    }

    // QByteArray -> uint8_t 字符串数组
    int u8Len = 0;
    uint8_t *u8Buf = KvSerializer::toUint8Array(again, &u8Len, true);
    qDebug() << "==== toUint8Array ====";
    qDebug() << "length:" << u8Len;
    if (u8Buf) {
        // QT4 无 toHex(separator)，手动拼十六进制便于对照
        QByteArray hexDump;
        for (int i = 0; i < u8Len; ++i) {
            if (i > 0) {
                hexDump.append(' ');
            }
            const char hexChars[] = "0123456789ABCDEF";
            hexDump.append(hexChars[(u8Buf[i] >> 4) & 0x0F]);
            hexDump.append(hexChars[u8Buf[i] & 0x0F]);
        }
        qDebug() << "hex   :" << hexDump.constData();
        qDebug() << "text  :" << reinterpret_cast<const char *>(u8Buf);
        qDebug() << "match original:"
                 << (u8Len == kTestPayloadLen
                     && memcmp(u8Buf, kTestPayload, static_cast<size_t>(u8Len)) == 0);

        // 调用方提供缓冲区的写法
        uint8_t stackBuf[256];
        int copied = 0;
        const bool ok = KvSerializer::toUint8Array(again, stackBuf, sizeof(stackBuf), &copied, true);
        qDebug() << "stack copy ok:" << ok << "len:" << copied;

        delete[] u8Buf;
    }

    return 0;
}
