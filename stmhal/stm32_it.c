/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * Original template from ST Cube library.  See below for header.
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
  ******************************************************************************
  * @file    Templates/Src/stm32f4xx_it.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    26-February-2014
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2014 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

#include <stdio.h>

#include "stm32_it.h"
#include STM32_HAL_H

#include "py/obj.h"
#include "pendsv.h"
#include "extint.h"
#include "timer.h"
#include "uart.h"
#include "storage.h"
#include "can.h"
#include "dma.h"

extern void __fatal_error(const char*);
extern PCD_HandleTypeDef pcd_handle;

/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

// Set the following to 1 to get some more information on the Hard Fault
// More information about decoding the fault registers can be found here:
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0646a/Cihdjcfc.html
#define REPORT_HARD_FAULT_REGS  0

#if REPORT_HARD_FAULT_REGS

#include "py/mphal.h"

STATIC char *fmt_hex(uint32_t val, char *buf) {
    const char *hexDig = "0123456789abcdef";

    buf[0] = hexDig[(val >> 28) & 0x0f];
    buf[1] = hexDig[(val >> 24) & 0x0f];
    buf[2] = hexDig[(val >> 20) & 0x0f];
    buf[3] = hexDig[(val >> 16) & 0x0f];
    buf[4] = hexDig[(val >> 12) & 0x0f];
    buf[5] = hexDig[(val >>  8) & 0x0f];
    buf[6] = hexDig[(val >>  4) & 0x0f];
    buf[7] = hexDig[(val >>  0) & 0x0f];
    buf[8] = '\0';

    return buf;
}

STATIC void print_reg(const char *label, uint32_t val) {
    char hexStr[9];

    mp_hal_stdout_tx_str(label);
    mp_hal_stdout_tx_str(fmt_hex(val, hexStr));
    mp_hal_stdout_tx_str("\r\n");
}

// The ARMv7M Architecture manual (section B.1.5.6) says that upon entry
// to an exception, that the registers will be in the following order on the
// // stack: R0, R1, R2, R3, R12, LR, PC, XPSR

typedef struct {
    uint32_t    r0, r1, r2, r3, r12, lr, pc, xpsr;
} ExceptionRegisters_t;

void HardFault_C_Handler(ExceptionRegisters_t *regs) {
    print_reg("R0    ", regs->r0);
    print_reg("R1    ", regs->r1);
    print_reg("R2    ", regs->r2);
    print_reg("R3    ", regs->r3);
    print_reg("R12   ", regs->r12);
    print_reg("LR    ", regs->lr);
    print_reg("PC    ", regs->pc);
    print_reg("XPSR  ", regs->xpsr);

    uint32_t cfsr = SCB->CFSR;

    print_reg("HFSR  ", SCB->HFSR);
    print_reg("CFSR  ", cfsr);
    if (cfsr & 0x80) {
        print_reg("MMFAR ", SCB->MMFAR);
    }
    if (cfsr & 0x8000) {
        print_reg("BFAR  ", SCB->BFAR);
    }
    /* Go to infinite loop when Hard Fault exception occurs */
    while (1) {
        __fatal_error("HardFault");
    }
}

// Naked functions have no compiler generated gunk, so are the best thing to
// use for asm functions.
__attribute__((naked))
void HardFault_Handler(void) {

    // From the ARMv7M Architecture Reference Manual, section B.1.5.6
    // on entry to the Exception, the LR register contains, amongst other
    // things, the value of CONTROL.SPSEL. This can be found in bit 3.
    //
    // If CONTROL.SPSEL is 0, then the exception was stacked up using the
    // main stack pointer (aka MSP). If CONTROL.SPSEL is 1, then the exception
    // was stacked up using the process stack pointer (aka PSP).

    __asm volatile(
    " tst lr, #4    \n"         // Test Bit 3 to see which stack pointer we should use.
    " ite eq        \n"         // Tell the assembler that the nest 2 instructions are if-then-else
    " mrseq r0, msp \n"         // Make R0 point to main stack pointer
    " mrsne r0, psp \n"         // Make R0 point to process stack pointer
    " b HardFault_C_Handler \n" // Off to C land
    );
}
#else
void HardFault_Handler(void) {
    /* Go to infinite loop when Hard Fault exception occurs */
    while (1) {
        __fatal_error("HardFault");
    }
}
#endif // REPORT_HARD_FAULT_REGS

