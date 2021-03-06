#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>
#include "neo_2_asm.h"
#include "configuration.h"

typedef volatile unsigned short vu16;
typedef volatile unsigned int vu32;
typedef volatile uint64_t vu64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef uint64_t u64;

// N64 hardware defines
#define PI_STATUS_REG       *(vu32*)(0xA4600010)
#define PI_BSD_DOM1_LAT_REG *(vu32*)(0xA4600014)
#define PI_BSD_DOM1_PWD_REG *(vu32*)(0xA4600018)
#define PI_BSD_DOM1_PGS_REG *(vu32*)(0xA460001C)
#define PI_BSD_DOM1_RLS_REG *(vu32*)(0xA4600020)
#define PI_BSD_DOM2_LAT_REG *(vu32*)(0xA4600024)
#define PI_BSD_DOM2_PWD_REG *(vu32*)(0xA4600028)
#define PI_BSD_DOM2_PGS_REG *(vu32*)(0xA460002C)
#define PI_BSD_DOM2_RLS_REG *(vu32*)(0xA4600030)

#define FRAM_STATUS_REG         *(vu32*)(0xA8000000)
#define FRAM_COMMAND_REG        *(vu32*)(0xA8010000)
#define FRAM_EXECUTE_CMD        (0xD2000000)
#define FRAM_STATUS_MODE_CMD    (0xE1000000)
#define FRAM_ERASE_OFFSET_CMD   (0x4B000000)
#define FRAM_WRITE_OFFSET_CMD   (0xA5000000)
#define FRAM_ERASE_MODE_CMD     (0x78000000)
#define FRAM_WRITE_MODE_CMD     (0xB4000000)
#define FRAM_READ_MODE_CMD      (0xF0000000)

// V1.2-A hardware
#define MYTH_IO_BASE (0xA8040000)
#define SAVE_IO   *(vu32*)(MYTH_IO_BASE | 0x00*2) // 0x00000000 = ext card save, 0x000F000F = off
#define CIC_IO    *(vu32*)(MYTH_IO_BASE | 0x04*2) // 0 = ext card CIC, 1 = 6101, 2 = 6102, 3 = 6103, 5 = 6105, 6 = 6106
#define ROM_BANK  *(vu32*)(MYTH_IO_BASE | 0x0C*2) // b3-0 = gba card A25-22 (8MB granularity)
#define ROM_SIZE  *(vu32*)(MYTH_IO_BASE | 0x08*2) // b3-0 = gba card A25-22 = masked N64 A25-22
#define RUN_IO    *(vu32*)(MYTH_IO_BASE | 0x20*2) // 0xFFFFFFFF = lock IO for game
#define INT_IO    *(vu32*)(MYTH_IO_BASE | 0x24*2) // 0xFFFFFFFF = enable multi-card mode
#define NEO_IO    *(vu32*)(MYTH_IO_BASE | 0x28*2) // 0xFFFFFFFF = 16 bit mode - read long from 0xB2000000 to 0xB3FFFFFF returns word
#define ROMC_IO   *(vu32*)(MYTH_IO_BASE | 0x30*2) // 0xFFFFFFFF = run card
#define ROMSW_IO  *(vu32*)(MYTH_IO_BASE | 0x34*2) // 0x00000000 = n64 menu at 0xB0000000 to 0xB1FFFFFF and gba card at 0xB2000000 to 0xB3FFFFFF
#define SRAM2C_I0 *(vu32*)(MYTH_IO_BASE | 0x36*2) // 0xFFFFFFFF = gba sram at 0xA8000000 to 0xA803FFFF (only when SAVE_IO = F)
#define RST_IO    *(vu32*)(MYTH_IO_BASE | 0x38*2) // 0x00000000 = RESET to game, 0xFFFFFFFF = RESET to menu
#define CIC_EN    *(vu32*)(MYTH_IO_BASE | 0x3C*2) // 0x00000000 = CIC use default, 0xFFFFFFFF = CIC open
#define CPID_IO   *(vu32*)(MYTH_IO_BASE | 0x40*2) // 0x00000000 = CPLD ID off, 0xFFFFFFFF = CPLD ID one (0x81 = V1.2, 0x82 = V2.0, 0x83 = V3.0)

// V2.0 hardware
#define SRAM2C_IO *(vu32*)(MYTH_IO_BASE | 0x2C*2) // 0xFFFFFFFF = gba sram at 0xA8000000 to 0xA803FFFF (only when SAVE_IO = F)

#define NEO2_RTC_OFFS (0x4000)
#define NEO2_GTC_OFFS (0x4800)
#define NEO2_CACHE_OFFS (0xC000)          // offset of NEO2 inner cache into SRAM

#define SD_OFF (0x0000)
#define SD_ON  (0x0480)

#define _neo_asic_op(cmd) *(vu32 *)(0xB2000000 | (cmd<<1))
#define _neo_asic_wop(cmd) *(vu16 *)(0xB2000000 | (cmd<<1))

typedef struct {
    u32 Magic;
    u32 Cpld;
    u32 MenuMan;
    u32 MenuDev;
    u32 GameMan;
    u32 GameDev;
} hwinfo;

static u32 neo_mode = SD_OFF;

static u8 __attribute__((aligned(16))) dmaBuf[128*1024];

extern unsigned int gBootCic;
extern unsigned int gFlashIosr;
extern unsigned int gPsramIosr;
extern unsigned int gPsramCr;
extern short int gPsramMode;            /* 0 = Neo2-SD IOSR/CR, 1 = Neo2-Pro IOSR/CR */
extern short int gCardType;             /* 0x0000 = newer flash, 0x0101 = new flash, 0x0202 = old flash */
extern unsigned int gCpldVers;          /* 0x81 = V1.2 hardware, 0x82 = V2.0 hardware, 0x83 = V3.0 hardware */

extern unsigned int sd_speed;
extern unsigned int fast_flag;

extern short int gCheats;               /* 0 = off, 1 = select, 2 = all */

struct gscEntry {
    char *description;
    char *gscodes;
    u16  count;
    u16  state;
    u16  mask;
    u16  value;
};
typedef struct gscEntry gscEntry_t;

extern gscEntry_t gGSCodes[];


extern void delay(int cnt);

extern int get_cic(unsigned char *buffer);

u32 PSRAM_ADDR = 0;
int DAT_SWAP = 0;

void neo_sync_bus(void)
{
    asm volatile
    (
        ".set   push\n"
        ".set   noreorder\n"
        "lui    $t0,0xb000\n"
        "lw     $zero,($t0)\n"
        "jr     $ra\n"
        "nop\n"
        ".set pop\n"
    );
}

// do a Neo Flash ASIC command
static unsigned short _neo_asic_cmd(unsigned int cmd, int full)
{
    unsigned int data;

    if (full)
    {
        // need to switch rom bus for ASIC operations
        INT_IO = 0xFFFFFFFF;            // enable multi-card mode
        neo_sync_bus();
        ROMSW_IO = 0x00000000;          // gba mapped to 0xB2000000 - 0xB3FFFFFF
        neo_sync_bus();
        ROM_BANK = 0x00000000;
        neo_sync_bus();
        ROM_SIZE = 0x000C000C;          // bank size = 256Mbit
        neo_sync_bus();
    }

    // at this point, assumes rom bus in a mode that allows ASIC operations

    NEO_IO = 0xFFFFFFFF;                // 16 bit mode
    neo_sync_bus();

    /* do unlocking sequence */
    _neo_asic_op(0x00FFD200);
    _neo_asic_op(0x00001500);
    _neo_asic_op(0x0001D200);
    _neo_asic_op(0x00021500);
    _neo_asic_op(0x00FE1500);
    /* do ASIC command */
    data = _neo_asic_op(cmd);

    NEO_IO = 0x00000000;                // 32 bit mode
    neo_sync_bus();

    return (unsigned short)(data & 0x0000FFFF);
}

unsigned int neo_id_card(void)
{
    unsigned int result, temp;
    vu32 *sram = (vu32 *)0xA8000000;

    // enable ID mode
    _neo_asic_cmd(0x00903500, 1);       // ID enabled

    // map GBA save ram into sram space
    SAVE_IO = 0x000F000F;               // save off
    neo_sync_bus();
    if ((gCpldVers & 0x0F) == 1)
        SRAM2C_I0 = 0xFFFFFFFF;         // enable gba sram
    else
        SRAM2C_IO = 0xFFFFFFFF;         // enable gba sram
    neo_sync_bus();

    // read ID from sram space
    temp = sram[0];
    result = (temp & 0xFF000000) | ((temp & 0x0000FF00)<<8);
    temp = sram[1];
    result |= ((temp & 0xFF000000)>>16) | ((temp & 0x0000FF00)>>8);

    // disable ID mode
    _neo_asic_cmd(0x00904900, 1);       // ID disabled

    if ((gCpldVers & 0x0F) == 1)
        SRAM2C_I0 = 0x00000000;         // disable gba sram
    else
        SRAM2C_IO = 0x00000000;         // disable gba sram
    neo_sync_bus();

    return result;
}

unsigned int neo_get_cpld(void)
{
    unsigned int data;

    INT_IO = 0xFFFFFFFF;                // enable multi-card mode
    neo_sync_bus();

    CPID_IO = 0xFFFFFFFF;               // enable CPLD ID read
    neo_sync_bus();
    data = *(vu32 *)0xB0000000;
    neo_sync_bus();
    CPID_IO = 0x00000000;               // disable CPLD ID read
    neo_sync_bus();

    return data;
}

