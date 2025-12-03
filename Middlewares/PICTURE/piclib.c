/**
 ****************************************************************************************************
 * @file        piclib.c
 * @author      ����ԭ���Ŷ�(ALIENTEK)
 * @version     V1.0
 * @date        2022-09-06
 * @brief       ͼƬ����� ����
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

#include "./PICTURE/piclib.h"
#include "./BSP/LCD/ltdc.h"
#include "./BSP/LCD/lcd.h"
#include "./PICTURE/hjpgd.h"

extern uint32_t *g_ltdc_framebuf[2];   /* LTDC LCD֡��������ָ��,����ָ���Ӧ��С���ڴ����� */

_pic_info picinfo;                   /* ͼƬ��Ϣ */
_pic_phy pic_phy;                    /* ͼƬ��ʾ�����ӿ� */


//lcd.hû���ṩ�����ߺ���,��Ҫ�Լ�ʵ��
void piclib_draw_hline(uint16_t x0, uint16_t y0, uint16_t len, uint16_t color)
{
    if ((len == 0) || (x0 > lcddev.width) || (y0 > lcddev.height))return;
    lcd_fill(x0, y0, x0 + len - 1, y0, color);
}

/**
 * @brief       �����ɫ
 * @param       x, y          : ��ʼ����
 * @param       width, height : ���Ⱥ͸߶�
 * @param       color         : ��ɫ����
 * @retval      ��
 */
static void piclib_fill_color(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t *color)
{
    uint16_t i, j;
    if (lcdltdc.pwidth != 0 && lcddev.dir == 0)
    {
        uint32_t draw_addr = ltdc_get_draw_buffer();

        for (i = 0; i < height; i++)
        {
            for (j = 0; j < width; j++)
            {
                *(uint16_t *)(draw_addr + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - j - 1) + y + i)) = color[i * width + j];
            }
        }
    }
    else
    {
        lcd_color_fill(x, y, x + width - 1, y + height - 1, color);     /* ��� */
    }
}

/**
 * @brief       ��ͼ��ʼ��
 * @note        �ڻ�ͼ֮ǰ,�����ȵ��ô˺���, ָ����غ���
 * @param       ��
 * @retval      ��
 */
void piclib_init(void)
{
    pic_phy.read_point = lcd_read_point;    /* ���㺯��ʵ��,��BMP��Ҫ */
    pic_phy.draw_point = lcd_draw_point;    /* ���㺯��ʵ�� */
    pic_phy.fill = lcd_fill;                /* ��亯��ʵ��,��GIF��Ҫ */
    pic_phy.draw_hline = lcd_draw_hline;    /* ���ߺ���ʵ��,��GIF��Ҫ */
    pic_phy.fillcolor = piclib_fill_color;  /* ��ɫ��亯��ʵ��,��TJPGD��Ҫ */

    picinfo.lcdwidth = lcddev.width;        /* �õ�LCD�Ŀ������� */
    picinfo.lcdheight = lcddev.height;      /* �õ�LCD�ĸ߶����� */

    picinfo.ImgWidth = 0;                   /* ��ʼ������Ϊ0 */
    picinfo.ImgHeight = 0;                  /* ��ʼ���߶�Ϊ0 */
    picinfo.Div_Fac = 0;                    /* ��ʼ������ϵ��Ϊ0 */
    picinfo.S_Height = 0;                   /* ��ʼ���趨�ĸ߶�Ϊ0 */
    picinfo.S_Width = 0;                    /* ��ʼ���趨�Ŀ���Ϊ0 */
    picinfo.S_XOFF = 0;                     /* ��ʼ��x���ƫ����Ϊ0 */
    picinfo.S_YOFF = 0;                     /* ��ʼ��y���ƫ����Ϊ0 */
    picinfo.staticx = 0;                    /* ��ʼ����ǰ��ʾ����x����Ϊ0 */
    picinfo.staticy = 0;                    /* ��ʼ����ǰ��ʾ����y����Ϊ0 */
}

/**
 * @brief       ����ALPHA BLENDING�㷨
 * @param       src           : ��ɫ��
 * @param       dst           : Ŀ����ɫ
 * @param       alpha         : ͸���̶�(0~32)
 * @retval      ��Ϻ����ɫ
 */
uint16_t piclib_alpha_blend(uint16_t src, uint16_t dst, uint8_t alpha)
{
    uint32_t src2;
    uint32_t dst2;
    /* Convert to 32bit |-----GGGGGG-----RRRRR------BBBBB| */
    src2 = ((src << 16) | src) & 0x07E0F81F;
    dst2 = ((dst << 16) | dst) & 0x07E0F81F;
    /* Perform blending R:G:B with alpha in range 0..32
     * Note that the reason that alpha may not exceed 32 is that there are only
     * 5bits of space between each R:G:B value, any higher value will overflow
     * into the next component and deliver ugly result. 
     */
    dst2 = ((((dst2 - src2) * alpha) >> 5) + src2) & 0x07E0F81F;
    return (dst2 >> 16) | dst2;
}

/**
 * @brief       ��ʼ�����ܻ���
 * @param       ��
 * @retval      ��
 */
