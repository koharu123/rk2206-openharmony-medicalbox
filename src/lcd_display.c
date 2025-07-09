#include "lcd.h"
#include "picture.h"
#include "lcd_display.h"
#include "medical_box.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>


void lcd_show_ui(void)
{
    lcd_show_picture(40,0,240,150, gImage);
    lcd_show_chinese(96, 150, "智能药盒", LCD_RED, LCD_WHITE, 32, 0);
    lcd_show_chinese(32, 182, "今日已吃", LCD_RED, LCD_WHITE, 32, 0);
    lcd_show_chinese(224,182, "次", LCD_RED, LCD_WHITE, 32, 0);
    

}

/***************************************************************
* 函数名称: lcd_set_cnt
* 说    明: 设置今日已吃次数
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void lcd_set_cnt()
{
    int cnt;

    cnt=get_take_cnt();
    
    uint8_t buf[50] = {0};

    sprintf(buf, "%d", cnt);

    lcd_show_chinese(176, 182, buf, LCD_RED, LCD_WHITE, 32, 0);
}

/***************************************************************
* 函数名称: lcd_dev_init
* 说    明: lcd初始化
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void lcd_dev_init(void)
{
    lcd_init();
    lcd_fill(0, 0, LCD_W, LCD_H, LCD_WHITE);
}