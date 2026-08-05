# KvSerializer（QT 4.8.5 / RK3568）

对 `unsigned char` 字节流做 Key-Value 序列化 / 反序列化。

## 协议

| 分隔符 | 含义 |
|--------|------|
| `;` | 字段分隔 |
| `:` | key / value 分隔（`:` 前为 key，后为 value） |
| `/` | 可选整段结束标记 |

示例：

```text
dataType:0;name:张明华;Id:P202607001;exam:胸部CTA;weight:70
```

中文等非 ASCII 内容按 **UTF-8** 编码存放在 value 中。

## 文件

```text
src/KvSerializer.h      类声明
src/KvSerializer.cpp    实现
src/main.cpp            测试用例演示
kvserializer.pro        qmake 工程（QT 4.8.5）
tests/verify_protocol.cpp  无 Qt 依赖的协议校验
```

## 用法

```cpp
#include "KvSerializer.h"

unsigned char buf[] = { /* ... */ };
KvSerializer s;
if (s.deserialize(buf, sizeof(buf))) {
    QString name = s.value("name");   // 张明华
    QString id   = s.value("Id");     // P202607001
}

s.setValue("weight", "70");
QByteArray out = s.serialize();                 // 再序列化
QStringList arr = KvSerializer::toStringList(out); // -> ["dataType:0", "name:...", ...]
```

## 编译（板端 / 交叉编译）

```bash
qmake kvserializer.pro
make
./kvserializer_demo
```

## 测试用例期望输出

输入十六进制：

```text
64 61 74 61 54 79 70 65 3A 30 3B 6E 61 6D 65 3A E5 BC A0 E6 98 8E E5 8D 8E
3B 49 64 3A 50 32 30 32 36 30 37 30 30 31 3B 65 78 61 6D 3A E8 83 B8 E9 83 A8
43 54 41 3B 77 65 69 67 68 74 3A 37 30
```

反序列化结果：

| key | value |
|-----|-------|
| dataType | 0 |
| name | 张明华 |
| Id | P202607001 |
| exam | 胸部CTA |
| weight | 70 |
