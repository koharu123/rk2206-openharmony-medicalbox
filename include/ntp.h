#ifndef __NTP_H__
#define __NTP_H__

#include <stdint.h>

/***************************************************************
* 函数名称: ntp_init
* 说    明: NTP服务初始化
* 参    数: 无
* 返 回 值: 0为成功，-1为失败
***************************************************************/
int ntp_init();


/***************************************************************
* 函数名称: ntp_reinit
* 说    明: NTP服务销毁
* 参    数: 无
* 返 回 值: 0为成功，-1为失败
***************************************************************/
int ntp_reinit();


/**
 * 从ntp服务器获取时间
 *
 * @param host_name ntp服务器地址, NULL: 使用默认的ntp服务地址
 *
 * @return >0: 获取到的ntp时间
 *         =0: 获取ntp时间失败
 */
uint32_t ntp_get_time(const char *host_name);


#endif /* __NTP_H__ */