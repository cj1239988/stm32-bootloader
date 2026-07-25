#include <stdint.h>
#include "console.h"
#include "stm32f4xx.h"
#include "tim_delay.h"
#include "elog.h"
//0x801000->sp
//0x801004->pc
//SCB->VTOR
//E000 ED00

#define APP_BASE_ADDRESS    0x08010000

extern void board_lowlevel_init(void);
extern void bootloader_main(void);
int main(void)
{
    board_lowlevel_init();
    console_init();
    tim_delay_init();

#ifdef ELOG_OUTPUT_ENABLE
    elog_init();
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_P_INFO);
    elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_P_INFO);
    elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_VERBOSE, ELOG_FMT_TAG);
    elog_start();
#endif
    bootloader_main();
    // extern void JumpApp(uint32_t base);
    // JumpApp(APP_BASE_ADDRESS);

    return 0;
}