/**
  * @brief   This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void) {
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void) {
    /* Go to infinite loop when Memory Manage exception occurs */
    while (1) {
        __fatal_error("MemManage");
    }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void) {
    /* Go to infinite loop when Bus Fault exception occurs */
    while (1) {
        __fatal_error("BusFault");
    }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void) {
    /* Go to infinite loop when Usage Fault exception occurs */
    while (1) {
        __fatal_error("UsageFault");
    }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void) {
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void) {
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void) {
    pendsv_isr_handler();
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void) {
    // Instead of calling HAL_IncTick we do the increment here of the counter.
    // This is purely for efficiency, since SysTick is called 1000 times per
    // second at the highest interrupt priority.
    // Note: we don't need uwTick to be declared volatile here because this is
    // the only place where it can be modified, and the code is more efficient
    // without the volatile specifier.
    extern uint32_t uwTick;
    uwTick += 1;

    // Read the systick control regster. This has the side effect of clearing
    // the COUNTFLAG bit, which makes the logic in sys_tick_get_microseconds
    // work properly.
    SysTick->CTRL;

    // Right now we have the storage and DMA controllers to process during
    // this interrupt and we use custom dispatch handlers.  If this needs to
    // be generalised in the future then a dispatch table can be used as
    // follows: ((void(*)(void))(systick_dispatch[uwTick & 0xf]))();

    if (STORAGE_IDLE_TICK(uwTick)) {
        NVIC->STIR = FLASH_IRQn;
    }

    if (DMA_IDLE_ENABLED() && DMA_IDLE_TICK(uwTick)) {
        dma_idle_handler(uwTick);
    }
}

/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f4xx.s).                                               */
/******************************************************************************/

/**
  * @brief  This function handles USB-On-The-Go FS global interrupt request.
  * @param  None
  * @retval None
  */
#if defined(USE_USB_FS)
#define OTG_XX_IRQHandler      OTG_FS_IRQHandler
#define OTG_XX_WKUP_IRQHandler OTG_FS_WKUP_IRQHandler
#elif defined(USE_USB_HS)
#define OTG_XX_IRQHandler      OTG_HS_IRQHandler
#define OTG_XX_WKUP_IRQHandler OTG_HS_WKUP_IRQHandler
#endif

#if defined(OTG_XX_IRQHandler)
void OTG_XX_IRQHandler(void) {
    HAL_PCD_IRQHandler(&pcd_handle);
}
#endif

/**
  * @brief  This function handles USB OTG FS or HS Wakeup IRQ Handler.
  * @param  None
  * @retval None
  */
#if defined(OTG_XX_WKUP_IRQHandler)
void OTG_XX_WKUP_IRQHandler(void) {

  if ((&pcd_handle)->Init.low_power_enable) {
    /* Reset SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR &= (uint32_t)~((uint32_t)(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk));

    /* Configures system clock after wake-up from STOP: enable HSE, PLL and select
    PLL as system clock source (HSE and PLL are disabled in STOP mode) */

    __HAL_RCC_HSE_CONFIG(RCC_HSE_ON);

    /* Wait till HSE is ready */
    while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) == RESET)
    {}

    /* Enable the main PLL. */
    __HAL_RCC_PLL_ENABLE();

    /* Wait till PLL is ready */
    while(__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) == RESET)
    {}

    /* Select PLL as SYSCLK */
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_SYSCLKSOURCE_PLLCLK);

    while (__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_CFGR_SWS_PLL)
    {}

    /* ungate PHY clock */
     __HAL_PCD_UNGATE_PHYCLOCK((&pcd_handle));
  }
#ifdef USE_USB_FS
  /* Clear EXTI pending Bit*/
  __HAL_USB_FS_EXTI_CLEAR_FLAG();
#elif defined(USE_USB_HS)
    /* Clear EXTI pending Bit*/
  __HAL_USB_HS_EXTI_CLEAR_FLAG();
#endif

}
#endif

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

// Handle a flash (erase/program) interrupt.
void FLASH_IRQHandler(void) {
    // This calls the real flash IRQ handler, if needed
    /*
    uint32_t flash_cr = FLASH->CR;
    if ((flash_cr & FLASH_IT_EOP) || (flash_cr & FLASH_IT_ERR)) {
        HAL_FLASH_IRQHandler();
    }
    */
    // This call the storage IRQ handler, to check if the flash cache needs flushing
    storage_irq_handler();
}

/**
  * @brief  These functions handle the EXTI interrupt requests.
  * @param  None
  * @retval None
  */
