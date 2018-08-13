#include "cdr_include.h"

void main()
{
    pthread_t pthread_record_data;
    pthread_t pthread_fmea;
    pthread_t pthread_led;
    
    /* 1、全局初始化，初始化全局变量，如果程序需要的目录不存在，需要创建目录 */
    cdr_global_init();

    /* 2、点灯：红色常亮 */
    cdr_set_led_state(CDR_LED_RED_CONTINUOUS);
    
    /* 3、can接口初始化 */
    if (cdr_main_can_if_init() != CDR_OK)
    {
        cdr_diag_log(CDR_LOG_ERROR, "cdr_main_can_if_init error");
        cdr_system_reboot();
        return;
    }
    
    /* 4、数据记录自检 */
    if (cdr_main_record_self_test() != CDR_OK)
    {
        cdr_diag_log(CDR_LOG_ERROR, "cdr_main_record_self_test error");
        cdr_system_reboot();
        return;
    }  
    
    /* 5、数据记录线程 */
    if (cdr_creat_pthread_record_can_data(&pthread_record_data) != CDR_OK)
    {
        cdr_diag_log(CDR_LOG_ERROR, "cdr_creat_pthread_record_can_data error");
        cdr_system_reboot();
        return;
    }
    
    /* 6、周期检测线程 */
    if (cdr_creat_pthread_fmea_test(&pthread_fmea) != CDR_OK)
    {
        cdr_diag_log(CDR_LOG_ERROR, "cdr_creat_pthread_fmea_test error");
        cdr_system_reboot();
        return;
    }
    
    /* 7、LED灯控制线程创建 */
    if (cdr_creat_pthread_led_control(&pthread_led) != CDR_OK) 
    {
        cdr_diag_log(CDR_LOG_ERROR, "pthread_create  cdr_led_control error");
    }
    cdr_diag_log(CDR_LOG_INFO, "pthread_create cdr_led_control ..............................ok");

    /* 8、打开看门狗 */
    if (cdr_main_init_watch_dog() != CDR_OK)
    {
        cdr_diag_log(CDR_LOG_ERROR, "cdr_main_init_watch_dog error");
        cdr_system_reboot();
        return;        
    }
    
    /* 9、点灯：绿色常亮 */
    cdr_set_led_state(CDR_LED_GREEN_CONTINUOUS);    
  
    /* 10、等待线程结束 */
    pthread_join(pthread_fmea, NULL);
    pthread_join(pthread_record_data, NULL);
    pthread_join(pthread_led, NULL);
    
    /* 11、结束处理 */
    write(g_wdt_fd, "V", 1);
    close(g_wdt_fd);
    close(g_socket_fd);
    
    return;
}

/* 初始化全局变量，如果程序需要的目录不存在，需要创建目录 */
void cdr_global_init()
{
    char *directory[] = 
    {
        CDR_FILE_DIR_RECORDER,
        CDR_FILE_DIR_DIAGLOG,
        CDR_FILE_DIR_DIAGLOG_BF,
        CDR_FILE_DIR_DISK1,
        CDR_FILE_DIR_DISK2,
        CDR_FILE_DIR_DISK1_CANDATA,
        CDR_FILE_DIR_DISK2_CANDATA,
        CDR_FILE_DIR_DISK1_CANDATA_BF,
        CDR_FILE_DIR_DISK2_CANDATA_BF,
    };
    int i;
    DIR *dir;
    
    /* 全局变量初始化 */
    g_socket_fd = 0;
    g_diaglog_record_busy = 0;
    g_pthread_record_data_active = 0;
    g_time_calibration_invalid = 0;
    memset(&g_led_state, 0, sizeof(g_led_state));
    memset(g_system_event_occur, 0, sizeof(g_system_event_occur));
    memset(g_system_event_occur_his, 0, sizeof(g_system_event_occur_his));
    memset(g_system_event_no_occur_num, 0, sizeof(g_system_event_no_occur_num));

    /* 目录不存在，创建目录 */
    for (i = 0; i < sizeof(directory)/sizeof(directory[0]); i++)
    {
        dir = opendir(directory[i]);
        if (dir == NULL)
        {
            if (mkdir(directory[i], 0777) < 0);
            {
                printf("mkdir %s, errno=%d\n",directory[i],errno);
                printf("Mesg:%s\n",strerror(errno));
            }
        }
        else
        {
            closedir(dir);
        }
    }    
    
    return;
}

/* can接口初始化 */
int cdr_main_can_if_init()
{
    int i;
    int ret;
    struct sockaddr_can addr;
    struct ifreq ifr;
    
    /* 设置速率 */
    system("ifconfig can0 down");
    system("echo 250000 > /sys/devices/platform/FlexCAN.0/bitrate");
    system("ifconfig can0 up");
    
    cdr_diag_log(CDR_LOG_INFO, "cdr_main_can_if_init PF_CAN=%u", PF_CAN);
    
    for (i = 0; i < CDR_FAIL_TRY_TIMES; i++)
    {
        /* 创建套接字 */
        g_socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);        
        if (g_socket_fd < 0) 
        {
            cdr_diag_log(CDR_LOG_ERROR, "cdr_main_can_if_init socket error, g_socket_fd=0x%x, times=%u", g_socket_fd, i);
            continue;
        }
        
        /* 把套接字绑定到can0接口    */
        strcpy(ifr.ifr_name, "can0");
        ret = ioctl(g_socket_fd, SIOCGIFINDEX, &ifr);                
        if (ret < 0) 
        {
            cdr_diag_log(CDR_LOG_ERROR, "cdr_main_can_if_init ioctl error, ret=0x%x, times=%u", ret, i);
            close(g_socket_fd);
            continue;
        }

        addr.can_family = PF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;

        ret = bind(g_socket_fd, (struct sockaddr *)&addr, sizeof(addr));    
        if (ret < 0) 
        {
            cdr_diag_log(CDR_LOG_ERROR, "cdr_main_can_if_init bind error, ret=0x%x, times=%u", ret, i);
            close(g_socket_fd);
            continue;
        }
        cdr_diag_log(CDR_LOG_INFO, "cdr_main_can_if_init ..............................ok");
        return CDR_OK;
    }

    cdr_diag_log(CDR_LOG_INFO, "cdr_main_can_if_init ..............................fail");
    return CDR_ERROR;
}


