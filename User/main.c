/**
 ****************************************************************************************************
 * @file        main.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-09-06
 * @brief       硬件JPEG解码 实验
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 阿波罗 H743开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/SDRAM/sdram.h"
#include "./USMART/usmart.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/KEY/key.h"
#include "./BSP/MPU/mpu.h"
#include "./TEXT/text.h"
#include "./BSP/LCD/ltdc.h"
#include "./MALLOC/malloc.h"
#include "./PICTURE/piclib.h"
#include "./FATFS/exfuns/exfuns.h"
#include "./BSP/SDMMC/sdmmc_sdcard.h"
#include "string.h"
#include "math.h"
#include "./BSP/JPEGCODEC/jpegcodec.h"


//#define EMO_DEBUG  1

/* 表情编号 -> GIF 文件名 映射表 */
typedef struct
{
    uint8_t code;        /* 协议中的 数据 字节，比如 0x01、0x05 ... */
    const char *name;    /* 对应的 GIF 文件名（放在 1:/PICTURE 目录下） */
} GifEntry;

/* 按照你 PC 端的 gif_map 一一对应填写 */
static const GifEntry g_gif_map[] =
{
    {0x01, "happy.gif"},          // "01"
    {0x05, "anger.gif"},          // "05"
    {0x0D, "arrogance.gif"},      // "0D"
    {0x02, "laughing.gif"},       // "02"
    {0x03, "curiosity.gif"},      // "03"
    {0x10, "encourage.gif"},      // "10"
    {0x08, "error.gif"},          // "08"
    {0x0A, "fear.gif"},           // "0A"
    {0x13, "focus.gif"},          // "13"
    {0x14, "helplessness.gif"},   // "14"
    {0x0E, "musical.gif"},        // "0E"
    {0x0C, "naughty.gif"},        // "0C"
    {0x00, "normal.gif"},         // "00"
    {0x06, "raining.gif"},        // "06"
    {0x04, "sadness.gif"},        // "04"
    {0x12, "sleepgif.gif"},       // "12"
    {0x11, "springfestival.gif"}, // "11"
    {0x0F, "sun.gif"},            // "0F"
    {0x09, "surprise.gif"},       // "09"
    {0x07, "touching.gif"},       // "07"
    {0x0B, "wait.gif"},           // "0B"
};

/* 映射表长度 */
#define GIF_MAP_NUM   (sizeof(g_gif_map) / sizeof(g_gif_map[0]))

/* 根据 code 查文件名，找不到返回 NULL */
static const char *gif_get_name_by_code(uint8_t code)
{
    for (uint32_t i = 0; i < GIF_MAP_NUM; i++)
    {
        if (g_gif_map[i].code == code)
        {
            return g_gif_map[i].name;
        }
    }
    return NULL;
}

/* 串口协议解析结果：收到一帧 AA 55 00 XX FB 就把 XX 放到这里 */
volatile uint8_t g_gif_cmd = 0;
volatile uint8_t g_gif_cmd_flag = 0;   /* =1 表示有新命令 */



/*----------------- GIF 拷贝用全局缓冲，避免占用栈 -----------------*/
#define GIF_COPY_BUF_SIZE   4096

static uint8_t g_gif_copy_buf[GIF_COPY_BUF_SIZE];   // 放在 BSS，全局变量


static uint8_t is_gif_file(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    if (strcasecmp(dot, ".GIF") == 0) return 1;
    return 0;
}

static uint16_t get_gif_count(const char *path)
{
    DIR dir;
    FILINFO info;
    uint16_t cnt = 0;

    if (f_opendir(&dir, path) != FR_OK)
    {
        return 0;
    }

    while (1)
    {
        if (f_readdir(&dir, &info) != FR_OK || info.fname[0] == 0)
            break;

        if (strstr(info.fname, ".GIF") || strstr(info.fname, ".gif"))
        {
            cnt++;
        }
    }

    f_closedir(&dir);
    return cnt;
}

/* 判断 dst_path 是否需要从 src_info 对应的文件更新
 * 返回 1 = 需要拷贝/覆盖
 *       0 = 可以跳过（文件存在且大小&时间都一样）
 */
