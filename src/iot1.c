#include <stdio.h>
#include <stdlib.h>

#include "MQTTClient.h"
#include "cJSON.h"
#include "cmsis_os2.h"
#include "config_network.h"
#include "iot1.h"
#include "los_task.h"
#include "ohos_init.h"
#include "box_event.h"

#define MQTT_DEVICES_PWD "50f87515628b944ca959978b3fb980aa4325ab42b0d89b951ee92694d376bfff"
#define USERNAME "dk4ft7im2bxf-1940310118675513401_medicalbox"
#define DEVICE_ID "dk4ft7im2bxf-1940310118675513401_medicalbox_0_0_2025070418"
//华为云服务器地址117.78.5.125
//通鸿117.78.16.25
// #define HOST_ADDR          "117.78.16.25"
#define HOST_ADDR          "cdc7f9876b.st1.iotda-device.cn-north-4.myhuaweicloud.com"
//通鸿ID：  dk4ft7im2bxf-1940310118675513401_medicalbox
//华为云ID：dk4ft7im2bxf-1940310118675513401_medicalbox
//#define DEVICE_ID          "dk4ft7im2bxf-1940310118675513401_medicalbox"
#define PUBLISH_TOPIC      "$oc/devices/" USERNAME "/sys/properties/report"
#define SUBCRIB_TOPIC      "$oc/devices/" USERNAME "/sys/commands/#"
#define RESPONSE_TOPIC     "$oc/devices/" USERNAME "/sys/commands/response"

#define MAX_BUFFER_LENGTH  512
#define MAX_STRING_LENGTH  64

static unsigned char sendBuf[MAX_BUFFER_LENGTH];
static unsigned char readBuf[MAX_BUFFER_LENGTH];

Network network;
MQTTClient client;

//华为云
//#define HOST_ADDR "cdc7f9876b.st1.iotda-device.cn-north-4.myhuaweicloud.com"
static char mqtt_devid[128] = DEVICE_ID;
static char mqtt_pwd[128] = MQTT_DEVICES_PWD;
static char mqtt_username[64] = USERNAME;

// //通鸿
// static char mqtt_devid[64] = DEVICE_ID;
// static char mqtt_pwd[64] = MQTT_DEVICES_PWD;
// static char mqtt_username[64] = DEVICE_ID;

static char mqtt_hostaddr[64] = HOST_ADDR;

static char publish_topic[128] = PUBLISH_TOPIC;
static char subcribe_topic[128] = SUBCRIB_TOPIC;
static char response_topic[128] = RESPONSE_TOPIC;

static unsigned int mqttConnectFlag = 0;

/***************************************************************
* 函数名称: send_msg_to_mqtt
* 说    明: 将本地设备数据封装为JSON发送到云端
* 参    数: e_iot_data *iot_data：数据
* 返 回 值: 无
***************************************************************/
void send_msg_to_mqtt(e_iot_data *iot_data)
{
    int rc;
    MQTTMessage message;
    char payload[MAX_BUFFER_LENGTH] = {0};
    char str[MAX_STRING_LENGTH] = {0};

    if (mqttConnectFlag == 0) {
        printf("mqtt not connect\n");
        return;
    }
    
    cJSON *root = cJSON_CreateObject();
    if (root != NULL) {
        cJSON *serv_arr = cJSON_AddArrayToObject(root, "services");
        cJSON *arr_item = cJSON_CreateObject();
        cJSON_AddStringToObject(arr_item, "service_id", "medicalBox");
        cJSON *pro_obj = cJSON_CreateObject();
        cJSON_AddItemToObject(arr_item, "properties", pro_obj);

        memset(str, 0, MAX_BUFFER_LENGTH);

        // 温度
        sprintf(str, "%5.2f℃", iot_data->temperature);
        cJSON_AddStringToObject(pro_obj, "temperature", str);

        // 湿度
        sprintf(str, "%5.2f%%", iot_data->humidity);
        cJSON_AddStringToObject(pro_obj, "humidity", str);

        // 在第几个盒子
        sprintf(str, "%5d", iot_data->no);
        cJSON_AddStringToObject(pro_obj, "no", str);

        // 吃了几次药
        sprintf(str, "%5d", iot_data->take_cnt);
        cJSON_AddStringToObject(pro_obj, "take_cnt", str);


        cJSON_AddItemToArray(serv_arr, arr_item);
        char *palyload_str = cJSON_PrintUnformatted(root);
        strcpy(payload, palyload_str);
        cJSON_free(palyload_str);
        cJSON_Delete(root);
    }

    message.qos = 0;
    message.retained = 0;
    message.payload = payload;
    message.payloadlen = strlen(payload);

    sprintf(publish_topic, "$oc/devices/%s/sys/properties/report", mqtt_devid);
    if ((rc = MQTTPublish(&client, publish_topic, &message)) != 0) {
        printf("Return code from MQTT publish is %d\n", rc);
        mqttConnectFlag = 0;
    } else {
        printf("mqtt publish success:%s\n", payload);
    }
}

