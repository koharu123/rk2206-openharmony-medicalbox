#include "medical_box.h"

#include <stdio.h>
#include <stdbool.h>
 
#include "iot_errno.h"
 
#include "iot_pwm.h"
#include "iot_gpio.h"
#include "su_03.h"
#include "iot1.h"
#include "adc_key.h"
#include "string.h"
#include "iot_pwm.h"

int no = 1;
int take_cnt = 0;
bool onoff = false;

 /**
  * @brief 按键处理函数
  * 
  * @param key_no 按键号
  */
void medical_box_key_process(int key_no)
{
    printf("medical_box_key_process:%d\n", key_no);
    if(key_no == KEY_UP) {
        printf("OPEN CLOSE OPERATION\n");
        if(!onoff) {
            servo_motor_90_degree();
            onoff = true;
        } else {
            servo_motor_0_degree();
            onoff = false;
        }
    } else if(key_no == KEY_DOWN) {
        printf("STOP BEEP\n");
        if(beep_is_on) {
            human_beep_off = true;
            take_cnt++;
            step_motor_rotate(); //转到下一格
        }
    } else if(key_no == KEY_LEFT) {
        printf("RESET\n");
        int where = get_no();
        if(where == 1) {
        } else {
            int i = 5 - where;
            while(i--) {
                step_motor_rotate();
            }
        }
    } else if(key_no == KEY_RIGHT) {
        printf("ROTATE OPENRATION\n");
        step_motor_rotate(); // 药盒正转一圈
    }
}


 /**
  * @brief 物联网的指令处理函数
  * 
  * @param iot_cmd iot的指令
  */
void medical_box_iot_cmd_process(int iot_cmd)
{
    switch (iot_cmd)
    {
        case IOT_CMD_SERVO_ON:
            servo_motor_90_degree();
            onoff = true;
            break;
        case IOT_CMD_SERVO_OFF:
            servo_motor_0_degree();
            onoff = false;
            break;
        case IOT_CMD_MOTOR:
            step_motor_rotate();
            break;
        case IOT_CMD_SET_TIME:
            box_set_time();
            break;
    }
}
 

 /**
  * @brief 语音管家发出的指令
  * 
  * @param su03t_cmd 语音管家的指令
  */
void medical_box_su03t_cmd_process(int su03t_cmd)
{
    switch (su03t_cmd)
    {
        case open_state_on: {
            printf("OPEN OPERATION\n");
            servo_motor_90_degree();
            onoff = true;
        }
            break;
        case open_state_off: {
            printf("CLOSE OPERATION\n");
            servo_motor_0_degree();
            onoff = false;
        }
            break;
        case rotation_state: {
            printf("ROTATE OPENRATION\n");
            step_motor_rotate();
        }
            break;
        case reset_state: {
            printf("RESET\n");
            int where = get_no();
            if(where == 1) {
            } else {
                int i = 5 - where;
                while(i--) {
                    step_motor_rotate();
                }
            }
        }
            break;
        case shut_state_on: {
            printf("STOP BEEP\n");
            if(beep_is_on) {
                human_beep_off = true;
                take_cnt++;
                step_motor_rotate(); //转到下一格
            }
        }
            break;
        case temperature_get: {
            double temp, humi;
            sht30_read_data(&temp, &humi);
            su03t_send_double_msg(1, temp);
        }
            break;
        case humidity_get: {
            double temp, humi;
            sht30_read_data(&temp, &humi);
            su03t_send_double_msg(2, humi);
        }
            break;
        case take_medicine_cnt: {
            double tcnt = get_take_cnt();
            su03t_send_double_msg(6, tcnt);
        }
        default:
            break;
    }
}

