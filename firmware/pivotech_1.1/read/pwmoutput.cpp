#ifndef _PWMOUTPUT_CPP_
#define _PWMOUTPUT_CPP_

#include "pico/stdlib.h"
#include "menu/ssd1306.hpp"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "menu/core.hpp"
#include "read/uartconfig.hpp"
#include "CONFIG_FLO.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdarg>

// 如果需要根据硬件版本限制 TC 频率，使用此函数统一处理。
static inline double enforce_tc_max_freq(double freq)
{
    extern int PIVOhw;
    if (PIVOhw == 1)
    {
        const double TC_HW1_MAX_HZ = 5000.0;
        if (freq > TC_HW1_MAX_HZ)
            return TC_HW1_MAX_HZ;
    }
    return freq;
}

// 频率区间-频率精度-占空比精度规则表，便于调试与权衡
struct PrecisionRule
{
    double f_min;
    double f_max;
    double freq_tol; // 允许/期望的频率精度（绝对误差）
    double duty_tol; // 期望的占空比分辨率（百分数转小数）
};

static constexpr PrecisionRule kPrecisionRules[] = {
    {10.0, 100.0, 0.01, 0.0001},             // 10~100 Hz，频率精度 0.01 Hz，占空精度 0.01%
    {100.0, 1000.0, 0.1, 0.0001},            // 100~1 kHz，频率精度 0.1 Hz，占空精度 0.01%
    {1000.0, 10000.0, 1.0, 0.001},           // 1~10 kHz，频率精度 1 Hz，占空精度 0.01%
    {10000.0, 100000.0, 100.0, 0.001},       // 10~100 kHz，频率精度 1 Hz，占空精度 0.1%
    {100000.0, 1000000.0, 1000.0, 0.01},     // 100 kHz~1 MHz，频率精度 1000 Hz，占空精度 1%
    {1000000.0, 20000000.0, 100000.0, 0.10}, // 1~10 MHz，频率精度 1000 Hz，占空精度 10%
};

// 状态变量
bool pwmoutitemfunctionIsRunning = false;
bool pwmoutitem_infunction = false;
bool pwmoutitem_outfuction = false;
// 新的配置窗口运行状态
bool pwmoutitemConfigIsRunning = false;
bool pwmoutitemConfigSubIsRunning = false;
// 顶层窗口的光标：0=类型(二左), 1=模式(二右), 2=数值(三)
uint8_t pwmoutConfigSelected = 0;
// 子窗口内三选一光标（类型/模式时使用）
uint8_t pwmoutConfigChoiceIndex = 0;
// 子窗口编辑上下文与备份
static uint8_t pwmoutConfigEditingControl = 0; // 0/1/2
static uint8_t pwmoutConfigChoiceBackup = 0;
static uint16_t pwmoutConfigValueBackup = 0;

// 参数变量
double param_a_f = 1234.56;
double param_b_f = 2000.00;
double param_c_f = 50.00;
double param_d_f = 600000.00;

double param_a_duty = 0.1234;
double param_b_duty = 0.20;
double param_c_duty = 0.5;
double param_d_duty = 0.5;

int page2YPosStart = 16;
int nowselect_pwmoutput_item = 0;
static uint32_t last_pc_sync_time = 0; // 上次PC同步时间

static inline PrecisionRule pick_precision_rule(double f_target)
{
    for (const auto &r : kPrecisionRules)
    {
        if (f_target >= r.f_min && f_target < r.f_max)
            return r;
    }
    // 超界时沿用最后一档
    return kPrecisionRules[std::size(kPrecisionRules) - 1];
}

// 代价函数惩罚参数：当归一化 score 超过阈值时，施加重惩罚
static constexpr double kScorePenaltyThreshold = 0.40; // 超过该阈值开始重惩罚
static constexpr double kScorePenaltyFactor = 5000.0;  // 超部分的放大量级，可按需要调大/调小

// 分频格式约束：8.4 固定小数（分频 = k / 16），硬件步进为 1/16
static constexpr double kClkdivScale = 16.0; // 8.4 格式的分频量化因子

static inline double heavy_penalize(double score)
{
    // 设计为连续函数：阈值前保持原值，阈值后线性大幅放大
    if (score <= kScorePenaltyThreshold)
        return score;
    return score + (score - kScorePenaltyThreshold) * (score - kScorePenaltyThreshold) * kScorePenaltyFactor;
}

// 结构体定义
typedef struct
{
    double *param;       // 参数指针（统一为 double 以兼容频率/duty）
    double paramBackup;  // 参数备份
    double pwmitem_step; // 步进值
    char *title;         // 标题
} ReadItem_t;

struct PwmOutputConfig
{
    uint8_t output_mode; // 0: PWM Output, 1: Freq Pulse (PFM), 2: VREF Output
    uint8_t run_mode;    // 0: Continuous, 1: Pulse Count, 2: Timer-Based
    uint16_t cfg_value;  // Pulse Count 或 ms/pulse（根据 run_mode 解释）
    double cfg_step;     // 数值步进（第三行选择）
};

typedef struct
{
    bool open;
    ReadItem_t *duty;
    ReadItem_t *frec;
    PwmOutputConfig cfg; // PWM 输出配置存放于通道层级，便于后续读取
} PWMitem_t;

struct PwmCalcResult
{
    uint32_t wrap;
    uint16_t clkdiv_raw; // 12bit 整数分频值（实际分频 = clkdiv_raw / 16）
    double clkdiv;
    uint32_t level;
    double f_real;
    double duty_real;
    double cost_freq;
    double cost_duty;
};

struct PwmRunJob
{
    bool active{false};
    uint32_t total_us{0};
    alarm_id_t alarm_id{0};
    absolute_time_t end_time{};
    bool flash_pending{false};
};

ReadItem_t menuItem_TA_f = {
    .param = &param_a_f,
    .paramBackup = param_a_f,
    .pwmitem_step = 10,
    .title = "TA频率"};

ReadItem_t menuItem_TA_duty = {
    .param = &param_a_duty,
    .paramBackup = param_a_duty,
    .pwmitem_step = 0.01,
    .title = "TA占空"};
ReadItem_t menuItem_TB_f = {
    .param = &param_b_f,
    .paramBackup = param_b_f,
    .pwmitem_step = 10,
    .title = "TB频率"};
ReadItem_t menuItem_TB_duty = {
    .param = &param_b_duty,
    .paramBackup = param_b_duty,
    .pwmitem_step = 0.01,
    .title = "TB占空"};
ReadItem_t menuItem_TC_f = {
    .param = &param_c_f,
    .paramBackup = param_c_f,
    .pwmitem_step = 10,
    .title = "TC频率"};
ReadItem_t menuItem_TC_duty = {
    .param = &param_c_duty,
    .paramBackup = param_c_duty,
    .pwmitem_step = 0.01,
    .title = "TC占空"};
ReadItem_t menuItem_TD_f = {
    .param = &param_d_f,
    .paramBackup = param_d_f,
    .pwmitem_step = 10,
    .title = "TD频率"};
ReadItem_t menuItem_TD_duty = {
    .param = &param_d_duty,
    .paramBackup = param_d_duty,
    .pwmitem_step = 0.01,
    .title = "TD占空"};

PWMitem_t PWMitem[4] = {
    {.open = false, .duty = &menuItem_TA_duty, .frec = &menuItem_TA_f, .cfg = {.output_mode = 0, .run_mode = 0, .cfg_value = 100, .cfg_step = 1.0}},
    {.open = false, .duty = &menuItem_TB_duty, .frec = &menuItem_TB_f, .cfg = {.output_mode = 0, .run_mode = 0, .cfg_value = 100, .cfg_step = 1.0}},
    {.open = false, .duty = &menuItem_TC_duty, .frec = &menuItem_TC_f, .cfg = {.output_mode = 0, .run_mode = 0, .cfg_value = 100, .cfg_step = 1.0}},
    {.open = false, .duty = &menuItem_TD_duty, .frec = &menuItem_TD_f, .cfg = {.output_mode = 0, .run_mode = 0, .cfg_value = 100, .cfg_step = 1.0}}};

PwmCalcResult pwm_results[4];
PwmRunJob pwm_jobs[4];

uint16_t pwmitem_height = 15;
uint16_t pwmitem_ChangeVal_Width = 84;
uint16_t pwmfont_height = 12;
uint16_t pwmfont_width = 7;
static uint8_t itemHeightOffset = (pwmitem_height - pwmfont_height) / 2 + 1;
uint8_t temlayer = layer + 1;
static double pwmitem_step = 0.1; // 默认步进值

ReadItem_t *item;
bool duty1_freq0;

extern "C"
{
    void Update_PwmOutput();
}

static inline uint32_t ms_from_us(int64_t us)
{
    if (us <= 0)
        return 0;
    return (uint32_t)((us + 999) / 1000);
}

static inline bool short_job_flash(uint8_t idx, double &elapsed_us)
{
    if (!(pwm_jobs[idx].active) || pwm_jobs[idx].total_us == 0 || pwm_jobs[idx].total_us >= 100000)
        return false;

    int64_t us_left = absolute_time_diff_us(get_absolute_time(), pwm_jobs[idx].end_time);
    if (us_left < 0)
        return false;

    double start_us = (double)pwm_jobs[idx].total_us;
    elapsed_us = start_us - (double)us_left;
    if (elapsed_us < 0.0)
        elapsed_us = 0.0;
    return true;
}

static inline double channel_progress(uint8_t idx)
{
    if (pwm_jobs[idx].active && pwm_jobs[idx].total_us > 0)
    {
        int64_t us_left = absolute_time_diff_us(get_absolute_time(), pwm_jobs[idx].end_time);
        double remain_us = (double)std::max<int64_t>(0, us_left);
        double p = remain_us / (double)pwm_jobs[idx].total_us;
        if (p < 0.0)
            return 0.0;
        if (p > 1.0)
            return 1.0;
        return p;
    }
    return PWMitem[idx].open ? 1.0 : 0.0;
}

static inline void cancel_job_timer(uint8_t idx)
{
    if (pwm_jobs[idx].alarm_id)
    {
        cancel_alarm(pwm_jobs[idx].alarm_id);
        pwm_jobs[idx].alarm_id = 0;
    }
}

static inline const char *channel_name(uint8_t idx)
{
    switch (idx)
    {
    case 0:
        return "TA";
    case 1:
        return "TB";
    case 2:
        return "TC";
    default:
        return "TD";
    }
}

