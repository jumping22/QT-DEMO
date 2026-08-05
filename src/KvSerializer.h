#ifndef KVSERIALIZER_H
#define KVSERIALIZER_H

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
