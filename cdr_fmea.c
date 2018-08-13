#include "cdr_include.h"


/* 获取硬盘当前使用状态，可用空间百分比=可用空间/总空间
    1、硬盘空间剩余20%提示，黄灯闪烁
    2、硬盘空间剩余15%告警，红灯闪烁，此时会覆盖部分数据
    3、硬盘空间剩余2%告警， 红灯闪烁，此时会覆盖所有数据
 */
void cdr_fmea_disk_free_size()
{
    int size_total_1;
    int size_total_2;
    int size_free_1;
    int size_free_2;
    int state_1;
    int state_2;
    
    /* 硬盘总空间 */
    size_total_1 = cdr_get_disk_size_total(CDR_FILE_DIR_DISK1);
    size_total_2 = cdr_get_disk_size_total(CDR_FILE_DIR_DISK2);
    
    /* 硬盘可用空间 */
    size_free_1  = cdr_get_disk_size_free(CDR_FILE_DIR_DISK1);
    size_free_2  = cdr_get_disk_size_free(CDR_FILE_DIR_DISK2);  
    
    if ((size_total_1 == 0) || (size_total_2 == 0))
    {    
        g_system_event_occur[CDR_EVENT_STORAGE_NULL] = 1; /* 获取失败，上报严重告警 */
        return;
    }
    
    state_1 = (size_free_1 * 100) / size_total_1;
    state_2 = (size_free_2 * 100) / size_total_2;
    
    //cdr_diag_log(CDR_LOG_DEBUG, "The cdr_fmea_disk_free_size  disk1： size_total %u size_free %u state%u", 
    //    size_total_1, size_free_1, state_1);
    //cdr_diag_log(CDR_LOG_DEBUG, "The cdr_fmea_disk_free_size  disk2： size_total %u size_free %u state%u", 
    //    size_total_2, size_free_2, state_2);
    
    if ((state_1 < CDR_STORAGE_NULL_THRESHOLD) || (state_2 < CDR_STORAGE_NULL_THRESHOLD)) /* 不足2% */
    {
        g_system_event_occur[CDR_EVENT_STORAGE_NULL] = 1; 
    }    
    else if ((state_1 < CDR_STORAGE_ALARM_THRESHOLD) || (state_2 < CDR_STORAGE_ALARM_THRESHOLD)) /* 不足10% */
    {
        g_system_event_occur[CDR_EVENT_STORAGE_NULL] = 0;
        g_system_event_occur[CDR_EVENT_STORAGE_ALARM] = 1;        
    }    
    else if ((state_1 < CDR_STORAGE_WARNING_THRESHOLD) || (state_2 < CDR_STORAGE_WARNING_THRESHOLD)) /* 不足20% */
    {
        g_system_event_occur[CDR_EVENT_STORAGE_NULL] = 0;
        g_system_event_occur[CDR_EVENT_STORAGE_ALARM] = 0;
        g_system_event_occur[CDR_EVENT_STORAGE_WARNING] = 1;
    }
    else
    {
        /* 存储空间正常，数据正常存储 */
        g_system_event_occur[CDR_EVENT_STORAGE_NULL] = 0;
        g_system_event_occur[CDR_EVENT_STORAGE_ALARM] = 0;
        g_system_event_occur[CDR_EVENT_STORAGE_WARNING] = 0;
    }

    return;
}

/* 设置LED状态灯的时候，需要根据优先级综合判断，最终的LED状态设置 */
void cdr_fmea_set_led_state(int led_state)
{
    int i;
    int led[] = 
    {
        /* 优先级排列，越前面，优先级别越高 */
        CDR_LED_RED_CONTINUOUS, 
        CDR_LED_RED_FLASH, 
        CDR_LED_YELLOW_CONTINUOUS, 
        CDR_LED_YELLOW_FLASH,
        CDR_LED_GREEN_FLASH, 
        CDR_LED_GREEN_CONTINUOUS,
    }; 
    
    /* 没有数据正常运行的时候，状态为绿灯常亮 */
    if (led_state == 0)
    {
        cdr_set_led_state(CDR_LED_GREEN_CONTINUOUS);
        return;        
    }
    
    for (i = 0; i < (sizeof(led) / sizeof(led[0])); i++)
    {
        if (led_state & led[i])
        {
            cdr_set_led_state(led[i]);
            return;
        }
    }
    
    return;
}

