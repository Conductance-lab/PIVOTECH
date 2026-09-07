#ifndef _CURVE_CPP_
#define _CURVE_CPP_

#ifdef __cplusplus
extern "C" {
#endif

///*************************内置曲线***************************************** */

float X2line_down(float start, float end, float lamda)
{
    return end + (start - end) * (1 - lamda) * (1 - lamda) * (1 - lamda);
}

float X2line_up(float start, float end, float lamda)
{
    return start + (end - start) * lamda * lamda;
}

// 缓入缓出曲线
float easeInOutQuad(float start, float end, float lamda)
{
    float c = end - start; // 计算变化量
    float t = lamda;       // 时间参数就是 lambda

    t *= 2; // 将 t 的范围调整到 [0, 2]

    if (t < 1)
    {
        return c / 2 * t * t + start; // 前半段使用简单的二次函数
    }
    else
    {
        t--;
        return -c / 2 * (t * (t - 2) - 1) + start; // 后半段使用稍复杂的二次函数
    }
}

float A_line(float start, float end, float mid, float lamda)
{
    float c = end - start; // 计算变化量
    float t = lamda;       // 时间参数就是 lambda
    t *= 2;                // 将 t 的范围调整到 [0, 2]

    if (t < 1)
    {
        return X2line_down(start, mid, t); // 前半段二次函数
    }
    else
    {
        t--;
        return X2line_up(mid, end, t); // 后半段二次函数
    }
}
/****************************************************************** */

#ifdef __cplusplus
}
#endif

#endif
