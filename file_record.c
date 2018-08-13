#include "cdr_include.h"


/*
can data ---->>>> can.data ----->>>> 20180813/can20180813142130567.data 
    can.data读取数据的存储文件，超过文件的最大限制，添加时间戳，缓存到当日的文件夹中
*/

/* ---------------------------------------- 文件内部函数 ---------------------------------------- */

/* 处理模拟的时间校准 */
void cdr_simtest_time_data_proc(cdr_can_frame_t *data)
{
    FILE *fp;
    char time_info[100] = {0};    
    int year, mon, day, hour, min, sec;
    
    if ((fp = fopen("/opt/myapp/test/time_data_sim", "r")) == NULL) /* 文件打开失败 */
    {
        return;
    }
    
    fgets(time_info, 100, fp);
    if (time_info[0] == '\0')
    {
        fclose(fp);    
        return;
    }
    
    year = 0;
    mon = 0;
    day = 0;
    hour = 0;
    min = 0;
    sec = 0;
    cdr_diag_log(CDR_LOG_DEBUG, "time_info %s", time_info);
    sscanf(time_info, "%d-%d-%d-%d-%d-%d", &year, &mon, &day, &hour, &min, &sec);
    
    data->id = CDR_CAN_DATA_TIME_CALIBRATION_PF << 16;
    data->data = (long long)year << 56;
    data->data |= (long long)mon << 48; 
    data->data |= (long long)day << 40; 
    data->data |= (long long)hour << 32; 
    data->data |= (long long)min << 24; 
    data->data |= (long long)sec << 16; 

    cdr_diag_log(CDR_LOG_DEBUG, "send---------------------------------------------id 0x%x  -------data 0x%x", data->id, data->data);    
    cdr_diag_log(CDR_LOG_DEBUG, "send-%d-%d-%d-%d-%d-%d", year, mon, day, hour, min, sec);
    
    fclose(fp);    
    remove("/opt/myapp/test/time_data_sim");
    return;
}

void cdr_simtest_can_data_proc(cdr_can_frame_t *data)
{
    FILE *fp;
    FILE *fp_tmp;
    char str_line[100] = {0};
    char str_line_temp[100] = {0};
    char can_id[10] = {0};
    int num = 0;
    int first_line = 1;
    
    fp = fopen("/opt/myapp/test/can_data_sim", "r");
    fp_tmp = fopen("/opt/myapp/test/can_data_sim_temp", "w");
    if (fp == NULL || fp_tmp == NULL)
    {
        cdr_diag_log(CDR_LOG_DEBUG, "cdr_simtest_can_data_proc open file fail");
        return;
    }    
    
    first_line    = 1;    
    while (!feof(fp))
    {
        memset(str_line, 0, sizeof(str_line));
        fgets(str_line, 1000, fp);    
        
        /* 最后一行 */
        if (str_line[0] == '\0' || str_line[0] == '\n')
        {
            break;
        }    
        
        if (first_line == 1)
        {
            sscanf(str_line, "%8s,%d;", can_id, &num);
            if (num <= 0)
            {                
                continue;
            }
            
            data->id = cdr_hex_to_int(can_id);
            data->data = num;
            cdr_diag_log(CDR_LOG_DEBUG, "send---------------------------------------------id %s  -------data %u", can_id, num);
            num--;
            sprintf(str_line_temp, "%8s,%d;\n", can_id, num);
            first_line = 0;
            continue;
        }
        
        fprintf(fp_tmp, "%s", str_line);
    }
    
    if (first_line == 1)
    {
        fclose(fp);
        fclose(fp_tmp);
        remove("/opt/myapp/test/can_data_sim");
        remove("/opt/myapp/test/can_data_sim_temp");
        return;
    }
    
    fprintf(fp_tmp, "%s", str_line_temp);
    fclose(fp);
    fclose(fp_tmp);
    
    remove("/opt/myapp/test/can_data_sim");
    rename("/opt/myapp/test/can_data_sim_temp", "/opt/myapp/test/can_data_sim");    
    return;    
}


