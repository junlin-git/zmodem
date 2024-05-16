#include "uart.h"
#include "zglobal.h"
static uint32_t g_speed[] = {B921600, B115200, B38400, B19200, B9600, B4800, B2400, B1200, B300, };
static UARTHAL_BIT_RATE g_name[]  = {HAL_UART_BITRATE_921600, HAL_UART_BITRATE_115200, HAL_UART_BITRATE_38400,
                                    HAL_UART_BITRATE_19200, HAL_UART_BITRATE_9600,
                                    HAL_UART_BITRATE_4800, HAL_UART_BITRATE_2400, HAL_UART_BITRATE_1200, HAL_UART_BITRATE_300, };

static uint32_t UART_SetSpeed(uint32_t fd, UARTHAL_BIT_RATE speed)
{
    uint32_t i;
    uint32_t status;
    struct termios Opt;
    tcgetattr(fd, &Opt);

    for (i = 0; i  < (sizeof(g_speed)/sizeof(g_speed[0])); i++)
    {
        if (speed == g_name[i])
        {
            tcflush(fd, TCIOFLUSH);
            cfsetispeed(&Opt, g_speed[i]);
            cfsetospeed(&Opt, g_speed[i]);
            status = tcsetattr(fd, TCSANOW, &Opt);

            if (status != 0)
            {
                perror("tcsetattr fd1");
            }

            tcflush(fd, TCIOFLUSH);
            break;
        }
    }

    if (i == (sizeof(g_speed) / sizeof(g_speed[0])))
    {
        log_fatal("no this bitrate\n");
        return FALSE;
    }

    return OK;
}

static uint32_t UART_SetParity(uint32_t fd, UARTHAL_DATA_BIT dataBits, UARTHAL_STOP_BIT stopBits, UARTHAL_PARITY parity)
{
    struct termios options;

    if  ( tcgetattr(fd, &options)  !=  0)
    {
        perror("SetupSerial 1");
        return (FALSE);
    }

    options.c_cflag &= ~CSIZE;

    switch (dataBits) /* Set the number of data bits */
    {
        case HAL_UART_DATABIT_7:
            options.c_cflag |= CS7;
            break;

        case HAL_UART_DATABIT_8:
            options.c_cflag |= CS8;
            break;

        default:
            fprintf(stderr, "Unsupported data size\n");
            return (FALSE);
    }

    switch (parity)
    {
        case HAL_UART_PARITY_N:
            options.c_cflag &= ~PARENB;   /* Clear parity enable */
            options.c_iflag &= ~INPCK;     /* Enable parity checking */
            break;

        case HAL_UART_PARITY_O:
            options.c_cflag |= (PARODD | PARENB); /* Set to odd effect */
            options.c_iflag |= INPCK;             /* Disnable parity checking */
            break;

        case HAL_UART_PARITY_E:
            options.c_cflag |= PARENB;     /* Enable parity */
            options.c_cflag &= ~PARODD;    /* Conversion to even test */
            options.c_iflag |= INPCK;      /* Disnable parity checking */
            break;

        case HAL_UART_PARITY_S:
            options.c_cflag &= ~PARENB;
            options.c_cflag &= ~CSTOPB;
            break;

        default:
            fprintf(stderr, "Unsupported parity\n");
            return (FALSE);
    }

    /* Set stop bit */
    switch (stopBits)
    {
        case HAL_UART_STOPBIT_1:
            options.c_cflag &= ~CSTOPB;
            break;

        case HAL_UART_STOPBIT_2:
            options.c_cflag |= CSTOPB;
            break;

        default:
            fprintf(stderr, "Unsupported stop bits\n");
            return (FALSE);
    }

    /* Set input parity option */
    if (parity != HAL_UART_PARITY_N)
    {
        options.c_iflag |= INPCK;
    }

    tcflush(fd, TCIFLUSH);
    options.c_cc[VTIME] = 30;  /* Set timeout 3 seconds */
    options.c_cc[VMIN] = 0;  /* Update the options and do it NOW */

    /* If it is not a development terminal or the like, only the serial port transmits data,
     * and does not need the serial port to process, then the Raw Mode is used to communicate.
     */

    options.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON | IXOFF | IXANY);
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN);
    options.c_oflag &= ~OPOST;

    if (tcsetattr(fd, TCSANOW, &options) != 0)
    {
        perror("SetupSerial 3");
        return (FALSE);
    }

    return OK;
}

static uint32_t HAL_UART_Init(const char *uartDev, int *uartFd)
{

    *uartFd = open(uartDev, O_RDWR  | O_NOCTTY | O_NONBLOCK);

    if (*uartFd == -1)
    {
        log_fatal("uart dev open failed\n");
        return FALSE;
    }

    return OK;
}

static uint32_t HAL_UART_SetAttr(uint32_t uartFd, const UARTHAL_ATTR *uartAttr)
{
    uint32_t ret;

    ret = UART_SetSpeed(uartFd, uartAttr->bitRate);
    if(OK != ret)
    {
        log_fatal("Set bit rate Error\n");
        return -1;
    }

    ret = UART_SetParity(uartFd, uartAttr->dataBits, uartAttr->stopBits, uartAttr->parity);

    if (ret != OK)
    {
        log_fatal("Set Parity Error\n");
        return -1;;
    }

    return OK;
}

static uint32_t uart_open_dev(int * pfd,const char * path,uint8_t BaudRate)
{
    uint32_t ret = OK;

    if(!pfd || !path){
        log_fatal("uart path or nill param\n");
        return FALSE;
    }
    if (*pfd != -1)
    {
        log_fatal("uart already init\n");
        return OK;
    }

    ret = HAL_UART_Init(path, pfd);
    if(OK != ret)
    {
        log_fatal("uart init %s failed\n",path);
        return ret;
    }

    UARTHAL_ATTR uartAttr = {0};
    uartAttr.bitRate  = BaudRate;
    uartAttr.dataBits = HAL_UART_DATABIT_8;
    uartAttr.stopBits = HAL_UART_STOPBIT_1;
    uartAttr.parity   = HAL_UART_PARITY_N;

    ret = HAL_UART_SetAttr(*pfd, &uartAttr);
    if(OK != ret)
    {
        log_fatal("uart set %s attr failed\n",path);
        return ret;
    }
    return OK;
}

uint32_t init_uart(int *fd,char *device)
{
    int ret=uart_open_dev(fd,device,HAL_UART_BITRATE_921600);
    if(ret!=OK)
    {
        log_fatal("init /dev/ttyUSB1 ERROR");
        return -1;
    }
    return *fd;
}
uint32_t deinit_uart(uint32_t uartFd)
{

    close(uartFd);
    return OK;
}

