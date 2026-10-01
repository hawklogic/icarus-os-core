/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "bsp/config.h"

#include "stm32h7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include "icarus/config.h"
#include "icarus/svc.h"
#include "bsp/retained_diag.h"
#ifndef HOST_TEST
#include "st7735.h"
#include "lcd.h"
#endif
#ifdef HOST_TEST
// For host testing, include mock header for os_yield_pendsv
#include "mock_asm.h"
#endif
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ITCM_FUNC and ITCM_FUNC are defined in icarus/config.h */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

#ifndef HOST_TEST
void retained_diag_terminal_entry(uint32_t raw_msp, uint32_t raw_psp,
                                  uint32_t exc_return, uint32_t kind)
    __attribute__((naked, noreturn));
void retained_diag_memmanage_recoverable(uint32_t raw_msp,
                                         uint32_t raw_psp,
                                         uint32_t exc_return,
                                         uint32_t cfsr);
/* Separate from every task stack. ARM stack budget must be checked from the
 * final writer .su/call graph before target use. The first nested trap spins
 * without touching this stack again. */
#define RETAINED_DIAG_EMERGENCY_BYTES 1024u
__attribute__((used, aligned(8))) uint32_t
    retained_diag_emergency_stack[RETAINED_DIAG_EMERGENCY_BYTES / 4u];
volatile uint32_t retained_diag_trap_active;

/* r0/r1/r2 already contain raw MSP/PSP/EXC_RETURN, r3 the fault kind.
 * No compiler prologue, stack access, SVC, HAL, FP or watchdog operation. */
__attribute__((naked, noreturn)) void retained_diag_terminal_entry(
    uint32_t raw_msp __attribute__((unused)),
    uint32_t raw_psp __attribute__((unused)),
    uint32_t exc_return __attribute__((unused)),
    uint32_t kind __attribute__((unused)))
{
  __asm__ volatile (
    "ldr r12, =retained_diag_trap_active\n"
    "movs r4, #1\n"
    "2: ldrex r5, [r12]\n"
    "cmp r5, #0\n"
    "bne 1f\n"
    "strex r5, r4, [r12]\n"
    "cmp r5, #0\n"
    "bne 2b\n"
    "dmb\n"
    "ldr r12, =retained_diag_emergency_stack + 1024\n"
    "msr msp, r12\n"
    "bl retained_diag_capture_terminal\n"
    "1: b 1b\n"
  );
}
#endif

/* External variables --------------------------------------------------------*/
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
#ifndef HOST_TEST
__attribute__((naked, noreturn)) void HardFault_Handler(void)
{
  __asm__ volatile (
    "mrs r0, msp\n"
    "mrs r1, psp\n"
    "mov r2, lr\n"
    "movs r3, #1\n"
    "b retained_diag_terminal_entry\n"
  );
}
#else
void HardFault_Handler(void) { while (1) {} }
#endif

/* Incremented by recoverable MemManage faults */
volatile uint32_t g_memmanage_fault_count = 0;

/* Debug: capture last fault address */
volatile uint32_t g_last_fault_addr = 0;
volatile uint32_t g_last_fault_pc = 0;

/**
 * @brief  Number of recovered MemManage faults since boot.
 * @details Each one is an unprivileged access the MPU rejected and the
 *          handler skipped; the faulting task read garbage or lost a
 *          write.  Applications should telemeter this and treat a non-zero
 *          value as a fault.
 */
uint32_t os_get_memmanage_fault_count(void)
{
  return g_memmanage_fault_count;
}

/**
  * @brief This function handles Memory management fault.
  *
  * @details If the fault is a recoverable data access violation (DACCVIOL)
  *          from unprivileged thread mode (PSP), we advance the stacked PC
  *          by 2 bytes (Thumb instruction) and return so the task continues.
  *          The task can then check g_memmanage_fault_count to detect it.
  *
  *          If the fault is not recoverable (instruction fetch, or from MSP),
  *          enter the terminal retained capture and halt.
  */
#ifndef HOST_TEST
/* Branch from the naked veneer with original EXC_RETURN still in LR. Only
 * the pre-existing DACCVIOL/!IACCVIOL/PSP case reaches this function. */