// 设置时间
void box_set_time() {
    alarm_cnt = getAlarmcnt();  // 在接收到下发闹铃设置后，更新目前设置的闹铃数量
    TimeSchedule t = getSetTime();
    h1 = t.time1.hour;
    m1 = t.time1.minute;
    h2 = t.time2.hour;
    m2 = t.time2.minute;
    h3 = t.time3.hour;
    m3 = t.time3.minute;
    h4 = t.time4.hour;
    m4 = t.time4.minute;
}


//开关舱门事件处理
#define motor_port EPWMDEV_PWM4_M0
#define PWM_FREQ 50
#define CYCLE_0_DEGREE 3
#define CYCLE_90_DEGREE 7
#define CYCLE_180_DEGREE 12
#define DELAY_TIME 1000

static bool isopen = false;

int servo_motor_init(){
    unsigned int ret;
    ret = IoTPwmInit(motor_port);
    if(ret != 0){
        printf("IoTPwmInit failed(%d)\n",motor_port);
        return -1;
    }

    //初始状态：舵机是0度
    IoTPwmStart(motor_port,CYCLE_0_DEGREE,PWM_FREQ);
    isopen = false;
    LOS_Msleep(DELAY_TIME);
    return 0;
}

int servo_motor_0_degree(){
    if(isopen) { //只有门开的时候才会执行这个关门函数
        IoTPwmStart(motor_port,CYCLE_0_DEGREE,PWM_FREQ);
        LOS_Msleep(DELAY_TIME);
        isopen= false;
    }
    return 0;
}

int servo_motor_90_degree(){
    if(!isopen) { //只有门关的时候才会执行这个开门函数
        IoTPwmStart(motor_port,CYCLE_90_DEGREE,PWM_FREQ);
        LOS_Msleep(DELAY_TIME);
        isopen = true;
    }
    return 0;
}

int get_open_state(){
    return isopen;
}

//药仓旋转事件处理
#define GPIO_ALARM_LIGHT     GPIO0_PA5
#define MOTOR_A         GPIO0_PB0
#define MOTOR_B         GPIO0_PB1
#define MOTOR_C         GPIO0_PB6
#define MOTOR_D         GPIO0_PB7

