#include <QCoreApplication>
#include <QDebug>
#include <QTextCodec>

#include <stdint.h>

#include "RecordInfoSerializer.h"

static void printRecord(const STR_RECORD_INFO &info)
{
    qDebug() << "---- cm (STR_CONTRAST_INFO) ----";
    qDebug() << "cm.name   :" << info.cm.name;
    qDebug() << "cm.concent:" << info.cm.concent;
    qDebug() << "cm.index  :" << info.cm.index;

    qDebug() << "---- patient (STR_PATIENT_INFO) ----";
    qDebug() << "patient.name     :" << info.patient.name;
    qDebug() << "patient.patientID:" << info.patient.patientID;
    qDebug() << "patient.checkID  :" << info.patient.checkID;
    qDebug() << "patient.exam     :" << info.patient.exam;
    qDebug() << "patient.weight   :" << info.patient.weight;
    qDebug() << "patient.brithday :" << info.patient.brithday;
    qDebug() << "patient.height   :" << info.patient.height;

    qDebug() << "---- record ----";
    qDebug() << "injector    :" << info.injector;
    qDebug() << "datetime    :" << info.datetime;
    qDebug() << "protocolName:" << info.protocolName;
    qDebug() << "method      :" << info.method;
}

static bool recordEquals(const STR_RECORD_INFO &a, const STR_RECORD_INFO &b)
{
    return a.cm.name == b.cm.name
        && a.cm.concent == b.cm.concent
        && a.cm.index == b.cm.index
        && a.patient.name == b.patient.name
        && a.patient.patientID == b.patient.patientID
        && a.patient.checkID == b.patient.checkID
        && a.patient.exam == b.patient.exam
        && a.patient.weight == b.patient.weight
        && a.patient.brithday == b.patient.brithday
        && a.patient.height == b.patient.height
        && a.injector == b.injector
        && a.datetime == b.datetime
        && a.protocolName == b.protocolName
        && a.method == b.method;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#if QT_VERSION < 0x050000
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
#endif

    // 构造 STR_RECORD_INFO 测试数据
    STR_RECORD_INFO record;
    record.cm.name = QString::fromUtf8("碘海醇");
    record.cm.concent = QLatin1String("350");
    record.cm.index = QLatin1String("A01");

    record.patient.name = QString::fromUtf8("张明华");
    record.patient.patientID = QLatin1String("P202607001");
    record.patient.checkID = QLatin1String("C20260701001");
    record.patient.exam = QString::fromUtf8("胸部CTA");
    record.patient.weight = 70;
    record.patient.brithday = QLatin1String("19880520");
    record.patient.height = 175;

    record.injector = QLatin1String("Injector-01");
    record.datetime = 20260701; // YYYYMMDD，适配 int 范围
    record.protocolName = QString::fromUtf8("胸部增强");
    record.method = 1;

    qDebug() << "==== source STR_RECORD_INFO ====";
    printRecord(record);

    // 序列化
    const QByteArray packed = RecordInfoSerializer::serialize(record);
    qDebug() << "==== serialize QByteArray ====";
    qDebug() << packed;

    // QByteArray -> uint8_t 字符串数组
    int u8Len = 0;
    uint8_t *u8Buf = RecordInfoSerializer::toUint8Array(record, &u8Len, false, true);
    qDebug() << "==== toUint8Array ====";
    qDebug() << "length:" << u8Len;
    if (u8Buf == 0) {
        qWarning() << "toUint8Array failed";
        return 1;
    }
    qDebug() << "text  :" << reinterpret_cast<const char *>(u8Buf);

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

    // 从 uint8_t 数组反序列化回结构体
    STR_RECORD_INFO restored;
    if (!RecordInfoSerializer::deserialize(u8Buf, u8Len, &restored)) {
        qWarning() << "deserialize failed";
        delete[] u8Buf;
        return 1;
    }

    qDebug() << "==== deserialize STR_RECORD_INFO ====";
    printRecord(restored);
    qDebug() << "round-trip ok:" << recordEquals(record, restored);

    // 调用方缓冲区写法
    uint8_t stackBuf[512];
    int copied = 0;
    const bool ok = RecordInfoSerializer::toUint8Array(record, stackBuf, sizeof(stackBuf), &copied);
    qDebug() << "stack copy ok:" << ok << "len:" << copied;

    delete[] u8Buf;
    return recordEquals(record, restored) ? 0 : 2;
}
