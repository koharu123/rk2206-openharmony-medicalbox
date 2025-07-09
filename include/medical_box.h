#ifndef __MEDICAL_BOX_H__
#define __MEDICAL_BOX_H__
 
#include <stdint.h>
#include <stdbool.h>
 

void medical_box_su03t_cmd_process(int su03t_cmd);
void medical_box_iot_cmd_process(int iot_cmd);
void medical_box_key_process(int key_no);

int get_no();
int get_take_cnt();

int step_motor_rotate();
int step_motor_run();
int step_motor_init();

void box_set_time();

void beep_init();
void beep_on();
void beep_off();
void beep_on_second (int second);

void set_led_green();
void set_led_red();
void set_led_yellow();
void pwm_light_init();

extern int h1, m1;
extern int h2, m2;
extern int h3, m3;
extern int h4, m4;

extern bool beep_is_on;
extern bool human_beep_off;

extern int alarm_cnt;

void overtime_action();
 
#endif
 