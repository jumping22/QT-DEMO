#ifndef RECORDINFOSERIALIZER_H
#define RECORDINFOSERIALIZER_H

#include <stdint.h>

#include <QByteArray>

#include "RecordInfo.h"

/**
 * @brief STR_RECORD_INFO 与 key:value; 协议互转（QT 4.8.5）
 *
 * 字段 key（扁平化，避免嵌套同名冲突）：
 *   cmName, cmConcent, cmIndex,
 *   patientName, patientID, checkID, exam, weight, brithday, height,
 *   injector, datetime, protocolName, method
 */
class RecordInfoSerializer
{
public:
    /** 结构体 -> 协议字节流 */
    static QByteArray serialize(const STR_RECORD_INFO &info, bool appendEndMark = false);

    /** 协议字节流 -> 结构体 */
    static bool deserialize(const QByteArray &data, STR_RECORD_INFO *out);
    static bool deserialize(const unsigned char *data, int length, STR_RECORD_INFO *out);

    /** 结构体 -> uint8_t 字符串数组（堆分配，调用方 delete[]） */
    static uint8_t *toUint8Array(const STR_RECORD_INFO &info,
                                 int *outLen = 0,
                                 bool appendEndMark = false,
                                 bool nullTerminate = true);

    /** 结构体 -> 调用方 uint8_t 缓冲区 */
    static bool toUint8Array(const STR_RECORD_INFO &info,
                             uint8_t *outBuf,
                             int bufSize,
                             int *outLen = 0,
                             bool appendEndMark = false,
                             bool nullTerminate = true);

private:
    RecordInfoSerializer();
};

#endif // RECORDINFOSERIALIZER_H
