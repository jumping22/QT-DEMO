# 削峰平滑滤波（C）

把窄**尖峰削掉**，再收成一条没有尖峰的平滑曲线。

两级处理：

1. **形态学开运算**（先腐蚀再膨胀）：宽度小于 `2R+1` 的尖峰被削平，两段较宽的下落会被保留。
2. **高斯平滑**：把台阶收成连续曲线。

默认 `R=8`（可削掉约 17 点宽以内的峰），`σ=4`。

## 构建

```bash
make
```

## 使用

```bash
./smooth                 # 处理当前目录 samples.txt
./smooth samples.txt
./smooth --csv
./smooth --svg out.svg
./smooth --radius 10 --sigma 5   # 峰更宽 / 曲线更软
./smooth --self-test
```

嵌入自己的程序：

```c
#include "smooth.h"

float y[N];
peakcut_filter(x, y, N, 8, 4.0f);
```

尖峰还在：加大 `--radius`。曲线还不够圆：加大 `--sigma`。

## 和 1-Euro 的区别

1-Euro 会跟着尖峰走，所以平台上的周期峰还在。本程序默认先削峰，再平滑，平台段会收成一条线。