static int64_t pwm_job_alarm_cb(alarm_id_t id, void *user_data)
{
    uint8_t idx = (uint8_t)(uintptr_t)user_data;
    PWMitem[idx].open = false;
    extern bool send_pc_flag;
    send_pc_flag = true; // 触发向上位机更新数据
    Update_PwmOutput();  // 相同延时位置
    pwm_jobs[idx].active = false;
    pwm_jobs[idx].total_us = 0;
    pwm_jobs[idx].alarm_id = 0;

    return 0; // one-shot
}

void update_item_select();

// 浮点近似比较
static inline bool approx_eq(double a, double b, double eps = 1e-12) { return fabs(a - b) <= eps; }

// 基于规则的步进阶梯：从最小精度开始按10倍递增到最大
static inline std::vector<double> make_steps(double min_step, double max_step)
{
    std::vector<double> steps;
    if (min_step <= 0.0)
        min_step = 1e-6;
    double s = min_step;
    for (int i = 0; i < 20 && s <= max_step + 1e-12; ++i)
    {
        steps.push_back(s);
        s *= 10.0;
    }
    std::sort(steps.begin(), steps.end());
    steps.erase(std::unique(steps.begin(), steps.end(), [](double a, double b)
                            { return approx_eq(a, b); }),
                steps.end());
    return steps;
}

// 在步进阶梯中找到当前步进的索引
static inline int find_step_index(double *steps, double current)
{
    for (int i = 0; i < 9; ++i)
    {
        if (approx_eq(steps[i], current) || fabs(steps[i] - current) < 1e-9)
            return i;
    }
    int best = 0;
    double best_diff = std::numeric_limits<double>::infinity();
    for (int i = 0; i < 9; ++i)
    {
        double d = fabs(steps[i] - current);
        if (d < best_diff)
        {
            best_diff = d;
            best = i;
        }
    }
    return best;
}

static inline PwmCalcResult calc_pwm_params(float f_clk, float f_target, float duty_target, uint8_t output_mode)
{
    PwmCalcResult r{};
    // n = TOP+1，k = CLKDIV*kClkdivScale（8.4 固定小数，步进 1/16）；约束：n∈[1,65536]，k∈[16,4095]
    const uint32_t N_MIN = 1u;
    const uint32_t N_MAX = 65536u;
    const uint32_t K_MIN = 16u;          // 8.4 分频的最小量化值，对应 clkdiv = 1.0
    const uint32_t K_MAX = 4095u;        // 8.4 分频的最大量化值，对应 clkdiv ≈ 255.9375
    const uint32_t DELTA = 256u;         // 微调窗口宽度（扩大搜索范围以覆盖更多解）
    const uint32_t COARSE_N_STEP = 512u; // 额外粗扫步长（n 方向）
    const uint32_t COARSE_K_STEP = 64u;  // 额外粗扫步长（k 方向）

    if (f_target <= 0.0f)
    {
        r.wrap = 0;
        r.clkdiv = 1.0f;
        r.level = 0;
        r.f_real = 0.0f;
        r.duty_real = 0.0f;
        return r;
    }

    // VREF 模式直接返回占位结果，不参与计算
    if (output_mode == 2)
    {
        // DC/VREF mode still needs a valid PWM base, otherwise wrap=0 keeps output low.
        // Use a fixed low PWM frequency and force 100% duty to emulate DC high level.
        const double f_dc = 1000.0;  // 1kHz carrier for filtered DC output
        const uint32_t n_dc = 1000u; // wrap = 999
        uint32_t k_dc = (uint32_t)llround((kClkdivScale * (double)f_clk) / (f_dc * (double)n_dc));
        if (k_dc < K_MIN)
            k_dc = K_MIN;
        if (k_dc > K_MAX)
            k_dc = K_MAX;

        r.wrap = n_dc - 1u;
        r.clkdiv_raw = (uint16_t)k_dc;
        r.clkdiv = (double)k_dc / kClkdivScale;
        r.level = r.wrap;
        r.f_real = (double)f_clk / (r.clkdiv * (double)n_dc);
        r.duty_real = 1.0;
        r.cost_freq = 0.0;
        r.cost_duty = 0.0;
        return r;
    }

    // Square Wave 模式要求 wrap > 0，因此最小 n 取 2
    const uint32_t n_lower_bound = (output_mode == 1) ? 2u : N_MIN;

    // 目标量 R = scale * f_clk / f_target，寻找使 k*n 逼近 R 的整数对（scale = 16）
    const double P = (double)f_clk / (double)f_target;
    const double R = kClkdivScale * P; // 目标：最小化 |k*n - R|

    struct Best
    {
        uint32_t k{K_MIN};
        uint32_t n{N_MIN};
        double err{std::numeric_limits<double>::infinity()};
        double cost{std::numeric_limits<double>::infinity()};
        double cost_freq{std::numeric_limits<double>::infinity()};
        double cost_duty{std::numeric_limits<double>::infinity()};
    } best;

    PrecisionRule rule = pick_precision_rule(f_target);

    // 评估当前 (k,n) 的误差；同误差优先 n 大以提升分辨率
    auto consider = [&](uint32_t k, uint32_t n)
    {
        if (output_mode == 1 && n <= 1)
            return; // 方波要求 wrap != 0
        double err = fabs((double)k * (double)n - R);
        double clkdiv_d = (double)k / kClkdivScale;
        double f_real = (double)f_clk / (clkdiv_d * (double)n);
        double freq_err = fabs(f_real - (double)f_target);
        double cost_freq = 0.0;
        double cost_duty = 0.0;
        double cost = 0.0;

        if (output_mode == 1)
        {
            // 方波仅关注频率绝对误差
            cost_freq = freq_err;
            cost_duty = 0.0;
            cost = cost_freq;
        }
        else
        {
            double freq_score = freq_err / rule.freq_tol;
            double duty_step = 1.0 / (double)n;
            double duty_score = duty_step / rule.duty_tol;
            // 对超过阈值的 score 进行大量惩罚，促使解优先满足区间精度要求
            cost_freq = heavy_penalize(freq_score);
            cost_duty = heavy_penalize(duty_score);
            cost = cost_freq + cost_duty;
        }

        if (cost < best.cost || (fabs(cost - best.cost) <= 1e-9 && (err < best.err || (fabs(err - best.err) <= 1e-9 && n > best.n))))
        {
            best.k = k;
            best.n = n;
            best.err = err;
            best.cost = cost;
            best.cost_freq = cost_freq;
            best.cost_duty = cost_duty;
        }
    };

    // 限制 n、k 在合法范围
    auto clamp_n = [&](int64_t v) -> uint32_t
    {
        if (v < (int64_t)n_lower_bound)
            return n_lower_bound;
        if (v > (int64_t)N_MAX)
            return N_MAX;
        return (uint32_t)v;
    };

    auto clamp_k = [&](int64_t v) -> uint32_t
    {
        if (v < (int64_t)K_MIN)
            return K_MIN;
        if (v > (int64_t)K_MAX)
            return K_MAX;
        return (uint32_t)v;
    };

    // 以中心 n 做 ±DELTA 微调，k 按 R/n 四舍五入取最优
    auto search_n_window = [&](uint32_t center)
    {
        uint32_t n_min = center > DELTA ? center - DELTA : n_lower_bound;
        uint32_t n_max = std::min<uint32_t>(N_MAX, center + DELTA);
        for (uint32_t n = n_min; n <= n_max; ++n)
        {
            uint64_t k64 = llround(R / (double)n);
            k64 = clamp_k((int64_t)k64);
            consider((uint32_t)k64, n);
        }
    };

    // 以中心 k 做 ±DELTA 微调，n 按 R/k 四舍五入取最优
    auto search_k_window = [&](uint32_t center)
    {
        uint32_t k_min = center > DELTA ? center - DELTA : K_MIN;
        uint32_t k_max = std::min<uint32_t>(K_MAX, center + DELTA);
        for (uint32_t k = k_min; k <= k_max; ++k)
        {
            uint64_t n64 = llround(R / (double)k);
            n64 = clamp_n((int64_t)n64);
            consider(k, (uint32_t)n64);
        }
    };

    // 候选生成：对 R 做有界连分数展开，遍历收敛分数 h/k 作为种子，再在其附近微调
    {
        std::vector<uint64_t> coeffs;
        double x = R;
        for (int i = 0; i < 24; ++i) // 限制阶数，防止溢出
        {
            uint64_t a = (uint64_t)floor(x);
            coeffs.push_back(a);
            double frac = x - (double)a;
            if (frac < 1e-12)
                break;
            // 若值过大，继续展开意义不大
            if (x > 1e18)
                break;
            x = 1.0 / frac;
        }

        uint64_t h_m2 = 0, h_m1 = 1;
        uint64_t k_m2 = 1, k_m1 = 0;
        for (size_t i = 0; i < coeffs.size(); ++i)
        {
            // h/k 是收敛分数
            long double h = (long double)coeffs[i] * (long double)h_m1 + (long double)h_m2;
            long double k = (long double)coeffs[i] * (long double)k_m1 + (long double)k_m2;

            if (h > (long double)K_MAX * 4.0L && k > (long double)N_MAX * 4.0L)
            {
                // 过大时提前结束
                break;
            }

            // 递推到下一阶收敛分数
            h_m2 = h_m1;
            h_m1 = (uint64_t)h;
            k_m2 = k_m1;
            k_m1 = (uint64_t)k;

            if (h_m1 <= K_MAX && k_m1 <= N_MAX)
            {
                // 以当前收敛分数为中心，分别对 n、k 开窗搜索
                search_n_window((uint32_t)k_m1);
                search_k_window((uint32_t)h_m1);
            }
        }
    }

    // 兜底：用极值分频推算的 n，再做窗口搜索，避免遗漏
    uint32_t seed_n_min_div = clamp_n((int64_t)llround(R / (double)K_MIN));
    uint32_t seed_n_max_div = clamp_n((int64_t)llround(R / (double)K_MAX));
    search_n_window(seed_n_min_div);
    search_n_window(seed_n_max_div);
    search_n_window(N_MAX);
    search_n_window(n_lower_bound);

    // 额外粗扫：在 n、k 维度以较大步长撒点，避免错过长周期或高分频组合
    for (uint32_t n = N_MIN; n <= N_MAX; n += COARSE_N_STEP)
    {
        uint64_t k64 = llround(R / (double)n);
        k64 = clamp_k((int64_t)k64);
        consider((uint32_t)k64, n);
    }

    for (uint32_t k = K_MIN; k <= K_MAX; k += COARSE_K_STEP)
    {
        uint64_t n64 = llround(R / (double)k);
        n64 = clamp_n((int64_t)n64);
        consider(k, (uint32_t)n64);
    }

    // 结果赋值
    uint32_t n = best.n;
    uint32_t k = best.k;

    uint32_t wrap = n - 1u;
    uint32_t level = 0;
    if (output_mode == 1)
    {
        level = (wrap + 1u) / 2u; // 方波 50% 占空
    }
    else
    {
        double lvl_d = llround((double)(1.0 - duty_target) * (double)n);
        if (lvl_d < 0.0)
            lvl_d = 0.0;
        if (lvl_d > (double)wrap)
            lvl_d = (double)wrap;
        level = (uint32_t)lvl_d;
    }

    float clkdiv = (float)((double)k / kClkdivScale);
    float f_real = (float)((double)f_clk / ((double)clkdiv * (double)n));
    float duty_real = (float)((double)level / (double)n);

    r.wrap = wrap;
    r.clkdiv_raw = (uint16_t)k; // 返回 12bit 整数分频值（硬件使用）；另行提供浮点形式方便日志
    r.clkdiv = clkdiv;
    r.level = level;
    r.f_real = f_real;
    r.duty_real = duty_real;
    r.cost_freq = best.cost_freq;
    r.cost_duty = best.cost_duty;
    return r;
}

