#include "box_event.h"
#include "ohos_init.h"
#include "los_task.h"

static unsigned int event_queue_id;

#define EVENT_QUEUE_LENGTH 10 //number of events
#define BUFFER_LEN         20 //bytes of event
 
/**
 * @brief 初始化事件队列
 * 
 * 创建用于线程间通信的消息队列
 */
void box_event_init(void) // medical_box_example()中调用
{
    unsigned int ret = LOS_OK;

    ret = LOS_QueueCreate("eventQ", 
                         EVENT_QUEUE_LENGTH,
                         &event_queue_id,
                         0,
                         BUFFER_LEN);
    if (ret != LOS_OK) {
        printf("Failed to create Message Queue, ret: 0x%x\n", ret);
        return;
    }
}

/**
 * @brief 发送事件到队列
 * 
 * @param event 要发送的事件指针
 */
void box_event_send(event_info_t *event) 
{
    LOS_QueueWriteCopy(event_queue_id,
                      event,
                      sizeof(event_info_t),
                      LOS_WAIT_FOREVER);
}

/**
 * @brief 等待接收事件
 * 
 * @param event 接收事件的内存指针
 * @param timeoutMs 超时时间(毫秒)
 * @return int 执行结果
 */
int box_event_wait(event_info_t *event, int timeoutMs) 
{
    return LOS_QueueReadCopy(event_queue_id,
                           event,
                           sizeof(event_info_t),
                           LOS_MS2Tick(timeoutMs));
}