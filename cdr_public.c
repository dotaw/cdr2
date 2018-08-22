#include "cdr_include.h"


/* ---------------------------------------- 文件内部函数 ---------------------------------------- */
/* 不同类型的日志打印 */
char *cdr_log_type(int type)
{
    switch (type)
    {
        case CDR_LOG_INFO :
        return "INFO";
        
        case CDR_LOG_ERROR :
        return "ERROR";
        
        case CDR_LOG_DEBUG :
        return "DEBUG";
        
        default :
        return "NONE";
    }
}

/* 诊断日志打印：等待上一个日志的打印，多进程打印日志可能会冲突，这里要保护下，防止重入 */
void cdr_wait_last_diaglog_proc()
{
    if (!g_diaglog_record_busy)
    {
        return;
    }

    while (1)
    {
        if (!g_diaglog_record_busy)
        {    
            return;
        }
    }
    
    return;
}


/* ---------------------------------------- 文件对外函数 ---------------------------------------- */

/* 获取系统时间，根据入参不同，返回的格式不同，返回格式见cdr_time_type_t */
void cdr_get_system_time(int type, char *time_info)
{
   time_t now_time;
   struct timeval time_ms;
   struct tm *info;
   int sec;

    /* 如果此时正在时间校准，等待时间校准完成 */
    if (g_dev_time_calibration_busy)
    {
        while (1)
        {
            if (!g_dev_time_calibration_busy)
            {
                break;
            }
        }
    }
   
    /* 最小时间为秒级，格式为2018-04-22 22:23:24 */    
    time(&now_time);
    info = localtime(&now_time);
    if (info == 0)
    {
       cdr_diag_log(CDR_LOG_ERROR, "get_system_time info is NULL");
       return;
    }
    
    if (type == CDR_TIME_D_DIGITAL)
    {
        sprintf(time_info, "%u%02u%02u", info->tm_year + 1900, info->tm_mon + 1, info->tm_mday);
        return;
    }
    
   /* 最小时间为毫秒级，格式为2018-04-22 22:23:24.999 */    
    if ((type == CDR_TIME_MS_STANDARD) || (type == CDR_TIME_MS_DIGITAL))
    {
        gettimeofday(&time_ms, NULL);
        sec = time_ms.tv_sec % 60;
        /*
            1、用localtime获取秒级时间，用gettimeofday获取ms级时间
            2、如果两者获取的“秒值”不一样，说明已经秒已经进位，需要调用localtime重新获取秒值。
        */        
        if (sec != info->tm_sec)
        {
            time(&now_time);
            info = localtime(&now_time);
            if (info == 0)
            {
                cdr_diag_log(CDR_LOG_ERROR, "get_system_time info is NULL");
                return;
            }
        }
        
        if (type == CDR_TIME_MS_STANDARD)
        {
            sprintf(time_info, "%u-%02u-%02u %02u:%02u:%02u.%03u", 
                info->tm_year + 1900, info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec, (int)(time_ms.tv_usec / 1000));
        }
        else
        {
            sprintf(time_info, "%u%02u%02u%02u%02u%02u%03u", 
                info->tm_year + 1900, info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec, (int)(time_ms.tv_usec / 1000));
        }
        return;       
    }
    
    if (type == CDR_TIME_S_STANDARD)
    {
        sprintf(time_info, "%u-%02u-%02u %02u:%02u:%02u", 
            info->tm_year + 1900, info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
    }
    else
    {
        sprintf(time_info, "%u%02u%02u%02u%02u%02u", 
            info->tm_year + 1900, info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
    }
    return;   
}

/* 获取文件大小，单位字节 */
unsigned long get_file_size(const char *file)  
{  
    unsigned long file_size = 0;
    struct stat stat_buff;
    
    if (stat(file, &stat_buff) < 0) /* 获取文件信息失败 */
    {  
        return file_size;  
    }

    file_size = stat_buff.st_size;
    return file_size;
}

/* 获取文件创建时间 */
unsigned long long get_file_create_time(const char *file)  
{  
    unsigned long long file_create_time = 0;
    struct stat stat_buff;
    
    if (stat(file, &stat_buff) < 0) /* 获取文件信息失败 */
    {  
        return file_create_time;  
    }

    file_create_time = stat_buff.st_ctime;
    return file_create_time;
}

/* 复制file_in内容到file_out */
int cdr_cpy_file(char *file_in, char *file_out)
{
    FILE *fp_in;
    FILE *fp_out;
    int c;
    
    if ((fp_in = fopen(file_in, "r")) == NULL)
    {
        cdr_diag_log(CDR_LOG_ERROR, "cdr_cpy_file faile, the file %s can not be opened", file_in);
        return CDR_ERROR;
    }
    
    if ((fp_out = fopen(file_out, "a")) == NULL)
    {
        cdr_diag_log(CDR_LOG_ERROR, "cdr_cpy_file faile, the file %s can not be opened", file_out);
        return CDR_ERROR;
    }
    
    /* 一个一个字符的复制，至到最后一个字符 */
    while (!feof(fp_in))
    {
        c = fgetc(fp_in);
        if (c == EOF)
        {
            break;
        }
        fputc(c, fp_out);
    }
    
    fclose(fp_in);
    fclose(fp_out);
    return CDR_OK;
}

/* 判断path目录下，file_name类型的文件个数 */
int cdr_get_file_num(char *path, char *file_name)
{
    DIR *dir = opendir(path);
    struct dirent *ptr = NULL;
    int file_num = 0;
    
    if (dir == NULL)
    {
        return file_num;
    }

    /* 循环读取目录 */    
    while((ptr = readdir(dir)) != NULL)
    {
        if(ptr->d_type == 8)    //8：代表文件file类型
        {
            if (strstr(ptr->d_name, file_name) != NULL)  /* 遍历的文件名，包含入参file_name，即满足条件 */
            {
                file_num++;
            }
        }
    }
    
    closedir(dir);
    return file_num;
}

void cdr_get_file_first_line(char *file_name, char *line_info, int len)
{
    FILE *fp;
    
    /* 读取文件，查看数据 */
    if ((fp = fopen(file_name, "r")) == NULL)
    {
        cdr_diag_log(CDR_LOG_ERROR, "cdr_get_file_first_line faile, the file %s can not be opened", file_name);
        return;
    }
    fgets(line_info, len, fp);
    fclose(fp);
    
    return;
}


/* 获取path目录下创建时间最久的文件，带出的file_name是有目录的 */
void cdr_get_oldest_filename(char *path, char *file_name)
{
    DIR *dir = opendir(path);
    struct dirent *ptr = NULL;
    unsigned long long file_create_time = 0;
    unsigned long long temp = 0;
    char name[200] = {0};
    char dir_file[200] = {0};
    
    if (dir == NULL)
    {
        return;
    }
    
    /* 循环读取目录 */
    while((ptr = readdir(dir)) != NULL)
    {
        if(ptr->d_type == 8)    //8：代表文件file类型
        {
            /* 目录+文件名，确认文件 */
            memset(dir_file, 0, sizeof(dir_file));
            sprintf(dir_file, "%s%s", path, ptr->d_name);
            
            /* 检查是否最旧的文件 */
            temp = get_file_create_time(dir_file);
            if ((file_create_time == 0) || (temp < file_create_time))
            {
                file_create_time = temp;
                memset(name, 0, sizeof(name));
                sprintf(name, "%s", dir_file);
            }
        }
    }
    
    sprintf(file_name, "%s", name);
    closedir(dir);
    return;
}

/*
    1、诊断日志的记录，目录diag/diag.log；
    2、单个日志的文件大小，大于CDR_FILE_DIR_MAX_SIZE_BYTE时，另起新文件开始记录，防止单个文件过大
*/
void cdr_diag_log(int type, const char *format, ...)
{
    FILE *fp;
    va_list args;
    char time_info[30] = {0};
    char file_info[200] = {0};
    
    if (CDR_DEBUG_INVALID && (type == CDR_LOG_DEBUG))
    {
        return; /* 屏蔽debug的打印 */
    } 
    
    cdr_wait_last_diaglog_proc();
    g_diaglog_record_busy = 1;
    
    if ((fp = fopen(CDR_FILE_DIAGLOG_RECORD, "a")) == NULL)
    {
        printf("The file %s can not be opened.\r\n", "diag.log");
        printf("errno=%d\n",errno);
        printf("Mesg:%s\n",strerror(errno));
        g_diaglog_record_busy = 0;
        return;
    }
    
    /* 在日志开始加上时间 */
    cdr_get_system_time(CDR_TIME_MS_STANDARD, time_info);
    fprintf(fp, "%23s [%5s]: ", time_info, cdr_log_type(type));
    
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    
    fprintf(fp, ".\r\n");
    fclose(fp);

    /* 当日志大小大于CDR_FILE_DIR_MAX_SIZE_BYTE时，需要另起新文件记录 */
    if (get_file_size(CDR_FILE_DIAGLOG_RECORD) >= CDR_FILE_DIR_MAX_SIZE_BYTE)
    {
        /* 预留的文件数量超过门限，删除最老的文件 */
        while (1)
        {
            if (cdr_get_file_num(CDR_FILE_DIR_DIAGLOG_BF, ".log") >= CDR_DIR_BF_DIAG_MAX_NUM)
            {
                memset(file_info, 0 , sizeof(file_info));
                cdr_get_oldest_filename(CDR_FILE_DIR_DIAGLOG_BF, file_info);                
                remove(file_info);
                printf("delete oldest diaglog file %s\r\n", file_info);
                continue;
            }
            break;
        }
        
        memset(time_info, 0 , sizeof(time_info));
        memset(file_info, 0 , sizeof(file_info));
        
        cdr_get_system_time(CDR_TIME_MS_DIGITAL, time_info);
        sprintf(file_info, CDR_FILE_DIAGLOG_BF_RECORD, time_info);
        rename(CDR_FILE_DIAGLOG_RECORD, file_info);
    }
    
    g_diaglog_record_busy = 0;
    return;
}

/* 获取硬盘总大小，单位M */
int cdr_get_disk_size_total(char *disk_dir)
{
    int size = 0;
    struct statfs disk_info;
    unsigned long long block_size;
    unsigned long long total_size;
    
    statfs(disk_dir, &disk_info); 
    block_size = disk_info.f_bsize;
    total_size = block_size * disk_info.f_blocks;
    
    size = (int)(total_size >> 20); /* 转换为M */
    return size;
}

/* 获取硬盘可用空间大小，单位M */
int cdr_get_disk_size_free(char *disk_dir)
{
    int size = 0;
    struct statfs disk_info;
    unsigned long long block_size;
    unsigned long long total_size;
    
    statfs(disk_dir, &disk_info); 
    block_size = disk_info.f_bsize;
    total_size = block_size * disk_info.f_bfree;
    
    size = (int)(total_size >> 20); /* 转换为M */
    return size;
}

/* 十六进制字符串转整形 */
int cdr_hex_to_int(char *hex_info)  
{  
    int i;  
    int n = 0;  

    for (i = 0; (hex_info[i] >= '0' && hex_info[i] <= '9') || (hex_info[i] >= 'a' && hex_info[i] <= 'z') || (hex_info[i] >='A' && hex_info[i] <= 'Z'); i++)  
    {  
        if (tolower(hex_info[i]) > '9')  
        {  
            n = 16 * n + (10 + tolower(hex_info[i]) - 'a');  
        }  
        else  
        {  
            n = 16 * n + (tolower(hex_info[i]) - '0');  
        }  
    }
    return n;  
}  

void cdr_set_led_state(int type)
{
    /* 状态灯未变化，避免重复设置 */
    if (g_led_state.state == type)
    {
        return;
    }    
    cdr_diag_log(CDR_LOG_INFO, "cdr_set_led_state led change, old=0x%x, new=0x%x", g_led_state.state, type);
    
    /* 设置状态灯 */
    memset(&g_led_state, 0, sizeof(g_led_state));
    g_led_state.state = type;    
    switch (type)
    {
        /* 红灯常亮 */
        case CDR_LED_RED_CONTINUOUS : 
        sprintf(g_led_state.set_red, "1");
        sprintf(g_led_state.set_green, "0");
        sprintf(g_led_state.set_blue, "0");
        g_led_state.is_flash = 0;
        break;
        
        /* 红灯闪烁 */
        case CDR_LED_RED_FLASH :
        sprintf(g_led_state.set_red, "1");
        sprintf(g_led_state.set_green, "0");
        sprintf(g_led_state.set_blue, "0");
        g_led_state.is_flash = 1;
        break;
        
        /* 黄灯常亮 */
        case CDR_LED_YELLOW_CONTINUOUS :
        sprintf(g_led_state.set_red, "1");
        sprintf(g_led_state.set_green, "1");
        sprintf(g_led_state.set_blue, "0");
        g_led_state.is_flash = 0;
        break;
        
        /* 黄灯闪烁 */
        case CDR_LED_YELLOW_FLASH :
        sprintf(g_led_state.set_red, "1");
        sprintf(g_led_state.set_green, "1");
        sprintf(g_led_state.set_blue, "0");
        g_led_state.is_flash = 1;
        break;
        
        /* 绿灯闪烁 */
        case CDR_LED_GREEN_FLASH :
        sprintf(g_led_state.set_red, "0");
        sprintf(g_led_state.set_green, "1");
        sprintf(g_led_state.set_blue, "0");
        g_led_state.is_flash = 1;
        break;
        
        /* 绿灯常亮 */
        case CDR_LED_GREEN_CONTINUOUS :
        sprintf(g_led_state.set_red, "0");
        sprintf(g_led_state.set_green, "1");
        sprintf(g_led_state.set_blue, "0");
        g_led_state.is_flash = 0;
        break;
        
        /* 蓝灯闪烁 */
        case CDR_LED_BLUE_FLASH :
        sprintf(g_led_state.set_red, "0");
        sprintf(g_led_state.set_green, "0");
        sprintf(g_led_state.set_blue, "1");
        g_led_state.is_flash = 1;
        break;
        
        /* 蓝灯常亮 */
        case CDR_LED_BLUE_CONTINUOUS :
        sprintf(g_led_state.set_red, "0");
        sprintf(g_led_state.set_green, "0");
        sprintf(g_led_state.set_blue, "1");
        g_led_state.is_flash = 0;
        break;
        
        default :
        break;
    }    
    
    return;
}

/* 设置系统重启 */
void cdr_system_reboot()
{
    cdr_diag_log(CDR_LOG_INFO, "cdr_system_reboot now==============================");
    //reboot(RB_AUTOBOOT);
    return;
}

void cdr_led_control()
{
    int fd_red;
    int fd_green;
    int fd_blue;

    cdr_diag_log(CDR_LOG_INFO, "cdr_led_control >>>>>>>>>>>>>>>>>>>>>>>>>>in");
    
    /* 打开文件进行操作，控制led状态 */
    fd_red    = open("/sys/class/gpio/gpio49/value", O_RDWR);
    if (fd_red < 0)
    {
        cdr_diag_log(CDR_LOG_ERROR, "cdr_led_control open gpio value error red %d", fd_red);
        return;
    }    
    fd_green  = open("/sys/class/gpio/gpio50/value", O_RDWR);
    if (fd_green < 0)
    {
        cdr_diag_log(CDR_LOG_ERROR, "cdr_led_control open gpio value error green %d", fd_green);
        close(fd_red);
        return;
    }    
    fd_blue = open("/sys/class/gpio/gpio102/value", O_RDWR);
    if (fd_blue < 0)
    {
        cdr_diag_log(CDR_LOG_ERROR, "cdr_led_control open gpio value error blue %d", fd_blue);
        close(fd_red);
        close(fd_green);
        return;
    }    
    
    while (1) 
    {
        write(fd_red,    g_led_state.set_red,    1);
        write(fd_green,  g_led_state.set_green,  1);
        write(fd_blue, g_led_state.set_blue, 1);
        usleep(300000); //延迟0.3s
        
        /* 处理闪烁 */
        if (g_led_state.is_flash)
        {
            write(fd_red,    "0", 1);
            write(fd_green,  "0", 1);
            write(fd_blue, "0", 1);
            usleep(300000); //延迟0.3s
        }
    }
    
    close(fd_red);
    close(fd_green);
    close(fd_blue);
    return;
}

void cdr_cpy_data_to_usb(char *data_dir, char *usb_dir, char *usb_dir_name)
{
    char command[200] = {0};    
    
    sprintf(command, "mkdir %s/%s", usb_dir, usb_dir_name);
    system(command);
    
    memset(command, 0, sizeof(command));
    sprintf(command, "cp -rf %s %s/%s", data_dir, usb_dir, usb_dir_name);
    system(command);
    
    return;
}

int cdr_usb_storage_space_enough(char *usb_dir)
{
    int data_size;
    int usb_size;
    
    data_size = cdr_get_disk_size_total(CDR_FILE_DIR_DISK1) 
                    - cdr_get_disk_size_free(CDR_FILE_DIR_DISK1);
    usb_size = cdr_get_disk_size_free(usb_dir);
    
    cdr_diag_log(CDR_LOG_INFO, "usb_storage_space_info data size %u, usb size %u", data_size, usb_size);
    
    if (usb_size < data_size * 1.2)
    {
        cdr_diag_log(CDR_LOG_INFO, "usb_storage_space_info not enough");
        return CDR_ERROR;
    }
    cdr_diag_log(CDR_LOG_INFO, "usb_storage_space_info enough");
    return CDR_OK;
}

void cdr_usb_detect()
{
    DIR *dir = NULL;
    struct dirent *ptr = NULL;
    int usb_insert = 0;
    int usb_insert_his = 0;
    char usb_dir[200] = {0};
    
    cdr_diag_log(CDR_LOG_INFO, "cdr_usb_detect >>>>>>>>>>>>>>>>>>>>>>>>>>in");
    
    while (1) 
    {
        sleep(5); /* 延时5s */
        
        usb_insert = 0;
        dir = opendir("/media/");
        while((ptr = readdir(dir)) != NULL)
        {
            if (ptr->d_type == 4) //4：代表目录类型
            {
                if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)
                {
                    continue;
                }
                
                usb_insert = 1;
                memset(usb_dir, 0, sizeof(usb_dir));
                sprintf(usb_dir, "/media/%s", ptr->d_name);
                break;
            }
        }
        
        /* 状态变化，打印日志 */
        if (usb_insert != usb_insert_his)
        {
            if (usb_insert)
            {
                cdr_diag_log(CDR_LOG_INFO, "usb pull in, dir: %s", usb_dir);
                g_system_event_occur[CDR_EVENT_USB_PULL_IN] = 1;
                
                sleep(3); /* 延时3s */
                
                if (cdr_usb_storage_space_enough(usb_dir) == CDR_OK)
                {                
                    g_system_event_occur[CDR_EVENT_DATA_TO_USB] = 1;
                    cdr_cpy_data_to_usb(CDR_FILE_DIR_DISK1_CANDATA, usb_dir, "cdr_can_data");
                    //cdr_cpy_data_to_usb(CDR_FILE_DIR_DISK2_CANDATA, usb_dir, "can_data_disk2");
                    sleep(3); /* 延时3s */
                    g_system_event_occur[CDR_EVENT_DATA_TO_USB] = 0;
                    g_system_event_occur[CDR_EVENT_DATA_TO_USB_END] = 1;
                }
                else
                {
                    g_system_event_occur[CDR_EVENT_USB_STORAGE_ALARM] = 1;
                }
            }
            else
            {
                cdr_diag_log(CDR_LOG_INFO, "usb pull out");
                g_system_event_occur[CDR_EVENT_USB_PULL_IN] = 0;
                g_system_event_occur[CDR_EVENT_USB_STORAGE_ALARM] = 0;
                g_system_event_occur[CDR_EVENT_DATA_TO_USB] = 0;
                g_system_event_occur[CDR_EVENT_DATA_TO_USB_END] = 0;
            }
        }
        usb_insert_his = usb_insert;
        closedir(dir);
    }
    
    closedir(dir);
    return;
}


/* 数组转换成16进制字符串，一个字符用两个字符 */
void cdr_char_array_to_str(char *array, int len, char *str)
{
    int i;
    
    memset(str, 0, len * 2 + 1);  /* 数组一个字符转换成两个字符，最后一位\0 */
    for (i = 0; i < len; i++)
    {
        sprintf(str + (i * 2), "%02x", array[i]);
    }
    
    return;
}

void cdr_aes_set_data_encryption(char *data_in, char *data_out)
{
    mbedtls_aes_context aes_ctx;
    unsigned char dec_plain[16]={0};
    char str[33] = {0};
    
    mbedtls_aes_init(&aes_ctx);    
    mbedtls_aes_setkey_enc(&aes_ctx, CDR_AES_KEY, 128);
    
    /* 加密 */
    cdr_char_array_to_str(data_in, 16, str);
    cdr_diag_log(CDR_LOG_DEBUG, "...... encryption before: %s", str);
    mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, data_in, data_out);
    cdr_char_array_to_str(data_out, 16, str);
    cdr_diag_log(CDR_LOG_DEBUG, "*********************************** encryption after: %s", str);
    
    /* 解密 */
    mbedtls_aes_setkey_dec(&aes_ctx, CDR_AES_KEY, 128);
    mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_DECRYPT, data_out, dec_plain);
    cdr_char_array_to_str(dec_plain, 16, str);
    cdr_diag_log(CDR_LOG_DEBUG, "...... unencryption after: %s\n", str);

    mbedtls_aes_free(&aes_ctx);
    return;
}