void Update_PwmOutput()
{

    // 提取公共参数和函数，减少重复代码

    auto update_channel = [&](bool is_open, uint gpio_pin, const PwmCalcResult &cfg, uint8_t output_mode)
    {
        // 无论开/关，都把本通道 PWM slice 的分频与周期写为计算值 cfg。
        // 原因：原先只在“开”分支写入，关闭态会残留启动时 LED 初始化遗留的
        // 分频/周期（slice0 通常为 1kHz / wrap=1000）。TD 是反相 B 通道，
        // 关闭需要 CC==WRAP 才输出 GND；此时 CC 被写成 cfg.wrap 而实际 WRAP
        // 仍是残留值，导致进入程序时 TD 输出残留频率+错误占空比（1kHz/约80%）
        // 而非 GND。手动开→关之所以正常，是因为“开”分支先写入了 WRAP=cfg.wrap。
        uint slice = pwm_gpio_to_slice_num(gpio_pin);
        // pwm_clear_irq(slice);
        pwm_set_clkdiv(slice, cfg.clkdiv);
        pwm_set_wrap(slice, cfg.wrap);

        if (is_open)
        {

            if (output_mode == 2)
            {
                // DC 100输出
                if (gpio_pin != TDPWMOUT)
                {
                    pwm_set_gpio_level(gpio_pin, 0);
                }
                else
                {
                    pwm_set_gpio_level(gpio_pin, cfg.wrap);
                }

            }
            else
            {
                if (gpio_pin != TDPWMOUT)
                {
                    // 相位反转
                    pwm_set_gpio_level(gpio_pin, cfg.wrap - cfg.level);
                }
                else
                {
                    pwm_set_gpio_level(gpio_pin, cfg.level);
                }

                
            }

            //LED 同步状态
            if (gpio_pin == TDPWMOUT)
                {
                    // 频率占空比同步给LEDTDPIN
                    uint slice_tdpwm = pwm_gpio_to_slice_num(LEDTDPIN);
                    // pwm_clear_irq(slice_tdpwm);
                    pwm_set_clkdiv(slice_tdpwm, cfg.clkdiv);
                    pwm_set_wrap(slice_tdpwm, cfg.wrap);
                    pwm_set_gpio_level(LEDTDPIN, cfg.level); // LEDTDPIN 输出与 TDPWMOUT 反相的占空比，以便区分显示（占空比越大 LED 越暗）
                    if (output_mode == 2)
                    {
                        // DC 100输出
                        pwm_set_gpio_level(LEDTDPIN, cfg.wrap);
                    }
                }
                else if (gpio_pin == TCPWMOUT)
                {
                    uint slice_tcpwm = pwm_gpio_to_slice_num(LEDTCPIN);
                    // pwm_clear_irq(slice_tcpwm);
                    pwm_set_clkdiv(slice_tcpwm, cfg.clkdiv);
                    pwm_set_wrap(slice_tcpwm, cfg.wrap);
                    pwm_set_gpio_level(LEDTCPIN, cfg.wrap - cfg.level); // LEDTCPIN 输出与 TCPWMOUT 反相的占空比，以便区分显示（占空比越大 LED 越暗）
                    if (output_mode == 2)
                    {
                        // DC 100输出
                        pwm_set_gpio_level(LEDTCPIN, cfg.wrap);
                    }
                }
        }
        else
        {

            // 关闭通道时输出低电平
            if (gpio_pin == TDPWMOUT)
            {
                pwm_set_gpio_level(gpio_pin, cfg.wrap);
            }
            else
            {
                pwm_set_gpio_level(gpio_pin, 0);
            }

            // LED
            if (gpio_pin == TDPWMOUT)
            {
                pwm_set_gpio_level(LEDTDPIN, 0);
            }
            else if (gpio_pin == TCPWMOUT)
            {
                pwm_set_gpio_level(LEDTCPIN, 0);
            }
        }
    };

    // 分别更新四个通道（使用预计算的 pwm_results）
    update_channel(PWMitem[0].open, TAGENPIN, pwm_results[0], PWMitem[0].cfg.output_mode);
    update_channel(PWMitem[1].open, TBGENPIN, pwm_results[1], PWMitem[1].cfg.output_mode);
    update_channel(PWMitem[2].open, TCPWMOUT, pwm_results[2], PWMitem[2].cfg.output_mode);
    update_channel(PWMitem[3].open, TDPWMOUT, pwm_results[3], PWMitem[3].cfg.output_mode);

    // 判定在哪个区间
    uint8_t ta_rule_idx = 0;
    uint8_t tb_rule_idx = 0;
    uint8_t tc_rule_idx = 0;
    uint8_t td_rule_idx = 0;
    for (uint8_t i = 0; i < std::size(kPrecisionRules); i++)
    {
        if (param_a_f >= kPrecisionRules[i].f_min && param_a_f < kPrecisionRules[i].f_max)
        {
            ta_rule_idx = i;
        }
        if (param_b_f >= kPrecisionRules[i].f_min && param_b_f < kPrecisionRules[i].f_max)
        {
            tb_rule_idx = i;
        }
        if (param_c_f >= kPrecisionRules[i].f_min && param_c_f < kPrecisionRules[i].f_max)
        {
            tc_rule_idx = i;
        }
        if (param_d_f >= kPrecisionRules[i].f_min && param_d_f < kPrecisionRules[i].f_max)
        {
            td_rule_idx = i;
        }
    }
}

void Update_pwmconfig()
{

    const double clock_freq = 125000000.0; // 125MHz系统时钟
    uint8_t idx = nowselect_pwmoutput_item / 3;

    // 对 TC 通道（索引2）应用硬件限制（如果有）
    double f_target = *PWMitem[idx].frec->param;
    if (idx == 2)
    {
        double f_capped = enforce_tc_max_freq(f_target);
        if (f_capped != f_target)
        {
            // 同步回存储，避免界面显示与实际不一致
            *PWMitem[idx].frec->param = f_capped;
            f_target = f_capped;
        }
    }

    pwm_results[idx] = calc_pwm_params(clock_freq, f_target, *PWMitem[idx].duty->param, PWMitem[idx].cfg.output_mode);
    if (PWMitem[idx].open)
    {
        Update_PwmOutput();
    }
}

void pwmoutputitem_init()
{
    update_item_select(); // 更新item指针和duty1_freq0标志

    if (duty1_freq0) // 根据频率限定
    {

        PrecisionRule rule = pick_precision_rule(*PWMitem[nowselect_pwmoutput_item / 3].frec->param);
        double min_duty_step = (double)rule.duty_tol;

        pwmitem_step = item->pwmitem_step;

        double duty_steps[4] = {0.0001, 0.001, 0.01, 0.1};
        int idx = find_step_index(duty_steps, std::max(pwmitem_step, min_duty_step));
        pwmitem_step = duty_steps[idx];
    }
    else
    { // freq 直接使用存储数据
        pwmitem_step = item->pwmitem_step;
    }
}

#include "read/functions.hpp"

