# 实时平滑滤波（C）

对流式采样做**因果、低延迟**平滑：每个新样本 O(1) 出结果，不看未来点，不搬窗口。

默认算法是 [1€ Filter](https://gery.casiez.net/1euro/)（CHI 2012）：信号几乎不动时加强平滑压抖动，变化快时自动提高截止频率，跟上陡升/陡降。

## 构建

```bash
make
```

## 使用

```bash
./smooth                 # 处理当前目录 samples.txt
./smooth samples.txt     # 指定输入（逗号/空白分隔的数字）
./smooth --csv           # 只输出 n,raw,smoothed
./smooth --svg out.svg   # 额外写出对比曲线
./smooth --dt 0.02       # 已知采样周期时传入（秒）
./smooth --method aema   # 更轻量的自适应 EMA
./smooth --method ema --alpha 0.3
./smooth --self-test
```

把滤波器嵌进自己的采样循环：

```c
#include "smooth.h"

OneEuro f;
one_euro_init(&f, 0.02f, 0.08f, 1.0f);

/* 每来一个采样调用一次，dt 为采样间隔（未知采样率时用 1） */
float y = one_euro_update(&f, x, 1.0f);
```

热路径里只有几次加减乘和一次绝对值，无分配、无查找。

## 方法怎么选

| `--method` | 每样本代价 | 特点 |
|---|---|---|
| `oneeuro`（默认） | ~15 FLOP | 按速度调截止频率，平台抖动小、沿边延迟低 |
| `aema` | ~8 FLOP | 按 \|x-y\| 提高 alpha，实现更短，适合 MCU |
| `ema` | ~3 FLOP | 固定 alpha，最简单，快变沿会明显拖尾 |

## 调参（1-Euro）

采样间隔按 `dt = 1`（一个样本一个时间单位）时：

- `--min-cutoff` 默认 `0.02`：越小，平台段越平滑，但慢变化也会更钝。
- `--beta` 默认 `0.08`：越大，大跳变跟得越紧，噪声大时可能略跟手抖。
- `--d-cutoff` 默认 `1.0`：导数预滤波，一般不用动。

输入序列里有两段十几的陡降；默认参数会跟过去，同时把平台上 ±2～5 的抖动压下去。