void neo_hw_info(hwinfo *ptr)
{
    u32 v;
    _neo_asic_cmd(0x00E21500, 1);       // GBA CARD WE ON !

    _neo_asic_cmd(0x00372002, 0);       // set cr = menu flash and write enabled
    _neo_asic_cmd(0x00DA0044, 0);       // set iosr = select menu flash
//    NEO_IO = 0xFFFFFFFF;                // 16 bit mode
//    neo_sync_bus();

    /* get menu flash ID */
    _neo_asic_op(0x00000000);
    _neo_asic_op(0x00000054) = 0x00000098; // ST CFI Query
    _neo_asic_op(0x00000000);
    v = _neo_asic_op(0x00000000);
    ptr->MenuMan = v >> 16;
    ptr->MenuDev = v & 0xFFFF;
    _neo_asic_op(0x00000000);
    _neo_asic_op(0x00000000) = 0x000000F0; // ST Read Array/Reset
    _neo_asic_op(0x00000000);

    _neo_asic_cmd(0x00372202, 0);       // set cr = game flash and write enabled
    _neo_asic_cmd(0x00DAAE44, 0);       // set iosr = select game flash
//    NEO_IO = 0xFFFFFFFF;                // 16 bit mode
//    neo_sync_bus();

    /* get game flash ID */
    _neo_asic_op(0x00000000);
    _neo_asic_wop(0x00000000) = 0x0098; // Intel CFI Query
    _neo_asic_op(0x00000000);
    ptr->GameMan = _neo_asic_wop(0x00000000);
    _neo_asic_op(0x00000000);
    ptr->GameDev = _neo_asic_wop(0x00000001);
    _neo_asic_op(0x00000000);
    _neo_asic_wop(0x00000000) = 0x00FF; // Intel Read Array/Reset
    _neo_asic_op(0x00000000);

    _neo_asic_cmd(0x00E2D200, 0);       // GBA CARD WE OFF !
}

void neo_select_menu(void)
{
    _neo_asic_cmd(0x00370002, 1);       // set cr = menu flash enabled
    _neo_asic_cmd(0x00DA0044, 0);       // set iosr = select menu flash

    ROMSW_IO = 0xFFFFFFFF;              // gba mapped to 0xB0000000 - 0xB3FFFFFF
    neo_sync_bus();
    ROM_SIZE = 0x00000000;              // bank size = 1Gbit
    neo_sync_bus();
}

void neo_select_game(void)
{
    _neo_asic_cmd(0x00370202, 1);       // set cr = game flash enabled
    _neo_asic_cmd(gFlashIosr, 0);       // set iosr = select game flash
    _neo_asic_cmd(0x00EE0630, 0);       // set cr1 = enable extended address bus

    ROMSW_IO = 0xFFFFFFFF;              // gba mapped to 0xB0000000 - 0xB3FFFFFF
    neo_sync_bus();
    ROM_SIZE = 0x00000000;              // bank size = 1Gbit
    neo_sync_bus();
}

void neo_select_psram(void)
{
    switch (gPsramMode)
    {
        case 1:
            // Neo2-Pro
            _neo_asic_cmd(0x00E21500, 1);       // GBA CARD WE ON !
            _neo_asic_cmd(0x00373202|neo_mode, 0); // set cr = game flash and write enabled, optionally enable SD interface
            _neo_asic_cmd(0x00DA674E, 0);       // select psram
            _neo_asic_cmd(0x00EE0630, 0);       // set cr1 = enable extended address bus
            break;
        default:
        case 0:
            // Neo2-SD
            _neo_asic_cmd(0x00E21500, 1);       // GBA CARD WE ON !
            _neo_asic_cmd(0x00372202|neo_mode, 0); // set cr = game flash and write enabled, optionally enable SD interface
            _neo_asic_cmd(0x00DAAF4E, 0);       // select psram
            _neo_asic_cmd(0x00EE0630, 0);       // set cr1 = enable extended address bus
            break;
    }

    ROMSW_IO = 0xFFFFFFFF;              // gba mapped to 0xB0000000 - 0xB3FFFFFF
    neo_sync_bus();
    ROM_SIZE = 0x000C000C;              // bank size = 256Mbit (largest psram available)
    neo_sync_bus();
}

void neo_psram_offset(int offs)
{
    _neo_asic_cmd(0x00C40000|offs, 1);  // set gba game flash offset

    ROMSW_IO = 0xFFFFFFFF;              // gba mapped to 0xB0000000 - 0xB3FFFFFF
    neo_sync_bus();
    ROM_SIZE = 0x000C000C;              // bank size = 256Mbit (largest psram available)
    neo_sync_bus();
    NEO_IO = 0xFFFFFFFF;                // 16 bit mode
    neo_sync_bus();
}

void neo_copyfrom_game(void *dest, int fstart, int len)
{
    neo_select_game();                  // select game flash
#if 0
    // copy data
    for (int ix=0; ix<len; ix+=4)
        *(u32 *)(dest + ix) = *(vu32 *)(0xB0000000 + fstart + ix);
#else
    if ((u32)dest & 7)
    {
        // not properly aligned - DMA sram space to buffer, then copy it
        data_cache_hit_writeback_invalidate(dmaBuf, len);
        while (dma_busy()) ;
        PI_STATUS_REG = 3;
        dma_read((void *)((u32)dmaBuf & 0x1FFFFFFF), 0xB0000000 + fstart, len);
        data_cache_hit_invalidate(dmaBuf, len);
        // copy DMA buffer to dst
        memcpy(dest, dmaBuf, len);
    }
    else
    {
        // destination is aligned - DMA sram space directly to dst
        data_cache_hit_writeback_invalidate(dest, len);
        while (dma_busy()) ;
        PI_STATUS_REG = 3;
        dma_read((void *)((u32)dest & 0x1FFFFFFF), 0xB0000000 + fstart, len);
        data_cache_hit_invalidate(dest, len);
    }
#endif
}

void neo_copyfrom_menu(void *dest, int fstart, int len)
{
    neo_select_menu();                  // select menu flash
#if 0
    // copy data
    for (int ix=0; ix<len; ix+=4)
        *(u32 *)(dest + ix) = *(vu32 *)(0xB0000000 + fstart + ix);
#else
    if ((u32)dest & 7)
    {
        // not properly aligned - DMA sram space to buffer, then copy it
        data_cache_hit_writeback_invalidate(dmaBuf, len);
        while (dma_busy()) ;
        PI_STATUS_REG = 3;
        dma_read((void *)((u32)dmaBuf & 0x1FFFFFFF), 0xB0000000 + fstart, len);
        data_cache_hit_invalidate(dmaBuf, len);
        // copy DMA buffer to dst
        memcpy(dest, dmaBuf, len);
    }
    else
    {
        // destination is aligned - DMA sram space directly to dst
        data_cache_hit_writeback_invalidate(dest, len);
        while (dma_busy()) ;
        PI_STATUS_REG = 3;
        dma_read((void *)((u32)dest & 0x1FFFFFFF), 0xB0000000 + fstart, len);
        data_cache_hit_invalidate(dest, len);
    }
#endif
}

void neo_copyto_psram(void *src, int pstart, int len)
{
    neo_select_psram();                 // psram enabled and write-enabled
    neo_xferto_psram(src,pstart,len);
}

void neo_copyfrom_psram(void *dest, int pstart, int len)
{
    neo_select_psram();                 // psram enabled and write-enabled

    // copy data
    for (int ix=0; ix<len; ix+=4)
        *(u32 *)(dest + ix) = *(vu32 *)(0xB0000000 + pstart + ix);
}

void neo_copyto_sram(void *src, int sstart, int len)
{
    u32 temp;

    temp = (sstart & 0x00030000) >> 13;
    _neo_asic_cmd(0x00E00000|temp, 1);  // set gba sram offset

    //neo_select_menu();
    neo_select_game();
    neo_sync_bus();

    // map GBA save ram into sram space
    SAVE_IO = 0x000F000F;               // save off
    neo_sync_bus();
    if ((gCpldVers & 0x0F) == 1)
        SRAM2C_I0 = 0xFFFFFFFF;         // enable gba sram
    else
        SRAM2C_IO = 0xFFFFFFFF;         // enable gba sram
    neo_sync_bus();

    // Init the PI for sram
    vu32 piLatReg = PI_BSD_DOM2_LAT_REG;
    vu32 piPwdReg = PI_BSD_DOM2_PWD_REG;
    vu32 piPgsReg = PI_BSD_DOM2_PGS_REG;
    vu32 piRlsReg = PI_BSD_DOM2_RLS_REG;
    PI_BSD_DOM2_LAT_REG = 0x00000005;
    PI_BSD_DOM2_PWD_REG = 0x0000000C;
    PI_BSD_DOM2_PGS_REG = 0x0000000D;
    PI_BSD_DOM2_RLS_REG = 0x00000002;

    // copy src to DMA buffer
    for (int ix=0; ix<len; ix++)
    {
        dmaBuf[ix*2 + 0] = ~(*(u8*)(src + ix));
        dmaBuf[ix*2 + 1] = *(u8*)(src + ix);
    }
    // DMA buffer to sram space
    data_cache_hit_writeback_invalidate(dmaBuf, len*2);
    while (dma_busy()) ;
    PI_STATUS_REG = 2;
    dma_write((void *)((u32)dmaBuf & 0x1FFFFFF8), 0xA8000000 + (sstart & 0xFFFF)*2, len*2);

    if ((gCpldVers & 0x0F) == 1)
        SRAM2C_I0 = 0x00000000;         // disable gba sram
    else
        SRAM2C_IO = 0x00000000;         // disable gba sram
    neo_sync_bus();

    PI_BSD_DOM2_LAT_REG = piLatReg;
    PI_BSD_DOM2_PWD_REG = piPwdReg;
    PI_BSD_DOM2_PGS_REG = piPgsReg;
    PI_BSD_DOM2_RLS_REG = piRlsReg;
    neo_sync_bus();

    _neo_asic_cmd(0x00E00000, 1);       // clear gba sram offset
}