void pwmoutputitemChangeValue()
{

    pwmoutputitem_init();

    // Odutyation move reaction
    if (opnEnter)
    {
        if (duty1_freq0)
        {
            // 获取duty的最小步进
            PrecisionRule rule = pick_precision_rule(*PWMitem[nowselect_pwmoutput_item / 3].frec->param);
            double min_duty_step = (double)rule.duty_tol;

            // // 数值精度更新仅按当前频率区间最小精度进行量化
            double step = min_duty_step;
            double q = round((double)(*item->param) / step) * step;
            *item->param = q;
        }
        else
        {
            // 频率不做处理
        }
        item->paramBackup = *item->param;
        pwmoutitemfunctionIsRunning = false;
        pwmoutitem_outfuction = 1;
        item->pwmitem_step = pwmitem_step;
        ResetOpn();
        // 保存频率时，更新系统占空比到当前可实现精度（确认后才应用到PWM）
        Update_pwmconfig();

        extern bool send_pc_flag;
        send_pc_flag = true; // 触发向上位机更新数据

        // 结束Update时 还会有一次动画 进行退出刷新
        return;
    }
    if (opnExit)
    {
        *item->param = item->paramBackup;
        pwmoutitemfunctionIsRunning = false;
        pwmoutitem_outfuction = 1;
        item->pwmitem_step = pwmitem_step;
        ResetOpn();
        // 结束Update时 还会有一次动画 进行退出刷新
        return;
    }

    // 根据数据类型进行分类
    if (duty1_freq0)
    {
        // 获取duty的最小步进
        PrecisionRule rule = pick_precision_rule(*PWMitem[nowselect_pwmoutput_item / 3].frec->param);
        double min_duty_step = (double)rule.duty_tol;

        double duty_steps[4] = {0.0001, 0.001, 0.01, 0.1};
        int idx = find_step_index(duty_steps, pwmitem_step);
        int min_idx = find_step_index(duty_steps, min_duty_step);
        // printf("min_duty_step: %.6f, min_idx: %d \n", min_duty_step, min_idx);

        if (opnCtrlUp)
        {
            idx = std::min(idx + 1, 3), 3;
            pwmitem_step = duty_steps[idx];
            item->pwmitem_step = pwmitem_step;
        }
        if (opnCtrlDown)
        {
            idx = std::max(std::max(idx - 1, 0), min_idx);
            pwmitem_step = duty_steps[idx];
            item->pwmitem_step = pwmitem_step;
        }
        // get_pwmitem_step();
    }
    else if (!duty1_freq0)
    {
        // 频率步进按照规则生成的阶梯（上限设定为100000）
        double freq_steps[9] = {0.01, 0.1, 1, 10, 100, 1000, 10000, 100000, 1000000};
        int idx = find_step_index(freq_steps, pwmitem_step);
        if (opnCtrlUp)
        {
            idx = std::min(idx + 1, 8);
            pwmitem_step = freq_steps[idx];
            item->pwmitem_step = pwmitem_step;
        }
        if (opnCtrlDown)
        {
            idx = std::max(idx - 1, 0);
            pwmitem_step = freq_steps[idx];
            item->pwmitem_step = pwmitem_step;
        }
    }

    if (opnUp)
    {
        *item->param += pwmitem_step;
        if (duty1_freq0)
        {
            // 获取duty的最小步进
            PrecisionRule rule = pick_precision_rule(*PWMitem[nowselect_pwmoutput_item / 3].frec->param);
            double min_duty_step = (double)rule.duty_tol;

            // // 数值精度更新仅按当前频率区间最小精度进行量化
            double step = min_duty_step;
            double q = round((double)(*item->param) / step) * step;
            *item->param = q;
        }
        else
        {
            // 频率不做处理
        }
        // 非确认不更新PWM，仅更新数值
    }

    if (opnDown)
    {
        *item->param -= pwmitem_step;
        if (duty1_freq0)
        {
            // 获取duty的最小步进
            PrecisionRule rule = pick_precision_rule(*PWMitem[nowselect_pwmoutput_item / 3].frec->param);
            double min_duty_step = (double)rule.duty_tol;

            // // 数值精度更新仅按当前频率区间最小精度进行量化
            double step = min_duty_step;
            double q = round((double)(*item->param) / step) * step;
            *item->param = q;
        }
        else
        {
            // 频率不做处理
        }
        // 非确认不更新PWM，仅更新数值
    }

    if (duty1_freq0)
    {
        // 0~100%
        if (*item->param > 1.0)
        {
            *item->param = 1.0;
        }
        if (*item->param < 0.0)
        {
            *item->param = 0.0;
        }
    }
    else if (!duty1_freq0)
    {
        // 10Hz ~ 12MHz
        if (*item->param > 12500000.0f)
        {
            *item->param = 12500000.0f;
        }
        if (*item->param < 10.0)
        {
            *item->param = 10.0;
        }
        // 对 TC 通道（索引2）在手动调整时也实时限制最大频率
        if ((nowselect_pwmoutput_item / 3) == 2)
        {
            double capped = enforce_tc_max_freq(*item->param);
            if (capped != *item->param)
            {
                *item->param = capped;
            }
        }
    }

    /************按键处理结束******** */

    ResetOpn();
}

void anni_pwmoutput();

// 配置窗口输入处理
void pwmoutputconfigChangeValue()
{
    uint8_t ch_idx = nowselect_pwmoutput_item / 3;
    PwmOutputConfig &cfg = PWMitem[ch_idx].cfg; // PWM 配置存放在 PWMitem

    if (!pwmoutitemConfigSubIsRunning)
    {
        // 顶层三控件的选择移动
        if (opnUp)
        {
            pwmoutConfigSelected = (pwmoutConfigSelected + 2) % 3;
        }
        if (opnDown)
        {
            pwmoutConfigSelected = (pwmoutConfigSelected + 1) % 3;
        }

        if (opnEnter)
        {
            if (cfg.run_mode == 0 && pwmoutConfigSelected == 2)
            {
                // 在持续输出模式下，禁止进入数值配置
                ResetOpn();
                return;
            }
            // 进入子窗口
            pwmoutitemConfigSubIsRunning = true;
            pwmoutConfigEditingControl = pwmoutConfigSelected;
            pwmoutConfigChoiceBackup = 0;
            pwmoutConfigValueBackup = cfg.cfg_value;

            if (pwmoutConfigEditingControl == 0)
            {
                pwmoutConfigChoiceIndex = std::min<uint8_t>(cfg.output_mode, 2);
                pwmoutConfigChoiceBackup = pwmoutConfigChoiceIndex;
            }
            else if (pwmoutConfigEditingControl == 1)
            {
                pwmoutConfigChoiceIndex = std::min<uint8_t>(cfg.run_mode, 2);
                pwmoutConfigChoiceBackup = pwmoutConfigChoiceIndex;
            }
            // 第三个数值选择不需要 choiceIndex 备份
            ResetOpn();
            return;
        }

        if (opnExit)
        {
            // 退出配置
            pwmoutitemConfigIsRunning = false;
            pwmoutitemConfigSubIsRunning = false;
            pwmoutitem_outfuction = 1;
            ResetOpn();
            return;
        }

        ResetOpn();
        return;
    }

    // 子窗口内处理
    if (pwmoutConfigEditingControl == 0)
    {
        // 类型三选一
        if (opnUp)
            pwmoutConfigChoiceIndex = (pwmoutConfigChoiceIndex + 2) % 3;
        if (opnDown)
            pwmoutConfigChoiceIndex = (pwmoutConfigChoiceIndex + 1) % 3;

        if (opnEnter)
        {
            cfg.output_mode = pwmoutConfigChoiceIndex;
            pwmoutitemConfigSubIsRunning = false;
            // 如果选择占空比的位置 改变了输出模式
            if (cfg.output_mode != 0 && nowselect_pwmoutput_item % 3 == 2)
            {

                // 占空比选项收缩 更改为频率
                nowselect_pwmoutput_item--;
            }
            // Log mode change
            extern bool send_pc_flag;
            send_pc_flag = true; // 触发向上位机更新数据

            if (pwmoutConfigEditingControl == 0)
            {
                // 如果修改了输出模式，立即更新PWM配置
                Update_pwmconfig();
            }
        }
        else if (opnExit)
        {
            // 恢复
            cfg.output_mode = pwmoutConfigChoiceBackup;
            pwmoutitemConfigSubIsRunning = false;
        }
        ResetOpn();
        return;
    }
    else if (pwmoutConfigEditingControl == 1)
    {
        // 模式三选一
        if (opnUp)
            pwmoutConfigChoiceIndex = (pwmoutConfigChoiceIndex + 2) % 3;
        if (opnDown)
            pwmoutConfigChoiceIndex = (pwmoutConfigChoiceIndex + 1) % 3;

        if (opnEnter)
        {
            cfg.run_mode = pwmoutConfigChoiceIndex;
            pwmoutitemConfigSubIsRunning = false;
            // Log run mode change
            extern bool send_pc_flag;
            send_pc_flag = true; // 触发向上位机更新数据
        }
        else if (opnExit)
        {
            cfg.run_mode = pwmoutConfigChoiceBackup;
            pwmoutitemConfigSubIsRunning = false;
        }
        ResetOpn();
        return;
    }
    else
    {
        // 数值选择：步进与数值
        // 参考已有的数值选择器，使用一组阶梯步进
        double steps[5] = {1, 10, 100, 1000, 10000};
        // 找到当前步进索引
        int sidx = 0;
        for (int i = 0; i < 5; ++i)
        {
            if (approx_eq(steps[i], cfg.cfg_step) || fabs(steps[i] - cfg.cfg_step) < 1e-9)
            {
                sidx = i;
                break;
            }
            if (fabs(steps[i] - cfg.cfg_step) < fabs(steps[sidx] - cfg.cfg_step))
                sidx = i;
        }

        if (opnCtrlUp)
        {
            sidx = std::min(sidx + 1, 4);
            cfg.cfg_step = steps[sidx];
        }
        if (opnCtrlDown)
        {
            sidx = std::max(sidx - 1, 0);
            cfg.cfg_step = steps[sidx];
        }
        if (opnUp)
        {
            uint32_t v = cfg.cfg_value;
            uint32_t inc = (uint32_t)llround(cfg.cfg_step);
            v = std::min<uint32_t>(65535u, v + std::max<uint32_t>(1u, inc));
            cfg.cfg_value = (uint16_t)v;
        }
        if (opnDown)
        {
            int32_t v = cfg.cfg_value;
            int32_t dec = (int32_t)llround(cfg.cfg_step);
            v = std::max<int32_t>(0, v - std::max<int32_t>(1, dec));
            cfg.cfg_value = (uint16_t)v;
        }

        if (opnEnter)
        {
            // 保存当前值并退出子窗口
            pwmoutitemConfigSubIsRunning = false;
            // Log value set
            extern bool send_pc_flag;
            send_pc_flag = true; // 触发向上位机更新数据
        }
        else if (opnExit)
        {
            // 取消修改
            cfg.cfg_value = pwmoutConfigValueBackup;
            pwmoutitemConfigSubIsRunning = false;
        }

        ResetOpn();
        return;
    }
}

// 菜单显示的频率和占空比格式化
void valuetochar(double freq, double duty, char *bufferfreq, char *bufferduty, size_t bufferSize)
{

    if (freq < 100.0)
    {
        // 频率，保留2位小数
        snprintf(bufferfreq, bufferSize, "%.2fHz", freq);
        snprintf(bufferduty, bufferSize, "%.2f%%", duty * 100.0);
    }
    else if (freq < 1000.0)
    {
        // 频率，保留1位小数
        snprintf(bufferfreq, bufferSize, "%.1fHz", freq);
        snprintf(bufferduty, bufferSize, "%.2f%%", duty * 100.0);
    }
    else if (freq < 10000.0)
    {
        // 频率，保留0位小数
        snprintf(bufferfreq, bufferSize, "%.3fkHz", freq / 1000.0f);
        snprintf(bufferduty, bufferSize, "%.1f%%", duty * 100.0);
    }
    else if (freq < 100000.0)
    {
        // 频率，保留1位小数
        snprintf(bufferfreq, bufferSize, "%.1fkHz", freq / 1000.0f);
        snprintf(bufferduty, bufferSize, "%.1f%%", duty * 100.0);
    }
    else if (freq < 1000000.0)
    {
        // 频率，保留0位小数
        snprintf(bufferfreq, bufferSize, "%.0fkHz", freq / 1000.0f);
        snprintf(bufferduty, bufferSize, "%.0f%%", duty * 100.0);
    }

    else if (freq < 10000000.0)
    {
        // 频率，保留2位小数
        snprintf(bufferfreq, bufferSize, "%.1fMHz", freq / 1000000.0f);
        snprintf(bufferduty, bufferSize, "%.0f/10", (duty * 10.0f));
    }
    else
    {
        // 频率，保留2位小数
        snprintf(bufferfreq, bufferSize, "~%.1fMHz", freq / 1000000.0);
        snprintf(bufferduty, bufferSize, "~%.0f/10", (duty * 10.0f));
    }
    // printf("duty: %.0f, bufferduty: %s \n", (int)(duty * 10.0), bufferduty);
}

