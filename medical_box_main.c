
#include <stdio.h>
#include <stdbool.h>
#include "los_task.h"
#include "ohos_init.h"
#include "cmsis_os.h"
#include "config_network.h"
#include "medical_box.h"
#include "box_event.h"
#include "su_03.h"
#include "iot1.h"
#include "adc_key.h"
#include "drv_sensors.h"
#include "lcd.h"
#include "lcd_display.h"



#include <sys/time.h>
#include <time.h>
#include "ntp.h"

#define ROUTE_SSID      "HUAWEI-10EBS0"       // WiFi账号
#define ROUTE_PASSWORD  "556ff58F"    // WiFi密码

// #define ROUTE_SSID      "medical_box"       // WiFi账号
// #define ROUTE_PASSWORD  "openharmony"    // WiFi密码

#define MSG_QUEUE_LENGTH  16
#define BUFFER_LEN       50

#define NTP_SYNC_INTERVAL 60

static unsigned int m_msg_queue;
unsigned int m_su03_msg_queue;

typedef struct {
    int sync_status; // 0未同步，1已同步
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
} ntp_time_t;

static ntp_time_t g_ntp_time = {0};
static uint32_t g_now_time = 0;

// 默认时间一天四次
int h1 = 25, m1 = 0;
int h2 = 25, m2 = 0;
int h3 = 25, m3 = 0;
int h4 = 25, m4 = 0;

bool beep_is_on = false;
bool human_beep_off = false;

int alarm_cnt = 0;

/***************************************************************
 * 函数名称: iot_thread
 * 说    明: iot线程
 * 参    数: 无
 * 返 回 值: 无
 ***************************************************************/
void iot_thread(void *args) 
{
    uint8_t mac_address[12] = {0x00, 0xdc, 0xb6, 0x90, 0x01, 0x00, 0};
    char ssid[32] = ROUTE_SSID;
    char password[32] = ROUTE_PASSWORD;
    char mac_addr[32] = {0};

    FlashDeinit();
    FlashInit();

    VendorSet(VENDOR_ID_WIFI_MODE, "STA", 3);
    VendorSet(VENDOR_ID_MAC, mac_address, 6);
    VendorSet(VENDOR_ID_WIFI_ROUTE_SSID, ssid, sizeof(ssid));
    VendorSet(VENDOR_ID_WIFI_ROUTE_PASSWD, password, sizeof(password));

reconnect:
    SetWifiModeOff();
    int ret = SetWifiModeOn();
    if (ret != 0) {
        printf("wifi connect failed, please check wifi config and the AP!\n");
        return;
    }
    ntp_init();
    mqtt_init();

    if (mqtt_is_connected()) {
        g_now_time = ntp_get_time(NULL);
        if (g_now_time > 1600000000) { // 简单验证是否合理(2020年后)
            g_ntp_time.sync_status = 1;
            printf("NTP sync success: %lu\n", g_now_time);
        } else {
            g_ntp_time.sync_status = 0;
            printf("NTP sync failed, got invalid timestamp\n");
        }
    }

    while (1) {
        if (!wait_message()) {
            goto reconnect;
        }
        LOS_Msleep(1);
    }
}

/***************************************************************
 * 函数名称: medical_box_thread
 * 说    明: 医疗箱主控线程
 * 参    数: 无
 * 返 回 值: 无
 ***************************************************************/
void medical_box_thread(void *arg)
{
    double *data_ptr = NULL;
    e_iot_data iot_data = {0};

    
    // 硬件初始化
    servo_motor_init();  // 舵机通道初始化
    step_motor_init();   // 步进电机通道初始化
    su03t_init();        // 语音模块初始化
    i2c_dev_init();
    beep_init();
    pwm_light_init();
    lcd_dev_init();

    lcd_show_ui();
    lcd_set_cnt();

    set_led_green();

    while (1) {
        event_info_t event_info = {0};

        // 等待事件触发（超时3秒） 主线程中持续检测队列
        int ret = box_event_wait(&event_info, 3000);

        if (ret == LOS_OK) {
            printf("event recv %d, %d\n", event_info.event, event_info.data.iot_data);
            
            // 处理事件
            switch (event_info.event) {
                case event_key_press:
                    medical_box_key_process(event_info.data.key_no);
                    break;
                case event_iot_cmd:
                    medical_box_iot_cmd_process(event_info.data.iot_data);
                    break;
                case event_su03t:
                    medical_box_su03t_cmd_process(event_info.data.su03t_data);
                    break;
                default:
                    break;
            }
        }

        // 要发送到云端的数据
        double temp, humi;
        sht30_read_data(&temp,&humi); // 获取传感器温度和湿度

        if (mqtt_is_connected()) {
            iot_data.temperature = temp;
            iot_data.humidity = humi;
            iot_data.no = get_no();
            iot_data.take_cnt = get_take_cnt();
            lcd_set_cnt();

            send_msg_to_mqtt(&iot_data);
        }
    }
}

