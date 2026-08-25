# 压力实时平滑滤波（C / FreeRTOS）

输入：**一个压力采样点**。输出：**平滑后的压力值**。来一个立刻出一个，O(1)，不看未来，无动态分配，可直接拷进 FreeRTOS 工程。

## 移植（只需两个文件）

把 `pressure_filter.h` 和 `pressure_filter.c` 加入工程，链接 `libm`（`-lm`）。不要把 `smooth.c` / `main.c` 下到 MCU——那些是 PC 演示和离线接口。

```c
#include "pressure_filter.h"

static PressureFilter s_press;   /* 每个传感器一个实例，静态即可 */

void app_init(void)
{
    pressure_filter_init(&s_press);          /* hold=16, smoothness=5 */
    /* pressure_filter_init_ex(&s_press, 16, 5.0f); */
}

void sensor_task(void *arg)
{
    (void)arg;
    for (;;) {
        float raw = read_pressure();                 /* ADC / I2C / SPI */
        float filtered = pressure_filter_update(&s_press, raw);
        /* 用 filtered 做显示、控制、上报 */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### FreeRTOS 注意

- 滤波器本身**不调用** FreeRTOS API，任务里怎么调都可以。
- **同一个** `PressureFilter` 不要同时在两个任务或 ISR 里 `update`。一个任务独占，或外面加 mutex。
- 状态全在结构体里（约 0.3 KB），不要 `malloc`。
- 用 `float` + `sqrtf`。Cortex-M4F/M7 请打开硬件 FPU。
- 采样周期尽量均匀；算法按「点」工作，不读时间戳。

### 参数

- `hold`（默认 16）：中等变化要持续这么多样点才跟。应大于周期齿的宽度。
- `smoothness`（默认 5）：越大转折越圆、略慢。

## 算法（简述）

1. **门控**：死区内的齿不跟；大跳变连续 2 点确认；确认后粘滞跟踪。反向（周期齿）或真走平才解锁；升降中途的短暂停不锁成台阶。
2. **min-jerk**：每步用五次多项式走向 `(目标, 目标速度)`，限加加速度，输出圆滑、无斜直线。

## PC 演示

```bash
make
./smooth --self-test
./smooth --csv samples.txt
```
