/**
 ****************************************************************************************************
 * @file        ltdc.h
 * @author      ����ԭ���Ŷ�(ALIENTEK)
 * @version     V1.0
 * @date        2022-09-06
 * @brief       LTDC ��������
 * @license     Copyright (c) 2020-2032, �������������ӿƼ����޹�˾
 ****************************************************************************************************
 * @attention
 *
 * ʵ��ƽ̨:����ԭ�� ������ H743������
 * ������Ƶ:www.yuanzige.com
 * ������̳:www.openedv.com
 * ��˾��ַ:www.alientek.com
 * �����ַ:openedv.taobao.com
 *
 * �޸�˵��
 * V1.0 20220906
 * ��һ�η���
 *
 ****************************************************************************************************
 */

#ifndef _LTDC_H
#define _LTDC_H

#include "./SYSTEM/sys/sys.h"


/* LCD LTDC��Ҫ������ */
typedef struct  
{
    uint32_t pwidth;      /* LTDC���Ŀ���,�̶�����,������ʾ����ı�,���Ϊ0,˵��û���κ�RGB������ */
    uint32_t pheight;     /* LTDC���ĸ߶�,�̶�����,������ʾ����ı� */
    uint16_t hsw;         /* ˮƽͬ������ */
    uint16_t vsw;         /* ��ֱͬ������ */
    uint16_t hbp;         /* ˮƽ���� */
    uint16_t vbp;         /* ��ֱ���� */
    uint16_t hfp;         /* ˮƽǰ�� */
    uint16_t vfp;         /* ��ֱǰ��  */
    uint8_t activelayer;  /* ��ǰ����:0/1 */
    uint8_t dir;          /* 0,����;1,����; */
    uint16_t width;       /* LTDC���� */
    uint16_t height;      /* LTDC�߶� */
    uint32_t pixsize;     /* ÿ��������ռ�ֽ��� */
}_ltdc_dev; 

extern _ltdc_dev lcdltdc;                   /* ����LCD LTDC���� */
extern LTDC_HandleTypeDef g_ltdc_handle;    /* LTDC��� */
extern DMA2D_HandleTypeDef g_dma2d_handle;  /* DMA2D��� */

/******************************************************************************************/
/* LTDC_BL/DE/VSYNC/HSYNC/CLK ���� ���� 
 * LTDC_R3~R7, G2~G7, B3~B7,��������̫��,�Ͳ������ﶨ����,ֱ����ltcd_init�����޸�.��������ֲ��ʱ��,
 * ���˸���5��IO��, ���ø�ltcd_init�����R3~R7, G2~G7, B3~B7���ڵ�IO��.
 */

/* BL��MCU������Ź��� �������ﲻ�ö��� BL���� */
#define LTDC_BL_GPIO_PORT               GPIOB
#define LTDC_BL_GPIO_PIN                GPIO_PIN_5
#define LTDC_BL_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)    /* ����IO��ʱ��ʹ�� */

#define LTDC_DE_GPIO_PORT               GPIOF
#define LTDC_DE_GPIO_PIN                GPIO_PIN_10
#define LTDC_DE_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOF_CLK_ENABLE(); }while(0)    /* ����IO��ʱ��ʹ�� */

#define LTDC_VSYNC_GPIO_PORT            GPIOI
#define LTDC_VSYNC_GPIO_PIN             GPIO_PIN_9
#define LTDC_VSYNC_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOI_CLK_ENABLE(); }while(0)    /* ����IO��ʱ��ʹ�� */

#define LTDC_HSYNC_GPIO_PORT            GPIOI
#define LTDC_HSYNC_GPIO_PIN             GPIO_PIN_10
#define LTDC_HSYNC_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOI_CLK_ENABLE(); }while(0)    /* ����IO��ʱ��ʹ�� */

#define LTDC_CLK_GPIO_PORT              GPIOG
#define LTDC_CLK_GPIO_PIN               GPIO_PIN_7
#define LTDC_CLK_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOG_CLK_ENABLE(); }while(0)    /* ����IO��ʱ��ʹ�� */


/* ������ɫ���ظ�ʽ,һ����RGB565 */
#define LTDC_PIXFORMAT           LTDC_PIXEL_FORMAT_RGB565

/* ����Ĭ�ϱ�������ɫ */
#define LTDC_BACKLAYERCOLOR      0X00000000

/* LTDC֡�������׵�ַ,���ﶨ����SDRAM����. */
#define LTDC_FRAME_BUF_ADDR      0XC0000000  

/* LTDC������� */
#define LTDC_BL(x)   do{ x ? \
                      HAL_GPIO_WritePin(LTDC_BL_GPIO_PORT, LTDC_BL_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LTDC_BL_GPIO_PORT, LTDC_BL_GPIO_PIN, GPIO_PIN_RESET); \
                     }while(0)

/******************************************************************************************/

void ltdc_switch(uint8_t sw);                                                                                   /* LTDC���� */
void ltdc_layer_switch(uint8_t layerx, uint8_t sw);                                                             /* �㿪�� */
void ltdc_select_layer(uint8_t layerx);                                                                         /* ��ѡ�� */
void ltdc_display_dir(uint8_t dir);                                                                             /* ��ʾ������� */
void ltdc_draw_point(uint16_t x, uint16_t y, uint32_t color);                                                   /* ���㺯�� */
uint32_t ltdc_read_point(uint16_t x, uint16_t y);                                                               /* ���㺯�� */
void ltdc_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color);                             /* ���ε�ɫ��亯�� */
void ltdc_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);                      /* ���β�ɫ��亯�� */
void ltdc_clear(uint32_t color);                                                                                /* �������� */
uint8_t ltdc_clk_set(uint32_t pll3n, uint32_t pll3m, uint32_t pll3r);                                           /* LTDCʱ������ */
void ltdc_layer_window_config(uint8_t layerx, uint16_t sx, uint16_t sy, uint16_t width, uint16_t height);       /* LTDC�㴰������ */
void ltdc_layer_parameter_config(uint8_t layerx, uint32_t bufaddr, uint8_t pixformat, uint8_t alpha, uint8_t alpha0, uint8_t bfac1, uint8_t bfac2, uint32_t bkcolor); /* LTDC������������ */
uint16_t ltdc_panelid_read(void);                                                                               /* LTDC ID��ȡ���� */
void ltdc_init(void);                                                                                           /* LTDC��ʼ������ */
uint8_t ltdc_is_double_buffered(void);                                                                          /* ˫���ջ��� */
uint32_t ltdc_get_draw_buffer(void);                                                                            /* ��ȡ��ǰ������ַ */
uint32_t ltdc_get_display_buffer(void);                                                                         /* ��ȡ��ǰ��ʾ��ַ */
void ltdc_present_draw_buffer(void);                                                                            /* ˫���������л� */

#endif 
