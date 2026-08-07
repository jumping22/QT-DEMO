#include "RecordInfoSerializer.h"

#include "KvSerializer.h"

void RecordInfoSerializer::writeRecordFields(KvSerializer &s, const STR_RECORD_INFO &info)
{
    // STR_CONTRAST_INFO
    s.setValue(QLatin1String("cmName"), info.cm.name);
    s.setValue(QLatin1String("cmConcent"), info.cm.concent);
    s.setValue(QLatin1String("cmIndex"), info.cm.index);

    // STR_PATIENT_INFO
    s.setValue(QLatin1String("patientName"), info.patient.name);
    s.setValue(QLatin1String("patientID"), info.patient.patientID);
    s.setValue(QLatin1String("checkID"), info.patient.checkID);
    s.setValue(QLatin1String("exam"), info.patient.exam);
    s.setValue(QLatin1String("weight"), QString::number(info.patient.weight));
    s.setValue(QLatin1String("brithday"), info.patient.brithday);
    s.setValue(QLatin1String("height"), QString::number(info.patient.height));

    // STR_RECORD_INFO 自身字段
    s.setValue(QLatin1String("injector"), info.injector);
    s.setValue(QLatin1String("datetime"), QString::number(info.datetime));
    s.setValue(QLatin1String("protocolName"), info.protocolName);
    s.setValue(QLatin1String("method"), QString::number(info.method));
}

void RecordInfoSerializer::readRecordFields(const KvSerializer &s, STR_RECORD_INFO *out)
{
    out->cm.name = s.value(QLatin1String("cmName"));
    out->cm.concent = s.value(QLatin1String("cmConcent"));
    out->cm.index = s.value(QLatin1String("cmIndex"));

    out->patient.name = s.value(QLatin1String("patientName"));
    out->patient.patientID = s.value(QLatin1String("patientID"));
    out->patient.checkID = s.value(QLatin1String("checkID"));
    out->patient.exam = s.value(QLatin1String("exam"));
    out->patient.weight = s.value(QLatin1String("weight")).toInt();
    out->patient.brithday = s.value(QLatin1String("brithday"));
    out->patient.height = s.value(QLatin1String("height")).toInt();

    out->injector = s.value(QLatin1String("injector"));
    out->datetime = s.value(QLatin1String("datetime")).toInt();
    out->protocolName = s.value(QLatin1String("protocolName"));
    out->method = s.value(QLatin1String("method")).toInt();
}

QByteArray RecordInfoSerializer::serialize(const STR_CIM_REPORT_INFO &info, bool appendEndMark)
{
    KvSerializer s;
    s.setValue(QLatin1String("dataType"), QString::number(info.dataType));
    writeRecordFields(s, info.record);
    return s.serialize(appendEndMark);
}

QByteArray RecordInfoSerializer::serialize(const STR_RECORD_INFO &info, bool appendEndMark)
{
    KvSerializer s;
    writeRecordFields(s, info);
    return s.serialize(appendEndMark);
}

bool RecordInfoSerializer::deserialize(const QByteArray &data, STR_CIM_REPORT_INFO *out)
{
    if (out == 0) {
        return false;
    }

    KvSerializer s;
    if (!s.deserialize(data)) {
        return false;
    }

    out->dataType = s.value(QLatin1String("dataType")).toInt();
    readRecordFields(s, &out->record);
    return true;
}

bool RecordInfoSerializer::deserialize(const unsigned char *data, int length, STR_CIM_REPORT_INFO *out)
{
    if (data == 0 || out == 0) {
        return false;
    }

    QByteArray raw;
    if (length < 0) {
        raw = QByteArray(reinterpret_cast<const char *>(data));
    } else {
        raw = QByteArray(reinterpret_cast<const char *>(data), length);
    }
    return deserialize(raw, out);
}

bool RecordInfoSerializer::deserialize(const QByteArray &data, STR_RECORD_INFO *out)
{
    if (out == 0) {
        return false;
    }

    KvSerializer s;
    if (!s.deserialize(data)) {
        return false;
    }

    readRecordFields(s, out);
    return true;
}

bool RecordInfoSerializer::deserialize(const unsigned char *data, int length, STR_RECORD_INFO *out)
{
    if (data == 0 || out == 0) {
        return false;
    }

    QByteArray raw;
    if (length < 0) {
        raw = QByteArray(reinterpret_cast<const char *>(data));
    } else {
        raw = QByteArray(reinterpret_cast<const char *>(data), length);
    }
    return deserialize(raw, out);
}

uint8_t *RecordInfoSerializer::toUint8Array(const STR_CIM_REPORT_INFO &info,
                                            int *outLen,
                                            bool appendEndMark,
                                            bool nullTerminate)
{
    return KvSerializer::toUint8Array(serialize(info, appendEndMark), outLen, nullTerminate);
}

bool RecordInfoSerializer::toUint8Array(const STR_CIM_REPORT_INFO &info,
                                        uint8_t *outBuf,
                                        int bufSize,
                                        int *outLen,
                                        bool appendEndMark,
                                        bool nullTerminate)
{
    return KvSerializer::toUint8Array(serialize(info, appendEndMark),
                                      outBuf,
                                      bufSize,
                                      outLen,
                                      nullTerminate);
}