int step_motor_init() {
    IoTGpioInit(GPIO_ALARM_LIGHT);
    IoTGpioInit(MOTOR_A);
    IoTGpioInit(MOTOR_B);
    IoTGpioInit(MOTOR_C);
    IoTGpioInit(MOTOR_D);

    IoTGpioSetDir(GPIO_ALARM_LIGHT, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(GPIO_ALARM_LIGHT, IOT_GPIO_VALUE0);

    IoTGpioSetDir(MOTOR_A, IOT_GPIO_DIR_OUT);
    IoTGpioSetDir(MOTOR_B, IOT_GPIO_DIR_OUT);
    IoTGpioSetDir(MOTOR_C, IOT_GPIO_DIR_OUT);
    IoTGpioSetDir(MOTOR_D, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(MOTOR_A, IOT_GPIO_VALUE0);
    return 0;
}

int step_motor_run() {
    int c = 0;
    //转轴旋转一周
    while (c < (64 * 8)) {
        IoTGpioSetOutputVal(MOTOR_A, IOT_GPIO_VALUE1);
        IoTGpioSetOutputVal(MOTOR_B, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_C, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_D, IOT_GPIO_VALUE0);
        LOS_Msleep(1);

        IoTGpioSetOutputVal(MOTOR_A, IOT_GPIO_VALUE1);
        IoTGpioSetOutputVal(MOTOR_B, IOT_GPIO_VALUE1);
        IoTGpioSetOutputVal(MOTOR_C, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_D, IOT_GPIO_VALUE0);
        LOS_Msleep(1);

        IoTGpioSetOutputVal(MOTOR_A, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_B, IOT_GPIO_VALUE1);
        IoTGpioSetOutputVal(MOTOR_C, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_D, IOT_GPIO_VALUE0);
        LOS_Msleep(1);

        IoTGpioSetOutputVal(MOTOR_A, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_B, IOT_GPIO_VALUE1);
        IoTGpioSetOutputVal(MOTOR_C, IOT_GPIO_VALUE1);
        IoTGpioSetOutputVal(MOTOR_D, IOT_GPIO_VALUE0);
        LOS_Msleep(1);

        IoTGpioSetOutputVal(MOTOR_A, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_B, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_C, IOT_GPIO_VALUE1);
        IoTGpioSetOutputVal(MOTOR_D, IOT_GPIO_VALUE0);
        LOS_Msleep(1);

        IoTGpioSetOutputVal(MOTOR_A, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_B, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_C, IOT_GPIO_VALUE1);
        IoTGpioSetOutputVal(MOTOR_D, IOT_GPIO_VALUE1);
        LOS_Msleep(1);

        IoTGpioSetOutputVal(MOTOR_A, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_B, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_C, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_D, IOT_GPIO_VALUE1);
        LOS_Msleep(1);

        IoTGpioSetOutputVal(MOTOR_A, IOT_GPIO_VALUE1);
        IoTGpioSetOutputVal(MOTOR_B, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_C, IOT_GPIO_VALUE0);
        IoTGpioSetOutputVal(MOTOR_D, IOT_GPIO_VALUE1);
        LOS_Msleep(1);
        
        c++;
    }
    IoTGpioSetOutputVal(MOTOR_A, IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(MOTOR_B, IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(MOTOR_C, IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(MOTOR_D, IOT_GPIO_VALUE0);
}

int step_motor_rotate() {
    IoTGpioSetOutputVal(GPIO_ALARM_LIGHT, IOT_GPIO_VALUE1);
    set_led_yellow();
    step_motor_run();
    if(no == 4) {
        no = 1;
    } else {
        no++;
    }
    IoTGpioSetOutputVal(GPIO_ALARM_LIGHT, IOT_GPIO_VALUE0);
    set_led_green();
    return 0;
}

int get_no() {
    return no;
}

int get_take_cnt() {
    return take_cnt;
}

void overtime_action() {
    step_motor_rotate();
    take_cnt++;
}

#define BEEP_PORT EPWMDEV_PWM5_M0

void beep_init() {
    unsigned int ret;
    ret = IoTPwmInit(BEEP_PORT);
    if (ret != 0) {
        printf("IoTPwmInit failed(%d)\n", BEEP_PORT);
    }
}
void beep_on() {
    IoTPwmStart(BEEP_PORT,10,1000);
    beep_is_on = true;
    set_led_red();
}

void beep_off() {
    beep_is_on = false;
    IoTPwmStop(BEEP_PORT);
    set_led_green();
}

void beep_on_second(int second) {
    beep_on();
    LOS_Msleep(second*1000);
    beep_off();
}

/* RGB对应的PWM通道 */
#define LED_R_PORT EPWMDEV_PWM1_M1
#define LED_G_PORT EPWMDEV_PWM7_M1
#define LED_B_PORT EPWMDEV_PWM0_M1

// 宏定义用于创建RGB颜色值
#define RGB(r, g, b) (((r) << 16) | ((g) << 8) | (b))

#define GET_R(x) ((x>>16)&0xff)
#define GET_G(x) ((x>>8)&0xff)
#define GET_B(x) ((x>>0)&0xff)

#define CALC_DUTY(x)  (x*100.0/0xff)

// 常见颜色的RGB值
#define BLACK   RGB(0, 0, 0)
#define WHITE   RGB(255, 255, 255)
#define RED     RGB(255, 0, 0)
#define GREEN   RGB(0, 255, 0)
#define BLUE    RGB(0, 0, 255)
#define YELLOW  RGB(255, 255, 0)
#define CYAN    RGB(0, 255, 255)
#define MAGENTA RGB(255, 0, 255)
#define ORANGE  RGB(255, 165, 0)
#define PURPLE  RGB(128, 0, 128)
#define GRAY    RGB(128, 128, 128)
#define LIGHT_GRAY RGB(211, 211, 211)
#define DARK_GRAY RGB(169, 169, 169)
#define BROWN   RGB(165, 42, 42)
#define PINK    RGB(255, 192, 203)
#define TURQUOISE RGB(64, 224, 208)

int duty_fix(int duty){

    if(duty > 99){
        duty = 99;
    }
    if(duty < 1){
        duty = 1;
    }
    return duty;
}

void pwm_light_init()
{
    unsigned int ret;
    
    ret = IoTPwmInit(LED_R_PORT);
    if (ret != 0) {
        printf("IoTPwmInit failed(%d)\n", LED_R_PORT);
    }

    ret = IoTPwmInit(LED_G_PORT);
    if (ret != 0) {
        printf("IoTPwmInit failed(%d)\n", LED_G_PORT);
    }

    ret = IoTPwmInit(LED_B_PORT);
    if (ret != 0) {
        printf("IoTPwmInit failed(%d)\n", LED_B_PORT);
    }
}

void set_led_green()
{
    unsigned int ret;
    int r_duty = CALC_DUTY(GET_R(GREEN));
    int g_duty = CALC_DUTY(GET_G(GREEN));
    int b_duty = CALC_DUTY(GET_B(GREEN));
    
    r_duty = duty_fix(r_duty);
    g_duty = duty_fix(g_duty);
    b_duty = duty_fix(b_duty);

    ret = IoTPwmStart(LED_R_PORT, r_duty, 1000);
    if (ret != 0) {
        printf("IoTPwmStart failed(%d)\n", LED_R_PORT);
    }

    ret = IoTPwmStart(LED_G_PORT, g_duty, 1000);
    if (ret != 0) {
        printf("IoTPwmStart failed(%d)\n", LED_G_PORT);
    }

    ret = IoTPwmStart(LED_B_PORT, b_duty, 1000);
    if (ret != 0) {
        printf("IoTPwmStart failed(%d)\n", LED_B_PORT);
    }
}

void set_led_red()
{
    unsigned int ret;
    int r_duty = CALC_DUTY(GET_R(RED));
    int g_duty = CALC_DUTY(GET_G(RED));
    int b_duty = CALC_DUTY(GET_B(RED));
    
    r_duty = duty_fix(r_duty);
    g_duty = duty_fix(g_duty);
    b_duty = duty_fix(b_duty);

    ret = IoTPwmStart(LED_R_PORT, r_duty, 1000);
    if (ret != 0) {
        printf("IoTPwmStart failed(%d)\n", LED_R_PORT);
    }

    ret = IoTPwmStart(LED_G_PORT, g_duty, 1000);
    if (ret != 0) {
        printf("IoTPwmStart failed(%d)\n", LED_G_PORT);
    }

    ret = IoTPwmStart(LED_B_PORT, b_duty, 1000);
    if (ret != 0) {
        printf("IoTPwmStart failed(%d)\n", LED_B_PORT);
    }
}

void set_led_yellow()
{
    unsigned int ret;
    int r_duty = CALC_DUTY(GET_R(YELLOW));
    int g_duty = CALC_DUTY(GET_G(YELLOW));
    int b_duty = CALC_DUTY(GET_B(YELLOW));
    
    r_duty = duty_fix(r_duty);
    g_duty = duty_fix(g_duty);
    b_duty = duty_fix(b_duty);

    ret = IoTPwmStart(LED_R_PORT, r_duty, 1000);
    if (ret != 0) {
        printf("IoTPwmStart failed(%d)\n", LED_R_PORT);
    }

    ret = IoTPwmStart(LED_G_PORT, g_duty, 1000);
    if (ret != 0) {
        printf("IoTPwmStart failed(%d)\n", LED_G_PORT);
    }

    ret = IoTPwmStart(LED_B_PORT, b_duty, 1000);
    if (ret != 0) {
        printf("IoTPwmStart failed(%d)\n", LED_B_PORT);
    }
}
