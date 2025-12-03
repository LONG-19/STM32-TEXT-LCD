/**
 ****************************************************************************************************
 * @file        malloc.h
 * @author      ����ԭ���Ŷ�(ALIENTEK)
 * @version     V1.0
 * @date        2022-09-06
 * @brief       �ڴ���� ����
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

#ifndef __MALLOC_H
#define __MALLOC_H

#include "./SYSTEM/sys/sys.h"

#ifndef NULL
#define NULL 0
#endif

/* ���������ڴ�� */
#define SRAMIN                  0                               /* AXI�ڴ��,AXI��512KB  */
#define SRAMEX                  1                               /* �ⲿ�ڴ��(SDRAM),SDRAM��32MB */
#define SRAM12                  2                               /* SRAM1/2/3�ڴ��,SRAM1+SRAM2,��256KB */
#define SRAM4                   3                               /* SRAM4�ڴ��,SRAM4��64KB */
#define SRAMDTCM                4                               /* DTCM�ڴ��,DTCM��128KB,�˲����ڴ��CPU��MDMA(ͨ��AHBS)���Է���!!!! */
#define SRAMITCM                5                               /* ITCM�ڴ��,DTCM��64 KB,�˲����ڴ��CPU��MDMA(ͨ��AHBS)���Է���!!!! */

#define SRAMBANK                6                               /* ����֧�ֵ�SRAM����. */

/* �����ڴ����������,������SDRAM��ʱ�򣬱���ʹ��uint32_t���ͣ�������Զ����uint16_t���Խ�ʡ�ڴ�ռ�� */
#define MT_TYPE     uint32_t

/* mem1�ڴ�����趨.mem1��H7�ڲ���AXI�ڴ�. */
#define MEM1_BLOCK_SIZE         64                              /* �ڴ���СΪ64�ֽ� */
#define MEM1_MAX_SIZE           448 * 1024                      /* �������ڴ� 448K,H7��AXI�ڴ��ܹ�512KB */
#define MEM1_ALLOC_TABLE_SIZE   MEM1_MAX_SIZE / MEM1_BLOCK_SIZE /* �ڴ����С */

/* mem2�ڴ�����趨.mem2���ⲿ��SDRAM�ڴ�
 * ��ע��: ���ڲ���LTDC˫������
 *  - SDRAM �� 32MB ��ʼ��ַ: 0xC0000000
 *  - ÿ��֡����(RGB565, 1280x800����): 1280*800*2 Byte = 0x1F4000 ��Լ 2MB��
 *  - ˫������Ҫ 2 ����ͬ�Ļ���: 0x1F4000 * 2 = 0x3E8000 ��Լ 4MB��
 *  - ���� mem2 ���ڴ� �� 32MB - 4MB = 28MB
 */
#define MEM2_BLOCK_SIZE         64                              /* �ڴ���СΪ64�ֽ� */
#define MEM2_MAX_SIZE           28672 * 1024                    /* �������ڴ�28672K,SDRAM 32MB,ǰ4MB����LTDC˫����,��ʣ28MB. */
#define MEM2_ALLOC_TABLE_SIZE   MEM2_MAX_SIZE / MEM2_BLOCK_SIZE /* �ڴ����С */

/* mem3�ڴ�����趨.mem3��H7�ڲ���SRAM1+SRAM2�ڴ� */
#define MEM3_BLOCK_SIZE         64                              /* �ڴ���СΪ64�ֽ� */
#define MEM3_MAX_SIZE           240 * 1024                      /* �������ڴ�240K,H7��SRAM1+SRAM2��256KB */
#define MEM3_ALLOC_TABLE_SIZE   MEM3_MAX_SIZE / MEM3_BLOCK_SIZE /* �ڴ����С */

/* mem4�ڴ�����趨.mem4��H7�ڲ���SRAM4�ڴ� */
#define MEM4_BLOCK_SIZE         64                              /* �ڴ���СΪ64�ֽ� */
#define MEM4_MAX_SIZE           60 * 1024                       /* �������ڴ�60K,H7��SRAM4��64KB */
#define MEM4_ALLOC_TABLE_SIZE   MEM4_MAX_SIZE / MEM4_BLOCK_SIZE /* �ڴ����С */

/* mem5�ڴ�����趨.mem5��H7�ڲ���DTCM�ڴ�,�˲����ڴ��CPU��MDMA���Է���!!!!!! */
#define MEM5_BLOCK_SIZE         64                              /* �ڴ���СΪ64�ֽ� */
#define MEM5_MAX_SIZE           120 * 1024                      /* �������ڴ�120K,H7��DTCM��128KB */
#define MEM5_ALLOC_TABLE_SIZE   MEM5_MAX_SIZE / MEM5_BLOCK_SIZE /* �ڴ����С */

/* mem6�ڴ�����趨.mem6��H7�ڲ���ITCM�ڴ�,�˲����ڴ��CPU��MDMA���Է���!!!!!! */
#define MEM6_BLOCK_SIZE         64                              /* �ڴ���СΪ64�ֽ� */
#define MEM6_MAX_SIZE           60 * 1024                       /* �������ڴ�60K,H7��ITCM��64KB */
#define MEM6_ALLOC_TABLE_SIZE   MEM6_MAX_SIZE / MEM6_BLOCK_SIZE /* �ڴ����С */

/* �ڴ���������� */
struct _m_mallco_dev
{
    void (*init)(uint8_t);              /* ��ʼ�� */
    uint16_t (*perused)(uint8_t);       /* �ڴ�ʹ���� */
    uint8_t *membase[SRAMBANK];         /* �ڴ�� ����SRAMBANK��������ڴ� */
    uint32_t *memmap[SRAMBANK];         /* �ڴ����״̬�� */
    uint8_t  memrdy[SRAMBANK];          /* �ڴ�����Ƿ���� */
};

extern struct _m_mallco_dev mallco_dev;                 /* ��mallco.c���涨�� */

/******************************************************************************************/

void my_mem_set(void *s, uint8_t c, uint32_t count);    /* �����ڴ� */
void my_mem_copy(void *des, void *src, uint32_t n);     /* �����ڴ� */
void my_mem_init(uint8_t memx);                         /* �ڴ������ʼ������(��/�ڲ�����) */
uint32_t my_mem_malloc(uint8_t memx, uint32_t size);    /* �ڴ����(�ڲ�����) */
uint8_t my_mem_free(uint8_t memx, uint32_t offset);     /* �ڴ��ͷ�(�ڲ�����) */
uint16_t my_mem_perused(uint8_t memx) ;                 /* ����ڴ�ʹ����(��/�ڲ�����)  */

/* �û����ú��� */
void myfree(uint8_t memx, void *ptr);                   /* �ڴ��ͷ�(�ⲿ����) */
void *mymalloc(uint8_t memx, uint32_t size);            /* �ڴ����(�ⲿ����) */
void *myrealloc(uint8_t memx, void *ptr, uint32_t size);/* ���·����ڴ�(�ⲿ����) */

#endif