void draw_pwmoutput_item()
{

    int16_t height = 12 * 1.2 * 3;
    int16_t width = pwmitem_ChangeVal_Width;
    int16_t x = (SCREEN_WIDTH - width) / 2;
    int16_t y = (SCREEN_HEIGHT - height) / 2;

    // 动态计算步进字符串

    // 显示标题
    UIDisplayStr_font12(x + 3, y + itemHeightOffset, item->title);

    // step显示 窗口内部
    char step_str_buf[12];
    if (duty1_freq0)
    {
        double freq_val = *PWMitem[nowselect_pwmoutput_item / 3].frec->param;
        double pct = pwmitem_step * 100.0;
        if (freq_val < 10)
            snprintf(step_str_buf, sizeof(step_str_buf), "+%.2f%%", pct);
        else if (freq_val < 100)
            snprintf(step_str_buf, sizeof(step_str_buf), "+%.2f%%", pct);
        else if (freq_val < 1000)
            snprintf(step_str_buf, sizeof(step_str_buf), "+%.2f%%", pct);
        else if (freq_val < 10000)
            snprintf(step_str_buf, sizeof(step_str_buf), "+%.1f%%", pct);
        else if (freq_val < 100000)
            snprintf(step_str_buf, sizeof(step_str_buf), "+%.1f%%", pct);
        else if (freq_val < 1000000)
            snprintf(step_str_buf, sizeof(step_str_buf), "+%.0f%%", pct);
        else if (freq_val < 20000000)
            snprintf(step_str_buf, sizeof(step_str_buf), "+%.0f/10", (pwmitem_step * 10.0));
    }
    else
    {
        if (pwmitem_step < 0.1)
            snprintf(step_str_buf, sizeof(step_str_buf), "+%.2f", pwmitem_step);
        else if (pwmitem_step < 1.0)
            snprintf(step_str_buf, sizeof(step_str_buf), "+%.1f", pwmitem_step);
        else
            snprintf(step_str_buf, sizeof(step_str_buf), "+%.0f", pwmitem_step);
    }

    // param显示 窗口内部
    char param_str_buf[12];

    int8_t deleteline_start_idx = 0;
    if (duty1_freq0)
    {
        double freq_val = *PWMitem[nowselect_pwmoutput_item / 3].frec->param;
        double pct = (*item->param) * 100.0;
        if (freq_val < 10)
            snprintf(param_str_buf, sizeof(param_str_buf), "%.2f%%", pct);
        else if (freq_val < 100)
            snprintf(param_str_buf, sizeof(param_str_buf), "%.2f%%", pct);
        else if (freq_val < 1000)
            snprintf(param_str_buf, sizeof(param_str_buf), "%.2f%%", pct);
        else if (freq_val < 10000)
            snprintf(param_str_buf, sizeof(param_str_buf), "%.1f%%", pct);
        else if (freq_val < 100000)
            snprintf(param_str_buf, sizeof(param_str_buf), "%.1f%%", pct);
        else if (freq_val < 1000000)
            snprintf(param_str_buf, sizeof(param_str_buf), "%.0f%%", pct);
        else if (freq_val < 20000000)
            snprintf(param_str_buf, sizeof(param_str_buf), "%.0f/10", ((*item->param) * 10.0f));
    }

    else if (!duty1_freq0)
    {
        double freq_val = *item->param;
        snprintf(param_str_buf, sizeof(param_str_buf), "%.2f", freq_val);

        if (freq_val < 10.0f)
        {
            deleteline_start_idx = -1; // 5.55
        }
        else if (freq_val < 100.0f)
        {
            deleteline_start_idx = -1; // 55.55
        }
        else if (freq_val < 1000.0f)
        {
            deleteline_start_idx = 5; // 555.5-
        }
        else if (freq_val < 10000.0f)
        {
            deleteline_start_idx = 5; // 5555.--
        }
        else if (freq_val < 100000.0f)
        {
            deleteline_start_idx = 3; // 555--.--
        }
        else if (freq_val < 1000000.0f)
        {
            deleteline_start_idx = 3; // 555---.--
        }
        else if (freq_val < 10000000.0f)
        {
            deleteline_start_idx = 2; // 55-----.--
        }
        else if (freq_val < 20000000.0f)
        {
            deleteline_start_idx = 3; // 155-----.--
        }
    }

    // 先绘制底层框
    // 选择参数值时的标注
    int pwmitem_paramWidth;
    pwmitem_paramWidth = (int)SSD1306_TextWidth(param_str_buf, &Font12);
    if (!opnCtrl)
        DrawRectangle(buf, x + 1, y + pwmitem_height, pwmitem_paramWidth + 5, pwmitem_height, 1, !opnCtrl);

    // 选择步进大小时的标注
    int pwmitem_stepWidth = (int)SSD1306_TextWidth(step_str_buf, &Font12) + (int)SSD1306_TextWidth("()", &Font12);
    if (opnCtrl)
    {
        DrawRectangle(buf, x + 1, y + 2 * pwmitem_height, pwmitem_stepWidth + 5, pwmitem_height, 1, opnCtrl);
    }

    // 显示当前参数字符串
    UIDisplayStr_font12(x + 3, y + pwmitem_height + itemHeightOffset, param_str_buf, 10, !opnCtrl);
    if ((!duty1_freq0) && (deleteline_start_idx != -1))
    {
        // 画删除线
        int delstartx = x + 3 + deleteline_start_idx * pwmfont_width;
        int delendy = y + pwmitem_height + itemHeightOffset + 5;
        DrawLine(buf, delstartx, delendy, delstartx + (pwmitem_paramWidth - deleteline_start_idx * pwmfont_width), delendy, opnCtrl);
    }
    // 显示当前步进字符串
    int step_open_x = x + 3;
    int step_text_x = step_open_x + (int)SSD1306_TextWidth("(", &Font12);
    int step_close_x = step_text_x + (int)SSD1306_TextWidth(step_str_buf, &Font12);
    UIDisplayStr_font12(step_text_x, y + 2 * pwmitem_height + itemHeightOffset, step_str_buf, 10, opnCtrl);
    UIDisplayStr_font12(step_open_x, y + 2 * pwmitem_height + itemHeightOffset, "(", 10, opnCtrl);
    UIDisplayStr_font12(step_close_x, y + 2 * pwmitem_height + itemHeightOffset, ")", 10, opnCtrl);

    return;
}

// 配置窗口绘制（两级）：顶层三控件 + 子窗口

// Moved up

static const char *kOutputTypeNames[3] = {"PWM", "频率", "DC"};
static const char *kRunModeNames[3] = {"连续", "计脉", "定时"};

