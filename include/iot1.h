#ifndef _IOT_H_
#define _IOT_H_

#include <stdbool.h>

typedef struct
{
    double temperature;
    double humidity;
    int no;
    int take_cnt;
} e_iot_data;

typedef struct {
    int hour;
    int minute;
} TimePoint;

typedef struct {
    TimePoint time1;
    TimePoint time2;
    TimePoint time3;
    TimePoint time4;
} TimeSchedule;

#define IOT_CMD_SERVO_ON 0x01
#define IOT_CMD_SERVO_OFF 0x02
#define IOT_CMD_MOTOR 0x03
#define IOT_CMD_SET_TIME 0x04

int wait_message();
void mqtt_init();
unsigned int mqtt_is_connected();
void send_msg_to_mqtt(e_iot_data *iot_data);

TimeSchedule getSetTime();
int getAlarmcnt();

#endif // _IOT_H_