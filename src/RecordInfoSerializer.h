#ifndef RECORDINFOSERIALIZER_H
#define RECORDINFOSERIALIZER_H

#include <stdint.h>

#include <QByteArray>

#include "RecordInfo.h"

/**
 * @brief STR_CIM_REPORT_INFO / STR_RECORD_INFO 与 key:value; 协议互转（QT 4.8.5）
 *
 * 字段 key（扁平化）：
 *   dataType,
 *   cmName, cmConcent, cmIndex,
 *   patientName, patientID, checkID, exam, weight, brithday, height,
 *   injector, datetime, protocolName, method
 */
class RecordInfoSerializer
{
public:
    /** STR_CIM_REPORT_INFO -> 协议字节流 */
    static QByteArray serialize(const STR_CIM_REPORT_INFO &info, bool appendEndMark = false);

    /** STR_RECORD_INFO -> 协议字节流（不含 dataType） */
    static QByteArray serialize(const STR_RECORD_INFO &info, bool appendEndMark = false);

    /** 协议字节流 -> STR_CIM_REPORT_INFO */
    static bool deserialize(const QByteArray &data, STR_CIM_REPORT_INFO *out);
    static bool deserialize(const unsigned char *data, int length, STR_CIM_REPORT_INFO *out);

    /** 协议字节流 -> STR_RECORD_INFO（忽略 dataType） */
    static bool deserialize(const QByteArray &data, STR_RECORD_INFO *out);
    static bool deserialize(const unsigned char *data, int length, STR_RECORD_INFO *out);

    /** STR_CIM_REPORT_INFO -> uint8_t 字符串数组（堆分配，调用方 delete[]） */
    static uint8_t *toUint8Array(const STR_CIM_REPORT_INFO &info,
                                 int *outLen = 0,
                                 bool appendEndMark = false,
                                 bool nullTerminate = true);

    /** STR_CIM_REPORT_INFO -> 调用方 uint8_t 缓冲区 */
    static bool toUint8Array(const STR_CIM_REPORT_INFO &info,
                             uint8_t *outBuf,
                             int bufSize,
                             int *outLen = 0,
                             bool appendEndMark = false,
                             bool nullTerminate = true);

private:
    RecordInfoSerializer();
    static void writeRecordFields(class KvSerializer &s, const STR_RECORD_INFO &info);
    static void readRecordFields(const class KvSerializer &s, STR_RECORD_INFO *out);
};

#endif // RECORDINFOSERIALIZER_H