void cdr_fmea_system_event_led_proc()
{
    int i;
    int event;
    int led_state = 0;
    int led_info[CDR_EVENT_MAX][2] = 
    {
        {CDR_EVENT_FILE_RECORD_FAULT,      CDR_LED_RED_FLASH},
        {CDR_EVENT_STORAGE_ALARM,          CDR_LED_RED_FLASH},
        {CDR_EVENT_STORAGE_WARNING,        CDR_LED_YELLOW_FLASH},
        {CDR_EVENT_DATA_RECORDING,         CDR_LED_GREEN_FLASH},
        {CDR_EVENT_STORAGE_NULL,           CDR_LED_RED_FLASH},
    };
    
    for (i = 0; i < CDR_EVENT_MAX - CDR_EVENT_FILE_RECORD_FAULT; i++)
    {
        event = led_info[i][0];
        cdr_diag_log(CDR_LOG_DEBUG, "cdr_fmea_system_event_led_proc event %u fault %u", event, g_system_event_occur[event]);
        
        /* 故障发生 */
        if (g_system_event_occur[event])
        {
            if (!g_system_event_occur_his[event]) /* 历史无故障 */
            {
                if (event != CDR_EVENT_DATA_RECORDING)
                {
                    cdr_diag_log(CDR_LOG_ERROR, "cdr_fmea_system_event_led_proc fault %u occurrence", event); /* 第一次发生，并且事件不是CDR_EVENT_DATA_RECORDING，记录日志 */
                }
                else
                {
                    cdr_diag_log(CDR_LOG_DEBUG, "cdr_fmea_system_event_led_proc %u occurrence", event);
                }
                g_system_event_occur_his[event] = g_system_event_occur[event];
            }
            led_state |= led_info[i][1];
            g_system_event_no_occur_num[event] = 0;
        }
        
        /* 无故障 */
        if (!g_system_event_occur[event])
        {
            /* 连续无故障次数超过3次，才进行恢复日志的打印，防止海量日志  */
            g_system_event_no_occur_num[event]++;
            if (g_system_event_no_occur_num[event] < CDR_EVENT_RECOVER_THRESHOLD)
            {
                led_state = g_led_state.state; /* 状态不变 */
                continue;
            }
            
            if (g_system_event_occur_his[event]) /* 历史有故障 */
            {    
                if (event != CDR_EVENT_DATA_RECORDING)
                {
                    cdr_diag_log(CDR_LOG_INFO, "cdr_fmea_system_event_led_proc fault %u recover", event); /* 故障恢复，并且事件不是CDR_EVENT_DATA_RECORDING，记录日志 */
                }
                else
                {
                    cdr_diag_log(CDR_LOG_DEBUG, "cdr_fmea_system_event_led_proc %u no data input", event);
                }
                g_system_event_occur_his[event] = g_system_event_occur[event];
            }
        }
    }
    
    cdr_fmea_set_led_state(led_state);
    return;
}

/* 故障模拟处理 */
void cdr_fmea_fault_sim_proc()
{
    FILE *fp;
    char fault_sim[6] = {0};
    int i;

    if (access(CDR_FILE_TEST_FAULT_SIM, F_OK) != 0) /* 文件不存在 */
    {
        return;
    }
    
    if ((fp = fopen(CDR_FILE_TEST_FAULT_SIM, "r")) == NULL) /* 文件打开失败 */
    {
        return;
    }
    
    fgets(fault_sim, sizeof(fault_sim), fp);
    if (fault_sim[0] == '\0')
    {
        return;
    }
    
    cdr_diag_log(CDR_LOG_DEBUG, "cdr_fmea_fault_sim_proc fault_sim %s", fault_sim);
    
    for (i = CDR_EVENT_FILE_RECORD_FAULT; i < CDR_EVENT_MAX; i++)
    {
        g_system_event_occur[i] = 0;
        if (fault_sim[i] == '2') /* 模拟状态 */
        {
            g_system_event_occur[i] = 1;
        }
    }

    fclose(fp);
    return;
}

/* 1s定时器 */
void cdr_fmea_1s_timer()
{
    cdr_fmea_disk_free_size();    
    cdr_fmea_fault_sim_proc();    
    cdr_fmea_system_event_led_proc();
    
    return;
}

/* 10s定时器 */
void cdr_fmea_10s_timer()
{    
    /* 数据记录和数据存储线程正常，喂狗 */
    cdr_diag_log(CDR_LOG_DEBUG, "cdr_fema g_pthread_record_data_active=%u, ", g_pthread_record_data_active); 
    if (g_pthread_record_data_active)
    {
        write(g_wdt_fd, "\0", 1);/* 喂狗 */
        g_pthread_record_data_active = 0;
        cdr_diag_log(CDR_LOG_DEBUG, "cdr_fema fedd watch dog is ok"); 
    }

    return;
}

/* ---------------------------------------- 文件对外函数 ---------------------------------------- */
int cdr_record_self_test()
{
    int test_data[] = {0x555, 0xaaa};
    int i;
    FILE *fp;
    char str_line[1000] = {0};
    
    for (i = 0; i < sizeof(test_data) / sizeof(test_data[0]); i++)
    {
        /* 数据写入文件 */
        if ((fp = fopen(CDR_FILE_INIT_SELFTEST,"w")) == NULL)
        {
            cdr_diag_log(CDR_LOG_ERROR, "The file %s can not be opened", CDR_FILE_INIT_SELFTEST);
            return CDR_ERROR;
        }        
        fprintf(fp, "%u", test_data[i]);
        fclose(fp);
        
        /* 读取文件，查看数据 */
        fp = fopen(CDR_FILE_INIT_SELFTEST,"r");
        fgets(str_line, 1000, fp);        
        fclose(fp);
        
        /* 结果对比，写入数据和读取数据一致，测试成功，否则失败 */
        if (atoi(str_line) != test_data[i])
        {
            cdr_diag_log(CDR_LOG_ERROR, "cdr_record_self_test ok write data:%u , read data:%s", test_data[i], str_line);
            return CDR_ERROR;
        }
        cdr_diag_log(CDR_LOG_INFO, "cdr_record_self_test ok write data:%u , read data:%s", test_data[i], str_line);
    }
    
    /* 删除文件 */
    remove(CDR_FILE_INIT_SELFTEST); 
    
    return CDR_OK;
}

void cdr_fmea_test()
{
    int i = 0;
    
    cdr_diag_log(CDR_LOG_INFO, "cdr_fmea_test >>>>>>>>>>>>>>>>>>>>>>>>>>in");

    while (1) 
    {
        sleep(1); /* 延时1s */
        
        cdr_fmea_1s_timer();
        
        i++;
        if (i >= 10)
        {
            cdr_fmea_10s_timer();
            i = 0;
        }
    }
    
    return;
}