void draw_pwmoutput_config()
{
    // 居中窗口，与数值选择窗口一致的尺寸风格
    int16_t height = 12 * 1.2 * 3;
    int16_t width = pwmitem_ChangeVal_Width;
    int16_t x = (SCREEN_WIDTH - width) / 2;
    int16_t y = (SCREEN_HEIGHT - height) / 2;

    uint8_t ch_idx = nowselect_pwmoutput_item / 3;
    const PwmOutputConfig &cfg = PWMitem[ch_idx].cfg; // 配置存放在 PWMitem

    if (!pwmoutitemConfigSubIsRunning)
    {
        // 第一行：TA:OUT
        char title_buf[16];
        snprintf(title_buf, sizeof(title_buf), "配置:%s", channel_name(ch_idx));
        UIDisplayStr_font12(x + 4, y + itemHeightOffset, title_buf);

        // 第二行：左（类型） 右（模式）
        const char *type_txt = kOutputTypeNames[std::min<uint8_t>(cfg.output_mode, 2)];
        const char *mode_txt = kRunModeNames[std::min<uint8_t>(cfg.run_mode, 2)];
        // 左显示
        UIDisplayStr_font12(x + 4, y + pwmitem_height + itemHeightOffset, (char *)type_txt, 10);
        // 右显示（放在右半区域）
        int rightX = x + width / 2 - 2;
        UIDisplayStr_font12(rightX + 1, y + pwmitem_height + itemHeightOffset, (char *)mode_txt, 10);

        // 第三行：数值（根据模式显示单位）
        char val_buf[18];
        if (cfg.run_mode == 1)
        {
            // Pulse Count
            snprintf(val_buf, sizeof(val_buf), "%u 脉冲", (unsigned)cfg.cfg_value);
        }
        else if (cfg.run_mode == 2)
        {
            // Timer-Based
            snprintf(val_buf, sizeof(val_buf), "%u ms", (unsigned)cfg.cfg_value);
        }
        else
        {
            snprintf(val_buf, sizeof(val_buf), "---");
        }
        UIDisplayStr_font12(x + 4, y + 2 * pwmitem_height + itemHeightOffset, val_buf, 10);

        // 选择框渲染（高亮）
        if (pwmoutConfigSelected == 0)
        {
            int w = (int)SSD1306_TextWidth(type_txt, &Font12) + 6;
            InvertRect(buf, x + 1, y + pwmitem_height, w, pwmitem_height);
        }
        else if (pwmoutConfigSelected == 1)
        {
            int w = (int)SSD1306_TextWidth(mode_txt, &Font12) + 6;
            InvertRect(buf, rightX - 2, y + pwmitem_height, w, pwmitem_height);
        }
        else
        {
            int w = (int)SSD1306_TextWidth(val_buf, &Font12) + 6;
            InvertRect(buf, x + 1, y + 2 * pwmitem_height, w, pwmitem_height);
        }
    }
    else
    {
        // 子窗口统一尺寸
        int16_t sx = x; // 复用同一居中区域
        int16_t sy = y;
        // DrawRectangle(buf, sx + 1, sy + 1, width - 1, height - 1, 1, 0);

        if (pwmoutConfigEditingControl == 0)
        {
            // 类型三选一
            const char *opts1[3] = {"标准PWM输出", "频率脉冲输出", "直流DC输出"};
            const char *opts2[3] = {"标准PWM输出", "频率脉冲输出", "直流DC输出"};

            for (int i = 0; i < 3; ++i)
            {
                if (nowselect_pwmoutput_item / 3 == 0 || nowselect_pwmoutput_item / 3 == 1)
                    UIDisplayStr_font12(sx + 4, sy + itemHeightOffset + i * pwmitem_height, (char *)opts1[i], 20);
                else
                {
                    UIDisplayStr_font12(sx + 4, sy + itemHeightOffset + i * pwmitem_height, (char *)opts2[i], 20);
                }
            }
            // 光标高亮
            if (nowselect_pwmoutput_item / 3 == 0 || nowselect_pwmoutput_item / 3 == 1)
            {
                int w = (int)SSD1306_TextWidth(opts1[pwmoutConfigChoiceIndex], &Font12) + 6;
                InvertRect(buf, sx + 1, sy + pwmoutConfigChoiceIndex * pwmitem_height, w, pwmitem_height);
            }
            else
            {
                int w = (int)SSD1306_TextWidth(opts2[pwmoutConfigChoiceIndex], &Font12) + 6;
                InvertRect(buf, sx + 1, sy + pwmoutConfigChoiceIndex * pwmitem_height, w, pwmitem_height);
            }
        }
        else if (pwmoutConfigEditingControl == 1)
        {
            // 模式三选一
            const char *opts[3] = {"连续", "脉冲计数", "定时模式"};
            for (int i = 0; i < 3; ++i)
            {
                UIDisplayStr_font12(sx + 4, sy + itemHeightOffset + i * pwmitem_height, (char *)opts[i], 10);
            }
            int w = (int)SSD1306_TextWidth(opts[pwmoutConfigChoiceIndex], &Font12) + 6;
            InvertRect(buf, sx + 1, sy + pwmoutConfigChoiceIndex * pwmitem_height, w, pwmitem_height);
        }
        else
        {
            // 数值选择：第一行标题（随模式变化），第二行数值，第三行步进
            const char *title = (cfg.run_mode == 1) ? "脉冲计数" : "定时模式";
            UIDisplayStr_font12(sx + 4, sy + itemHeightOffset, (char *)title);

            char num_buf[18];
            if (cfg.run_mode == 1)
                snprintf(num_buf, sizeof(num_buf), "%u 脉冲", (unsigned)cfg.cfg_value);
            else
                snprintf(num_buf, sizeof(num_buf), "%u ms", (unsigned)cfg.cfg_value);
            UIDisplayStr_font12(sx + 4, sy + pwmitem_height + itemHeightOffset, num_buf, 10);

            char step_buf[18];

            snprintf(step_buf, sizeof(step_buf), "(+%.0f)", cfg.cfg_step);
            UIDisplayStr_font12(sx + 4, sy + 2 * pwmitem_height + itemHeightOffset, step_buf, 10);

            // 高亮两行数字与步进
            if (opnCtrl)
            {
                int ws = (int)SSD1306_TextWidth(step_buf, &Font12) + 6;
                InvertRect(buf, sx + 1, sy + 2 * pwmitem_height, ws, pwmitem_height);
            }
            else
            {
                int wv = (int)SSD1306_TextWidth(num_buf, &Font12) + 6;
                InvertRect(buf, sx + 1, sy + pwmitem_height, wv, pwmitem_height);
            }
        }
    }
}

void dutyvaluetochar(double value, char *buffer, size_t bufferSize)
{
    // 占空比，保留3位小数
    snprintf(buffer, bufferSize, "%.3f%%", value * 100.0);
}

void draw_pwmout_read(uint16_t page2Ypos = 48)
{
    // 格式化 TA/TB 的显示内容
    char ta_f_str[9], ta_duty_str[9];
    char tb_f_str[9], tb_duty_str[9];
    char tc_f_str[9], tc_duty_str[9];
    char td_f_str[9], td_duty_str[9];

    valuetochar(param_a_f, param_a_duty, ta_f_str, ta_duty_str, sizeof(ta_f_str));
    valuetochar(param_b_f, param_b_duty, tb_f_str, tb_duty_str, sizeof(tb_f_str));
    valuetochar(param_c_f, param_c_duty, tc_f_str, tc_duty_str, sizeof(tc_f_str));
    valuetochar(param_d_f, param_d_duty, td_f_str, td_duty_str, sizeof(td_f_str));

    // 居中打印 TA/TB 的内容
    DrawRectangle(buf, 0, 0, 64, 16, 0, 1);
    DrawRectangle(buf, 64, 0, 64, 16, 0, 1);
    Paint_DrawString_EN_CenterAtX(buf, 32, 2, "TA", &Font16, 1);
    Paint_DrawString_EN_CenterAtX(buf, 96, 2, "TB", &Font16, 1);

    // TA (0)
    if (PWMitem[0].cfg.output_mode == 0)
    {
        Paint_DrawString_EN_CenterAtX(buf, 32, 19, ta_f_str, &Font12, 1);
        Paint_DrawString_EN_CenterAtX(buf, 32, 35, ta_duty_str, &Font12, 1);
        DrawLine(buf, 0, 31, 63, 31, 1);
    }
    else if (PWMitem[0].cfg.output_mode == 1)
    {
        Paint_DrawString_EN_CenterAtX(buf, 32, 27, ta_f_str, &Font12, 1);
    }
    else
    {
        Paint_DrawString_EN_CenterAtX(buf, 32, 27, "DC输出", &Font12, 1);
    }

    // TB (1)
    if (PWMitem[1].cfg.output_mode == 0)
    {
        Paint_DrawString_EN_CenterAtX(buf, 96, 19, tb_f_str, &Font12, 1);
        Paint_DrawString_EN_CenterAtX(buf, 96, 35, tb_duty_str, &Font12, 1);
        DrawLine(buf, 64, 31, 127, 31, 1);
    }
    else if (PWMitem[1].cfg.output_mode == 1)
    {
        Paint_DrawString_EN_CenterAtX(buf, 96, 27, tb_f_str, &Font12, 1);
    }
    else
    {
        Paint_DrawString_EN_CenterAtX(buf, 96, 27, "DC输出", &Font12, 1);
    }

    DrawRectangle(buf, 0, page2Ypos, 128, 64, 1, 0);
    DrawRectangle(buf, 0, page2Ypos, 64, 16, 0, 1);
    DrawRectangle(buf, 64, page2Ypos, 64, 16, 0, 1);

    // DrawLine(buf, 0, page2Ypos + 47, 127,page2Ypos +  47, 1);
    Paint_DrawString_EN_CenterAtX(buf, 32, 2 + page2Ypos, "TC*", &Font16, 1);
    Paint_DrawString_EN_CenterAtX(buf, 96, 2 + page2Ypos, "TD*", &Font16, 1);

    // TC (2)
    if (PWMitem[2].cfg.output_mode == 0)
    {
        Paint_DrawString_EN_CenterAtX(buf, 32, 19 + page2Ypos, tc_f_str, &Font12, 1);
        Paint_DrawString_EN_CenterAtX(buf, 32, 35 + page2Ypos, tc_duty_str, &Font12, 1);
        DrawLine(buf, 0, page2Ypos + 31, 63, page2Ypos + 31, 1);
    }
    else if (PWMitem[2].cfg.output_mode == 1)
    {
        Paint_DrawString_EN_CenterAtX(buf, 32, 27 + page2Ypos, tc_f_str, &Font12, 1);
    }
    else
    {
        Paint_DrawString_EN_CenterAtX(buf, 32, 27 + page2Ypos, "DC输出", &Font12, 1);
    }

    // TD (3)
    if (PWMitem[3].cfg.output_mode == 0)
    {
        Paint_DrawString_EN_CenterAtX(buf, 96, 19 + page2Ypos, td_f_str, &Font12, 1);
        Paint_DrawString_EN_CenterAtX(buf, 96, 35 + page2Ypos, td_duty_str, &Font12, 1);
        DrawLine(buf, 64, page2Ypos + 31, 127, page2Ypos + 31, 1);
    }
    else if (PWMitem[3].cfg.output_mode == 1)
    {
        Paint_DrawString_EN_CenterAtX(buf, 96, 27 + page2Ypos, td_f_str, &Font12, 1);
    }
    else
    {
        Paint_DrawString_EN_CenterAtX(buf, 96, 27 + page2Ypos, "DC输出", &Font12, 1);
    }
}

