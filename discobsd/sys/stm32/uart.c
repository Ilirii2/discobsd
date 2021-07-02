/*
 * UART driver for STM32.
 *
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/kconfig.h>
#include <machine/uart.h>
#include <machine/stm32f4xx_ll_bus.h>
#include <machine/stm32f4xx_ll_gpio.h>
#include <machine/stm32f4xx_ll_usart.h>

#define CONCAT(x,y) x ## y
#define BBAUD(x) CONCAT(B,x)

#ifndef UART_BAUD
#define UART_BAUD 115200
#endif

/*
 * STM32 USART/UART port.
 */
struct uart_port {
    GPIO_TypeDef        *port;
    char                 port_name;
    u_long                pin;
};

/*
 * STM32 USART/UART instance.
 */
struct uart_inst {
    USART_TypeDef       *inst;
    struct               uart_port tx;
    struct               uart_port rx;
    u_int                apb_div;
    u_int                af;
};

/*
 * STM32 USART/UART.
 */
static const struct uart_inst uart[NUART] = {
#define PIN2             LL_GPIO_PIN_2
#define PIN3             LL_GPIO_PIN_3
#define AF7              LL_GPIO_AF_7
    { USART1, { GPIOA, 'A', PIN2 }, { GPIOA, 'A', PIN3 }, 4, AF7 }, // XXX
    { USART2, { GPIOA, 'A', PIN2 }, { GPIOA, 'A', PIN3 }, 4, AF7 },
    { USART3, { GPIOA, 'A', PIN2 }, { GPIOA, 'A', PIN3 }, 4, AF7 }, // XXX
    { UART4,  { GPIOA, 'A', PIN2 }, { GPIOA, 'A', PIN3 }, 4, AF7 }, // XXX
    { UART5,  { GPIOA, 'A', PIN2 }, { GPIOA, 'A', PIN3 }, 4, AF7 }, // XXX
    { USART6, { GPIOA, 'A', PIN2 }, { GPIOA, 'A', PIN3 }, 4, AF7 }  // XXX
};

struct tty uartttys[NUART];

static unsigned speed_bps [NSPEEDS] = {
    0,       50,      75,      150,     200,    300,     600,     1200,
    1800,    2400,    4800,    9600,    19200,  38400,   57600,   115200,
    230400,  460800,  500000,  576000,  921600, 1000000, 1152000, 1500000,
    2000000, 2500000, 3000000, 3500000, 4000000
};

void cnstart(struct tty *tp);

// USART1: APB2 84 MHz AF7: TX on PA.09<-USED, RX on PA.10<-free BAD
//                     AF7: TX on PB.06<-USED, RX on PB.07<-free BAD
// USART2: APB1 42 MHz AF7: TX on PA.02<-free, RX on PA.03<-free GOOD
//                     AF7: TX on PD.05<-USED, RX on PD.06<-free BAD
// USART3: APB1 42 MHZ AF7: TX on PB.10<-USED, RX on PB.11<-free BAD
//                     AF7: TX on PD.08<-free, RX on PD.09<-free GOOD
//                     AF7: TX on PC.10<-USED, RX on PC.11<-free BAD
// UART4:  APB1 42 MHz AF8: TX on PA.00<-USED, RX on PA.01<-free BAD
//                     AF8: TX on PC.10<-USED, RX on PC.11<-free BAD
// UART5:  APB1 42 MHz AF8: TX on PC.12<-USED, RX on PD.02<-free BAD
// USART6: APB2 84 MHz AF8: TX on PC.06<-free, RX on PC.07<-USED BAD

/*
 * Setup USART/UART.
 */