void EXTI0_IRQHandler(void) {
    Handle_EXTI_Irq(0);
}

void EXTI1_IRQHandler(void) {
    Handle_EXTI_Irq(1);
}

void EXTI2_IRQHandler(void) {
    Handle_EXTI_Irq(2);
}

void EXTI3_IRQHandler(void) {
    Handle_EXTI_Irq(3);
}

void EXTI4_IRQHandler(void) {
    Handle_EXTI_Irq(4);
}

void EXTI9_5_IRQHandler(void) {
    Handle_EXTI_Irq(5);
    Handle_EXTI_Irq(6);
    Handle_EXTI_Irq(7);
    Handle_EXTI_Irq(8);
    Handle_EXTI_Irq(9);
}

void EXTI15_10_IRQHandler(void) {
    Handle_EXTI_Irq(10);
    Handle_EXTI_Irq(11);
    Handle_EXTI_Irq(12);
    Handle_EXTI_Irq(13);
    Handle_EXTI_Irq(14);
    Handle_EXTI_Irq(15);
}

void PVD_IRQHandler(void) {
    Handle_EXTI_Irq(EXTI_PVD_OUTPUT);
}

void RTC_Alarm_IRQHandler(void) {
    Handle_EXTI_Irq(EXTI_RTC_ALARM);
}

#if defined(ETH)    // The 407 has ETH, the 405 doesn't
void ETH_WKUP_IRQHandler(void)  {
    Handle_EXTI_Irq(EXTI_ETH_WAKEUP);
}
#endif

void TAMP_STAMP_IRQHandler(void) {
    Handle_EXTI_Irq(EXTI_RTC_TIMESTAMP);
}

void RTC_WKUP_IRQHandler(void) {
    RTC->ISR &= ~(1 << 10); // clear wakeup interrupt flag
    Handle_EXTI_Irq(EXTI_RTC_WAKEUP); // clear EXTI flag and execute optional callback
}

void TIM1_BRK_TIM9_IRQHandler(void) {
    timer_irq_handler(9);
}

void TIM1_UP_TIM10_IRQHandler(void) {
    timer_irq_handler(1);
    timer_irq_handler(10);
}

void TIM1_TRG_COM_TIM11_IRQHandler(void) {
    timer_irq_handler(11);
}

void TIM2_IRQHandler(void) {
    timer_irq_handler(2);
}

void TIM3_IRQHandler(void) {
    HAL_TIM_IRQHandler(&TIM3_Handle);
}

void TIM4_IRQHandler(void) {
    timer_irq_handler(4);
}

void TIM5_IRQHandler(void) {
    timer_irq_handler(5);
    HAL_TIM_IRQHandler(&TIM5_Handle);
}

void TIM6_DAC_IRQHandler(void) {
    timer_irq_handler(6);
}

void TIM7_IRQHandler(void) {
    timer_irq_handler(7);
}

void TIM8_BRK_TIM12_IRQHandler(void) {
    timer_irq_handler(12);
}

void TIM8_UP_TIM13_IRQHandler(void) {
    timer_irq_handler(8);
    timer_irq_handler(13);
}

void TIM8_TRG_COM_TIM14_IRQHandler(void) {
    timer_irq_handler(14);
}

// UART/USART IRQ handlers
void USART1_IRQHandler(void) {
    uart_irq_handler(1);
}

void USART2_IRQHandler(void) {
    uart_irq_handler(2);
}

void USART3_IRQHandler(void) {
    uart_irq_handler(3);
}

void UART4_IRQHandler(void) {
    uart_irq_handler(4);
}

void UART5_IRQHandler(void) {
    uart_irq_handler(5);
}

void USART6_IRQHandler(void) {
    uart_irq_handler(6);
}

#if MICROPY_HW_ENABLE_CAN
void CAN1_RX0_IRQHandler(void) {
    can_rx_irq_handler(PYB_CAN_1, CAN_FIFO0);
}

void CAN1_RX1_IRQHandler(void) {
    can_rx_irq_handler(PYB_CAN_1, CAN_FIFO1);
}

void CAN2_RX0_IRQHandler(void) {
    can_rx_irq_handler(PYB_CAN_2, CAN_FIFO0);
}

void CAN2_RX1_IRQHandler(void) {
    can_rx_irq_handler(PYB_CAN_2, CAN_FIFO1);
}
#endif // MICROPY_HW_ENABLE_CAN