/***************************************************************
* 函数名称: set_servo_state
* 说    明: 设置舵机状态
* 参    数: cJSON *root
* 返 回 值: 无
***************************************************************/
void set_servo_state(cJSON *root)
{
    cJSON *para_obj = NULL;
    cJSON *status_obj = NULL;
    char *value = NULL;

    event_info_t event = {0};
    event.event = event_iot_cmd;

    para_obj = cJSON_GetObjectItem(root, "paras");
    status_obj = cJSON_GetObjectItem(para_obj, "onoff");
    if (status_obj != NULL) {
        value = cJSON_GetStringValue(status_obj);
        if (!strcmp(value, "ON")) { //strcmp()返回0的时候说明字符串相等
            event.data.iot_data = IOT_CMD_SERVO_ON;
        } else if (!strcmp(value, "OFF")) {
            event.data.iot_data = IOT_CMD_SERVO_OFF;
        }
        box_event_send(&event);
    }
}

/***************************************************************
* 函数名称: set_step_motor_state
* 说    明: 设置步进电机状态
* 参    数: cJSON *root
* 返 回 值: 无
***************************************************************/
void set_step_motor_state(cJSON *root)
{
    cJSON *para_obj = NULL;
    cJSON *status_obj = NULL;
    char *value = NULL;

    event_info_t event = {0};
    event.event = event_iot_cmd;

    para_obj = cJSON_GetObjectItem(root, "paras");
    status_obj = cJSON_GetObjectItem(para_obj, "turn");

    if (status_obj != NULL) {
        value = cJSON_GetStringValue(status_obj);
        if (!strcmp(value, "TURN")) { //只有下发为TURN的时候才转动
            event.data.iot_data = IOT_CMD_MOTOR;
        } 
        box_event_send(&event);
    }   
}

TimeSchedule schedule;
int howmanyalarm = 0;

static int is_valid_time(int hour, int minute) {
    return (hour >= 0 && hour < 24 && minute >= 0 && minute < 60);
}

