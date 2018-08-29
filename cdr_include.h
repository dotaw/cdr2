#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/reboot.h>
#include <sys/statfs.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/socket.h>
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/raw.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/watchdog.h>
#include "mbedtls/mbedtls/aes.h"

/* ---------------------------------------- 宏定义 ---------------------------------------- */

#define CDR_DEBUG_INVALID    0         /* 是否调试无效，用于日志的控制打印，软件最后发布时要定义成1 */

#define CDR_FAIL_TRY_TIMES               3   /* 失败最大尝试次数 */
#define CDR_EVENT_RECOVER_THRESHOLD      3   /* 恢复门限：连续无故障次数 */ 
#define CDR_WATCH_DOG_THRESHOLD          60  /* 喂狗门限，超过门限不喂狗，系统复位，单位s */ 

#define CDR_FILE_DIR_MAX_SIZE_BYTE       1000000   /* 1M */

#define CDR_DIR_BF_DIAG_MAX_NUM                 15      /* 备份诊断日志的最大数量 */

#define CDR_CAN_DATA_TIME_CALIBRATION_PF        0x6f   /* 时间校准PF标识 */

#define CDR_AES_KEY "1qaz2wsx3edc9027"


#define CDR_OK      0  /* 正常返回值 */
#define CDR_ERROR   1  /* 错误返回值 */

#define CDR_STORAGE_WARNING_THRESHOLD    20    /* 硬盘空间剩余20%提示 */
#define CDR_STORAGE_ALARM_THRESHOLD      10    /* 硬盘空间剩余10%告警，部分数据覆盖存储 */
#define CDR_STORAGE_NULL_THRESHOLD       2     /* 硬盘空间剩余2%告警，所有的数据都要覆盖存储 */


#define CDR_ID_TO_PRIORITY(id)     (((id) >> 26) & 0x7)    /* 根据CAN ID解析PRIORITY */
#define CDR_ID_TO_RSV(id)          (((id) >> 24) & 0x3)    /* 根据CAN ID解析RSV */
#define CDR_ID_TO_PF(id)           (((id) >> 16) & 0xff)   /* 根据CAN ID解析PF */
#define CDR_ID_TO_PS_DA(id)        (((id) >> 8) & 0xff)    /* 根据CAN ID解析PS/DA */
#define CDR_ID_TO_SA(id)           ((id) & 0xff)           /* 根据CAN ID解析SA */

#define CDR_FILE_DIR_RECORDER          "/opt/myapp/cdr_recorder/"               /* 记录仪程序目录 */
#define CDR_FILE_DIR_DIAGLOG           "/opt/myapp/cdr_recorder/diag/"          /* 诊断日志打印目录，用于问题的定位 */
#define CDR_FILE_DIR_DIAGLOG_BF        "/opt/myapp/cdr_recorder/diag/bf/"       /* 诊断日志的缓存目录，数量有最大门限CDR_DIR_BF_DIAG_MAX_NUM */
#define CDR_FILE_DIR_DISK1             "/opt/myapp/disk1/"                      /* 硬盘1对应的目录 */
//#define CDR_FILE_DIR_DISK2             "/opt/myapp/disk2/"                      /* 硬盘2对应的目录 */
#define CDR_FILE_DIR_DISK1_CANDATA     "/opt/myapp/disk1/data/"                 /* 硬盘1文件存储目录 */
//#define CDR_FILE_DIR_DISK2_CANDATA     "/opt/myapp/disk2/data/"                 /* 硬盘2文件存储目录 */
#define CDR_FILE_DIR_DISK1_CANDATA_BF  "/opt/myapp/disk1/data/bf/"              /* 硬盘1文件历史存储目录 */
//#define CDR_FILE_DIR_DISK2_CANDATA_BF  "/opt/myapp/disk2/data/bf/"              /* 硬盘2文件历史存储目录 */

#define CDR_FILE_DISK1_CANDATA         "/opt/myapp/disk1/data/can.data"       /* 硬盘1的记录文件 */
//#define CDR_FILE_DISK2_CANDATA         "/opt/myapp/disk2/data/can.data"       /* 硬盘2的记录文件 */
#define CDR_FILE_DISK1_DIR_INFO         "/opt/myapp/disk1/data/dir.info"      /* 硬盘1的历史记录文件的目录信息 */
//#define CDR_FILE_DISK2_DIR_INFO         "/opt/myapp/disk2/data/dir.info"      /* 硬盘2的历史记录文件的目录信息 */

#define CDR_FILE_DIAGLOG_RECORD       "/opt/myapp/cdr_recorder/diag/diag.log"        /* 诊断日志记录文件 */
#define CDR_FILE_DIAGLOG_BF_RECORD    "/opt/myapp/cdr_recorder/diag/bf/diag%s.log"   /* 保存历史诊断日志文件 */
#define CDR_FILE_INIT_SELFTEST        "/opt/myapp/cdr_recorder/self_test.txt"        /* 用于处理启动自检的文件 */
#define CDR_FILE_TEST_FAULT_SIM       "/opt/myapp/test/fault_data_sim"               /* 用于处理模拟数据的文件 */

#ifndef AF_CAN
#define AF_CAN 29
#endif
#ifndef PF_CAN
#define PF_CAN AF_CAN
#endif

#define WDT "/dev/watchdog"    /* 看门狗程序位置 */

