# 实时削峰平滑（C）

数据是**逐点进来**的：每来一个采样立刻输出一个点，把这些点连起来就是一条没有尖峰的平滑曲线。

实时路径不看未来点：

1. **因果开运算**（过去 `2R+1` 个点的 min 再 max）：窄尖峰被削掉  
2. **双极点低通**（由 `--sigma` 决定）：保证相邻输出连成光滑曲线  

大段下落会晚大约 `2R` 个点才跟上，这是不偷看未来的代价。离线对照用 `--offline`。

## 构建

```bash
make
```

## 实时嵌入

```c
#include "smooth.h"

PeakCutStream s;
peakcut_stream_init(&s, 8, 4.0f);

/* 采样回调 / 主循环里：来一个 x，出一个 y */
float y = peakcut_stream_update(&s, x);
/* 把 y 画到曲线上 */
```

热路径无 malloc，每次 O(R)。

## 命令行

```bash
./smooth                      # 把 samples.txt 当成实时流，逐点处理
./smooth --csv
./smooth --svg out.svg
./smooth --live --csv         # 从 stdin 读实时数字，来一个打一个
./smooth --radius 10 --sigma 5
./smooth --offline            # 原来的整段批处理（会用到未来点）
./smooth --self-test
```

管道示例：

```bash
cat samples.txt | tr ',' ' ' | ./smooth --live --csv
```

尖峰还在：加大 `--radius`。曲线不够圆：加大 `--sigma`。