void parse_time_string(const char *time_str, TimeSchedule *schedule) {
    schedule->time1.hour = 25; schedule->time1.minute = 0;
    schedule->time2.hour = 25; schedule->time2.minute = 0;
    schedule->time3.hour = 25; schedule->time3.minute = 0;
    schedule->time4.hour = 25; schedule->time4.minute = 0;

    if (time_str == NULL || time_str[0] == '\0') {
        printf("Empty time string, all times set to 25:00\n");
        howmanyalarm = 0;
        return;
    }

    int parsed = sscanf(time_str, 
                       "%d:%d;%d:%d;%d:%d;%d:%d",
                       &schedule->time1.hour, &schedule->time1.minute,
                       &schedule->time2.hour, &schedule->time2.minute,
                       &schedule->time3.hour, &schedule->time3.minute,
                       &schedule->time4.hour, &schedule->time4.minute);
    
    switch (parsed / 2) {
        case 4: // xx:xx;xx:xx;xx:xx;xx:xx
            howmanyalarm = 4;
            break;
        case 3: // xx:xx;xx:xx;xx:xx
            howmanyalarm = 3;
            schedule->time4.hour = 25;
            break;
        case 2: //  xx:xx;xx:xx
            howmanyalarm = 2;
            schedule->time3.hour = 25;
            schedule->time4.hour = 25;
            break;
        case 1: // xx:xx
            howmanyalarm = 1;
            schedule->time2.hour = 25;
            schedule->time3.hour = 25;
            schedule->time4.hour = 25;
            break;
        default:
            printf("Invalid time format: %s\n", time_str);
            return;
    }

    if ((schedule->time1.hour != 25 && !is_valid_time(schedule->time1.hour, schedule->time1.minute)) ||
        (schedule->time2.hour != 25 && !is_valid_time(schedule->time2.hour, schedule->time2.minute)) ||
        (schedule->time3.hour != 25 && !is_valid_time(schedule->time3.hour, schedule->time3.minute)) ||
        (schedule->time4.hour != 25 && !is_valid_time(schedule->time4.hour, schedule->time4.minute))) {
        printf("Invalid time value in: %s\n", time_str);
        schedule->time1.hour = 25;
        schedule->time2.hour = 25;
        schedule->time3.hour = 25;
        schedule->time4.hour = 25;
    }
}

/***************************************************************
* 函数名称: set_time
* 说    明: 设置时间
* 参    数: cJSON *root
* 返 回 值: 无
***************************************************************/
void set_time(cJSON *root) {
    cJSON *para_obj = NULL;
    cJSON *status_obj = NULL;
    event_info_t event = {0};
    event.event = event_iot_cmd;

    if ((para_obj = cJSON_GetObjectItem(root, "paras")) && 
        (status_obj = cJSON_GetObjectItem(para_obj, "time"))) {
        
        const char *value = cJSON_GetStringValue(status_obj);
        parse_time_string(value, &schedule);

        printf("%d:%d;%d:%d;%d:%d;%d:%d\n",schedule.time1.hour, schedule.time1.minute,
            schedule.time2.hour, schedule.time2.minute,
            schedule.time3.hour, schedule.time3.minute,
            schedule.time4.hour, schedule.time4.minute);
        
        event.data.iot_data = IOT_CMD_SET_TIME;
        box_event_send(&event);
        
        if (value && value[0] == '\0') {
            printf("Empty time string processed\n");
        }
    }
}

TimeSchedule getSetTime() {
    return schedule;
}

// 获取目前闹铃的数量
int getAlarmcnt() {
    return howmanyalarm;
}

