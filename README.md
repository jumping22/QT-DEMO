# KvSerializer（QT 4.8.5 / RK3568）

对 `unsigned char` / `uint8_t` 字节流做 Key-Value 序列化 / 反序列化，并支持 `STR_RECORD_INFO` 结构体编解码。

## 协议

| 分隔符 | 含义 |
|--------|------|
| `;` | 字段分隔 |
| `:` | key / value 分隔（`:` 前为 key，后为 value） |
| `/` | 可选整段结束标记 |

中文等非 ASCII 内容按 **UTF-8** 编码存放在 value 中。

## 结构体

```cpp
typedef struct {
    QString name;
    QString concent;
    QString index;
} STR_CONTRAST_INFO;

typedef struct {
    QString name;
    QString patientID;
    QString checkID;
    QString exam;
    int weight;
    QString brithday;
    int height;
} STR_PATIENT_INFO;

typedef struct {
    STR_CONTRAST_INFO cm;
    STR_PATIENT_INFO patient;
    QString injector;
    int datetime;
    QString protocolName;
    int method;
} STR_RECORD_INFO;
```

`STR_RECORD_INFO` 扁平化 key：

`cmName, cmConcent, cmIndex, patientName, patientID, checkID, exam, weight, brithday, height, injector, datetime, protocolName, method`

## 文件

```text
src/KvSerializer.h/.cpp           通用 key:value; 编解码
src/RecordInfo.h                  业务结构体
src/RecordInfoSerializer.h/.cpp   STR_RECORD_INFO 编解码
src/main.cpp                      结构体序列化测试
kvserializer.pro                  qmake 工程（QT 4.8.5）
```

## 用法

```cpp
STR_RECORD_INFO record;
// ... 填充字段 ...

QByteArray out = RecordInfoSerializer::serialize(record);

int len = 0;
uint8_t *u8 = RecordInfoSerializer::toUint8Array(record, &len);

STR_RECORD_INFO restored;
RecordInfoSerializer::deserialize(u8, len, &restored);
delete[] u8;
```

## 编译

```bash
qmake kvserializer.pro
make
./kvserializer_demo
```
