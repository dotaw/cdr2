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

void cdr_can_data_to_array_format(cdr_can_frame_t *data, char *data_info)
{
    data_info[0] = (data->id >> 24) & 0xff;
    data_info[1] = (data->id >> 16) & 0xff;
    data_info[2] = (data->id >> 8) & 0xff;
    data_info[3] = (data->id) & 0xff;
    data_info[4] = (data->data >> 56) & 0xff;
    data_info[5] = (data->data >> 48) & 0xff;
    data_info[6] = (data->data >> 40) & 0xff;
    data_info[7] = (data->data >> 32) & 0xff;
    data_info[8] = (data->data >> 24) & 0xff;
    data_info[9] = (data->data >> 16) & 0xff;
    data_info[10] = (data->data >> 8) & 0xff;
    data_info[11] = (data->data) & 0xff;
    data_info[12] = (data->len) & 0xff;
    
    return;
}

/* 将获取到的can数据写到文件can.data */
void cdr_write_can_data_to_file(char *can_file, cdr_can_frame_t *data)
{
    FILE *fp;
    char time_info[16] = {0};
    char data_info[16] = {0};
    char key_time[16] = {0};    /* 加密后的数据 */
    char key_data[16] = {0};    /* 加密后的数据 */
    char send_net_data[150] = {0};    /* 加密后的数据 */
    char print_info[150] = {0};
    int i;
    int num;
    
    if ((fp = fopen(can_file,"a")) == NULL)
    {
        cdr_diag_log(CDR_LOG_ERROR, "The file %s can not be opened", can_file);
        g_system_event_occur[CDR_EVENT_FILE_RECORD_FAULT] = 1;
        return;
    }
    
    /* 讲数据添加时间戳写入文件 */
    cdr_get_system_time(CDR_TIME_S_DIGITAL, time_info);
    
    /* 加密 */
    cdr_aes_set_data_encryption(time_info, key_time);
    cdr_can_data_to_array_format(data, data_info);
    cdr_aes_set_data_encryption(data_info, key_data);
    
    /* 按照16进制的格式，输出到字符串 */
    for (i = 0; i < (sizeof(key_time) + sizeof(key_data)); i++)
    {
        if (i < sizeof(key_time))
        {
            sprintf(print_info + (i * 2), "%02x", key_time[i]);
        }
        else
        {
            sprintf(print_info + (i * 2), "%02x", key_data[i - sizeof(key_time)]);
        }
    }

    /* 写入到文件 */
    fprintf(fp, "%s\n", print_info);
    fclose(fp);
    
    /* 按照16进制的格式，输出到字符串 */
    for (i = 0; i < (sizeof(time_info) + sizeof(data_info)); i++)
    {
        if (i < sizeof(time_info))
        {
            sprintf(send_net_data + (i * 2), "%02x", time_info[i]);
        }
        else
        {
            sprintf(send_net_data + (i * 2), "%02x", data_info[i - sizeof(time_info)]);
        }
    }
    num = sendto(g_net_sockfd, send_net_data, strlen(send_net_data), 0, (struct sockaddr *)&g_net_addr_remote, sizeof(struct sockaddr_in));
    if (num < 0)
    {
        cdr_diag_log(CDR_LOG_ERROR, "Failed to send net data!");
    }
    cdr_diag_log(CDR_LOG_DEBUG, "SEND:%s", send_net_data);
    
    g_system_event_occur[CDR_EVENT_FILE_RECORD_FAULT] = 0;
    return;
}

/* 存储空间不足时，需要删除旧文件
    当文件大于CDR_FILE_DIR_MAX_SIZE_BYTE时，需要另起新文件记录 */