static uint8_t need_update_file(const char *dst_path, const FILINFO *src_info)
{
    FILINFO dst_info;
    FRESULT res;

    /* 查一下 1:/PICTURE 里是否已经有这个文件 */
    res = f_stat(dst_path, &dst_info);
    if (res != FR_OK)
    {
        /* 查不到，说明不存在 -> 必须拷 */
        return 1;
    }

    /* 1) 先看文件大小 */
    if (dst_info.fsize != src_info->fsize)
    {
        return 1;   /* 大小不一样，说明内容肯定变了 */
    }

    /* 大小相同、日期相同，就认为没变，跳过 */
    return 0;
}


/* 从 SD 卡 0:/PICTURE 同步所有 GIF 到 W25Q256 的 1:/PICTURE
 * 只更新“新增/有变化”的文件，大小 & 时间完全一样的文件直接跳过。
 */
void copy_gif_to_flash(void)
{
    DIR dir;
    FILINFO info;
    FRESULT res;
    FIL src, dst;
    UINT br, bw;

    char path_sd[64];
    char path_flash[64];

    uint16_t total;          // GIF 总数
    uint16_t index = 0;      // 当前是第几张

    uint32_t total_size;     // 当前这张的总字节数
    uint32_t copied;         // 已拷字节
    uint32_t last_report;    // 上次打印进度的位置

    /* 1. 统计 0:/PICTURE 中 GIF 个数 */
    total = get_gif_count("0:/PICTURE");
    if (total == 0)
    {
#ifdef EMO_DEBUG
        printf("0:/PICTURE 中没有 GIF 文件\r\n");
#endif
        return;
    }

#ifdef EMO_DEBUG
    printf("准备同步 %d 个 GIF 到 1:/PICTURE（只更新有变化的）\r\n", total);
#endif

    /* 2. 确保 1:/PICTURE 目录存在（没有就建，有就什么也不做） */
    f_mkdir("1:/PICTURE");

    /* 3. 打开 0:/PICTURE 目录，开始逐个处理 */
    res = f_opendir(&dir, "0:/PICTURE");
    if (res != FR_OK)
    {
#ifdef EMO_DEBUG
        printf("copy_gif_to_flash: 打开 0:/PICTURE 失败, res=%d\r\n", res);
#endif
        return;
    }

    while (1)
    {
        res = f_readdir(&dir, &info);
        if (res != FR_OK || info.fname[0] == 0)
        {
            /* 出错或读到末尾 */
            break;
        }

        /* 跳过子目录 */
        if (info.fattrib & AM_DIR)
            continue;

        /* 只处理 .GIF/.gif */
        if (!(strstr(info.fname, ".GIF") || strstr(info.fname, ".gif")))
            continue;

        index++;

        snprintf(path_sd,    sizeof(path_sd),    "0:/PICTURE/%s", info.fname);
        snprintf(path_flash, sizeof(path_flash), "1:/PICTURE/%s", info.fname);

        /* 先判断目标文件是否需要更新（不存在 / 大小变了 / 时间变了） */
        if (!need_update_file(path_flash, &info))
        {
#ifdef EMO_DEBUG
            printf("[%d/%d] %s 已是最新，跳过\r\n",
                   index, total, path_flash);
#endif
            continue;   /* 这个文件不拷了，处理下一个 */
        }

        total_size  = info.fsize;
        copied      = 0;
        last_report = 0;

#ifdef EMO_DEBUG
        printf("\r\n[%d/%d] %s -> %s  大小:%lu KB （需更新）\r\n",
               index, total,
               path_sd, path_flash,
               (unsigned long)(info.fsize / 1024));
#endif

        /* ---- 打开源文件 (SD 卡) ---- */
        res = f_open(&src, path_sd, FA_READ);
        if (res != FR_OK)
        {
#ifdef EMO_DEBUG
            printf("  打开源文件失败, 跳过此文件, res=%d\r\n", res);
#endif
            continue;
        }

        /* ---- 打开目标文件 (1:/，NORFLASH) ----
         * 这里直接用 CREATE_ALWAYS：每次都完整覆盖写，
         * 保证内容与 SD 卡保持一致。
         */
        res = f_open(&dst, path_flash, FA_CREATE_ALWAYS | FA_WRITE);
        if (res != FR_OK)
        {
#ifdef EMO_DEBUG
            printf("  打开目标文件失败, 跳过此文件, res=%d\r\n", res);
#endif
            f_close(&src);
            continue;
        }

        /* ---- 真正的拷贝循环 ---- */
        while (1)
        {
            res = f_read(&src, g_gif_copy_buf, GIF_COPY_BUF_SIZE, &br);
            if (res != FR_OK)
            {
#ifdef EMO_DEBUG
                printf("  f_read 出错, res=%d\r\n", res);
#endif
                break;
            }

            if (br == 0)
            {
#ifdef EMO_DEBUG
                printf("  读到文件末尾\r\n");
#endif
                break;
            }

            res = f_write(&dst, g_gif_copy_buf, br, &bw);
            if (res != FR_OK || bw < br)
            {
#ifdef EMO_DEBUG
                printf("  f_write 出错, res=%d, bw=%u\r\n", res, bw);
#endif
                break;
            }

            copied += br;

            /* 每 32KB 或最后一次打印一次进度 */
            if (copied - last_report >= 32 * 1024 || copied == total_size)
            {
#ifdef EMO_DEBUG
                printf("  已拷 %lu / %lu KB\r\n",
                       (unsigned long)(copied / 1024),
                       (unsigned long)(total_size / 1024));
#endif
                last_report = copied;
            }
        }

        f_close(&src);
        f_close(&dst);
    }

    f_closedir(&dir);
#ifdef EMO_DEBUG
    printf("GIF 同步完成\r\n");
#endif
}


