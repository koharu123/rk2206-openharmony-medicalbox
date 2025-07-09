#ifndef __SU_03_H__
#define __SU_03_H__

#include <stdint.h>

enum open_command
{
    open_state_on = 0x0001,
    open_state_off,
};
 
enum rotation_command
{
    rotation_state = 0x0101,
    reset_state,
};
 
enum shut_command
{
    shut_state_on = 0x0201,
};

enum sensor_command
{
    temperature_get = 0x0301,
    humidity_get,
};

enum else_command
{
    take_medicine_cnt = 0x0401,
};

void su03t_init(void);
void su03t_send_double_msg(uint8_t index, double dat);
 
#endif
 