/* ---------------------------------------- 结构体或枚举定义 ---------------------------------------- */
typedef enum cdr_log {
    CDR_LOG_INFO = 1,       /* 关键事件诊断日志记录 */ 
    CDR_LOG_ERROR,          /* 错误事件诊断日志记录 */ 
    CDR_LOG_DEBUG,          /* 调试诊断日志记录，CDR_DEBUG_INVALID置1日志将不会打印 */  
} cdr_log_t;

typedef enum cdr_time_type {
    CDR_TIME_MS_STANDARD = 1,        /* 获取时间精度到ms,格式2018-04-22 22:23:24.999 */
    CDR_TIME_MS_DIGITAL,             /* 获取时间精度到ms,格式20180422222324999 */
    CDR_TIME_S_STANDARD,             /* 获取时间精度到s,格式2018-04-22 22:23:24 */
    CDR_TIME_S_DIGITAL,              /* 获取时间精度到s,格式20180422222324 */
    CDR_TIME_D_DIGITAL,              /* 获取时间精度到day,格式20180422 */
} cdr_time_type_t;

typedef enum cdr_led_state {
    CDR_LED_RED_CONTINUOUS      = 0x1,        /* 红灯常亮 */
    CDR_LED_RED_FLASH           = 0x2,        /* 红灯闪烁 */
    CDR_LED_YELLOW_CONTINUOUS   = 0x4,        /* 黄灯常亮 */
    CDR_LED_YELLOW_FLASH        = 0x8,        /* 黄灯闪烁 */
    CDR_LED_GREEN_FLASH         = 0x10,       /* 绿灯闪烁 */
    CDR_LED_GREEN_CONTINUOUS    = 0x20,       /* 绿灯常亮 */
    CDR_LED_BLUE_FLASH          = 0x40,       /* 蓝灯闪烁 */
    CDR_LED_BLUE_CONTINUOUS     = 0x80,       /* 蓝灯常亮 */        
} cdr_led_state_t;

typedef enum cdr_event {  
    CDR_EVENT_FILE_RECORD_FAULT = 1,    /* 1、文件记录故障事件 */
    CDR_EVENT_STORAGE_WARNING,          /* 2、存储空间不足提示事件 */
    CDR_EVENT_STORAGE_ALARM,            /* 3、存储空间不足告警事件 */
    CDR_EVENT_STORAGE_NULL,             /* 4、存储空间不足事件 */
    CDR_EVENT_DATA_RECORDING,           /* 5、数据正常存储事件 */
    CDR_EVENT_USB_PULL_IN,              /* 6、usb设备插入 */
    CDR_EVENT_DATA_TO_USB,              /* 7、cpy数据到usb设备 */
    CDR_EVENT_USB_STORAGE_ALARM,        /* 8、cpy数据到usb设备空间不足 */
    CDR_EVENT_DATA_TO_USB_END,          /* 9、cpy数据到usb设备已经完成 */
    CDR_EVENT_MAX,
} cdr_event_t;

typedef struct cdr_can_frame {
    int id;             /* can帧id */
    long long data;     /* can数据 */
    int len;            /* can数据长度 */
} cdr_can_frame_t;

typedef struct cdr_led_set_state {
    int state;                                             /* LED的状态 */
    char set_red[4];                                       /* 红灯是否输出 */
    char set_green[4];                                     /* 绿灯是否输出 */
    char set_blue[4];                                      /* 蓝灯是否输出 */
    int is_flash;                                          /* 灯是否闪烁 */
} cdr_led_set_state_t;

/* ---------------------------------------- 全局变量定义 ---------------------------------------- */
int g_socket_fd;                                  /* 创建的套接字 */
int g_dev_time_calibration_busy;                  /* 是否正在进行时间校准 */
int g_diaglog_record_busy;                        /* 诊断日志记录是否正在被操作，防止多线程重入 */
int g_system_event_occur[CDR_EVENT_MAX];          /* 系统事件是否发生，事件见列表cdr_event_t */
int g_system_event_occur_his[CDR_EVENT_MAX];      /* 系统事件是否发生过，事件见列表cdr_event_t */
int g_system_event_no_occur_num[CDR_EVENT_MAX];   /* 系统事件连续没有发生的次数，事件见列表cdr_event_t */
int g_wdt_fd;                                     /* 看门狗 */
int g_pthread_record_data_active;                 /* 数据记录线程是否正常运行 */  
int g_time_calibration_invalid;                   /* 时间校准是否生效，开机运行仅第一次时间校准生效 */
cdr_led_set_state_t g_led_state;                  /* LED状态灯当前状态 */
 

/* ---------------------------------------- 函数声明 ---------------------------------------- */
/* cdr_main */
void cdr_global_init();
int cdr_main_can_if_init();
int cdr_main_record_self_test();
int cdr_creat_pthread_record_can_data(pthread_t *pthread_record_data);
int cdr_creat_pthread_fmea_test(pthread_t *pthread_fmea);
int cdr_main_init_watch_dog();

/* cdr_public */
void cdr_get_system_time(int type, char *time_info);
unsigned long get_file_size(const char *file);
int cdr_cpy_file(char *file_in, char *file_out);
void cdr_diag_log(int type, const char *format, ...);
void cdr_set_led_state(int type);
void cdr_system_reboot();
void cdr_led_control();
void cdr_usb_detect();

/* file_record */
void cdr_record_can_data();

/* cdr_fmea */
void cdr_fmea_test();