void ntp_thread(void *arg)
{
    uint32_t ntp_time = 0;
    int sync_counter = 0;
    
    while (1) {
        if (sync_counter++ >= NTP_SYNC_INTERVAL) {
            sync_counter = 0;
            
            if (mqtt_is_connected()) {
                printf("NTP sync start...\n");
                ntp_time = ntp_get_time(NULL);
                
                if (ntp_time != 0) {
                    g_now_time = ntp_time;
                    g_ntp_time.sync_status = 1;
                    printf("NTP sync success: %lu\n", ntp_time);
                } else {
                    g_ntp_time.sync_status = 0;
                    printf("NTP sync failed\n");
                }
            }
        }
        
        // 更新本地时间
        g_now_time++;
        
        time_t now_time_t = (time_t)g_now_time;
        struct tm *now_tm = localtime(&now_time_t);
        
        // 更新全局时间结构体
        g_ntp_time.year = now_tm->tm_year + 1900;
        g_ntp_time.month = now_tm->tm_mon + 1;
        g_ntp_time.day = now_tm->tm_mday;
        g_ntp_time.hour = now_tm->tm_hour;
        g_ntp_time.min = now_tm->tm_min;
        g_ntp_time.sec = now_tm->tm_sec;
        
        // 打印时间信息
        if (now_tm->tm_sec % 30 == 0) {
            printf("Current time: %04d-%02d-%02d %02d:%02d:%02d %s\n",
                   g_ntp_time.year, g_ntp_time.month, g_ntp_time.day,
                   g_ntp_time.hour, g_ntp_time.min, g_ntp_time.sec,
                   g_ntp_time.sync_status ? "(synced)" : "(not synced)");
        }
        
        LOS_Msleep(1000); // 每秒执行一次
    }
}

void alarm_monitor_thread(void *arg)
{
    while (1) {
        // 只有当NTP时间已同步时才进行监测
        if (g_ntp_time.sync_status == 1) {
            // 获取当前时间
            int current_hour = g_ntp_time.hour;
            int current_min = g_ntp_time.min;
            
            int current_year = g_ntp_time.year;
            if (current_year < 2025) {
                printf("Waiting for valid time sync...\n");
                LOS_Msleep(1000);
                continue;
            }
            
            // 检查是否到达任意一个预设时间
            if ((current_hour == h1 && current_min == m1) ||
                (current_hour == h2 && current_min == m2) ||
                (current_hour == h3 && current_min == m3) ||
                (current_hour == h4 && current_min == m4)) {
                
                printf("ALARM! It's time to take medicine! %02d:%02d\n", current_hour, current_min);
                su03t_send_double_msg(5, 1.00);
                
                human_beep_off = false;
                int beep_count = 0;
                const int max_beep_times = 3;
                const int beep_duration = 20000;
                const int interval = 40000;
                
                while (beep_count < max_beep_times && !human_beep_off) {
                    // 开始蜂鸣
                    beep_on();
                    uint32_t start_time = LOS_TickCountGet();
                    
                    while (!human_beep_off) {
                        if (LOS_TickCountGet() - start_time > beep_duration) {
                            break;
                        }
                        LOS_Msleep(100);
                    }
                    
                    // 停止蜂鸣
                    beep_off();
                    beep_count++;
                    if(beep_count == max_beep_times) {
                        overtime_action();
                    }
                    
                    // 如果还没达到最大次数，等待间隔时间
                    if (beep_count < max_beep_times) {
                        LOS_Msleep(interval);
                    }
                }
                
                // 等待1分钟，避免短时间内重复触发
                LOS_Msleep(60000);
            }
        } else {
            printf("Alarm monitor waiting for NTP sync...\n");
        }
        
        LOS_Msleep(1000); // 每秒检查一次
    }
}