void cdr_can_data_file_proc(char *can_file, char *dirinfo_file, char *can_file_bf_dir)
{
    char time_info[30] = {0};
    char file_info[200] = {0};
    char dir_info[200] = {0};
    char command_info[200] = {0};
    char bf_dir[100] = {0};
    DIR *dir;
    FILE *fp;    
    
    /* 存储空间不足时，需要删除旧文件 */
    if (g_system_event_occur[CDR_EVENT_STORAGE_NULL] == 1)
    {
        memset(dir_info, 0, sizeof(dir_info)); 
        memset(command_info, 0, sizeof(command_info));
        
        /* 获取最旧的目录文件，然后删除 */                 
        cdr_get_file_first_line(dirinfo_file, dir_info, sizeof(dir_info)); /* 获取最旧的目录 */         
        sprintf(command_info, "rm -rf %s", dir_info);
        system(command_info);        
        printf("delete %s\r\n", dir_info);
        
        /* 删除第一行文件信息记录 */
        memset(command_info, 0, sizeof(command_info));  
        sprintf(command_info, "sed -i 1d %s", dirinfo_file);
        system(command_info);
    }
    
    if (get_file_size(can_file) < CDR_FILE_DIR_MAX_SIZE_BYTE)
    {
        return;
    }
    
    /* 查看是否存在当前日期的目录名，如果不存在，需要创建目录 */
    cdr_get_system_time(CDR_TIME_D_DIGITAL, time_info);
    sprintf(bf_dir, "%s%s", can_file_bf_dir, time_info);
    dir = opendir(bf_dir);
    if (dir == NULL)
    {
        if (mkdir(bf_dir, 0777) < 0);
        {
            cdr_diag_log(CDR_LOG_ERROR, "mkdir %s, errno=%d\n", bf_dir,errno);
            cdr_diag_log(CDR_LOG_ERROR, "Mesg:%s\n", strerror(errno));
        }
        
        /* 将目录信息保存到文件信息中，便于后续查询 */
        if ((fp = fopen(dirinfo_file, "a")) == NULL)
        {
            cdr_diag_log(CDR_LOG_ERROR, "The file %s can not be opened", dirinfo_file);
            cdr_diag_log(CDR_LOG_ERROR, "errno=%d\n",errno);
            cdr_diag_log(CDR_LOG_ERROR, "Mesg:%s\n",strerror(errno));
        }
        else
        {
            fprintf(fp, "%s\n", bf_dir);
            fclose(fp);
        }
        
    }
    else
    {
        closedir(dir);
    }
    
    /* 文件移植到当日的目录下 */
    memset(time_info, 0, sizeof(time_info));
    memset(file_info, 0, sizeof(file_info));    
    cdr_get_system_time(CDR_TIME_MS_DIGITAL, time_info);
    sprintf(file_info, "%s/can%s.data", bf_dir, time_info);
    rename(can_file, file_info);    
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
    //cdr_write_can_data_to_file(CDR_FILE_DISK2_CANDATA, data);
    
    cdr_can_data_file_proc(CDR_FILE_DISK1_CANDATA, CDR_FILE_DISK1_DIR_INFO, CDR_FILE_DIR_DISK1_CANDATA_BF);
    //cdr_can_data_file_proc(CDR_FILE_DISK2_CANDATA, CDR_FILE_DISK2_DIR_INFO, CDR_FILE_DIR_DISK2_CANDATA_BF);
    
    g_system_event_occur[CDR_EVENT_DATA_RECORDING] = 1;
    return;
}

/* ---------------------------------------- 文件对外函数 ---------------------------------------- */
void cdr_record_can_data()
{
    cdr_can_frame_t data = {0};
    socklen_t sock_len;
    int yes;
    
    cdr_diag_log(CDR_LOG_INFO, "cdr_record_can_data >>>>>>>>>>>>>>>>>>>>>>>>>>in");

    /* 创建socket udp */
    g_net_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_net_sockfd < 0) {
        cdr_diag_log(CDR_LOG_ERROR, "cdr_record_can_data creat g_net_sockfd error");
        cdr_system_reboot();
        return;
    }
    cdr_diag_log(CDR_LOG_INFO, "cdr_record_can_data creat g_net_sockfd ok");
    
    /* 设置通讯方式对广播，即本程序发送的一个消息，网络上所有主机均可以收到 */
    if (is_broadcast == 1)
    {
        yes = 1;
        setsockopt(g_net_sockfd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    }

    g_net_addr_remote.sin_family = AF_INET;
    g_net_addr_remote.sin_port = htons(9027); //发数据的端口号
    inet_pton(AF_INET, send_net_ip, &g_net_addr_remote.sin_addr);
    memset(g_net_addr_remote.sin_zero,0,8);

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