void neo_copyfrom_sram(void *dst, int sstart, int len)
{
    u32 temp;

    temp = (sstart & 0x00030000) >> 13;
    _neo_asic_cmd(0x00E00000|temp, 1);  // set gba sram offset

    //neo_select_menu();
    neo_select_game();
    neo_sync_bus();

    // map GBA save ram into sram space
    SAVE_IO = 0x000F000F;               // save off
    neo_sync_bus();
    if ((gCpldVers & 0x0F) == 1)
        SRAM2C_I0 = 0xFFFFFFFF;         // enable gba sram
    else
        SRAM2C_IO = 0xFFFFFFFF;         // enable gba sram
    neo_sync_bus();

    // Init the PI for sram
    vu32 piLatReg = PI_BSD_DOM2_LAT_REG;
    vu32 piPwdReg = PI_BSD_DOM2_PWD_REG;
    vu32 piPgsReg = PI_BSD_DOM2_PGS_REG;
    vu32 piRlsReg = PI_BSD_DOM2_RLS_REG;
    PI_BSD_DOM2_LAT_REG = 0x00000005;
    PI_BSD_DOM2_PWD_REG = 0x0000000C;
    PI_BSD_DOM2_PGS_REG = 0x0000000D;
    PI_BSD_DOM2_RLS_REG = 0x00000002;

    // DMA sram space to buffer
    data_cache_hit_writeback_invalidate(dmaBuf, len*2);
    while (dma_busy()) ;
    PI_STATUS_REG = 3;
    dma_read((void *)((u32)dmaBuf & 0x1FFFFFF8), 0xA8000000 + (sstart & 0xFFFF)*2, len*2);
    data_cache_hit_invalidate(dmaBuf, len*2);
    // copy DMA buffer to dst
    for (int ix=0; ix<len; ix++)
        *(u8*)(dst + ix) = dmaBuf[ix*2 + 1];

    if ((gCpldVers & 0x0F) == 1)
        SRAM2C_I0 = 0x00000000;         // disable gba sram
    else
        SRAM2C_IO = 0x00000000;         // disable gba sram
    neo_sync_bus();

    PI_BSD_DOM2_LAT_REG = piLatReg;
    PI_BSD_DOM2_PWD_REG = piPwdReg;
    PI_BSD_DOM2_PGS_REG = piPgsReg;
    PI_BSD_DOM2_RLS_REG = piRlsReg;
    neo_sync_bus();

    _neo_asic_cmd(0x00E00000, 1);       // clear gba sram offset
}

void neo_copyto_nsram(void *src, int sstart, int len, int mode)
{
    neo_select_game();
    neo_sync_bus();

    SAVE_IO = (mode << 16) | mode;
    neo_sync_bus();
    neo_sync_bus();
    if ((gCpldVers & 0x0F) == 1)
        SRAM2C_I0 = 0x00000000;         // disable gba sram
    else
        SRAM2C_IO = 0x00000000;         // disable gba sram
    neo_sync_bus();
    neo_sync_bus();

    // Init the PI for sram
    vu32 piLatReg = PI_BSD_DOM2_LAT_REG;
    vu32 piPwdReg = PI_BSD_DOM2_PWD_REG;
    vu32 piPgsReg = PI_BSD_DOM2_PGS_REG;
    vu32 piRlsReg = PI_BSD_DOM2_RLS_REG;
    PI_BSD_DOM2_LAT_REG = 0x00000005;
    PI_BSD_DOM2_PWD_REG = 0x0000000C;
    PI_BSD_DOM2_PGS_REG = 0x0000000D;
    PI_BSD_DOM2_RLS_REG = 0x00000002;

    if ((u32)src & 7)
    {
        // source not properly aligned, copy src to DMA buffer, then DMA it
        memcpy(dmaBuf, src, len);
        // DMA buffer to sram space
        data_cache_hit_writeback_invalidate(dmaBuf, len);
        while (dma_busy()) ;
        PI_STATUS_REG = 2;
        dma_write((void *)((u32)dmaBuf & 0x1FFFFFFF), 0xA8000000 + sstart, len);
    }
    else
    {
        // source is aligned, DMA src directly to sram space
        data_cache_hit_writeback_invalidate(src, len);
        while (dma_busy()) ;
        PI_STATUS_REG = 2;
        dma_write((void *)((u32)src & 0x1FFFFFFF), 0xA8000000 + sstart, len);
    }

    SAVE_IO = 0x000F000F;               // save off
    neo_sync_bus();

    PI_BSD_DOM2_LAT_REG = piLatReg;
    PI_BSD_DOM2_PWD_REG = piPwdReg;
    PI_BSD_DOM2_PGS_REG = piPgsReg;
    PI_BSD_DOM2_RLS_REG = piRlsReg;
    neo_sync_bus();
}

void neo_copyfrom_nsram(void *dst, int sstart, int len, int mode)
{
    neo_select_game();
    neo_sync_bus();

    SAVE_IO = (mode << 16) | mode;
    neo_sync_bus();
    if ((gCpldVers & 0x0F) == 1)
        SRAM2C_I0 = 0x00000000;         // disable gba sram
    else
        SRAM2C_IO = 0x00000000;         // disable gba sram
    neo_sync_bus();

    // Init the PI for sram
    vu32 piLatReg = PI_BSD_DOM2_LAT_REG;
    vu32 piPwdReg = PI_BSD_DOM2_PWD_REG;
    vu32 piPgsReg = PI_BSD_DOM2_PGS_REG;
    vu32 piRlsReg = PI_BSD_DOM2_RLS_REG;
    PI_BSD_DOM2_LAT_REG = 0x00000005;
    PI_BSD_DOM2_PWD_REG = 0x0000000C;
    PI_BSD_DOM2_PGS_REG = 0x0000000D;
    PI_BSD_DOM2_RLS_REG = 0x00000002;

    if ((u32)dst & 7)
    {
        // not properly aligned - DMA sram space to buffer, then copy it
        data_cache_hit_writeback_invalidate(dmaBuf, len);
        while (dma_busy()) ;
        PI_STATUS_REG = 3;
        dma_read((void *)((u32)dmaBuf & 0x1FFFFFFF), 0xA8000000 + sstart, len);
        data_cache_hit_invalidate(dmaBuf, len);
        // copy DMA buffer to dst
        memcpy(dst, dmaBuf, len);
    }
    else
    {
        // destination is aligned - DMA sram space directly to dst
        data_cache_hit_writeback_invalidate(dst, len);
        while (dma_busy()) ;
        PI_STATUS_REG = 3;
        dma_read((void *)((u32)dst & 0x1FFFFFFF), 0xA8000000 + sstart, len);
        data_cache_hit_invalidate(dst, len);
    }

    SAVE_IO = 0x000F000F;               // save off
    neo_sync_bus();

    PI_BSD_DOM2_LAT_REG = piLatReg;
    PI_BSD_DOM2_PWD_REG = piPwdReg;
    PI_BSD_DOM2_PGS_REG = piPgsReg;
    PI_BSD_DOM2_RLS_REG = piRlsReg;
    neo_sync_bus();
}

void neo_copyto_eeprom(void *src, int sstart, int len, int mode)
{
    neo_select_game();
    neo_sync_bus();

    SAVE_IO = (mode<<16) | mode;        // set EEPROM mode
    neo_sync_bus();
    if ((gCpldVers & 0x0F) == 1)
        SRAM2C_I0 = 0x00000000;         // disable gba sram
    else
        SRAM2C_IO = 0x00000000;         // disable gba sram
    neo_sync_bus();

    for (int ix=0; ix<len; ix+=8)
    {
        unsigned long long data;
        memcpy((void*)&data, (void*)((u32)src + ix), 8);
        eeprom_write((sstart + ix)>>3, (uint8_t*)&data);
    }

    SAVE_IO = 0x000F000F;               // save off
    neo_sync_bus();

    //neo_select_menu();
}

void neo_copyfrom_eeprom(void *dst, int sstart, int len, int mode)
{
    neo_select_game();
    neo_sync_bus();

    SAVE_IO = (mode<<16) | mode;        // set EEPROM mode
    neo_sync_bus();
    if ((gCpldVers & 0x0F) == 1)
        SRAM2C_I0 = 0x00000000;         // disable gba sram
    else
        SRAM2C_IO = 0x00000000;         // disable gba sram
    neo_sync_bus();

    for (int ix=0; ix<len; ix+=8)
    {
        unsigned long long data;
        eeprom_read((sstart + ix)>>3, (uint8_t*)&data);
        memcpy((void*)((u32)dst + ix), (void*)&data, 8);
    }

    SAVE_IO = 0x000F000F;               // save off
    neo_sync_bus();

    //neo_select_menu();
}

void neo_copyto_fram(void *src, int sstart, int len, int mode)
{
}

