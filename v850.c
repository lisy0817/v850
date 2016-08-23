#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <pthread.h>

#define V850_IO_MAGIC           'V'
#define V850_IOR_TX_RETRY       _IOR(V850_IO_MAGIC, 1, int)
#define V850_IOR_TX_GAP         _IOR(V850_IO_MAGIC, 2, int)
#define V850_IOR_RX_TIMEOUT     _IOR(V850_IO_MAGIC, 3, int)
#define V850_IOW_TX_RETRY       _IOW(V850_IO_MAGIC, 4, int)
#define V850_IOW_TX_GAP         _IOW(V850_IO_MAGIC, 5, int)
#define V850_IOW_RX_TIMEOUT     _IOW(V850_IO_MAGIC, 6, int)

#define V850_IOR_DATA_DUMP      _IOR(V850_IO_MAGIC, 7, int)
#define V850_IOW_DATA_DUMP      _IOW(V850_IO_MAGIC, 8, int)

#define V850_IOR_MCU_DUMP       _IOR(V850_IO_MAGIC, 9, int)
#define V850_IOW_MCU_DUMP       _IOW(V850_IO_MAGIC, 10, int)

#define V850_IO_MAX_NR          10



#define V850_DEV        "/dev/v850/v850"
#define V850_SET        "/dev/v850/set"
#define V850_DATA       "/dev/v850/data"

#define CMD_FILE        "cmd.txt"

#define V850_RECEIVE    1

static void hex2str(char *dest, unsigned char *src, int len)
{
    char ddl, ddh;
    int i;

    for (i = 0; i < len; i++) {
        ddh = 48 + src[i] / 16;
        ddl = 48 + src[i] % 16;
        if (ddh > 57)
            ddh = ddh + 7;
        if (ddl > 57)
            ddl = ddl + 7;
        dest[i * 3] = ddh;
        dest[i * 3 + 1] = ddl;
        dest[i * 3 + 2] = ' ';
    }

    dest[len * 3] = 0;
}

static void data_dump(unsigned char *data, int len)
{
    char *str;

    if (NULL == data || len <= 0)
        return;

    str = malloc(3 * len + 4);
    if (str) {
        hex2str(str, data, len);
        printf("%s\n", str);
        free(str);
    }
}

static int delspace(char *p)
{
    int i, j = 0;
    for (i = 0; p[i] != '\0'; i++) {
        if (p[i] != ' ')
            p[j++] = p[i];
    }
    p[j] = '\0';
    return j;
}
static unsigned char hex2byte(char hex_ch)
{
    if (hex_ch >= '0' && hex_ch <= '9')
        return hex_ch - '0';
    if (hex_ch >= 'a' && hex_ch <= 'f')
        return hex_ch - 'a' + 10;
    if (hex_ch >= 'A' && hex_ch <= 'F')
        return hex_ch - 'A' + 10;
    return 0x00;
}
static int hexstr2bin(char *hex, unsigned char *bin)
{
    unsigned int bin_len = 0;
    unsigned int hex_len = strlen((char *)hex);
    unsigned int index = 0;
    if (hex_len % 2 == 1)
        hex_len -= 1;
    bin_len = hex_len / 2;
    for (index = 0; index < hex_len; index += 2) {
        bin[index / 2] = ((hex2byte(hex[index]) << 4) & 0xF0) +
                         hex2byte(hex[index + 1]);
    }

    bin[bin_len] = 0;
    return bin_len;
}

static unsigned char checksum(unsigned char *data, unsigned int len)
{
    unsigned int i;
    unsigned char sum = 0;

    for (i = 0; i < len; i++)
        sum += data[i];

    sum ^= 0xFF;
    return sum;
}

#define v850_options            "ht:g:r:f:d:x"
#define CMD_MAX_LEN             256

static int fd_v850 = -1;
static FILE *fp_cmd = NULL;
static char cmd_str[CMD_MAX_LEN];
static unsigned char cmd_bin[CMD_MAX_LEN];

static inline void usage(char *name)
{
    printf("Usage:\n" \
           "    %s [-h(help)] [-t tx_retry(ms) -g frame_gap(ms) -r rx_timeout(ms)]\n" \
           "    [-f data_file] [-d 1/0(dump mcu data)] [-x(rx thread)]\n" \
           "note: if \"-f\" missed, file \"cmd.txt\" must in current directory\n", name);
}