int cdr_receive_can_data_simtest(cdr_can_frame_t *data)
{
    int sim_info;
    
    /* 没有模拟数据 */
    if ((access("/opt/myapp/test/can_data_sim", F_OK) != 0) 
        && (access("/opt/myapp/test/time_data_sim", F_OK) != 0))
    {
        return CDR_ERROR;
    }
    
    /* 时间校准存在，首先处理时间校准数据 */
    if (access("/opt/myapp/test/time_data_sim", F_OK) == 0) /* 模拟can数据 */
    {
        cdr_simtest_time_data_proc(data);
        return CDR_OK;
    }

    if (access("/opt/myapp/test/can_data_sim", F_OK) == 0) /* 模拟can数据 */
    {
        cdr_simtest_can_data_proc(data);
        return CDR_OK;
    }    
    
    return CDR_ERROR;
}


/* 将获取到的can数据写到文件can.data */
void cdr_write_can_data_to_file(char *can_file, cdr_can_frame_t *data)
{
    FILE *fp;
    char time_info[30] = {0};
    char data_info[20] = {0};

    if ((fp = fopen(can_file,"a")) == NULL)
    {
        cdr_diag_log(CDR_LOG_ERROR, "The file %s can not be opened", can_file);
        g_system_event_occur[CDR_EVENT_FILE_RECORD_FAULT] = 1;
        return;
    }
    
    /* 讲数据添加时间戳写入缓存文件cache.0 */
    cdr_get_system_time(CDR_TIME_S_STANDARD, time_info);
    fprintf(fp, "%s %04x %08llx %x\r\n", time_info, data->id, data->data, data->len); /* 数据均是十六进制保存 */
    fclose(fp);
    
    g_system_event_occur[CDR_EVENT_FILE_RECORD_FAULT] = 0;
    return;
}

/* 当文件大于CDR_FILE_DIR_MAX_SIZE_BYTE时，需要另起新文件记录 */
void cdr_can_data_file_proc(char *can_file, char *can_file_bf_dir)
{
    char time_info[30] = {0};
    char file_info[200] = {0};
    char bf_dir[100] = {0};
    
    if (get_file_size(can_file) < CDR_FILE_DIR_MAX_SIZE_BYTE)
    {
        return;
    }
    
    /* 查看是否存在当前日期的目录名，如果不存在，需要创建目录 */
    cdr_get_system_time(CDR_TIME_D_DIGITAL, time_info);
    sprintf(bf_dir, "%s%s", can_file_bf_dir, time_info);
    if (opendir(bf_dir) == NULL)
    {
        if (mkdir(bf_dir, 0777) < 0);
        {
            printf("mkdir %s, errno=%d\n", bf_dir,errno);
            printf("Mesg:%s\n", strerror(errno));
        }
    }
    
    /* 文件移植到当日的目录下 */
    memset(time_info, 0, sizeof(time_info));
    memset(file_info, 0, sizeof(file_info));    
    cdr_get_system_time(CDR_TIME_MS_DIGITAL, time_info);
    sprintf(file_info, CDR_FILE_DIAGLOG_BF_RECORD, time_info);
    
    return;
}


