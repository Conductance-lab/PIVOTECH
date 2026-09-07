#ifndef READ_GENERALREAD_HPP
#define READ_GENERALREAD_HPP
/*
本部分描述
需读取PIO部分协同控制
现支持2X10Mhz 高频采集 完全不冲突 10khz以下支持0.01%精度PWM检测 
基于复杂的中断管理 复用引脚功能 双核计算 pio可编程IO（core1同步高频信息） 实现高频率采集 
*/

/*
在中断结束判定中间 根据F 决定：直流检测 根据双悬空 决定：电阻检测

直流电平判断规则
CMOS状态    非EN状态    判定
group       
1V-           0          相对低电压     
1V+           2V+          相对高电压    

group       
1V-     2V-          相对低电压     
1V+     2V+          相对高电压 

3.3 5       0           悬空

V           V       正常电压

0           0          接地


只有双悬空会进入短路测试（理论最大允许相对压差3V3）
*/

typedef enum SoundtMode{
    none,
    multi_conduction,
    oneway_conduction1,
    oneway_conduction2
};

extern SoundtMode sound_mode;
extern SoundtMode last_sound_mode;

void Frequency_Read_Init(void);
void Frequency_Read_Deinit(void);
bool draw_general_read(bool getdraw = 0);
void general_read(void);

#endif // READ_GENERALREAD_HPP