void anni_pwmoutput()
{
    int xEnd, yEnd, widthEnd, heighEND;
    int page2YPosEnd;
    if (nowselect_pwmoutput_item < 6)
    {
        page2YPosEnd = 47;
    }
    else if (nowselect_pwmoutput_item >= 6)
    {
        page2YPosEnd = 16;
    }

    if (!pwmoutitemConfigIsRunning && !pwmoutitemfunctionIsRunning && opnCtrl)
    {
        switch (nowselect_pwmoutput_item / 3)
        {
        case 0:
            xEnd = 1;
            yEnd = 1;
            widthEnd = 61;
            heighEND = 44;
            break;
        case 1:
            xEnd = 65;
            yEnd = 1;
            widthEnd = 61;
            heighEND = 44;
            break;
        case 2:
            xEnd = 1;
            yEnd = 17;
            widthEnd = 61;
            heighEND = 44;
            break;
        case 3:
            xEnd = 65;
            yEnd = 17;
            widthEnd = 61;
            heighEND = 44;
            break;
        default:
            break;
        }
    }

    else if (!pwmoutitemConfigIsRunning && !pwmoutitemfunctionIsRunning)
    {

        switch (nowselect_pwmoutput_item)
        {
        case 0:
            xEnd = 1;
            yEnd = 1;
            widthEnd = 61;
            heighEND = 13;
            break;

        case 1:
            xEnd = 1;
            widthEnd = 61;
            // TA Item 1 (Freq)
            if (PWMitem[0].cfg.output_mode == 0)
            {
                yEnd = 17;
                heighEND = 12;
            }
            else
            {
                yEnd = 17;
                heighEND = 28;
            } // Merge 1&2
            break;

        case 2:
            xEnd = 1;
            widthEnd = 61;
            // TA Item 2 (Duty)
            if (PWMitem[0].cfg.output_mode == 0)
            {
                yEnd = 33;
                heighEND = 12;
            }
            else
            {
                yEnd = 17;
                heighEND = 28;
            } // Merge 1&2
            break;

        case 3:
            xEnd = 65;
            yEnd = 1;
            widthEnd = 61;
            heighEND = 13;
            break;

        case 4:
            xEnd = 65;
            widthEnd = 61;
            if (PWMitem[1].cfg.output_mode == 0)
            {
                yEnd = 17;
                heighEND = 12;
            }
            else
            {
                yEnd = 17;
                heighEND = 28;
            }
            break;

        case 5:
            xEnd = 65;
            widthEnd = 61;
            if (PWMitem[1].cfg.output_mode == 0)
            {
                yEnd = 33;
                heighEND = 12;
            }
            else
            {
                yEnd = 17;
                heighEND = 28;
            }
            break;

        case 6:
            xEnd = 1;
            yEnd = 17;
            widthEnd = 61;
            heighEND = 13;
            break;

        case 7:
            xEnd = 1;
            widthEnd = 61;
            // TC Item 1. Original: yEnd = 33, heighEND = 12
            if (PWMitem[2].cfg.output_mode == 0)
            {
                yEnd = 33;
                heighEND = 12;
            }
            else
            {
                yEnd = 33;
                heighEND = 29;
            }
            break;

        case 8:
            xEnd = 1;
            widthEnd = 61;
            // TC Item 2. Original: yEnd = 49, heighEND = 13
            if (PWMitem[2].cfg.output_mode == 0)
            {
                yEnd = 49;
                heighEND = 13;
            }
            else
            {
                yEnd = 33;
                heighEND = 29;
            }
            break;

        case 9:
            xEnd = 65;
            yEnd = 17;
            widthEnd = 61;
            heighEND = 13;
            break;

        case 10:
            xEnd = 65;
            widthEnd = 61;
            // TD Item 1. Original: yEnd = 33
            if (PWMitem[3].cfg.output_mode == 0)
            {
                yEnd = 33;
                heighEND = 12;
            }
            else
            {
                yEnd = 33;
                heighEND = 29;
            }
            break;

        case 11:
            xEnd = 65;
            widthEnd = 61;
            // TD Item 2. Original: yEnd = 49
            if (PWMitem[3].cfg.output_mode == 0)
            {
                yEnd = 49;
                heighEND = 13;
            }
            else
            {
                yEnd = 33;
                heighEND = 29;
            }
            break;

        default:
            break;
        }
    }
    else if (pwmoutitemConfigSubIsRunning)
    {
        heighEND = pwmitem_height * 3 + 1 + 2;
        widthEnd = pwmitem_ChangeVal_Width + 16;
        xEnd = (SCREEN_WIDTH - widthEnd) / 2;
        yEnd = (SCREEN_HEIGHT - heighEND) / 2;
    }
    else
    {
        heighEND = pwmitem_height * 3 + 1 + 2;
        widthEnd = pwmitem_ChangeVal_Width + 2;
        xEnd = (SCREEN_WIDTH - widthEnd) / 2;
        yEnd = (SCREEN_HEIGHT - heighEND) / 2;
    }

    if (pwmoutitem_infunction)
    {
        seedvalue = to_ms_since_boot(get_absolute_time());
    }

    // 判定如果xEnd,yEnd,widthEnd,heighEND与start相同则不进行动画
    bool if_no_animation = false;
    if (xEnd == xStart && yEnd == yStart && widthEnd == lengthStart && heighEND == heightStart)
    {
        if_no_animation = true;
    }

    if (!if_no_animation && footlength != 1)
    {
        float footlengthtemp;
        if (pwmoutitemConfigIsRunning && !pwmoutitem_infunction)
        {
            footlengthtemp = 2 * footlength;
        }
        else
        {
            footlengthtemp = footlength;
        }
        for (double i = 0; i <= 1; i += (footlengthtemp))
        {

            int x = X2line_down(xStart, xEnd, i);
            int y = easeInOutQuad(yStart, yEnd, i);
            int width = X2line_down(lengthStart, widthEnd, i);
            int heigh = easeInOutQuad(heightStart, heighEND, i);
            int page2Ypos = X2line_down(page2YPosStart, page2YPosEnd, i);

            draw_pwmout_read(page2Ypos);

            if ((pwmoutitemfunctionIsRunning || pwmoutitemConfigIsRunning) && !pwmoutitem_infunction)
            {
                ApplyBlurEffect(buf, 1); // 背景模糊层
            }
            else if (pwmoutitem_infunction && (pwmoutitemfunctionIsRunning || pwmoutitemConfigIsRunning))
            {
                ApplyBlurEffect(buf, X2line_up(0, 1, i)); // 背景模糊层
            }
            else if (pwmoutitem_outfuction)
            {
                ApplyBlurEffect(buf, X2line_down(0.8, 0, i)); // 背景模糊层
            }
            else if (!pwmoutitemfunctionIsRunning && !pwmoutitemConfigIsRunning)
            {
                // 无模糊 先画Sgate
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }

            auto draw_prog = [&](int idx, int x, int y)
            {
                double elapsed_us = 0.0;
                if (pwm_jobs[idx].flash_pending)
                {
                    InvertRect(buf, x, y, 64, 16);
                    pwm_jobs[idx].flash_pending = false;
                    return;
                }
                if (short_job_flash(idx, elapsed_us))
                {
                    if (elapsed_us < 50000.0)
                    {
                        InvertRect(buf, x, y, 64, 16); // 闪烁一次
                    }
                    return;
                }

                double prog = channel_progress(idx);
                if (prog <= 0.0)
                    return;
                int bar_w = std::max(1, (int)round(64.0 * prog));
                InvertRect(buf, x, y, bar_w, 16);
            };

            draw_prog(0, 0, 0);
            draw_prog(1, 64, 0);
            draw_prog(2, 0, page2Ypos);
            draw_prog(3, 64, page2Ypos);

            if ((pwmoutitemfunctionIsRunning || pwmoutitemConfigIsRunning) && !pwmoutitem_infunction)
            {
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 2);
            }
            else if (pwmoutitem_infunction && (pwmoutitemfunctionIsRunning || pwmoutitemConfigIsRunning))
            {
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 2);
            }
            else if (pwmoutitem_outfuction)
            {
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }

            if (pwmoutitemfunctionIsRunning)
            {
                draw_pwmoutput_item();
            }
            else if (pwmoutitemConfigIsRunning)
            {
                draw_pwmoutput_config();
            }

            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
        }
    }
    lengthStart = widthEnd;
    heightStart = heighEND;
    yStart = yEnd;
    xStart = xEnd;
    page2YPosStart = page2YPosEnd;
    pwmoutitem_infunction = false;
    pwmoutitem_outfuction = false;

    // if (pwmoutitemfunctionIsRunning && nowselect_pwmoutput_item % 3 == 0)
    // {
    //     pwmoutitemfunctionIsRunning = false; // open项目注销运行标注
    // }

    // End静态绘制
    draw_pwmout_read(page2YPosEnd);

    if (pwmoutitemfunctionIsRunning || pwmoutitemConfigIsRunning)
    {
        ApplyBlurEffect(buf, 1); // 背景模糊层
    }
    if (!(pwmoutitemfunctionIsRunning || pwmoutitemConfigIsRunning))
    {
        // 无模糊 先画Sgate
        UIDrawSgate(xEnd, yEnd, widthEnd, heighEND, 8, 6, 1, 1);
    }

    auto draw_prog_static = [&](int idx, int x, int y)
    {
        double elapsed_us = 0.0;
        if (pwm_jobs[idx].flash_pending)
        {
            InvertRect(buf, x, y, 64, 16);
            pwm_jobs[idx].flash_pending = false;
            return;
        }
        if (short_job_flash(idx, elapsed_us))
        {
            if (elapsed_us < 50000.0)
            {
                InvertRect(buf, x, y, 64, 16);
            }
            return;
        }

        double prog = channel_progress(idx);
        if (prog <= 0.0)
            return;
        int bar_w = std::max(1, (int)round(64.0 * prog));
        InvertRect(buf, x, y, bar_w, 16);
    };

    draw_prog_static(0, 0, 0);
    draw_prog_static(1, 64, 0);
    draw_prog_static(2, 0, page2YPosEnd);
    draw_prog_static(3, 64, page2YPosEnd);

    if (pwmoutitemfunctionIsRunning || pwmoutitemConfigIsRunning)
    {
        UIDrawSgate(xEnd, yEnd, widthEnd, heighEND, 8, 6, 1, 2);
    }

    if (pwmoutitemfunctionIsRunning)
    {
        draw_pwmoutput_item();
    }
    else if (pwmoutitemConfigIsRunning)
    {
        draw_pwmoutput_config();
    }

    render(buf, &frame_area);
    memset(buf, 0, SSD1306_BUF_LEN);
}

void update_item_select()
{
    if (nowselect_pwmoutput_item == 1)
    {
        item = &menuItem_TA_f;
        duty1_freq0 = false;
    }
    else if (nowselect_pwmoutput_item == 2)
    {
        item = &menuItem_TA_duty;
        duty1_freq0 = true;
    }
    else if (nowselect_pwmoutput_item == 4)
    {
        item = &menuItem_TB_f;
        duty1_freq0 = false;
    }
    else if (nowselect_pwmoutput_item == 5)
    {
        item = &menuItem_TB_duty;
        duty1_freq0 = true;
    }
    else if (nowselect_pwmoutput_item == 7)
    {
        item = &menuItem_TC_f;
        duty1_freq0 = false;
    }
    else if (nowselect_pwmoutput_item == 8)
    {
        item = &menuItem_TC_duty;
        duty1_freq0 = true;
    }
    else if (nowselect_pwmoutput_item == 10)
    {
        item = &menuItem_TD_f;
        duty1_freq0 = false;
    }
    else if (nowselect_pwmoutput_item == 11)
    {
        item = &menuItem_TD_duty;
        duty1_freq0 = true;
    }
}

