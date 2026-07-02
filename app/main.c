#include <stdint.h>
#include "console.h"
#include "stm32f4xx.h"
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
    bootloader_main();

    // extern void JumpApp(uint32_t base);
    // JumpApp(APP_BASE_ADDRESS);

    return 0;
}