void neo_copyfrom_fram(void *dst, int sstart, int len, int mode)
{
    neo_select_game();
    neo_sync_bus();

    SAVE_IO = (mode<<16) | mode;        // set FRAM mode
    neo_sync_bus();
    if ((gCpldVers & 0x0F) == 1)
        SRAM2C_I0 = 0x00000000;         // disable gba sram
    else
        SRAM2C_IO = 0x00000000;         // disable gba sram
    neo_sync_bus();

    // Init the PI for sram
    vu32 piLatReg = PI_BSD_DOM2_LAT_REG;
    vu32 piPwdReg = PI_BSD_DOM2_PWD_REG;
    vu32 piPgsReg = PI_BSD_DOM2_PGS_REG;
    vu32 piRlsReg = PI_BSD_DOM2_RLS_REG;
    PI_BSD_DOM2_LAT_REG = 0x00000005;
    PI_BSD_DOM2_PWD_REG = 0x0000000C;
    PI_BSD_DOM2_PGS_REG = 0x0000000D;
    PI_BSD_DOM2_RLS_REG = 0x00000002;

    FRAM_COMMAND_REG = FRAM_EXECUTE_CMD;
    delay(10);
    FRAM_COMMAND_REG = FRAM_EXECUTE_CMD;
    delay(10);
    FRAM_COMMAND_REG = FRAM_STATUS_MODE_CMD;
    delay(10);

    while (len > 0)
    {
        FRAM_COMMAND_REG = FRAM_READ_MODE_CMD;

        if ((u32)dst & 7)
        {
            // not properly aligned - DMA sram space to buffer, then copy it
            data_cache_hit_writeback_invalidate(dmaBuf, 128);
            while (dma_busy()) ;
            PI_STATUS_REG = 3;
            dma_read((void *)((u32)dmaBuf & 0x1FFFFFFF), 0xA8000000 + sstart/2, 128);
            data_cache_hit_invalidate(dmaBuf, 128);
            // copy DMA buffer to dst
            memcpy(dst, dmaBuf, 128);
        }
        else
        {
            // destination is aligned - DMA sram space directly to dst
            data_cache_hit_writeback_invalidate(dst, 128);
            while (dma_busy()) ;
            PI_STATUS_REG = 3;
            dma_read((void *)((u32)dst & 0x1FFFFFFF), 0xA8000000 + sstart/2, 128);
            data_cache_hit_invalidate(dst, 128);
        }

        dst += 128;
        sstart += 128;
        len -= 128;
    }

    SAVE_IO = 0x000F000F;               // save off
    neo_sync_bus();

    PI_BSD_DOM2_LAT_REG = piLatReg;
    PI_BSD_DOM2_PWD_REG = piPwdReg;
    PI_BSD_DOM2_PGS_REG = piPgsReg;
    PI_BSD_DOM2_RLS_REG = piRlsReg;
    neo_sync_bus();
}

void neo_get_rtc(unsigned char *rtc)
{
}

void neo2_enable_sd(void)
{
    neo_mode = SD_ON;
    neo_select_psram();

    NEO_IO = 0xFFFFFFFF;                // 16 bit mode
    neo_sync_bus();
}

void neo2_disable_sd(void)
{
    neo_mode = SD_OFF;
    neo_select_psram();

    NEO_IO = 0x00000000;                // 32 bit mode
    neo_sync_bus();
}

void neo2_pre_sd(void)
{
    // set the PI for myth sd
    if (sd_speed)
        return;

    switch(fast_flag)
    {
        case 1:
            PI_BSD_DOM1_LAT_REG = 0x00000010;
            PI_BSD_DOM1_RLS_REG = 0x00000003;
            PI_BSD_DOM1_PWD_REG = 0x00000003;
            PI_BSD_DOM1_PGS_REG = 0x00000007;
        return;

        case 2:
            PI_BSD_DOM1_LAT_REG = 0x00000000;
            PI_BSD_DOM1_RLS_REG = 0x00000000;
            PI_BSD_DOM1_PWD_REG = 0x00000003;
            PI_BSD_DOM1_PGS_REG = 0x00000000;
        return;
    }
}

void neo2_post_sd(void)
{
    if ((!sd_speed) && (fast_flag))
    {
        // restore the PI for rom
        PI_BSD_DOM1_LAT_REG = 0x00000040;
        PI_BSD_DOM1_RLS_REG = 0x00000003;
        PI_BSD_DOM1_PWD_REG = 0x00000012;
        PI_BSD_DOM1_PGS_REG = 0x00000007;
    }
}

/*
int neo2_recv_sd_multi(unsigned char *buf, int count)
{
    int res;

    asm(".set push\n"
        ".set noreorder\n\t"
        "lui $15,0xB30E\n\t"            // $15 = 0xB30E0000
        "ori $14,%1,0\n\t"              // $14 = buf
        "ori $12,%2,0\n"                // $12 = count

        "oloop:\n\t"
        "lui $11,0x0001\n"              // $11 = timeout = 64 * 1024

        "tloop:\n\t"
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "andi $2,$2,0x0100\n\t"         // eqv of (data>>8)&0x01
        "beq $2,$0,getsect\n\t"         // start bit detected
        "nop\n\t"
        "addiu $11,$11,-1\n\t"
        "bne $11,$0,tloop\n\t"          // not timed out
        "nop\n\t"
        "beq $11,$0,exit\n\t"           // timeout
        "ori %0,$0,0\n"                 // res = FALSE

        "getsect:\n\t"
        "ori $13,$0,128\n"              // $13 = long count

        "gsloop:\n\t"
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4 => -a-- -a--
        "lui $10,0xF000\n\t"            // $10 = mask = 0xF0000000
        "sll $2,$2,4\n\t"               // a--- a---

        "lw $3,0x6060($15)\n\t"         // rdMmcDatBit4 => -b-- -b--
        "and $2,$2,$10\n\t"             // a000 0000
        "lui $10,0x0F00\n\t"            // $10 = mask = 0x0F000000
        "and $3,$3,$10\n\t"             // 0b00 0000

        "lw $4,0x6060($15)\n\t"         // rdMmcDatBit4 => -c-- -c--
        "lui $10,0x00F0\n\t"            // $10 = mask = 0x00F00000
        "or $11,$3,$2\n\t"              // $11 = ab00 0000
        "srl $4,$4,4\n\t"               // --c- --c-

        "lw $5,0x6060($15)\n\t"         // rdMmcDatBit4 => -d-- -d--
        "and $4,$4,$10\n\t"             // 00c0 0000
        "lui $10,0x000F\n\t"            // $10 = mask = 0x000F0000
        "srl $5,$5,8\n\t"               // ---d ---d
        "or $11,$11,$4\n\t"             // $11 = abc0 0000

        "lw $6,0x6060($15)\n\t"         // rdMmcDatBit4 => -e-- -e--
        "and $5,$5,$10\n\t"             // 000d 0000
        "ori $10,$0,0xF000\n\t"         // $10 = mask = 0x0000F000
        "sll $6,$6,4\n\t"               // e--- e---
        "or $11,$11,$5\n\t"             // $11 = abcd 0000

        "lw $7,0x6060($15)\n\t"         // rdMmcDatBit4 => -f-- -f--
        "and $6,$6,$10\n\t"             // 0000 e000
        "ori $10,$0,0x0F00\n\t"         // $10 = mask = 0x00000F00
        "or $11,$11,$6\n\t"             // $11 = abcd e000
        "and $7,$7,$10\n\t"             // 0000 0f00

        "lw $8,0x6060($15)\n\t"         // rdMmcDatBit4 => -g-- -g--
        "ori $10,$0,0x00F0\n\t"         // $10 = mask = 0x000000F0
        "or $11,$11,$7\n\t"             // $11 = abcd ef00
        "srl $8,$8,4\n\t"               // --g- --g-

        "lw $9,0x6060($15)\n\t"         // rdMmcDatBit4 => -h-- -h--
        "and $8,$8,$10\n\t"             // 0000 00g0
        "ori $10,$0,0x000F\n\t"         // $10 = mask = 0x000000F
        "or $11,$11,$8\n\t"             // $11 = abcd efg0

        "srl $9,$9,8\n\t"               // ---h ---h
        "and $9,$9,$10\n\t"             // 0000 000h
        "or $11,$11,$9\n\t"             // $11 = abcd efgh

        "sw $11,0($14)\n\t"             // save sector data
        "addiu $13,$13,-1\n\t"
        "bne $13,$0,gsloop\n\t"
        "addiu $14,$14,4\n\t"           // inc buffer pointer

        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4 - just toss checksum bytes
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4
        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4

        "lw $2,0x6060($15)\n\t"         // rdMmcDatBit4 - clock out end bit

        "addiu $12,$12,-1\n\t"          // count--
        "bne $12,$0,oloop\n\t"          // next sector
        "nop\n\t"

        "ori %0,$0,1\n"                 // res = TRUE

        "exit:\n"
        ".set pop\n"
        : "=r" (res)                    // output
        : "r" (buf), "r" (count)        // inputs
        : "$0" );                       // clobbered

    return res;
}
*/

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------

#define CIC_6101 1
#define CIC_6102 2
#define CIC_6103 3
#define CIC_6104 4
#define CIC_6105 5
#define CIC_6106 6

