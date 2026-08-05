#ifndef KVSERIALIZER_H
#define KVSERIALIZER_H

#include <stdint.h>

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

/**
 * @brief Key-Value 字符串序列化 / 反序列化（QT 4.8.5）
 *
 * 协议格式：
 *   field 之间以 ';' 分隔
 *   每个 field 为 key:value（':' 前为 key，':' 后为 value）
 *   可选以 '/' 作为整段数据结束标记
 *
 * 示例：
 *   dataType:0;name:张明华;Id:P202607001;exam:胸部CTA;weight:70
 */
class KvSerializer
{
public:
    KvSerializer();
    ~KvSerializer();

    /** 清空已解析 / 已设置的字段 */
    void clear();

    /**
     * 反序列化 unsigned char 缓冲区
     * @param data   输入缓冲区（可为 UTF-8 字节流）
     * @param length 字节长度；若 data 以 '\\0' 结尾也可传 -1 表示按 C 字符串处理
     * @return 成功返回 true
     */
    bool deserialize(const unsigned char *data, int length);

    /** 反序列化 QByteArray */
    bool deserialize(const QByteArray &data);

    /**
     * 序列化为协议字节流（UTF-8）
     * @param appendEndMark 是否在末尾追加 '/'
     */
    QByteArray serialize(bool appendEndMark = false) const;

    /**
     * 将序列化结果 QByteArray 转为字符串数组（按 ';' 拆分为各字段）
     * 例："dataType:0;name:张明华" -> ["dataType:0", "name:张明华"]
     * 会去除可选结束标记 '/'，并跳过空字段
     */
    static QStringList toStringList(const QByteArray &serialized);

    /**
     * 将当前对象先序列化，再转为字符串数组
     * @param appendEndMark 序列化时是否追加 '/'（转数组前仍会去掉）
     */
    QStringList toStringList(bool appendEndMark = false) const;

    /**
     * 将序列化结果 QByteArray 拷贝为 uint8_t 字符串数组（调用方提供缓冲区）
     * @param serialized 序列化得到的 QByteArray
     * @param outBuf     输出缓冲区；可为 NULL，仅查询所需长度
     * @param bufSize    outBuf 容量（字节）
     * @param outLen     实际数据长度（不含结尾 '\\0'）；可为 NULL
     * @param nullTerminate 是否在末尾额外写入 '\\0'（需多预留 1 字节）
     * @return 成功返回 true；缓冲区不足或参数非法返回 false
     *
     * 所需最小 bufSize：serialized.size() + (nullTerminate ? 1 : 0)
     */
    static bool toUint8Array(const QByteArray &serialized,
                             uint8_t *outBuf,
                             int bufSize,
                             int *outLen = 0,
                             bool nullTerminate = true);

    /**
     * 分配并返回 uint8_t 字符串数组（内容为序列化字节流）
     * 调用方必须使用 delete[] 释放返回指针
     * @param serialized 序列化得到的 QByteArray
     * @param outLen     实际数据长度（不含结尾 '\\0'）；可为 NULL
     * @param nullTerminate 是否在末尾追加 '\\0'
     * @return 成功返回堆上数组；失败返回 NULL
     */
    static uint8_t *toUint8Array(const QByteArray &serialized,
                                 int *outLen = 0,
                                 bool nullTerminate = true);

    /**
     * 将当前对象序列化后写入调用方提供的 uint8_t 缓冲区
     */
    bool toUint8Array(uint8_t *outBuf,
                      int bufSize,
                      int *outLen = 0,
                      bool appendEndMark = false,
                      bool nullTerminate = true) const;

    /**
     * 将当前对象序列化后分配为 uint8_t 字符串数组（调用方 delete[]）
     */
    uint8_t *toUint8Array(int *outLen = 0,
                          bool appendEndMark = false,
                          bool nullTerminate = true) const;

    /** 设置 / 获取字段 */
    void setValue(const QString &key, const QString &value);
    QString value(const QString &key, const QString &defaultValue = QString()) const;
    bool contains(const QString &key) const;
    void remove(const QString &key);

    /** 全部字段（按插入顺序的 key 列表 + map） */
    QStringList keys() const;
    QMap<QString, QString> fields() const;
    int count() const;

private:
    bool parse(const QByteArray &raw);
    static QByteArray stripEndMark(const QByteArray &raw);

    QMap<QString, QString> m_fields;
    QStringList m_keys; // 保留插入顺序，便于稳定序列化
};

#endif // KVSERIALIZER_H