void Pwm_Output_Init()
{

    // 重置复用引脚状态
    gpio_init(TEST_SPI_CS_PIN);
    gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
    gpio_disable_pulls(TEST_SPI_CS_PIN);

    // 先重置引脚状态
    gpio_init(TAGENPIN);
    gpio_init(TBGENPIN);
    gpio_init(TCPWMOUT);
    gpio_init(TDPWMOUT);

    // 设置为输出模式并拉低
    gpio_set_dir(TAGENPIN, GPIO_OUT);
    gpio_set_dir(TBGENPIN, GPIO_OUT);
    gpio_set_dir(TCPWMOUT, GPIO_OUT);
    gpio_set_dir(TDPWMOUT, GPIO_OUT);

    gpio_put(TAGENPIN, 0);
    gpio_put(TBGENPIN, 0);
    gpio_put(TCPWMOUT, 0);
    gpio_put(TDPWMOUT, 0);

    // 初始化GPIO为PWM功能
    gpio_set_function(TAGENPIN, GPIO_FUNC_PWM);
    gpio_set_function(TBGENPIN, GPIO_FUNC_PWM);
    gpio_set_function(TCPWMOUT, GPIO_FUNC_PWM);
    gpio_set_function(TDPWMOUT, GPIO_FUNC_PWM);

    // 获取PWM slice号
    uint slice_a = pwm_gpio_to_slice_num(TAGENPIN);
    uint slice_b = pwm_gpio_to_slice_num(TBGENPIN);
    uint slice_c = pwm_gpio_to_slice_num(TCPWMOUT);
    uint slice_d = pwm_gpio_to_slice_num(TDPWMOUT);

    // 启用PWM
    pwm_set_enabled(slice_a, true);
    pwm_set_enabled(slice_b, true);
    pwm_set_enabled(slice_c, true);
    pwm_set_enabled(slice_d, true);

    // 计算初始参数
    const double clock_freq = 125000000.0; // 125MHz系统时钟
    for (int i = 0; i < 4; i++)
    {
        pwm_results[i] = calc_pwm_params(clock_freq, *PWMitem[i].frec->param, *PWMitem[i].duty->param, PWMitem[i].cfg.output_mode);
    }
    // 更新值
    Update_PwmOutput();
}

static inline uint64_t calc_pulse_duration_us(uint8_t idx)
{
    uint32_t pulses = PWMitem[idx].cfg.cfg_value;
    double freq = *PWMitem[idx].frec->param;
    if (pulses == 0 || freq <= 0.0)
        return 1000; // >=1ms作为最小
    double dur_us = ((double)pulses / freq) * 1000000.0;
    if (dur_us < 1000.0)
        dur_us = 1000.0;
    return (uint64_t)llround(dur_us);
}

static void print_channel_status(uint8_t idx)
{
}

static void start_continuous(uint8_t idx)
{
    cancel_job_timer(idx);
    pwm_jobs[idx].active = false;
    pwm_jobs[idx].total_us = 0;
    PWMitem[idx].open = !PWMitem[idx].open;
    if (PWMitem[idx].open)
    {
        extern bool send_pc_flag;
        send_pc_flag = true; // 触发向上位机更新数据
    }
    else
    {
        extern bool send_pc_flag;
        send_pc_flag = true; // 触发向上位机更新数据
    }
    Update_PwmOutput();
}

static void start_pulse_count(uint8_t idx)
{
    uint64_t duration_us = calc_pulse_duration_us(idx);
    cancel_job_timer(idx);
    pwm_jobs[idx].active = true;
    pwm_jobs[idx].total_us = (uint32_t)std::min<uint64_t>(duration_us, 0xFFFFFFFFull);
    pwm_jobs[idx].flash_pending = duration_us < 20000; // 短任务标记一次性闪烁
    pwm_jobs[idx].end_time = delayed_by_us(get_absolute_time(), (int64_t)duration_us);
    pwm_jobs[idx].alarm_id = add_alarm_in_us((int64_t)duration_us, pwm_job_alarm_cb, (void *)(uintptr_t)idx, true);
    PWMitem[idx].open = true;
    extern bool send_pc_flag;
    send_pc_flag = true; // 触发向上位机更新数据
    Update_PwmOutput();
}

static void start_timer_based(uint8_t idx)
{
    uint32_t duration_ms = std::max<uint32_t>(1, PWMitem[idx].cfg.cfg_value);
    uint64_t duration_us = (uint64_t)duration_ms * 1000ull;
    cancel_job_timer(idx);
    pwm_jobs[idx].active = true;
    pwm_jobs[idx].total_us = duration_us;
    pwm_jobs[idx].flash_pending = duration_us < 20000; // 短任务标记一次性闪烁
    pwm_jobs[idx].end_time = delayed_by_us(get_absolute_time(), (int64_t)duration_us);
    pwm_jobs[idx].alarm_id = add_alarm_in_us((int64_t)duration_us, pwm_job_alarm_cb, (void *)(uintptr_t)idx, true);
    PWMitem[idx].open = true;
    extern bool send_pc_flag;
    send_pc_flag = true; // 触发向上位机更新数据
    Update_PwmOutput();
}

extern "C" void pwmoutput_apply_channel_state(uint8_t idx)
{
    if (idx >= 4)
    {
        return;
    }

    cancel_job_timer(idx);
    pwm_jobs[idx].active = false;
    pwm_jobs[idx].total_us = 0;
    pwm_jobs[idx].flash_pending = false;

    if (!PWMitem[idx].open)
    {
        Update_PwmOutput();
        extern bool send_pc_flag;
        send_pc_flag = true;
        return;
    }

    if (PWMitem[idx].cfg.run_mode == 0)
    {
        Update_PwmOutput();
    }
    else if (PWMitem[idx].cfg.run_mode == 1)
    {
        start_pulse_count(idx);
        return;
    }
    else
    {
        start_timer_based(idx);
        return;
    }

    extern bool send_pc_flag;
    send_pc_flag = true;
}

void pwmoutput()
{

    // ResetOpn();
    anni_pwmoutput();
    bool if_opnctrled = 0;

    extern bool send_pc_flag;
    send_pc_flag = true; // 触发向上位机更新数据

    while (true)
    {

        extern bool opnPCchangemode;
        if (opnLeft || opnRight || opnPCchangemode)
        {
            // 不清除状态
            return;
        }

        if (opnEnter || opnExit || opnUp || opnDown || opnCtrlUp || opnCtrlDown)
        {
            if (pwmoutitemfunctionIsRunning)
            {
                pwmoutputitemChangeValue();
            }
            else if (pwmoutitemConfigIsRunning)
            {
                pwmoutputconfigChangeValue();
            }

            if (opnUp)
            {
                extern bool send_pc_flag;
                send_pc_flag = true; // 触发向上位机更新数据
                do
                {
                    nowselect_pwmoutput_item--;
                    if (nowselect_pwmoutput_item < 0)
                        nowselect_pwmoutput_item = 11;
                } while ((nowselect_pwmoutput_item % 3 == 2) && (PWMitem[nowselect_pwmoutput_item / 3].cfg.output_mode != 0));
            }
            if (opnDown)
            {
                extern bool send_pc_flag;
                send_pc_flag = true; // 触发向上位机更新数据
                do
                {
                    nowselect_pwmoutput_item++;
                    if (nowselect_pwmoutput_item > 11)
                        nowselect_pwmoutput_item = 0;
                } while ((nowselect_pwmoutput_item % 3 == 2) && (PWMitem[nowselect_pwmoutput_item / 3].cfg.output_mode != 0));
            }
            if (opnCtrlUp)
            {
                extern bool send_pc_flag;
                send_pc_flag = true; // 触发向上位机更新数据
                nowselect_pwmoutput_item -= 3;
                if (nowselect_pwmoutput_item < 0)
                {
                    nowselect_pwmoutput_item += 12;
                }
                // Sanity check for merged items after jump
                if ((nowselect_pwmoutput_item % 3 == 2) && (PWMitem[nowselect_pwmoutput_item / 3].cfg.output_mode != 0))
                {
                    nowselect_pwmoutput_item--; // Fallback to Item 1
                }
            }
            if (opnCtrlDown)
            {
                extern bool send_pc_flag;
                send_pc_flag = true; // 触发向上位机更新数据
                nowselect_pwmoutput_item += 3;
                if (nowselect_pwmoutput_item > 11)
                {
                    nowselect_pwmoutput_item -= 12;
                }
                // Sanity check for merged items after jump
                if ((nowselect_pwmoutput_item % 3 == 2) && (PWMitem[nowselect_pwmoutput_item / 3].cfg.output_mode != 0))
                {
                    nowselect_pwmoutput_item--; // Fallback to Item 1
                }
            }
            if (opnExit && !pwmoutitemfunctionIsRunning && !pwmoutitemConfigIsRunning)
            {
                // 进入配置窗口（两级）
                pwmoutitemConfigIsRunning = true;
                pwmoutitemConfigSubIsRunning = false;
                pwmoutConfigSelected = 0;
                pwmoutitem_infunction = 1; // 触发进入时的模糊动画
                // ResetOpn();
                // anni_pwmoutput();
                // continue;
            }
            if (opnEnter && !pwmoutitemfunctionIsRunning && !pwmoutitemConfigIsRunning)
            {
                int nowselect_group = nowselect_pwmoutput_item / 3;

                // DC(VREF)模式下，选中参数项(非开关项)点击Enter无效
                if (PWMitem[nowselect_group].cfg.output_mode == 2 && (nowselect_pwmoutput_item % 3 != 0))
                {
                    ResetOpn();
                    anni_pwmoutput();
                    continue;
                }

                pwmoutputitem_init();
                switch (nowselect_pwmoutput_item % 3)
                {
                case 0:
                    if (PWMitem[nowselect_group].cfg.run_mode == 0)
                    {
                        start_continuous(nowselect_group);
                    }
                    else if (PWMitem[nowselect_group].cfg.run_mode == 1)
                    {
                        start_pulse_count(nowselect_group);
                    }
                    else
                    {
                        start_timer_based(nowselect_group);
                    }
                    break;

                case 1:
                    draw_pwmoutput_item(); // 初始化参数 但其实没有渲染
                    pwmoutitemfunctionIsRunning = true;
                    pwmoutitem_infunction = 1;

                case 2:
                    draw_pwmoutput_item(); // 初始化参数 但其实没有渲染
                    pwmoutitemfunctionIsRunning = true;
                    pwmoutitem_infunction = 1;
                    break;

                default:
                    break;
                }
            }

            ResetOpn();
        }

        if (opnCtrl || if_opnctrled)
        {
            if_opnctrled = 1;
            if (!opnCtrl)
            {
                if_opnctrled = 0;
            }
        }

        anni_pwmoutput();

        // printf("Exiting pwmoutput loop\n");
    }
}

#endif