void flash_picture_first_init(void)
{
#ifdef EMO_DEBUG
    printf("执行 flash_picture_first_init：同步 0:/PICTURE -> 1:/PICTURE（增量更新）\r\n");
#endif

    /* 不再判断是不是“第一次使用”，
       每次上电都跑一遍增量同步逻辑即可。
       第一次时 1:/PICTURE 不存在，copy_gif_to_flash 里会 f_mkdir 并把所有文件拷进去；
       之后再上电，只更新有变化的文件。 */
    copy_gif_to_flash();
}



/* 删除 1:/PICTURE 目录下的所有文件，并把目录本身也删掉 */
void clear_flash_pictures(void)
{
    DIR dir;
    FILINFO info;
    char path[64];

    /* 先尝试打开目录 */
    if (f_opendir(&dir, "1:/PICTURE") != FR_OK)
        return;

    while (1)
    {
        if (f_readdir(&dir, &info) != FR_OK || info.fname[0] == 0)
            break;

        if (info.fname[0] == '.')        /* 跳过 . 和 .. */
            continue;

        sprintf(path, "1:/PICTURE/%s", info.fname);
        f_unlink(path);                  /* 删除文件 */
    }
    f_closedir(&dir);

    /* 把空目录本身删掉 */
    f_unlink("1:/PICTURE");
}

#ifdef EMO_DEBUG
/* 调试用：列出 SD 卡 0:/ 根目录下的文件和目录 */
void debug_list_sd_root(void)
{
    DIR dir;
    FILINFO info;
    FRESULT res;

    printf("列出 0:/ 根目录内容:\r\n");

    res = f_opendir(&dir, "0:/");
    if (res != FR_OK)
    {
        printf("  打开 0:/ 失败, res=%d\r\n", res);
        return;
    }

    while (1)
    {
        res = f_readdir(&dir, &info);
        if (res != FR_OK || info.fname[0] == 0)
            break;

        if (info.fattrib & AM_DIR)
            printf("  <DIR> %s\r\n", info.fname);
        else
            printf("  %s\r\n", info.fname);
    }

    f_closedir(&dir);
    printf("0:/ 列表结束\r\n");
}

#endif

/**
 * @brief       得到path路径下,目标文件的总个数
 * @param       path : 路径
 * @retval      总有效文件数
 */
uint16_t pic_get_tnum(char *path)
{
    uint8_t res;
    uint16_t rval = 0;
    DIR tdir;                                                 /* 临时目录 */
    FILINFO *tfileinfo;                                       /* 临时文件信息 */
    tfileinfo = (FILINFO *)mymalloc(SRAMIN, sizeof(FILINFO)); /* 申请内存 */
    res = f_opendir(&tdir, (const TCHAR *)path);              /* 打开目录 */

    if (res == FR_OK && tfileinfo)
    {
        while (1)                                             /* 查询总的有效文件数 */
        {
            res = f_readdir(&tdir, tfileinfo);                /* 读取目录下的一个文件 */

            if (res != FR_OK || tfileinfo->fname[0] == 0)
            {
                break;                                        /* 错误了/到末尾了,退出 */
            }

            res = exfuns_file_type(tfileinfo -> fname);

            if ((res & 0XF0) == 0X50)                         /* 取高四位,看看是不是图片文件 */
            {
                rval++;                                       /* 有效文件数增加1 */
            }
        }
    }

    myfree(SRAMIN, tfileinfo);                                /* 释放内存 */
    return rval;
}