// Simulated PIF ROM bootcode adapted from DaedalusX64 emulator
void simulate_pif_boot(u32 cic_chip)
{
    u32 ix, sz, cart, country;
    vu32 *src, *dst;
    u32 info = *(vu32 *)0xB000003C;
    vu64 *gGPR = (vu64 *)0xA03E0000;
    vu32 *codes = (vu32 *)0xA0000180;
    u64 bootAddr = 0xFFFFFFFFA4000040LL;
    char *cp, *vp, *tp;
    char temp[8];
    int i, type, val;
    int curr_cheat = 0;

    cart = info >> 16;
    country = (info >> 8) & 0xFF;

    // clear XBUS/Flush/Freeze
    ((vu32 *)0xA4100000)[3] = 0x15;

    // copy the memsize for different boot loaders
    sz = (gBootCic != CIC_6105) ? *(vu32 *)0xA0000318 : *(vu32 *)0xA00003F0;
    if (cic_chip == CIC_6105)
        *(vu32 *)0xA00003F0 = sz;
    else
        *(vu32 *)0xA0000318 = sz;

    // clear some OS globals for cleaner boot
    *(vu32 *)0xA000030C = 0;             // cold boot
    memset((void *)0xA000031C, 0, 64);   // clear app nmi buffer

    if (gCheats)
    {
        u16 xv, yv, zv;
        u32 xx;
        vu32 *sp, *dp;
        // get rom os boot segment - note, memcpy won't work for copying rom
        sp = (vu32 *)0xB0001000;
        dp = (vu32 *)0xA02A0000;
        for (ix=0; ix<0x100000; ix++)
            *dp++ = *sp++;
        // default boot address with cheats
        sp = (vu32 *)0xB0000008;
        bootAddr = 0xFFFFFFFF00000000LL | *sp;

        // move general int handler
        sp = (vu32 *)0xA0000180;
        dp = (vu32 *)0xA0000120;
        for (ix=0; ix<0x60; ix+=4)
            *dp++ = *sp++;

        // insert new general int handler prologue
        *codes++ = 0x401a6800;  // mfc0     k0,c0_cause
        *codes++ = 0x241b005c;  // li       k1,23*4
        *codes++ = 0x335a007c;  // andi     k0,k0,0x7c
        *codes++ = 0x175b0012;  // bne      k0,k1,0x1d8
        *codes++ = 0x00000000;  // nop
        *codes++ = 0x40809000;  // mtc0     zero,c0_watchlo
        *codes++ = 0x401b7000;  // mfc0     k1,c0_epc
        *codes++ = 0x8f7a0000;  // lw       k0,0(k1)
        *codes++ = 0x3c1b03e0;  // lui      k1,0x3e0
        *codes++ = 0x035bd024;  // and      k0,k0,k1
        *codes++ = 0x001ad142;  // srl      k0,k0,0x5
        *codes++ = 0x3c1ba000;  // lui      k1,0xa000
        *codes++ = 0x8f7b01cc;  // lw       k1,0x01cc(k1)
        *codes++ = 0x035bd025;  // or       k0,k0,k1
        *codes++ = 0x3c1ba000;  // lui      k1,0xa000
        *codes++ = 0xaf7a01cc;  // sw       k0,0x01cc(k1)
        *codes++ = 0x3c1b8000;  // lui      k1,0x8000
        *codes++ = 0xbf7001cc;  // cache    0x10,0x01cc(k1)
        *codes++ = 0x3c1aa000;  // lui      k0,0xa000
        *codes++ = 0x37400120;  // ori      zero,k0,0x120
        *codes++ = 0x42000018;  // eret
        *codes++ = 0x00000000;  // nop

        // process cheats
        while (gGSCodes[curr_cheat].count != 0xFFFF)
        {
            if (!gGSCodes[curr_cheat].state || !gGSCodes[curr_cheat].count)
            {
                // cheat not enabled or no codes, skip
                curr_cheat++;
                continue;
            }

            for (i=0; i<gGSCodes[curr_cheat].count; i++)
            {
                cp = &gGSCodes[curr_cheat].gscodes[i*16 + 0];
                vp = &gGSCodes[curr_cheat].gscodes[i*16 + 9];

                temp[0] = cp[0];
                temp[1] = cp[1];
                temp[2] = 0;
                type = strtol(temp, (char **)NULL, 16);

                switch(type)
                {
                    case 0x80:
                        // write 8-bit value to (cached) ram continuously
                        // 80XXYYYY 00ZZ
                        temp[0] = cp[2];
                        temp[1] = cp[3];
                        temp[2] = 0;
                        xv = strtol(temp, (char **)NULL, 16);
                        temp[0] = cp[4];
                        temp[1] = cp[5];
                        temp[2] = cp[6];
                        temp[3] = cp[7];
                        temp[4] = 0;
                        yv = strtol(temp, (char **)NULL, 16);
                        if (yv & 0x8000) xv++; // adjust for sign extension of yv
                        if (gGSCodes[curr_cheat].mask)
                        {
                            zv = gGSCodes[curr_cheat].value & gGSCodes[curr_cheat].mask;
                        }
                        else
                        {
                            temp[0] = vp[2];
                            temp[1] = vp[3];
                            temp[2] = 0;
                            zv = strtol(temp, (char **)NULL, 16);
                        }
                        *codes++ = 0x3c1a8000 | xv; // lui  k0,80xx
                        *codes++ = 0x241b0000 | zv; // li   k1,00zz
                        *codes++ = 0xa35b0000 | yv; // sb   k1,yyyy(k0)
                        break;
                    case 0x81:
                        // write 16-bit value to (cached) ram continuously
                        // 81XXYYYY 00ZZ
                        temp[0] = cp[2];
                        temp[1] = cp[3];
                        temp[2] = 0;
                        xv = strtol(temp, (char **)NULL, 16);
                        temp[0] = cp[4];
                        temp[1] = cp[5];
                        temp[2] = cp[6];
                        temp[3] = cp[7];
                        temp[4] = 0;
                        yv = strtol(temp, (char **)NULL, 16);
                        if (yv & 0x8000) xv++; // adjust for sign extension of yv
                        if (gGSCodes[curr_cheat].mask)
                        {
                            zv = gGSCodes[curr_cheat].value & gGSCodes[curr_cheat].mask;
                        }
                        else
                        {
                            temp[0] = vp[0];
                            temp[1] = vp[1];
                            temp[2] = vp[2];
                            temp[3] = vp[3];
                            temp[4] = 0;
                            zv = strtol(temp, (char **)NULL, 16);
                        }
                        *codes++ = 0x3c1a8000 | xv; // lui  k0,80xx
                        *codes++ = 0x241b0000 | zv; // li   k1,zzzz
                        *codes++ = 0xa75b0000 | yv; // sh   k1,yyyy(k0)
                        break;
                    case 0x88:
                        // write 8-bit value to (cached) ram on GS button pressed - unimplemented
                        // 88XXYYYY 00ZZ
                        break;
                    case 0x89:
                        // write 16-bit value to (cached) ram on GS button pressed - unimplemented
                        // 89XXYYYY ZZZZ
                        break;
                    case 0xA0:
                        // write 8-bit value to (uncached) ram continuously
                        // A0XXYYYY 00ZZ
                        temp[0] = cp[2];
                        temp[1] = cp[3];
                        temp[2] = 0;
                        xv = strtol(temp, (char **)NULL, 16);
                        temp[0] = cp[4];
                        temp[1] = cp[5];
                        temp[2] = cp[6];
                        temp[3] = cp[7];
                        temp[4] = 0;
                        yv = strtol(temp, (char **)NULL, 16);
                        if (yv & 0x8000) xv++; // adjust for sign extension of yv
                        if (gGSCodes[curr_cheat].mask)
                        {
                            zv = gGSCodes[curr_cheat].value & gGSCodes[curr_cheat].mask;
                        }
                        else
                        {
                            temp[0] = vp[2];
                            temp[1] = vp[3];
                            temp[2] = 0;
                            zv = strtol(temp, (char **)NULL, 16);
                        }
                        *codes++ = 0x3c1aa000 | xv; // lui  k0,A0xx
                        *codes++ = 0x241b0000 | zv; // li   k1,00zz
                        *codes++ = 0xa35b0000 | yv; // sb   k1,yyyy(k0)
                        break;
                    case 0xA1:
                        // write 16-bit value to (uncached) ram continuously
                        // A1XXYYYY 00ZZ
                        temp[0] = cp[2];
                        temp[1] = cp[3];
                        temp[2] = 0;
                        xv = strtol(temp, (char **)NULL, 16);
                        temp[0] = cp[4];
                        temp[1] = cp[5];
                        temp[2] = cp[6];
                        temp[3] = cp[7];
                        temp[4] = 0;
                        yv = strtol(temp, (char **)NULL, 16);
                        if (yv & 0x8000) xv++; // adjust for sign extension of yv
                        if (gGSCodes[curr_cheat].mask)
                        {
                            zv = gGSCodes[curr_cheat].value & gGSCodes[curr_cheat].mask;
                        }
                        else
                        {
                            temp[0] = vp[0];
                            temp[1] = vp[1];
                            temp[2] = vp[2];
                            temp[3] = vp[3];
                            temp[4] = 0;
                            zv = strtol(temp, (char **)NULL, 16);
                        }
                        *codes++ = 0x3c1aa000 | xv; // lui  k0,A0xx
                        *codes++ = 0x241b0000 | zv; // li   k1,zzzz
                        *codes++ = 0xa75b0000 | yv; // sh   k1,yyyy(k0)
                        break;
                    case 0xCC:
                        // deactivate expansion ram using 3rd method
                        // CC000000 0000
                        if (cic_chip == CIC_6105)
                            *(vu32 *)0xA00003F0 = 0x00400000;
                        else
                            *(vu32 *)0xA0000318 = 0x00400000;
                        break;
                    case 0xD0:
                        // do next gs code if ram location is equal to 8-bit value
                        // D0XXYYYY 00ZZ
                        temp[0] = cp[2];
                        temp[1] = cp[3];
                        temp[2] = 0;
                        xv = strtol(temp, (char **)NULL, 16);
                        temp[0] = cp[4];
                        temp[1] = cp[5];
                        temp[2] = cp[6];
                        temp[3] = cp[7];
                        temp[4] = 0;
                        yv = strtol(temp, (char **)NULL, 16);
                        if (yv & 0x8000) xv++; // adjust for sign extension of yv
                        temp[0] = vp[2];
                        temp[1] = vp[3];
                        temp[2] = 0;
                        zv = strtol(temp, (char **)NULL, 16);
                        *codes++ = 0x3c1a8000 | xv; // lui  k0,0x80xx
                        *codes++ = 0x835a0000 | yv; // lb   k0,yyyy(k0)
                        *codes++ = 0x241b0000 | zv; // li   k1,00zz
                        *codes++ = 0x175b0004;      // bne  k0,k1,4
                        *codes++ = 0x00000000;      // nop
                        break;
                    case 0xD1:
                        // do next gs code if ram location is equal to 16-bit value
                        // D1XXYYYY 00ZZ
                        temp[0] = cp[2];
                        temp[1] = cp[3];
                        temp[2] = 0;
                        xv = strtol(temp, (char **)NULL, 16);
                        temp[0] = cp[4];
                        temp[1] = cp[5];
                        temp[2] = cp[6];
                        temp[3] = cp[7];
                        temp[4] = 0;
                        yv = strtol(temp, (char **)NULL, 16);
                        if (yv & 0x8000) xv++; // adjust for sign extension of yv
                        temp[0] = vp[0];
                        temp[1] = vp[1];
                        temp[2] = vp[2];
                        temp[3] = vp[3];
                        temp[4] = 0;
                        zv = strtol(temp, (char **)NULL, 16);
                        *codes++ = 0x3c1a8000 | xv; // lui  k0,0x80xx
                        *codes++ = 0x875a0000 | yv; // lh   k0,yyyy(k0)
                        *codes++ = 0x241b0000 | zv; // li   k1,zzzz
                        *codes++ = 0x175b0004;      // bne  k0,k1,4
                        *codes++ = 0x00000000;      // nop
                        break;
                    case 0xD2:
                        // do next gs code if ram location is not equal to 8-bit value
                        // D2XXYYYY 00ZZ
                        temp[0] = cp[2];
                        temp[1] = cp[3];
                        temp[2] = 0;
                        xv = strtol(temp, (char **)NULL, 16);
                        temp[0] = cp[4];
                        temp[1] = cp[5];
                        temp[2] = cp[6];
                        temp[3] = cp[7];
                        temp[4] = 0;
                        yv = strtol(temp, (char **)NULL, 16);
                        if (yv & 0x8000) xv++; // adjust for sign extension of yv
                        temp[0] = vp[2];
                        temp[1] = vp[3];
                        temp[2] = 0;
                        zv = strtol(temp, (char **)NULL, 16);
                        *codes++ = 0x3c1a8000 | xv; // lui  k0,0x80xx
                        *codes++ = 0x835a0000 | yv; // lb   k0,yyyy(k0)
                        *codes++ = 0x241b0000 | zv; // li   k1,00zz
                        *codes++ = 0x135b0004;      // beq  k0,k1,4
                        *codes++ = 0x00000000;      // nop
                        break;
                    case 0xD3:
                        // do next gs code if ram location is not equal to 16-bit value
                        // D3XXYYYY 00ZZ
                        temp[0] = cp[2];
                        temp[1] = cp[3];
                        temp[2] = 0;
                        xv = strtol(temp, (char **)NULL, 16);
                        temp[0] = cp[4];
                        temp[1] = cp[5];
                        temp[2] = cp[6];
                        temp[3] = cp[7];
                        temp[4] = 0;
                        yv = strtol(temp, (char **)NULL, 16);
                        if (yv & 0x8000) xv++; // adjust for sign extension of yv
                        temp[0] = vp[0];
                        temp[1] = vp[1];
                        temp[2] = vp[2];
                        temp[3] = vp[3];
                        temp[4] = 0;
                        zv = strtol(temp, (char **)NULL, 16);
                        *codes++ = 0x3c1a8000 | xv; // lui  k0,0x80xx
                        *codes++ = 0x875a0000 | yv; // lh   k0,yyyy(k0)
                        *codes++ = 0x241b0000 | zv; // li   k1,zzzz
                        *codes++ = 0x135b0004;      // beq  k0,k1,4
                        *codes++ = 0x00000000;      // nop
                        break;
                    case 0xDD:
                        // deactivate expansion ram using 2nd method
                        // DD000000 0000
                        if (cic_chip == CIC_6105)
                            *(vu32 *)0xA00003F0 = 0x00400000;
                        else
                            *(vu32 *)0xA0000318 = 0x00400000;
                        break;
                    case 0xDE:
                        // set game boot address
                        // DEXXXXXX 0000 => boot address = 800XXXXX, msn ignored
                        temp[0] = cp[2];
                        temp[1] = cp[3];
                        temp[2] = cp[4];
                        temp[3] = cp[5];
                        temp[4] = cp[6];
                        temp[5] = cp[7];
                        temp[6] = 0;
                        val = strtol(temp, (char **)NULL, 16);
                        bootAddr = 0xFFFFFFFF80000000LL | (val & 0xFFFFF);
                        break;
                    case 0xEE:
                        // deactivate expansion ram using 1st method
                        // EE000000 0000
                        if (cic_chip == CIC_6105)
                            *(vu32 *)0xA00003F0 = 0x00400000;
                        else
                            *(vu32 *)0xA0000318 = 0x00400000;
                        break;
                    case 0xF0:
                        // write 8-bit value to ram before boot
                        // F0XXXXXX 00YY
                        temp[0] = cp[2];
                        temp[1] = cp[3];
                        temp[2] = cp[4];
                        temp[3] = cp[5];
                        temp[4] = cp[6];
                        temp[5] = cp[7];
                        temp[6] = 0;
                        val = strtol(temp, (char **)NULL, 16);
                        val -= (bootAddr & 0xFFFFFF);
                        tp = (char *)(0xFFFFFFFFA02A0000LL + val);
                        if (gGSCodes[curr_cheat].mask)
                        {
                            val = gGSCodes[curr_cheat].value & gGSCodes[curr_cheat].mask;
                        }
                        else
                        {
                            temp[0] = vp[2];
                            temp[1] = vp[3];
                            temp[2] = 0;
                            val = strtol(temp, (char **)NULL, 16);
                        }
                        *tp = val & 0x00FF;
                        break;
                    case 0xF1:
                        // write 16-bit value to ram before boot
                        // F1XXXXXX YYYY
                        temp[0] = cp[2];
                        temp[1] = cp[3];
                        temp[2] = cp[4];
                        temp[3] = cp[5];
                        temp[4] = cp[6];
                        temp[5] = cp[7];
                        temp[6] = 0;
                        val = strtol(temp, (char **)NULL, 16);
                        val -= (bootAddr & 0xFFFFFF);
                        tp = (char *)(0xFFFFFFFFA02A0000LL + val);
                        if (gGSCodes[curr_cheat].mask)
                        {
                            val = gGSCodes[curr_cheat].value & gGSCodes[curr_cheat].mask;
                        }
                        else
                        {
                            temp[0] = vp[0];
                            temp[1] = vp[1];
                            temp[2] = vp[2];
                            temp[3] = vp[3];
                            temp[4] = 0;
                            val = strtol(temp, (char **)NULL, 16);
                        }
                        *tp++ = (val >> 8) & 0x00FF;
                        *tp = val & 0x00FF;
                        break;
                    case 0xFF:
                        // set code base
                        // FFXXXXXX 0000
                        temp[0] = cp[2];
                        temp[1] = cp[3];
                        temp[2] = cp[4];
                        temp[3] = cp[5];
                        temp[4] = cp[6];
                        temp[5] = cp[7];
                        temp[6] = 0;
                        val = strtol(temp, (char **)NULL, 16);
                        //codes = (vu32 *)(0xA0000000 | (val & 0xFFFFFF));
                        break;
                }
            }
            curr_cheat++;
        }

        // generate jump to moved general int handler
        *codes++ = 0x3c1a8000;  // lui  k0,0x8000
        *codes++ = 0x375a0120;  // ori  k0,k0,0x120
        *codes++ = 0x03400008;  // jr   k0
        *codes++ = 0x00000000;  // nop

        // flush general int handler memory
        data_cache_hit_writeback_invalidate((void *)0x80000120, 0x2E0);
        inst_cache_hit_invalidate((void *)0x80000120, 0x2E0);

        // flush os boot segment
        data_cache_hit_writeback_invalidate((void *)0x802A0000, 0x100000);

        // flush os boot segment memory
        data_cache_hit_writeback_invalidate((void *)bootAddr, 0x100000);
        inst_cache_hit_invalidate((void *)bootAddr, 0x100000);
    }

    // Copy low 0x1000 bytes to DMEM
    src = (vu32 *)0xB0000000;
    dst = (vu32 *)0xA4000000;
    for (ix=0; ix<(0x1000>>2); ix++)
        dst[ix] = src[ix];

    // Need to copy crap to IMEM for CIC-6105 boot.
    dst = (vu32 *)0xA4001000;

    // register values due to pif boot for CiC chip and country code, and IMEM crap
    gGPR[0]=0x0000000000000000LL;
    gGPR[6]=0xFFFFFFFFA4001F0CLL;
    gGPR[7]=0xFFFFFFFFA4001F08LL;
    gGPR[8]=0x00000000000000C0LL;
    gGPR[9]=0x0000000000000000LL;
    gGPR[10]=0x0000000000000040LL;
    gGPR[11]=bootAddr; // 0xFFFFFFFFA4000040LL;
    gGPR[16]=0x0000000000000000LL;
    gGPR[17]=0x0000000000000000LL;
    gGPR[18]=0x0000000000000000LL;
    gGPR[19]=0x0000000000000000LL;
    gGPR[21]=0x0000000000000000LL;
    gGPR[26]=0x0000000000000000LL;
    gGPR[27]=0x0000000000000000LL;
    gGPR[28]=0x0000000000000000LL;
    gGPR[29]=0xFFFFFFFFA4001FF0LL;
    gGPR[30]=0x0000000000000000LL;

    switch (country)
    {
        case 0x44: //Germany
        case 0x46: //french
        case 0x49: //Italian
        case 0x50: //Europe
        case 0x53: //Spanish
        case 0x55: //Australia
        case 0x58: // ????
        case 0x59: // X (PAL)
        {
            switch (cic_chip)
            {
                case CIC_6102:
                    gGPR[5]=0xFFFFFFFFC0F1D859LL;
                    gGPR[14]=0x000000002DE108EALL;
                    gGPR[24]=0x0000000000000000LL;
                    break;
                case CIC_6103:
                    gGPR[5]=0xFFFFFFFFD4646273LL;
                    gGPR[14]=0x000000001AF99984LL;
                    gGPR[24]=0x0000000000000000LL;
                    break;
                case CIC_6105:
                    dst[0x04>>2] = 0xBDA807FC;
                    gGPR[5]=0xFFFFFFFFDECAAAD1LL;
                    gGPR[14]=0x000000000CF85C13LL;
                    gGPR[24]=0x0000000000000002LL;
                    break;
                case CIC_6106:
                    gGPR[5]=0xFFFFFFFFB04DC903LL;
                    gGPR[14]=0x000000001AF99984LL;
                    gGPR[24]=0x0000000000000002LL;
                    break;
            }

            gGPR[20]=0x0000000000000000LL;
            gGPR[23]=0x0000000000000006LL;
            gGPR[31]=0xFFFFFFFFA4001554LL;
            break;
        }
        case 0x37: // 7 (Beta)
        case 0x41: // ????
        case 0x45: //USA
        case 0x4A: //Japan
        default:
        {
            switch (cic_chip)
            {
                case CIC_6102:
                    gGPR[5]=0xFFFFFFFFC95973D5LL;
                    gGPR[14]=0x000000002449A366LL;
                    break;
                case CIC_6103:
                    gGPR[5]=0xFFFFFFFF95315A28LL;
                    gGPR[14]=0x000000005BACA1DFLL;
                    break;
                case CIC_6105:
                    dst[0x04>>2] = 0x8DA807FC;
                    gGPR[5]=0x000000005493FB9ALL;
                    gGPR[14]=0xFFFFFFFFC2C20384LL;
                    break;
                case CIC_6106:
                    gGPR[5]=0xFFFFFFFFE067221FLL;
                    gGPR[14]=0x000000005CD2B70FLL;
                    break;
            }

            gGPR[20]=0x0000000000000001LL;
            gGPR[23]=0x0000000000000000LL;
            gGPR[24]=0x0000000000000003LL;
            gGPR[31]=0xFFFFFFFFA4001550LL;
            break;
        }
    }

    switch (cic_chip)
    {
        case CIC_6101:
            gGPR[22]=0x000000000000003FLL;
            break;
        case CIC_6102:
            gGPR[1]=0x0000000000000001LL;
            gGPR[2]=0x000000000EBDA536LL;
            gGPR[3]=0x000000000EBDA536LL;
            gGPR[4]=0x000000000000A536LL;
            gGPR[12]=0xFFFFFFFFED10D0B3LL;
            gGPR[13]=0x000000001402A4CCLL;
            gGPR[15]=0x000000003103E121LL;
            gGPR[22]=0x000000000000003FLL;
            gGPR[25]=0xFFFFFFFF9DEBB54FLL;
            break;
        case CIC_6103:
            gGPR[1]=0x0000000000000001LL;
            gGPR[2]=0x0000000049A5EE96LL;
            gGPR[3]=0x0000000049A5EE96LL;
            gGPR[4]=0x000000000000EE96LL;
            gGPR[12]=0xFFFFFFFFCE9DFBF7LL;
            gGPR[13]=0xFFFFFFFFCE9DFBF7LL;
            gGPR[15]=0x0000000018B63D28LL;
            gGPR[22]=0x0000000000000078LL;
            gGPR[25]=0xFFFFFFFF825B21C9LL;
            break;
        case CIC_6105:
            dst[0x00>>2] = 0x3C0DBFC0;
            dst[0x08>>2] = 0x25AD07C0;
            dst[0x0C>>2] = 0x31080080;
            dst[0x10>>2] = 0x5500FFFC;
            dst[0x14>>2] = 0x3C0DBFC0;
            dst[0x18>>2] = 0x8DA80024;
            dst[0x1C>>2] = 0x3C0BB000;
            gGPR[1]=0x0000000000000000LL;
            gGPR[2]=0xFFFFFFFFF58B0FBFLL;
            gGPR[3]=0xFFFFFFFFF58B0FBFLL;
            gGPR[4]=0x0000000000000FBFLL;
            gGPR[12]=0xFFFFFFFF9651F81ELL;
            gGPR[13]=0x000000002D42AAC5LL;
            gGPR[15]=0x0000000056584D60LL;
            gGPR[22]=0x0000000000000091LL;
            gGPR[25]=0xFFFFFFFFCDCE565FLL;
            break;
        case CIC_6106:
            gGPR[1]=0x0000000000000000LL;
            gGPR[2]=0xFFFFFFFFA95930A4LL;
            gGPR[3]=0xFFFFFFFFA95930A4LL;
            gGPR[4]=0x00000000000030A4LL;
            gGPR[12]=0xFFFFFFFFBCB59510LL;
            gGPR[13]=0xFFFFFFFFBCB59510LL;
            gGPR[15]=0x000000007A3C07F4LL;
            gGPR[22]=0x0000000000000085LL;
            gGPR[25]=0x00000000465E3F72LL;
            break;
    }


    // set HW registers
    PI_STATUS_REG = 3;
    switch (cart)
    {
        case 0x4258: // 'BX' - Battle Tanx
            PI_BSD_DOM1_LAT_REG = 0x00000080;
            PI_BSD_DOM1_RLS_REG = 0x00000037;
            PI_BSD_DOM1_PWD_REG = 0x00000012;
            PI_BSD_DOM1_PGS_REG = 0x00000040;
            break;
        default:
            break;
    }


    // now set MIPS registers - set CP0, and then GPRs, then jump thru gpr11 (which is usually 0xA400040)
    if (!gCheats)
        asm(".set noat\n\t"
            ".set noreorder\n\t"
            "li $8,0x34000000\n\t"
            "mtc0 $8,$12\n\t"
            "nop\n\t"
            "li $9,0x0006E463\n\t"
            "mtc0 $9,$16\n\t"
            "nop\n\t"
            "li $8,0x00005000\n\t"
            "mtc0 $8,$9\n\t"
            "nop\n\t"
            "li $9,0x0000005C\n\t"
            "mtc0 $9,$13\n\t"
            "nop\n\t"
            "li $8,0x007FFFF0\n\t"
            "mtc0 $8,$4\n\t"
            "nop\n\t"
            "li $9,0xFFFFFFFF\n\t"
            "mtc0 $9,$14\n\t"
            "nop\n\t"
            "mtc0 $9,$30\n\t"
            "nop\n\t"
            "lui $8,0\n\t"
            "mthi $8\n\t"
            "nop\n\t"
            "mtlo $8\n\t"
            "nop\n\t"
            "ctc1 $8,$31\n\t"
            "nop\n\t"
            "lui $31,0xA03E\n\t"
            "ld $1,0x08($31)\n\t"
            "ld $2,0x10($31)\n\t"
            "ld $3,0x18($31)\n\t"
            "ld $4,0x20($31)\n\t"
            "ld $5,0x28($31)\n\t"
            "ld $6,0x30($31)\n\t"
            "ld $7,0x38($31)\n\t"
            "ld $8,0x40($31)\n\t"
            "ld $9,0x48($31)\n\t"
            "ld $10,0x50($31)\n\t"
            "ld $11,0x58($31)\n\t"
            "ld $12,0x60($31)\n\t"
            "ld $13,0x68($31)\n\t"
            "ld $14,0x70($31)\n\t"
            "ld $15,0x78($31)\n\t"
            "ld $16,0x80($31)\n\t"
            "ld $17,0x88($31)\n\t"
            "ld $18,0x90($31)\n\t"
            "ld $19,0x98($31)\n\t"
            "ld $20,0xA0($31)\n\t"
            "ld $21,0xA8($31)\n\t"
            "ld $22,0xB0($31)\n\t"
            "ld $23,0xB8($31)\n\t"
            "ld $24,0xC0($31)\n\t"
            "ld $25,0xC8($31)\n\t"
            "ld $26,0xD0($31)\n\t"
            "ld $27,0xD8($31)\n\t"
            "ld $28,0xE0($31)\n\t"
            "ld $29,0xE8($31)\n\t"
            "ld $30,0xF0($31)\n\t"
            "ld $31,0xF8($31)\n\t"
            "jr $11\n\t"
            "nop"
            ::: "$8" );
    else
        asm(".set noreorder\n\t"
            "li $8,0x34000000\n\t"
            "mtc0 $8,$12\n\t"
            "nop\n\t"
            "li $9,0x0006E463\n\t"
            "mtc0 $9,$16\n\t"
            "nop\n\t"
            "li $8,0x00005000\n\t"
            "mtc0 $8,$9\n\t"
            "nop\n\t"
            "li $9,0x0000005C\n\t"
            "mtc0 $9,$13\n\t"
            "nop\n\t"
            "li $8,0x007FFFF0\n\t"
            "mtc0 $8,$4\n\t"
            "nop\n\t"
            "li $9,0xFFFFFFFF\n\t"
            "mtc0 $9,$14\n\t"
            "nop\n\t"
            "mtc0 $9,$30\n\t"
            "nop\n\t"
            "lui $8,0\n\t"
            "mthi $8\n\t"
            "nop\n\t"
            "mtlo $8\n\t"
            "nop\n\t"
            "ctc1 $8,$31\n\t"
            "nop\n\t"
            "li $9,0x00000183\n\t"
            "mtc0 $9,$18\n\t"
            "nop\n\t"
            "mtc0 $zero,$19\n\t"
            "nop\n\t"
            "lui $8,0xA03C\n\t"
            "la $9,2f\n\t"
            "la $10,9f\n\t"
            ".set noat\n"
            "1:\n\t"
            "lw $2,($9)\n\t"
            "sw $2,($8)\n\t"
            "addiu $8,$8,4\n\t"
            "addiu $9,$9,4\n\t"
            "bne $9,$10,1b\n\t"
            "nop\n\t"
            "lui $8,0xA03C\n\t"
            "jr $8\n\t"
            "nop\n"
            "2:\n\t"
            "lui $9,0xB000\n\t"
            "lw $9,8($9)\n\t"
            "lui $8,0x2000\n\t"
            "or $8,$8,$9\n\t"
            "lui $9,0xA02A\n\t"
            "lui $10,0xA03A\n\t"
            "3:\n\t"
            "lw $2,($9)\n\t"
            "sw $2,($8)\n\t"
            "addiu $8,$8,4\n\t"
            "addiu $9,$9,4\n\t"
            "bne $9,$10,3b\n\t"
            "nop\n\t"
            "lui $31,0xA03E\n\t"
            "ld $1,0x08($31)\n\t"
            "ld $2,0x10($31)\n\t"
            "ld $3,0x18($31)\n\t"
            "ld $4,0x20($31)\n\t"
            "ld $5,0x28($31)\n\t"
            "ld $6,0x30($31)\n\t"
            "ld $7,0x38($31)\n\t"
            "ld $8,0x40($31)\n\t"
            "ld $9,0x48($31)\n\t"
            "ld $10,0x50($31)\n\t"
            "ld $11,0x58($31)\n\t"
            "ld $12,0x60($31)\n\t"
            "ld $13,0x68($31)\n\t"
            "ld $14,0x70($31)\n\t"
            "ld $15,0x78($31)\n\t"
            "ld $16,0x80($31)\n\t"
            "ld $17,0x88($31)\n\t"
            "ld $18,0x90($31)\n\t"
            "ld $19,0x98($31)\n\t"
            "ld $20,0xA0($31)\n\t"
            "ld $21,0xA8($31)\n\t"
            "ld $22,0xB0($31)\n\t"
            "ld $23,0xB8($31)\n\t"
            "ld $24,0xC0($31)\n\t"
            "ld $25,0xC8($31)\n\t"
            "ld $26,0xD0($31)\n\t"
            "ld $27,0xD8($31)\n\t"
            "ld $28,0xE0($31)\n\t"
            "ld $29,0xE8($31)\n\t"
            "ld $30,0xF0($31)\n\t"
            "ld $31,0xF8($31)\n\t"
            "jr $11\n\t"
            "nop\n"
            "9:\n"
            ::: "$8" );
}