/* 数据存储记录自检，如果失败，尝试3次，3次都失败，上报失败 */
int cdr_main_record_self_test()
{
    int i;
    int ret;

    for (i = 0; i < CDR_FAIL_TRY_TIMES; i++)
    {
        ret = cdr_record_self_test();
        if (ret != CDR_OK) 
        {
            cdr_diag_log(CDR_LOG_ERROR, "cdr_record_self_test error, ret=0x%x, times=%u", ret, i);
            continue;
        }
        cdr_diag_log(CDR_LOG_INFO, "cdr_main_record_self_test ..............................ok");
        return CDR_OK;
    }
    
    cdr_diag_log(CDR_LOG_ERROR, "cdr_main_record_self_test ..............................fail");
    return CDR_ERROR;
}

/* 启动数据记录线程，如果失败，尝试3次，3次都失败，上报失败 */
int cdr_creat_pthread_record_can_data(pthread_t *pthread_record_data)
{
    int i;
    int ret;

    for (i = 0; i < CDR_FAIL_TRY_TIMES; i++)
    {
        ret = pthread_create(pthread_record_data, NULL, (void *)&cdr_record_can_data, NULL); /* 线程创建 */
        if (ret != CDR_OK) 
        {
            cdr_diag_log(CDR_LOG_ERROR, "pthread_create_record_can_data error, ret=0x%x, times=%u", ret, i);
            continue;
        }
        cdr_diag_log(CDR_LOG_INFO, "cdr_creat_pthread_record_can_data ..............................ok");
        return CDR_OK;
    }
    
    cdr_diag_log(CDR_LOG_ERROR, "cdr_creat_pthread_record_can_data ..............................fail");
    return CDR_ERROR;
}


/* 启动周期自检线程，如果失败，尝试3次，3次都失败，上报失败 */
int cdr_creat_pthread_fmea_test(pthread_t *pthread_fmea)
{
    int i;
    int ret;

    for (i = 0; i < CDR_FAIL_TRY_TIMES; i++)
    {
        ret = pthread_create(pthread_fmea, NULL, (void *)&cdr_fmea_test, NULL);  /* 线程创建 */
        if (ret != CDR_OK) 
        {
            cdr_diag_log(CDR_LOG_ERROR, "pthread_create_fmea error, ret=0x%x, times=%u", ret, i);
            continue;
        }
        cdr_diag_log(CDR_LOG_INFO, "cdr_creat_pthread_fmea_test ..............................ok");
        return CDR_OK;
    }
    
    cdr_diag_log(CDR_LOG_ERROR, "cdr_creat_pthread_fmea_test ..............................fail");
    return CDR_ERROR;
}


/* 启动led控制线程，如果失败，尝试3次，3次都失败，上报失败 */
int cdr_creat_pthread_led_control(pthread_t *pthread_led)
{
    int i;
    int ret;

    for (i = 0; i < CDR_FAIL_TRY_TIMES; i++)
    {
        ret = pthread_create(pthread_led, NULL, (void *)&cdr_led_control, NULL);  /* 线程创建 */
        if (ret != CDR_OK) 
        {
            cdr_diag_log(CDR_LOG_ERROR, "cdr_creat_pthread_led_control error, ret=0x%x, times=%u", ret, i);
            continue;
        }
        cdr_diag_log(CDR_LOG_INFO, "cdr_creat_pthread_led_control ..............................ok");
        return CDR_OK;
    }
    
    cdr_diag_log(CDR_LOG_ERROR, "cdr_creat_pthread_led_control ..............................fail");
    return CDR_ERROR;
}


/* 看门狗初始化，如果失败，尝试3次，3次都失败，上报失败 */
int cdr_main_init_watch_dog()
{
    int i;
    int timeout;
return 0;
    for (i = 0; i < CDR_FAIL_TRY_TIMES; i++)
    {
        g_wdt_fd = open(WDT, O_WRONLY);
        if (g_wdt_fd == -1) 
        {
            cdr_diag_log(CDR_LOG_ERROR, "cdr_main_init_watch_dog open error, times=%u", i);
            continue;
        }
        
        cdr_diag_log(CDR_LOG_INFO, "cdr_main_init_watch_dog wdt is opened");
        
        /* 设置门限，超过门限不喂狗系统复位 */
        timeout = CDR_WATCH_DOG_THRESHOLD;
        ioctl(g_wdt_fd, WDIOC_SETTIMEOUT, &timeout);
        ioctl(g_wdt_fd, WDIOC_GETTIMEOUT, &timeout);

        cdr_diag_log(CDR_LOG_INFO, "cdr_main_init_watch_dog timeout was is %d seconds\n", timeout);
        cdr_diag_log(CDR_LOG_INFO, "cdr_main_init_watch_dog ..............................ok");
        return CDR_OK;
    }
    
    cdr_diag_log(CDR_LOG_ERROR, "cdr_main_init_watch_dog ..............................fail");
    return CDR_ERROR;
}
