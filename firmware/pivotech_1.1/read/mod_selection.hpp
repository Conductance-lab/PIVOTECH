#ifndef _MOD_SELECTION_HPP_
#define _MOD_SELECTION_HPP_



/**
 * @brief 启动主菜单，包含以下功能项（按顺序）：
 *        0: A&B phase encoder
 *        1: SSD1306          (第二行第一个)
 *        2: MPU6050          (第二行第二个)
 *        3: HC-05 AT mode
 *        4: Stick X&Y ADC
 *
 * 上下键循环选择，Enter 执行当前功能（阻塞直到子功能返回），
 * Exit 键播放缩放动画后回到菜单（无效退出）。
 */
#ifdef __cplusplus
extern "C" {
#endif

void main_mod_selection(void);
extern int nowselect_moditem;

#ifdef __cplusplus
}
#endif



#endif // _MOD_SELECTION_HPP_