/***************************************************************
* 函数名称: mqtt_message_arrived
* 说    明: 本地接收mqtt数据
* 参    数: MessageData *data
* 返 回 值: 无
***************************************************************/
void mqtt_message_arrived(MessageData *data)
{
    int rc;
    cJSON *root = NULL;
    cJSON *cmd_name = NULL;
    char *cmd_name_str = NULL;
    char *request_id_idx = NULL;
    char request_id[40] = {0};
    MQTTMessage message;
    char payload[MAX_BUFFER_LENGTH];
    
    char rsptopic[128] = {0};

    printf("Message arrived on topic %.*s: %.*s\n",
           data->topicName->lenstring.len, data->topicName->lenstring.data,
           data->message->payloadlen, data->message->payload);

    // get request id
    request_id_idx = strstr(data->topicName->lenstring.data, "request_id=");
    strncpy(request_id, request_id_idx + 11, 36);

    // create response topic
    //sprintf(response_topic, "$oc/devices/%s/sys/commands/response", mqtt_devid);
    sprintf(rsptopic, "%s/request_id=%s", response_topic, request_id);

    // response message
    message.qos = 0;
    message.retained = 0;
    message.payload = payload;
    sprintf(payload, "{ \
        \"result_code\": 0, \
        \"response_name\": \"COMMAND_RESPONSE\", \
        \"paras\": { \
            \"result\": \"success\" \
        } \
    }");
    message.payloadlen = strlen(payload);

    // publish the msg to responese topic
    if ((rc = MQTTPublish(&client, rsptopic, &message)) != 0) {
        printf("Return code from MQTT publish is %d\n", rc);
        mqttConnectFlag = 0;
    }

    // 提取云端下发的命令，在本地进行处理
    /*{"command_name":"cmd","paras":{"cmd_value":"1"},"service_id":"server"}*/
    root = cJSON_ParseWithLength(data->message->payload, data->message->payloadlen);
    if (root != NULL) {
        cmd_name = cJSON_GetObjectItem(root, "command_name");
        if (cmd_name != NULL) {
            cmd_name_str = cJSON_GetStringValue(cmd_name);
            if (!strcmp(cmd_name_str, "step_motor_control")) {
                set_step_motor_state(root);
            } else if (!strcmp(cmd_name_str, "servo_control")) {
                set_servo_state(root);
            } else if (!strcmp(cmd_name_str, "set_time")) {
                set_time(root);
            }
        }
    }

    cJSON_Delete(root);
}

/***************************************************************
* 函数名称: wait_message
* 说    明: 等待信息
* 参    数: 无
* 返 回 值: 无
***************************************************************/
int wait_message()
{
    uint8_t rec = MQTTYield(&client, 5000);
    if (rec != 0) {
        mqttConnectFlag = 0;
    }
    if (mqttConnectFlag == 0) {
        return 0;
    }
    return 1;
}

/***************************************************************
* 函数名称: mqtt_init
* 说    明: mqtt初始化
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void mqtt_init()
{
    int rc;

    printf("Starting MQTT...\n");

    /*网络初始化*/
    NetworkInit(&network);

begin:
    /* 连接网络*/
    printf("NetworkConnect  ...\n");
    NetworkConnect(&network, HOST_ADDR, 1883);
    printf("MQTTClientInit  ...\n");
    /*MQTT客户端初始化*/
    MQTTClientInit(&client, &network, 2000, sendBuf, sizeof(sendBuf), readBuf,
                  sizeof(readBuf));

    MQTTString clientId = MQTTString_initializer;
    clientId.cstring = mqtt_devid;

    MQTTString userName = MQTTString_initializer;
    userName.cstring = mqtt_username;

    MQTTString password = MQTTString_initializer;
    password.cstring = mqtt_pwd;

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.clientID = clientId;
    data.username = userName;
    data.password = password;
    data.willFlag = 0;
    data.MQTTVersion = 4;
    data.keepAliveInterval = 60;
    data.cleansession = 1;

    printf("MQTTConnect  ...\n");
    rc = MQTTConnect(&client, &data);
    if (rc != 0) {
        printf("MQTTConnect: %d\n", rc);
        NetworkDisconnect(&network);
        MQTTDisconnect(&client);
        osDelay(200);
        goto begin;
    }

    printf("MQTTSubscribe  ...\n");
    //sprintf(subcribe_topic, "$oc/devices/%s/sys/commands/+", mqtt_devid);
    rc = MQTTSubscribe(&client, subcribe_topic, 0, mqtt_message_arrived);
    if (rc != 0) {
        printf("MQTTSubscribe: %d\n", rc);
        osDelay(200);
        goto begin;
    }

    mqttConnectFlag = 1;
}

/***************************************************************
* 函数名称: mqtt_is_connected
* 说    明: mqtt连接状态
* 参    数: 无
* 返 回 值: unsigned int 状态
***************************************************************/
unsigned int mqtt_is_connected()
{
    return mqttConnectFlag;
}