void sht30_monitor_thread(void *arg)
{   
    while (1) {
        double temp, humi;
        sht30_read_data(&temp, &humi);

        printf("[SHT30 Monitor] Temperature: %.2f°C, Humidity: %.2f%%\n", temp, humi);


        printf("\r\n");

        LOS_Msleep(1500);

        if(humi > 70.0) {
            su03t_send_double_msg(3, humi);
            set_led_red();
            LOS_Msleep(5000);
            set_led_green();
        }
        LOS_Msleep(2000);
        if(temp > 40.0) {
            su03t_send_double_msg(4, temp);
            set_led_red();
            LOS_Msleep(5000);
            set_led_green();
        }
        // 等待30秒
        LOS_Msleep(30000);
    }
}


    


/***************************************************************
 * 函数名称: medical_box_example
 * 说    明: 开机自启动调用函数
 * 参    数: 无
 * 返 回 值: 无
 ***************************************************************/
void medical_box_example()
{
    unsigned int thread_id_1, thread_id_2, thread_id_3, thread_id_4, thread_id_5, thread_id_6;
    TSK_INIT_PARAM_S task_1 = {0};
    TSK_INIT_PARAM_S task_2 = {0};
    TSK_INIT_PARAM_S task_3 = {0};
    TSK_INIT_PARAM_S task_4 = {0};
    TSK_INIT_PARAM_S task_5 = {0};
    TSK_INIT_PARAM_S task_6 = {0};
    unsigned int ret = LOS_OK;


    
    box_event_init();
   
    // 创建医疗箱主线程
    printf("medical box thread\r\n");
    task_1.pfnTaskEntry = (TSK_ENTRY_FUNC)medical_box_thread;
    task_1.uwStackSize = 2048;
    task_1.pcName = "medical box thread";
    task_1.usTaskPrio = 24;
    ret = LOS_TaskCreate(&thread_id_1, &task_1);
    if (ret != LOS_OK) {
        printf("Failed to create medical box thread, ret:0x%x\n", ret);
        return;
    }

    // 创建按键检测线程
    printf("key thread\r\n");
    task_2.pfnTaskEntry = (TSK_ENTRY_FUNC)adc_key_thread;
    task_2.uwStackSize = 2048;
    task_2.pcName = "key thread";
    task_2.usTaskPrio = 24;
    ret = LOS_TaskCreate(&thread_id_2, &task_2);
    if (ret != LOS_OK) {
        printf("Failed to create key thread, ret:0x%x\n", ret);
        return;
    }

    // 创建网络线程
    printf("iot thread\r\n");
    task_3.pfnTaskEntry = (TSK_ENTRY_FUNC)iot_thread;
    task_3.uwStackSize = 20480 * 5;
    task_3.pcName = "iot thread";
    task_3.usTaskPrio = 24;
    ret = LOS_TaskCreate(&thread_id_3, &task_3);
    if (ret != LOS_OK) {
        printf("Failed to create iot thread, ret:0x%x\n", ret);
        return;
    }

    // 创建NTP时间同步线程
    printf("ntp thread\r\n");
    task_4.pfnTaskEntry = (TSK_ENTRY_FUNC)ntp_thread;
    task_4.uwStackSize = 2048;
    task_4.pcName = "ntp thread";
    task_4.usTaskPrio = 24;
    ret = LOS_TaskCreate(&thread_id_4, &task_4);
    if (ret != LOS_OK) {
        printf("Failed to create ntp thread, ret:0x%x\n", ret);
        return;
    }

    // 时钟监测线程
    printf("alarm monitor thread\r\n");
    task_5.pfnTaskEntry = (TSK_ENTRY_FUNC)alarm_monitor_thread;
    task_5.uwStackSize = 2048;
    task_5.pcName = "alarm monitor thread";
    task_5.usTaskPrio = 24;
    ret = LOS_TaskCreate(&thread_id_5, &task_5);
    if (ret != LOS_OK) {
        printf("Failed to create alarm monitor thread, ret:0x%x\n", ret);
        return;
    }


    // 温湿度监测线程
    printf("sht30 monitor thread\r\n");
    task_6.pfnTaskEntry = (TSK_ENTRY_FUNC)sht30_monitor_thread;
    task_6.uwStackSize = 2048;
    task_6.pcName = "sht30 monitor thread";
    task_6.usTaskPrio = 24;
    ret = LOS_TaskCreate(&thread_id_6, &task_6);
    if (ret != LOS_OK) {
        printf("Failed to create sht30 monitor thread, ret:0x%x\n", ret);
        return;
    }


}

APP_FEATURE_INIT(medical_box_example);