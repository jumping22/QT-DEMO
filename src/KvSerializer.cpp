#include "KvSerializer.h"

#include <string.h>

KvSerializer::KvSerializer()
{
}

KvSerializer::~KvSerializer()
{
}

void KvSerializer::clear()
{
    m_fields.clear();
    m_keys.clear();
}

bool KvSerializer::deserialize(const unsigned char *data, int length)
{
    if (data == 0) {
        return false;
    }

    QByteArray raw;
    if (length < 0) {
        raw = QByteArray(reinterpret_cast<const char *>(data));
    } else {
        raw = QByteArray(reinterpret_cast<const char *>(data), length);
    }

    return deserialize(raw);
}

bool KvSerializer::deserialize(const QByteArray &data)
{
    return parse(stripEndMark(data));
}

QByteArray KvSerializer::serialize(bool appendEndMark) const
{
    QByteArray out;
    for (int i = 0; i < m_keys.size(); ++i) {
        const QString &key = m_keys.at(i);
        if (!m_fields.contains(key)) {
            continue;
        }
        if (!out.isEmpty()) {
            out.append(';');
        }
        out.append(key.toUtf8());
        out.append(':');
        out.append(m_fields.value(key).toUtf8());
    }
    if (appendEndMark) {
        out.append('/');
    }
    return out;
}

QStringList KvSerializer::toStringList(const QByteArray &serialized)
{
    QStringList result;
    const QByteArray raw = stripEndMark(serialized);
    if (raw.isEmpty()) {
        return result;
    }

    const QList<QByteArray> parts = raw.split(';');
    for (int i = 0; i < parts.size(); ++i) {
        const QByteArray part = parts.at(i).trimmed();
        if (part.isEmpty()) {
            continue;
        }
        result.append(QString::fromUtf8(part.constData(), part.size()));
    }
    return result;
}

QStringList KvSerializer::toStringList(bool appendEndMark) const
{
    return toStringList(serialize(appendEndMark));
}

bool KvSerializer::toUint8Array(const QByteArray &serialized,
                                uint8_t *outBuf,
                                int bufSize,
                                int *outLen,
                                bool nullTerminate)
{
    const int dataLen = serialized.size();
    const int need = dataLen + (nullTerminate ? 1 : 0);

    if (outLen) {
        *outLen = dataLen;
    }

    // 仅查询长度
    if (outBuf == 0) {
        return true;
    }

    if (bufSize < need) {
        return false;
    }

    if (dataLen > 0) {
        memcpy(outBuf, serialized.constData(), static_cast<size_t>(dataLen));
    }
    if (nullTerminate) {
        outBuf[dataLen] = 0;
    }
    return true;
}

uint8_t *KvSerializer::toUint8Array(const QByteArray &serialized,
                                    int *outLen,
                                    bool nullTerminate)
{
    const int dataLen = serialized.size();
    const int need = dataLen + (nullTerminate ? 1 : 0);
    uint8_t *buf = new uint8_t[need];
    if (!toUint8Array(serialized, buf, need, outLen, nullTerminate)) {
        delete[] buf;
        return 0;
    }
    return buf;
}

bool KvSerializer::toUint8Array(uint8_t *outBuf,
                                int bufSize,
                                int *outLen,
                                bool appendEndMark,
                                bool nullTerminate) const
{
    return toUint8Array(serialize(appendEndMark), outBuf, bufSize, outLen, nullTerminate);
}

uint8_t *KvSerializer::toUint8Array(int *outLen,
                                    bool appendEndMark,
                                    bool nullTerminate) const
{
    return toUint8Array(serialize(appendEndMark), outLen, nullTerminate);
}

void KvSerializer::setValue(const QString &key, const QString &value)
{
    if (key.isEmpty()) {
        return;
    }
    if (!m_fields.contains(key)) {
        m_keys.append(key);
    }
    m_fields.insert(key, value);
}

QString KvSerializer::value(const QString &key, const QString &defaultValue) const
{
    if (!m_fields.contains(key)) {
        return defaultValue;
    }
    return m_fields.value(key);
}

bool KvSerializer::contains(const QString &key) const
{
    return m_fields.contains(key);
}

void KvSerializer::remove(const QString &key)
{
    if (!m_fields.contains(key)) {
        return;
    }
    m_fields.remove(key);
    m_keys.removeAll(key);
}

QStringList KvSerializer::keys() const
{
    return m_keys;
}

QMap<QString, QString> KvSerializer::fields() const
{
    return m_fields;
}

int KvSerializer::count() const
{
    return m_fields.size();
}

QByteArray KvSerializer::stripEndMark(const QByteArray &raw)
{
    QByteArray data = raw.trimmed();
    // 协议可选结束符 '/'
    while (data.endsWith('/')) {
        data.chop(1);
        data = data.trimmed();
    }
    return data;
}

bool KvSerializer::parse(const QByteArray &raw)
{
    clear();

    if (raw.isEmpty()) {
        return true;
    }

    // 在字节层按 ';' 拆分字段；':' / ';' 均为 ASCII，不会破坏 UTF-8 多字节字符
    const QList<QByteArray> parts = raw.split(';');
    for (int i = 0; i < parts.size(); ++i) {
        const QByteArray part = parts.at(i).trimmed();
        if (part.isEmpty()) {
            continue;
        }

        const int colonPos = part.indexOf(':');
        if (colonPos < 0) {
            // 非法字段：缺少 key:value 分隔
            clear();
            return false;
        }

        const QByteArray keyBytes = part.left(colonPos).trimmed();
        const QByteArray valueBytes = part.mid(colonPos + 1); // value 保留原样（可含空格）

        if (keyBytes.isEmpty()) {
            clear();
            return false;
        }

        const QString key = QString::fromUtf8(keyBytes.constData(), keyBytes.size());
        const QString value = QString::fromUtf8(valueBytes.constData(), valueBytes.size());
        setValue(key, value);
    }

    return true;
}
