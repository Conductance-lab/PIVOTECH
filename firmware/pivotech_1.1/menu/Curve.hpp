#ifndef MENU_CURVE_HPP
#define MENU_CURVE_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Declarations for Curve-related functions used in menu
void CurveInit(void);

// Curve/easing helpers used by animations
float X2line_down(float start, float end, float lamda);
float X2line_up(float start, float end, float lamda);
float easeInOutQuad(float start, float end, float lamda);
float A_line(float start, float end, float mid, float lamda);

#ifdef __cplusplus
}
#endif

#endif // MENU_CURVE_HPP