void piclib_ai_draw_init(void)
{
    float temp, temp1;
    temp = (float)picinfo.S_Width / picinfo.ImgWidth;
    temp1 = (float)picinfo.S_Height / picinfo.ImgHeight;

    if (temp < temp1)temp1 = temp;  /* ȡ��С���Ǹ� */

    if (temp1 > 1)temp1 = 1;

    /* ʹͼƬ��������������м� */
    picinfo.S_XOFF += (picinfo.S_Width - temp1 * picinfo.ImgWidth) / 2;
    picinfo.S_YOFF += (picinfo.S_Height - temp1 * picinfo.ImgHeight) / 2;
    temp1 *= 8192;  /* ����8192�� */
    picinfo.Div_Fac = temp1;
    picinfo.staticx = 0xffff;
    picinfo.staticy = 0xffff;   /* �ŵ�һ�������ܵ�ֵ���� */
}

/**
 * @brief       �ж���������Ƿ������ʾ
 * @param       x, y          : ����ԭʼ����
 * @param       chg           : ���ܱ���
 * @param       ��
 * @retval      �������
 *   @arg       0   , ����Ҫ��ʾ
 *   @arg       1   , ��Ҫ��ʾ
 */
__inline uint8_t piclib_is_element_ok(uint16_t x, uint16_t y, uint8_t chg)
{
    if (x != picinfo.staticx || y != picinfo.staticy)
    {
        if (chg == 1)
        {
            picinfo.staticx = x;
            picinfo.staticy = y;
        }

        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief       ���ܻ�ͼ
 * @note        ͼƬ����x,y��width, height�޶�����������ʾ.
 *
 * @param       filename      : ����·�����ļ���(.bmp/.jpg/.jpeg/.gif��)
 * @param       x, y          : ��ʼ����
 * @param       width, height : ��ʾ����
 * @param       fast          : ʹ�ܿ��ٽ���
 *   @arg                       0, ��ʹ��
 *   @arg                       1, ʹ��
 * @note                        ͼƬ�ߴ�С�ڵ���Һ���ֱ���,��֧�ֿ��ٽ���
 * @retval      ��
 */
uint8_t piclib_ai_load_picfile(char *filename, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t fast)
{
    uint8_t res;                                                /* ����ֵ */
    uint8_t temp;

    if ((x + width) > picinfo.lcdwidth)return PIC_WINDOW_ERR;   /* x���곬��Χ�� */

    if ((y + height) > picinfo.lcdheight)return PIC_WINDOW_ERR; /* y���곬��Χ�� */

    /* �õ���ʾ�����С */
    if (width == 0 || height == 0)return PIC_WINDOW_ERR;        /* �����趨���� */

    picinfo.S_Height = height;
    picinfo.S_Width = width;

    /* ��ʾ������Ч */
    if (picinfo.S_Height == 0 || picinfo.S_Width == 0)
    {
        picinfo.S_Height = lcddev.height;
        picinfo.S_Width = lcddev.width;
        return FALSE;
    }

    if (pic_phy.fillcolor == NULL)fast = 0;                     /* ��ɫ��亯��δʵ��,���ܿ�����ʾ */

    /* ��ʾ�Ŀ�ʼ����� */
    picinfo.S_YOFF = y;
    picinfo.S_XOFF = x;

    /* ˫���ڿɹ����£�����ʹ�õ�ǰδ��ʾ�ķ���� */
    ltdc_use_inactive_buffer();

    /* �ļ������� */
    temp = exfuns_file_type(filename);                          /* �õ��ļ������� */

    switch (temp)
    {
        case T_BMP:
            res = stdbmp_decode(filename);                      /* ����bmp */
            break;

        case T_JPG:
        case T_JPEG:
            if (fast)   /* ������ҪӲ������ */
            {
                res = jpg_get_size(filename, &picinfo.ImgWidth, &picinfo.ImgHeight);

                if (res == 0)
                {
                    if (picinfo.ImgWidth <= lcddev.width && picinfo.ImgHeight <= lcddev.height &&       /* ����ֱ���С�ڵ�����Ļ�ֱ��� */
                        picinfo.ImgWidth <= picinfo.S_Width && picinfo.ImgHeight <= picinfo.S_Height && /* ����ͼƬ����Ϊ16�������� */
                        (picinfo.ImgWidth % 16) == 0)                                                   /* �����Ӳ������ */
                    {
                        res = hjpgd_decode(filename);       /* ����Ӳ����JPG/JPEG */
                    }
                    else
                    {
                        res = jpg_decode(filename, fast);   /* ������������JPG/JPEG */
                    }
                }
            }
            else
            {
                res = jpg_decode(filename, fast);           /* ͳһ������������JPG/JPEG */
            }
            break;

        case T_GIF:
            res = gif_decode(filename, x, y, width, height);    /* ����gif */
            break;

        default:
            res = PIC_FORMAT_ERR;                               /* ��ͼƬ��ʽ!!! */
            break;
    }

    if (res == 0 && temp != T_GIF)
    {
        ltdc_present_draw_buffer();
    }

    return res;
}

/**
 * @brief       ��̬�����ڴ�
 * @param       size          : Ҫ������ڴ��С(�ֽ�)
 * @retval      ���䵽���ڴ��׵�ַ
 */
void *piclib_mem_malloc (uint32_t size)
{
    return (void *)mymalloc(SRAMIN, size);
}

/**
 * @brief       �ͷ��ڴ�
 * @param       paddr         : �ڴ��׵�ַ
 * @retval      ��
 */
void piclib_mem_free (void *paddr)
{
    myfree(SRAMIN, paddr);
}