/*
Address 0x1D0000 arrangement:
every 64 bytes is a item for one game, inside the 64 bytes:
[0] : game flag, 0xFF-> game's item,  0x00->end item
[1] : high byte of rom bank
[2] : low byte of rom bank
[3] : high byte of rom size
[4] : low byte of rom size
[5] : save value, which range from 0x00~0x06
[6] : game cic value, which range from 0x00~0x06
[7] : game mode value, which range from 0x00~0x0F
[8~63] : game name string

about the word of rombank and romsize:
  0M ROM_BANK   0 0 0 0  -> 0x0000
 64M ROM_BANK   0 0 0 1  -> 0x0001
128M ROM_BANK   0 0 1 0  -> 0x0002
256M ROM_BANK   0 1 0 0  -> 0x0004
512M ROM_BANK   1 0 0 0  -> 0x0008
  1G ROM_BANK   0 0 0 0  -> 0x0000 // only when ROM SIZE also que to zero

 64M ROM_SIZE   1 1 1 1  -> 0x000F
128M ROM_SIZE   1 1 1 0  -> 0x000E
256M ROM_SIZE   1 1 0 0  -> 0x000C
512M ROM_SIZE   1 0 0 0  -> 0x0008
  1G ROM_SIZE   0 0 0 0  -> 0x0000
*/