/* 根据 code 显示表情 GIF */
static void show_emotion_by_code(uint8_t code)
{
    const char *name = gif_get_name_by_code(code);
    if (name == NULL)
    {
#ifdef EMO_DEBUG
        printf("未知表情 code=0x%02X（未在 g_gif_map 中配置）\r\n", code);
#endif        
				return;
    }

    char fullpath[64];
    sprintf(fullpath, "1:/PICTURE/%s", name);
#ifdef EMO_DEBUG
    printf("显示表情 code=0x%02X, 文件=%s\r\n", code, fullpath);
#endif   
//    lcd_clear(BLACK);			//目前覆盖全屏的图片，不进行清屏效果更好
    piclib_ai_load_picfile(fullpath, 0, 0, lcddev.width, lcddev.height, 1);//
}



int main(void)
{
    uint8_t res;
    DIR picdir;                                  /* 图片目录 */
    FILINFO *picfileinfo;                        /* 文件信息 */
    char *pname;                                 /* 带路径的文件名 */
    uint16_t totpicnum;                          /* 图片文件总数 */
    uint16_t curindex;                           /* 图片当前索引 */
    uint8_t key;                                 /* 键值 */
    uint8_t pause = 0;                           /* 暂停标记 */
    uint8_t t;
    uint16_t temp;
    uint32_t *picoffsettbl;                      /* 图片文件offset索引表 */

    sys_cache_enable();                          /* 打开L1-Cache */
    HAL_Init();                                  /* 初始化HAL库 */
    sys_stm32_clock_init(160, 5, 2, 4);          /* 设置时钟, 400Mhz */
    delay_init(400);                             /* 延时初始化 */
    usart_init(115200);                          /* 串口初始化 */
    usmart_init(200);                            /* 初始化USMART */
    mpu_memory_protection();                     /* 保护相关存储区域 */
    led_init();                                  /* 初始化LED */
    key_init();                                  /* 初始化KEY */
    sdram_init();                                /* 初始化SDRAM */
    lcd_init();                                  /* 初始化LCD */
    my_mem_init(SRAMIN);                         /* 初始化内部内存池(AXI) */
    my_mem_init(SRAMEX);                         /* 初始化外部内存池(SDRAM) */
    my_mem_init(SRAM12);                         /* 初始化SRAM12内存池(SRAM1+SRAM2) */
    my_mem_init(SRAM4);                          /* 初始化SRAM4内存池(SRAM4) */
    my_mem_init(SRAMDTCM);                       /* 初始化DTCM内存池(DTCM) */
    my_mem_init(SRAMITCM);                       /* 初始化ITCM内存池(ITCM) */
    
    exfuns_init();                               /* 为fatfs相关变量申请内存 */
    f_mount(fs[0], "0:", 1);                     /* 挂载SD卡 */
    f_mount(fs[1], "1:", 1);                     /* 挂载SPI FLASH */
    f_mount(fs[2], "2:", 1);                     /* 挂载NAND FLASH */
		
		
//		/* ★★★ 下载图片 ★★★ */
//		clear_flash_pictures();
//    flash_picture_first_init();                  /* 如果是第一次, 就把0:/PICTURE 拷到 1:/PICTURE */

    while (fonts_init())                         /* 检查字库 */
    {
        lcd_show_string(30, 50, 200, 16, 16, "Font Error!", RED);
        delay_ms(200);
        lcd_fill(30, 50, 240, 66, WHITE);        /* 清除显示 */
        delay_ms(200);
    }

    text_show_string(30, 50, 200, 16, "正点原子STM32开发板",16,0, RED);
    text_show_string(30, 70, 200, 16, "硬件JPEG解码 实验", 16, 0, RED);
    text_show_string(30, 90, 200, 16, "正点原子@ALIENTEK", 16, 0, RED);
    text_show_string(30, 110, 200, 16, "KEY0:NEXT KEY1:PREV", 16, 0, RED);
    text_show_string(30, 130, 200, 16, "KEY_UP:PAUSE:", 16, 0, RED);

    while (f_opendir(&picdir, "1:/PICTURE"))                                /* 打开图片文件夹 */
    {
        text_show_string(30, 150, 240, 16, "PICTURE文件夹错误!", 16, 0, RED);
        delay_ms(200);
        lcd_fill(30, 150, 240, 186, WHITE);                                 /* 清除显示 */
        delay_ms(200);
    }

    totpicnum = pic_get_tnum("1:/PICTURE");                                 /* 得到总有效文件数 */

    
    while (totpicnum == NULL)                                               /* 图片文件为0 */
    {
        text_show_string(30, 150, 240, 16, "没有图片文件!", 16, 0, RED);
        delay_ms(200);
        lcd_fill(30, 150, 240, 186, WHITE);                                 /* 清除显示 */
        delay_ms(200);
    }
    
    picfileinfo = (FILINFO *)mymalloc(SRAMIN, sizeof(FILINFO));             /* 申请内存 */
    pname = mymalloc(SRAMIN, FF_MAX_LFN * 2 + 1);                           /* 为带路径的文件名分配内存 */
    picoffsettbl = mymalloc(SRAMIN, 4 * totpicnum);                         /* 申请4*totpicnum个字节的内存,用于存放图片索引 */

    while (!picfileinfo || !pname || !picoffsettbl)                         /* 内存分配出错 */
    {
        text_show_string(30, 150, 240, 16, "内存分配失败!", 16, 0, RED);
        delay_ms(200);
        lcd_fill(30, 150, 240, 186, WHITE);                                 /* 清除显示 */
        delay_ms(200);
    }

    /* 记录索引 */
    res = f_opendir(&picdir, "1:/PICTURE");                                 /* 打开目录 */

    if (res == FR_OK)
    {
        curindex = 0;                                                       /* 当前索引为0 */
        
        while (1)                                                           /* 全部查询一遍 */
        {
            temp = picdir.dptr;                                             /* 记录当前dptr偏移 */
            res = f_readdir(&picdir, picfileinfo);                          /* 读取目录下的一个文件 */

            if (res != FR_OK || picfileinfo -> fname[0] == 0)
            {
                break;                                                      /* 错误了/到末尾了,退出 */
            }

            res = exfuns_file_type(picfileinfo -> fname);

            if ((res & 0XF0) == 0X50)                                       /* 取高四位,看看是不是图片文件 */
            {
                picoffsettbl[curindex] = temp;                              /* 记录索引 */
                curindex++;
            }
        }
    }

    text_show_string(30, 150, 240, 16, "开始显示...", 16, 0, RED);
    delay_ms(1500);
		
		
		/* 初始化画图 */
    piclib_init();                                                           /* 初始化画图 */

    lcd_clear(BLACK);
		
	  /* --------- 串口表情循环播放 + 超时回退逻辑 --------- */

    const uint8_t DEFAULT_CODE = 0x09;           /* 默认表情编号，对应 normal.gif */
    uint8_t  play_code    = DEFAULT_CODE;        /* 当前要循环播放的表情 */
    uint32_t last_rx_tick = HAL_GetTick();       /* 上一次收到有效指令的时间戳(ms) */
    const uint32_t REVERT_MS = 5000;             /* 5s 超时回退 */

    while (1)
    {
        uint32_t now = HAL_GetTick();

        /* 1) 处理串口新指令：AA 55 00 XX FB */
        if (g_gif_cmd_flag)
        {
            uint8_t code = g_gif_cmd;
            g_gif_cmd_flag = 0;

            last_rx_tick = now;      /* 更新“最近一次指令时间” */
            play_code    = code;     /* 更新当前要播放的表情 */
#ifdef EMO_DEBUG
            printf("收到新表情指令 code=0x%02X\r\n", code);
#endif   
        }

        /* 2) 超时检测：超过 REVERT_MS 未收到新指令，回到默认表情 */
        if ((now - last_rx_tick) > REVERT_MS)
        {
            if (play_code != DEFAULT_CODE)
            {
#ifdef EMO_DEBUG
                printf("超时%lu ms未收到新指令，回到默认表情 0x%02X\r\n",
                       (unsigned long)REVERT_MS, DEFAULT_CODE);
#endif						
            }
            play_code    = DEFAULT_CODE;
            last_rx_tick = now;      /* 重置基准，避免多次进入 */
        }

        /* 3) 循环播放当前表情：每次播放完整一遍 GIF */
        show_emotion_by_code(play_code);
        /* 函数内部会从第一帧播到最后一帧，播完返回，然后下一轮 while 再播一遍，
           直到 play_code 被新的串口指令或超时逻辑改成其他表情为止。*/
				
				/* 小 delay，避免主循环占满 CPU；不再频繁闪 LED 了 */
//        delay_ms(10);
    }
}