void
uartinit(int unit)
{
    register USART_TypeDef      *inst;
    register GPIO_TypeDef       *tx_port;
    register u_int               tx_pin;
    register GPIO_TypeDef       *rx_port;
    register u_int               rx_pin;
    register u_int               apb_div;
    register u_int               af;

    if (unit < 0 || unit >= NUART)
        return;

    inst    = uart[unit].inst;
    tx_port = uart[unit].tx.port;
    tx_pin  = uart[unit].tx.pin;
    rx_port = uart[unit].rx.port;
    rx_pin  = uart[unit].rx.pin;
    apb_div = uart[unit].apb_div;
    af      = uart[unit].af;

    switch (unit) {
    case 0:
        break;
    case 1:     /* USART2: APB1 42 MHz AF7: TX on PA.02, RX on PA.03 */
        LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);

        break;
    case 2:
        break;
    case 3:
        break;
    case 4:
        break;
    case 5:
        break;
    default:
        break;
    }

    /* Config Tx Pin as: Alt func, High Speed, Push pull, Pull up */
    LL_GPIO_SetPinMode(tx_port, tx_pin, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(tx_port, tx_pin, af);
    LL_GPIO_SetPinSpeed(tx_port, tx_pin, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinOutputType(tx_port, tx_pin, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinPull(tx_port, tx_pin, LL_GPIO_PULL_UP);

    /* Config Rx Pin as: Alt func, High Speed, Push pull, Pull up */
    LL_GPIO_SetPinMode(rx_port, rx_pin, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(rx_port, rx_pin, af);
    LL_GPIO_SetPinSpeed(rx_port, rx_pin, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinOutputType(rx_port, rx_pin, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinPull(rx_port, rx_pin, LL_GPIO_PULL_UP);

    /* Transmit/Receive, 8 data bit, 1 start bit, 1 stop bit, no parity. */
    LL_USART_SetTransferDirection(inst, LL_USART_DIRECTION_TX_RX);
    LL_USART_ConfigCharacter(inst,
        LL_USART_DATAWIDTH_8B, LL_USART_PARITY_NONE, LL_USART_STOPBITS_1);
    LL_USART_SetBaudRate(inst,
        SystemCoreClock/apb_div, LL_USART_OVERSAMPLING_16, UART_BAUD);
    LL_USART_Enable(inst);
}

int uartopen(dev_t dev, int flag, int mode)
{
    register struct uartreg *reg;
    register struct tty *tp;
    register int unit = minor(dev);

    if (unit < 0 || unit >= NUART)
        return (ENXIO);

    tp = &uartttys[unit];
    if (! tp->t_addr)
        return (ENXIO);

    reg = (struct uartreg*) tp->t_addr;
    tp->t_oproc = uartstart;
    if ((tp->t_state & TS_ISOPEN) == 0) {
        if (tp->t_ispeed == 0) {
            tp->t_ispeed = BBAUD(UART_BAUD);
            tp->t_ospeed = BBAUD(UART_BAUD);
        }
        ttychars(tp);
        tp->t_state = TS_ISOPEN | TS_CARR_ON;
        tp->t_flags = ECHO | XTABS | CRMOD | CRTBS | CRTERA | CTLECH | CRTKIL;
    }
    if ((tp->t_state & TS_XCLUDE) && u.u_uid != 0)
        return (EBUSY);

#if 0 // XXX
    reg->sta = 0;
    reg->brg = PIC32_BRG_BAUD (BUS_KHZ * 1000, speed_bps [tp->t_ospeed]);
    reg->mode = PIC32_UMODE_PDSEL_8NPAR |
                PIC32_UMODE_ON;
    reg->staset = PIC32_USTA_URXEN | PIC32_USTA_UTXEN;

    /* Enable receive interrupt. */
    if (uirq[unit].rx < 32) {
            IECSET(0) = 1 << uirq[unit].rx;
    } else if (uirq[unit].rx < 64) {
            IECSET(1) = 1 << (uirq[unit].rx-32);
    } else {
            IECSET(2) = 1 << (uirq[unit].rx-64);
    }
#endif // XXX
    return ttyopen(dev, tp);
}

/*ARGSUSED*/
int
uartclose (dev_t dev, int flag, int mode)
{
    register int unit = minor(dev);
    register struct tty *tp = &uartttys[unit];

    if (! tp->t_addr)
        return ENODEV;

    ttywflush(tp);
    ttyclose(tp);
    return(0);
}

/*ARGSUSED*/
int
uartread (dev, uio, flag)
    dev_t dev;
    struct uio *uio;
    int flag;
{
    register int unit = minor(dev);
    register struct tty *tp = &uartttys[unit];

    if (! tp->t_addr)
        return ENODEV;

    return ttread(tp, uio, flag);
}

/*ARGSUSED*/
int
uartwrite (dev, uio, flag)
    dev_t dev;
    struct uio *uio;
    int flag;
{
    register int unit = minor(dev);
    register struct tty *tp = &uartttys[unit];

    if (! tp->t_addr)
        return ENODEV;

    return ttwrite(tp, uio, flag);
}

int
uartselect (dev, rw)
    register dev_t dev;
    int rw;
{
    register int unit = minor(dev);
    register struct tty *tp = &uartttys[unit];

    if (! tp->t_addr)
        return ENODEV;

    return (ttyselect (tp, rw));
}

/*ARGSUSED*/
int
uartioctl (dev, cmd, addr, flag)
    dev_t dev;
    register u_int cmd;
    caddr_t addr;
    int flag;
{
    register int unit = minor(dev);
    register struct tty *tp = &uartttys[unit];
    register int error;

    if (! tp->t_addr)
        return ENODEV;

    error = ttioctl(tp, cmd, addr, flag);
    if (error < 0)
        error = ENOTTY;
    return (error);
}

void
uartintr (dev)
    dev_t dev;
{
    register int c;
    register int unit = minor(dev);
    register struct tty *tp = &uartttys[unit];
    register struct uartreg *reg = (struct uartreg *)tp->t_addr;

    if (! tp->t_addr)
        return;

#if 0 // XXX
    /* Receive */
    while (reg->sta & PIC32_USTA_URXDA) {
        c = reg->rxreg;
        ttyinput(c, tp);
    }
    if (reg->sta & PIC32_USTA_OERR)
        reg->staclr = PIC32_USTA_OERR;

    if (uirq[unit].rx < 32) {
        IFSCLR(0) = (1 << uirq[unit].rx) | (1 << uirq[unit].er);
    } else if (uirq[unit].rx < 64) {
        IFSCLR(1) = (1 << (uirq[unit].rx-32)) | (1 << (uirq[unit].er-32));
    } else {
        IFSCLR(2) = (1 << (uirq[unit].rx-64)) | (1 << (uirq[unit].er-64));
    }

    /* Transmit */
    if (reg->sta & PIC32_USTA_TRMT) {
        led_control (LED_TTY, 0);

        if (uirq[unit].tx < 32) {
            IECCLR(0) = 1 << uirq[unit].tx;
            IFSCLR(0) = 1 << uirq[unit].tx;
        } else if (uirq[unit].tx < 64) {
            IECCLR(1) = 1 << (uirq[unit].tx - 32);
            IFSCLR(1) = 1 << (uirq[unit].tx - 32);
        } else {
            IECCLR(2) = 1 << (uirq[unit].tx - 64);
            IFSCLR(2) = 1 << (uirq[unit].tx - 64);
        }

        if (tp->t_state & TS_BUSY) {
            tp->t_state &= ~TS_BUSY;
            ttstart(tp);
        }
    }
#endif // XXX
}

void uartstart (register struct tty *tp)
{
    register struct uartreg *reg = (struct uartreg*) tp->t_addr;
    register int c, s;
    register int unit = minor(tp->t_dev);

    if (! tp->t_addr)
        return;

    s = spltty();
    if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
out:    /* Disable transmit_interrupt. */
        led_control (LED_TTY, 0);
        splx (s);
        return;
    }
    ttyowake(tp);
    if (tp->t_outq.c_cc == 0)
        goto out;

#if 0 // XXX
    if (reg->sta & PIC32_USTA_TRMT) {
        c = getc(&tp->t_outq);
        reg->txreg = c & 0xff;
        tp->t_state |= TS_BUSY;
    }

    /* Enable transmit interrupt. */
    if (uirq[unit].tx < 32) {
        IECSET(0) = 1 << uirq[unit].tx;
    } else if (uirq[unit].tx < 64) {
        IECSET(1) = 1 << (uirq[unit].tx - 32);
    } else {
        IECSET(2) = 1 << (uirq[unit].tx - 64);
    }
#endif // XXX
    led_control (LED_TTY, 1);
    splx (s);
}

void
uartputc(dev_t dev, char c)
{
    int unit = minor(dev);
    struct tty *tp = &uartttys[unit];
    register USART_TypeDef *inst = uart[unit].inst;
    register int s, timo;

    s = spltty();
again:
    /*
     * Try waiting for the console tty to come ready,
     * otherwise give up after a reasonable time.
     */
    timo = 30000;
    while (!LL_USART_IsActiveFlag_TXE(inst))
        if (--timo == 0)
            break;

    if (tp->t_state & TS_BUSY) {
        uartintr (dev);
        goto again;
    }
    led_control (LED_TTY, 1);
    LL_USART_ClearFlag_TC(inst);
    LL_USART_TransmitData8(inst, c);

    timo = 30000;
    while (!LL_USART_IsActiveFlag_TC(inst))
        if (--timo == 0)
            break;

#if 0 /* XXX */
    /* Clear TX interrupt. */
    if (uirq[unit].tx < 32) {
        IECCLR(0) = 1 << uirq[unit].tx;
    } else if (uirq[unit].tx < 64) {
        IECCLR(1) = 1 << (uirq[unit].tx - 32);
    } else {
        IECCLR(2) = 1 << (uirq[unit].tx - 64);
    }
#endif /* XXX */
    led_control(LED_TTY, 0);
    splx(s);
}

char uartgetc(dev_t dev)
{
    int unit = minor(dev);
// XXX    register struct uartreg *reg = uart[unit];
    int s, c;

    s = spltty();
#if 0 // XXX
    for (;;) {
        /* Wait for key pressed. */
        if (reg->sta & PIC32_USTA_URXDA) {
            c = reg->rxreg;
            break;
        }
    }

    if (uirq[unit].rx < 32) {
        IFSCLR(0) = (1 << uirq[unit].rx) | (1 << uirq[unit].er);
    } else if (uirq[unit].rx < 64) {
        IFSCLR(1) = (1 << (uirq[unit].rx-32)) | (1 << (uirq[unit].er-32));
    } else {
        IFSCLR(2) = (1 << (uirq[unit].rx-64)) | (1 << (uirq[unit].er-64));
    }
#endif // XXX
    splx(s);
    return (unsigned char) c;
}

/*
 * Test to see if device is present.
 * Return true if found and initialized ok.
 */
static int
uartprobe(config)
    struct conf_device *config;
{
    int unit = config->dev_unit - 1;
    int is_console = (CONS_MAJOR == UART_MAJOR &&
                      CONS_MINOR == unit);

    if (unit < 0 || unit >= NUART)
        return 0;

    printf("uart%d: pins tx=P%c%d/rx=P%c%d, af=%d", unit+1,
        uart[unit].tx.port_name, gpio_pinno(uart[unit].tx.pin),
        uart[unit].rx.port_name, gpio_pinno(uart[unit].rx.pin),
        uart[unit].af);

    if (is_console)
        printf(", console");
    printf("\n");

    /* Initialize the device. */
    uartttys[unit].t_addr = (caddr_t) &uart[unit];
    if (! is_console)
        uartinit(unit);

    return 1;
}

struct driver uartdriver = {
    "uart", uartprobe,
};
