/**
 * @file    general_def.h
 * @brief   工程通用常量:单位换算与数学宏
 * @layer   FRAMEWORK
 * @author  modified by 小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#ifndef GENERAL_DEF_H
#define GENERAL_DEF_H

// 一些module的通用数值型定义,注意条件macro兼容,一些宏可能在math.h中已经定义过了

#ifndef FW_PI
#define FW_PI 3.1415926535f
#endif
#define FW_TWO_PI (FW_PI * 2.0f) // 2 pi

#define FW_RAD_TO_DEG 57.2957795f    // 180/pi
#define FW_DEG_TO_RAD 0.01745329252f // pi/180

#define FW_RPM_TO_DEG_S 6.0f         // ×360°/60sec
#define FW_RPM_TO_RAD_S   0.104719755f // ×2pi/60sec

/* 通用绝对值:参数必须带括号,避免 MATH_ABS(a-b) 展开成 -a-b */
#ifndef MATH_ABS
#define MATH_ABS(x) (((x) > 0) ? (x) : -(x))
#endif

#endif // !GENERAL_DEF_H