__attribute__((used, noinline)) void retained_diag_memmanage_recoverable(
    uint32_t raw_msp, uint32_t raw_psp, uint32_t exc_return,
    uint32_t cfsr)
{
  if (retained_diag_memmanage_should_terminate(g_memmanage_fault_count)) {
    g_memmanage_fault_count++;
    retained_diag_terminal_entry(raw_msp, raw_psp, exc_return,
                                 RETAINED_DIAG_KIND_MEMORY);
  }
  uint32_t *sp = (uint32_t *)(uintptr_t)raw_psp;
  if ((cfsr & SCB_CFSR_MMARVALID_Msk) != 0u) {
    g_last_fault_addr = SCB->MMFAR;
  } else {
    g_last_fault_addr = 0xFFFFFFFFu;
  }
  g_last_fault_pc = sp[6];
  /* Preserve the existing Thumb instruction-skip and fault-count behavior. */
  uint16_t instr = *(const uint16_t *)(uintptr_t)sp[6];
  uint8_t instr_len = ((instr & 0xF800u) >= 0xE800u) ? 4u : 2u;
  sp[6] += instr_len;
  SCB->CFSR = cfsr;
  g_memmanage_fault_count++;
}

__attribute__((naked)) void MemManage_Handler(void)
{
  __asm__ volatile (
    "mrs r0, msp\n"
    "mrs r1, psp\n"
    "mov r2, lr\n"
    "ldr r12, =0xE000ED28\n" /* SCB->CFSR */
    "ldr r12, [r12]\n"
    "tst r12, #2\n"         /* DACCVIOL */
    "beq 1f\n"
    "tst r12, #1\n"         /* IACCVIOL */
    "bne 1f\n"
    "tst r2, #4\n"          /* PSP, as in original handler */
    "beq 1f\n"
    "mov r3, r12\n"         /* Preserve the status read used for recovery */
    "b retained_diag_memmanage_recoverable\n"
    "1: movs r3, #2\n"
    "b retained_diag_terminal_entry\n"
  );
}
#else
void MemManage_Handler(void) { while (1) {} }
#endif

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
#ifndef HOST_TEST
__attribute__((naked, noreturn)) void BusFault_Handler(void)
{
  __asm__ volatile (
    "mrs r0, msp\n"
    "mrs r1, psp\n"
    "mov r2, lr\n"
    "movs r3, #3\n"
    "b retained_diag_terminal_entry\n"
  );
}
#else
void BusFault_Handler(void) { while (1) {} }
#endif

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
#ifndef HOST_TEST
__attribute__((naked, noreturn)) void UsageFault_Handler(void)
{
  __asm__ volatile (
    "mrs r0, msp\n"
    "mrs r1, psp\n"
    "mov r2, lr\n"
    "movs r3, #4\n"
    "b retained_diag_terminal_entry\n"
  );
}
#else
void UsageFault_Handler(void) { while (1) {} }
#endif

/**
  * @brief This function handles System service call via SWI instruction.
  */
#ifdef HOST_TEST
void SVC_Handler(void)
{
  /* Empty for host testing */
}
#else
ITCM_FUNC __attribute__((naked)) void SVC_Handler(void)
{
  __asm__ volatile (
    "tst lr, #4\n"
    "ite eq\n"
    "mrseq r0, msp\n"
    "mrsne r0, psp\n"
    "b SVC_Handler_C\n"
  );
}
#endif

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
#ifdef HOST_TEST
// For host testing, use regular function (not naked) to call mock
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */
  os_yield_pendsv();
  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}
#else
ITCM_FUNC __attribute__ ((naked)) void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */
  __asm__ volatile (
    "b os_yield_pendsv"
  );
  
  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}
#endif

/**
  * @brief This function handles System tick timer.
  * @note  Must run in privileged mode to write to os_tick_count in DTCM
  */
ITCM_FUNC void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */
  extern volatile uint32_t os_tick_count;
  extern volatile uint8_t os_running;
  extern volatile uint32_t current_task_ticks_remaining;
  extern volatile bool scheduler_enabled;
  
  os_tick_count++;

  /* Count the slice down, saturating at zero.  If the slice expires inside
   * a critical section (scheduler disabled) the switch is deferred to the
   * first tick after the section ends.  Decrementing unconditionally here
   * used to wrap the counter to 0xFFFFFFFF, so that task was never
   * preempted again until it yielded on its own. */
  if ((os_running != 0u) && (current_task_ticks_remaining > 0u)) {
    current_task_ticks_remaining--;
  }
  if ((os_running != 0u) && (current_task_ticks_remaining == 0u) && (scheduler_enabled)) {
    current_task_ticks_remaining = ICARUS_TICKS_PER_TASK;  // Reset for next task
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
  }
  
  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32H7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles USB On The Go FS global interrupt.
  */
void OTG_FS_IRQHandler(void)
{
  /* USER CODE BEGIN OTG_FS_IRQn 0 */

  /* USER CODE END OTG_FS_IRQn 0 */
  HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
  /* USER CODE BEGIN OTG_FS_IRQn 1 */

  /* USER CODE END OTG_FS_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