#define N64_MENU_ENTRY  (0xB0000000 + 0x1D0000)


void neo_run_menu(void)
{
    u32 mythaware;

    neo_select_menu();                  // select menu flash

    // set myth hw for selected rom
    ROM_BANK = 0x00000000;
    neo_sync_bus();
    ROM_SIZE = 0x000F000F;
    neo_sync_bus();
    SAVE_IO  = 0x00080008;
    neo_sync_bus();
    CIC_IO   = 0x00020002;
    neo_sync_bus();
    RST_IO   = 0xFFFFFFFF;
    neo_sync_bus();
    mythaware = !memcmp((void *)0xB0000020, "N64 Myth", 8);
    RUN_IO   = mythaware ? 0x00000000 : 0xFFFFFFFF;
    neo_sync_bus();

    // start cart
    disable_interrupts();
    simulate_pif_boot(2);               // should never return
    enable_interrupts();
}

void neo_run_game(u8 *option, int reset)
{
    u32 rombank=0;
    u32 romsize=0;
    u32 romsave=0;
    u32 romcic=0;
    u32 rommode=0;
    u32 gamelen;
    u32 runcic;
    u32 mythaware;

    rombank = (option[1]<<8) + option[2];
    if (rombank > 16)
        rombank = rombank / 64;
    rombank += (rombank<<16);

    romsize = (option[3]<<8) + option[4];
    romsize += (romsize<<16);

    romsave = (option[5]<<16) + option[5];
    romcic  = (option[6]<<16) + option[6];
    rommode = (option[7]<<16) + option[7];

    if ((romsize & 0xFFFF) > 0x000F)
    {
        // romsize is number of Mbits in rom
        gamelen = (romsize & 0xFFFF)*128*1024;
        if (gamelen <= (8*1024*1024))
            romsize = 0x000F000F;
        else if (gamelen <= (16*1024*1024))
            romsize = 0x000E000E;
        else if (gamelen <= (32*1024*1024))
            romsize = 0x000C000C;
        else if (gamelen <= (64*1024*1024))
            romsize = 0x00080008;
        else
            romsize = 0x00000000;
    }

    neo_select_game();                  // select game flash

    // set myth hw for selected rom
    ROM_BANK = rombank;
    neo_sync_bus();
    ROM_SIZE = romsize;
    neo_sync_bus();
    SAVE_IO  = romsave;
    neo_sync_bus();
    // if set to use card cic, figure out cic for simulated start
    if (romcic == 0)
        runcic = get_cic((unsigned char *)0xB0000040);
    else
        runcic = romcic & 7;
    if (!reset && (runcic != 2))
        reset = 1;                      // reset back to menu since cannot reset to game in hardware
    CIC_IO   = romcic; //reset ? 0x00020002 : romcic;
    neo_sync_bus();
    RST_IO = reset ? 0xFFFFFFFF : 0x00000000;
    neo_sync_bus();
    mythaware = !memcmp((void *)0xB0000020, "N64 Myth", 8);
    RUN_IO   = mythaware ? 0x00000000 : 0xFFFFFFFF;
    neo_sync_bus();

    // start cart
    disable_interrupts();
    simulate_pif_boot(runcic);          // should never return
    enable_interrupts();
}