int cdr_receive_can_data(cdr_can_frame_t *data)
{
    int i;
    int ret;
    int read_bytes = 0;
    long long val = 0;
    struct can_frame frame;
    struct timeval time_out;
    fd_set rset;    

     /* 数据模拟 */
    if (cdr_receive_can_data_simtest(data) == CDR_OK)
    {
        if (data->id == 0 && data->data == 0)
        {
            return CDR_ERROR;
        }
        return CDR_OK;
    }
    
    /* 检测can数据，最大检测时间1s */
    FD_ZERO(&rset);
    FD_SET(g_socket_fd, &rset);    
    time_out.tv_sec = 1;
    time_out.tv_usec = 0;
    ret = select((g_socket_fd + 1), &rset, NULL, NULL, &time_out);
    if (ret == 0)
    {
        cdr_diag_log(CDR_LOG_DEBUG, "cdr_receive_can_data select time_out");
        return CDR_ERROR;
    }    
    
    /* 接受报文 */
    memset(&frame, 0 , sizeof(frame));
    read_bytes = read(g_socket_fd, &frame, sizeof(frame));
    if (read_bytes < sizeof(frame))     /* 有问题报文 */
    {
        cdr_diag_log(CDR_LOG_DEBUG, "cdr_receive_can_data frame error, read_bytes=0x%x(0x%x)", read_bytes, sizeof(frame));
        return CDR_ERROR;
    }
    
    if (frame.can_dlc > 8) /* 长度不对 */
    {
        cdr_diag_log(CDR_LOG_DEBUG, "cdr_receive_can_data frame data len err, len=%u", frame.can_dlc);
        return CDR_ERROR;
    }
    
    /* 数据赋值，带出 */
    data->id = frame.can_id;
    data->len = frame.can_dlc;    
    for (i = 0; i < data->len; i++)
    {
        val = frame.data[i];
        data->data |= val << (8 * (data->len - 1 - i)); /* 8个1字节数据，转换成long long */
    }
    
    cdr_diag_log(CDR_LOG_DEBUG, "cdr_receive_can_data id=0x%08x  len=0x%x data=0x%llx", data->id, data->len, data->data);
    return CDR_OK;
}


void cdr_time_calibration_proc(cdr_can_frame_t *data)
{
    char time_info[50] = {0};
    int year, mon, day, hour, min, sec, usec;
    int i;
    
    g_dev_time_calibration_busy = 1;
    
    /* 解析时间 */
    usec  =  (data->data & 0xffff);
    sec   =  (data->data >> 16) & 0xff;
    min   = (data->data >> 24) & 0xff;
    hour  = (data->data >> 32) & 0xff;
    day   = (data->data >> 40) & 0xff;
    mon   = (data->data >> 48) & 0xff;
    year  = ((data->data >> 56) & 0xff) + 100;
    
    
    /* 时间校准 */
    memset(time_info, 0, sizeof(time_info));
    sprintf(time_info, "date %04u.%02u.%02u-%02u:%02u:%02u", year + 1900, mon, day, hour, min, sec);
    system(time_info);
    system("hwclock -w"); /* 同步到硬件时钟 */
    
    g_dev_time_calibration_busy = 0;
    
    cdr_diag_log(CDR_LOG_INFO, "cdr_time_calibration_proc set time %u-%02u-%02u %02u:%02u:%02u.%03u", 
        year + 1900, mon, day, hour, min, sec, usec);

    return;
}

void cdr_can_data_proc(cdr_can_frame_t *data)
{
    /* 时间校准处理 */
    if ((CDR_ID_TO_PF(data->id) == CDR_CAN_DATA_TIME_CALIBRATION_PF) && (g_time_calibration_invalid == 0))
    {
        cdr_time_calibration_proc(data);
        g_time_calibration_invalid = 1;
    }

    /* 文件记录 */
    cdr_write_can_data_to_file(CDR_FILE_DISK1_CANDATA, data);
    cdr_write_can_data_to_file(CDR_FILE_DISK2_CANDATA, data);
    
    cdr_can_data_file_proc(CDR_FILE_DISK1_CANDATA, CDR_FILE_DIR_DISK1_CANDATA_BF);
    cdr_can_data_file_proc(CDR_FILE_DISK2_CANDATA, CDR_FILE_DIR_DISK2_CANDATA_BF);
    
    g_system_event_occur[CDR_EVENT_DATA_RECORDING] = 1;
    return;
}

/* ---------------------------------------- 文件对外函数 ---------------------------------------- */
void cdr_record_can_data()
{
    cdr_can_frame_t data = {0};

    cdr_diag_log(CDR_LOG_INFO, "cdr_record_can_data >>>>>>>>>>>>>>>>>>>>>>>>>>in");

    while (1) 
    {        
        g_pthread_record_data_active = 1;
        
        memset(&data, 0, sizeof(data));
        
        /* 未接收到can数据 */        
        if (cdr_receive_can_data(&data) != CDR_OK)
        {
            g_system_event_occur[CDR_EVENT_DATA_RECORDING] = 0;
            continue;
        }
        
        cdr_can_data_proc(&data);
    }
    
    return;
}