#if V850_RECEIVE
unsigned char rx[320];
static void *v850_receive(void *data)
{
    int size;

    printf("v850 rx thread enter\n");
    for (;;) {
        size = read(fd_v850, rx, sizeof(rx));
        if (size < 0)
            printf("read \"%s\" fail: %d\n", V850_DEV, size);
        //else
        //    printf("read from \"%s\" %d byte(s)\n", V850_DEV, size);
    }
    printf("v850 rx thread exit\n");
}
#endif

int main(int argc, char *argv[])
{
    int result;
    int cmd;
    unsigned long tx_retry = 0;
    unsigned long tx_gap = 0;
    unsigned long rx_timeout = 0;
    const char *filename = NULL;
    int ret = 0;
    int size;
    int dump = -1;
    int rx_thread = 0;
#if V850_RECEIVE
    pthread_t rev_thread;
#endif

    /*get parameters*/
    while ((result = getopt(argc, argv, v850_options)) != -1) {
        switch (result) {
            case 'h':
                usage(argv[0]);
                return 0;
            case 't':
                tx_retry = atol(optarg);
                break;
            case 'g':
                tx_gap = atol(optarg);
                break;
            case 'r':
                rx_timeout = atol(optarg);
                break;
            case 'f':
                filename = optarg;
                break;
            case 'd':
                dump = atoi(optarg);
                break;
            case 'x':
                rx_thread = 1;
                break;
            case '?':
                printf("invalid option: %c\n", optopt);
                usage(argv[0]);
                return -1;
            default:
                break;
        }
    }

    /*open cmd file and device node*/
    if (!filename)
        filename = CMD_FILE;
    fp_cmd = fopen(filename, "r");
    if (NULL == fp_cmd) {
        printf("!!err, open \"%s\" fail\n", filename);
        usage(argv[0]);
        ret = -1;
        goto fail;
    }
    fd_v850 = open(V850_DEV, O_RDWR);
    if (fd_v850 < 0) {
        printf("!!err, open \"%s\" fail\n", V850_DEV);
        ret = -1;
        goto fail;
    }

    /*config v850 driver(if need)*/
    if (tx_retry) {
        cmd = V850_IOW_TX_RETRY;
        if (ioctl(fd_v850, cmd, &tx_retry) < 0)
            printf("set tx retry time fail\n");
    }
    if (tx_gap) {
        cmd = V850_IOW_TX_GAP;
        if (ioctl(fd_v850, cmd, &tx_gap) < 0)
            printf("set tx gap time fail\n");
    }
    if (rx_timeout) {
        cmd = V850_IOW_RX_TIMEOUT;
        if (ioctl(fd_v850, cmd, &rx_timeout) < 0)
            printf("set rx timeout fail\n");
    }
    if (0 <= dump) {
        cmd = V850_IOW_MCU_DUMP;
        ioctl(fd_v850, cmd, &dump);
    }


    /*print current v850 driver config*/
    cmd = V850_IOR_TX_RETRY;
    if (ioctl(fd_v850, cmd, &tx_retry) < 0) {
        printf("get tx retry time fail\n");
    } else {
        printf("v850 tx retry time: %lu ms\n", tx_retry);
    }
    cmd = V850_IOR_TX_GAP;
    if (ioctl(fd_v850, cmd, &tx_gap) < 0) {
        printf("get tx tx_gap time fail\n");
    } else {
        printf("v850 tx gap time: %lu ms\n", tx_gap);
    }
    cmd = V850_IOR_RX_TIMEOUT;
    if (ioctl(fd_v850, cmd, &rx_timeout) < 0) {
        printf("get rx time out fail\n");
    } else {
        printf("v850 rx time out: %lu ms\n", rx_timeout);
    }

#if V850_RECEIVE
    /*create receive thread to receive data from v850*/
    if (rx_thread) {
        ret = pthread_create(&rev_thread, NULL, v850_receive, NULL);
        if (ret) {
            printf("create v850 receive thread fail: %d\n", ret);
            goto fail;
        }
    }
#endif
    /*send cmd(in cmd file) to v850*/
    for (;;) {
        while ((fgets(cmd_str, CMD_MAX_LEN, fp_cmd)) != NULL) {
            size = delspace(cmd_str);
            size = hexstr2bin(cmd_str, cmd_bin);
            ret = write(fd_v850, cmd_bin, size);
            if (ret < 0)
                printf("send data \"%s\" fail: %d\n", ret);
        }
        fseek(fp_cmd, 0, SEEK_SET);
    }

fail:
    if (fd_v850 > 0)
        close(fd_v850);
    if (NULL != fp_cmd)
        fclose(fp_cmd);
    return ret;
}