void neo_run_psram(u8 *option, int reset)
{
    u32 romsize=0;
    u32 romsave=0;
    u32 romcic=0;
    u32 rommode=0;
    u32 gamelen;
    u32 runcic;
    u32 mythaware;
    int wait;

    romsize = (option[3]<<8) + option[4];
    romsize += (romsize<<16);

    romsave = (option[5]<<16) + option[5];
    romcic  = (option[6]<<16) + option[6];
    rommode = (option[7]<<16) + option[7];
    gamelen = (romsize & 0xFFFF)*128*1024;

    if(gamelen > (32*1024*1024))
        romsize = 0x00000000;               //extended mode(WIP)
    else if ((romsize & 0xFFFF) > 0x000F)
    {
        if (gamelen <= (8*1024*1024))
            romsize = 0x000F000F;
        else if (gamelen <= (16*1024*1024))
            romsize = 0x000E000E;
        else if (gamelen <= (32*1024*1024))
            romsize = 0x000C000C;
        else if (gamelen <= (64*1024*1024))
            romsize = 0x00080008;
        else
            romsize = 0x00000000;
    }

    neo_mode = SD_OFF;                  // make sure SD card interface is disabled
    neo_select_psram();                 // select gba psram
    _neo_asic_cmd(0x00E2D200, 1);       // GBA CARD WE OFF !

    // set myth hardware for selected rom
    ROMSW_IO = 0xFFFFFFFF;              // gba mapped to 0xB0000000 - 0xB3FFFFFF
    neo_sync_bus();
    ROM_BANK = 0x00000000;
    neo_sync_bus();
    ROM_SIZE = romsize;
    neo_sync_bus();
    SAVE_IO  = romsave;
    neo_sync_bus();

    // if set to use card cic, figure out cic for simulated start
    if (romcic == 0)
    {
        runcic = get_cic((unsigned char *)0xB0000040);
    }
    else
    {
        runcic = romcic & 7;
    }

    if((!reset) && (runcic != 2))
    {
        reset = 1;                      // reset back to menu since cannot reset to game in hardware
    }

    CIC_IO   = romcic; //reset ? 0x00020002 : romcic;
    neo_sync_bus();
    RST_IO = reset ? 0xFFFFFFFF : 0x00000000;
    neo_sync_bus();
    mythaware = !memcmp((void *)0xB0000020, "N64 Myth", 8);
    RUN_IO   = mythaware ? 0x00000000 : 0xFFFFFFFF;
    neo_sync_bus();

    // start cart
    while (dma_busy()){} //sanity check

    disable_interrupts();

    for(wait = 0;wait < 200;wait++)
    {
        //Just waste a little time until any interrupts that have been triggered BEFORE ints where disabled have jumped back to their caller
        neo_sync_bus();
    }

    simulate_pif_boot(runcic);          // should never return
    enable_interrupts();
}

