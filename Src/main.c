/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include <stdio.h>
#include <string.h>

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);


/* Function processing DMA Rx data. Counts how many capital and small letters are in sentence.
 * Result is supposed to be stored in global variable of type "letter_count_" that is defined in "main.h"
 *
 * @param1 - received sign
 */
void proccesDmaData(uint8_t sign);


/* Space for your global variables. */

#define STRING_START_SIGN	'#'
#define STRING_END_SIGN		'$'
#define STRING_MAX_SIGNS	35U			//max. number of signs after "#" (including "$")
#define BUFFER_INFO_PERIOD_MS	2000U		//0.5 Hz

/* Result of the last valid string (small and capital letters) */
letter_count_ letterCount = {0, 0};

/* State of string reception (used only in proccesDmaData) */
static uint8_t stringReceiving = 0;					//1 = "#" was received, waiting for "$"
static uint8_t stringSignCount = 0;					//number of signs received after "#"
static char stringBuffer[STRING_MAX_SIGNS];			//signs between "#" and "$"
static letter_count_ stringLetters = {0, 0};		//letters counted in current string

/* Last valid string - written in USART interrupt (callback), read in main loop */
static char validString[STRING_MAX_SIGNS];
static volatile uint8_t validStringReady = 0;

/* Buffer for USART2 DMA transmission */
static uint8_t txBuffer[128];

static void USART2_SendString(const char *text);


int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  /* Configure the system clock */
  SystemClock_Config();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();

  /* Space for your local variables, callback registration ...*/

  uint32_t ms_counter = 0;								//ms since last buffer info message
  char message[128];
  char string_copy[STRING_MAX_SIGNS];
  letter_count_ letters_copy;
  uint16_t occupied;
  uint32_t load_x100;									//load in % * 100 (2 decimal places)

  /* register function processing received data as callback for USART2 */
  USART2_RegisterCallback(proccesDmaData);

  while (1)
  {
	  /* Periodic transmission of information about DMA Rx buffer state.
	   * Transmission frequency - 0.5Hz.
	   * Message format - "Buffer capacity: %d bytes, occupied memory: %d bytes, load [in %]: %f%"
	   * Example message (what I wish to see in terminal) - Buffer capacity: 1000 bytes, occupied memory: 231 bytes, load [in %]: 23.1%
	   */

	  /* Valid text string information transmission.
	   * Transmission frequency - when new valid string is received.
	   * Message format - "Valid string: %s, lower-case: %d, upper-case: %d"
	   * Example message (what I wish to see in terminal) - Valid string: Platn15uborZnakov, lower-case: 13, upper-case: 2
	   */

	  /* SysTick counts 1 ms - COUNTFLAG is set every 1 ms and cleared by reading (no interrupt needed) */
	  if(LL_SYSTICK_IsActiveCounterFlag())
	  {
		  ms_counter++;
	  }

	  /* periodic buffer state information, 0.5 Hz */
	  if(ms_counter >= BUFFER_INFO_PERIOD_MS)
	  {
		  ms_counter = 0;

		  occupied = USART2_GetDmaRxOccupied();
		  load_x100 = ((uint32_t)occupied * 10000U) / DMA_USART2_BUFFER_SIZE;

		  sprintf(message, "Buffer capacity: %d bytes, occupied memory: %d bytes, load [in %%]: %u.%02u%%\r\n",
				  DMA_USART2_BUFFER_SIZE, occupied, (unsigned int)(load_x100 / 100U), (unsigned int)(load_x100 % 100U));
		  USART2_SendString(message);
	  }

	  /* new valid string was received */
	  if(validStringReady)
	  {
		  /* copy data written in interrupt (interrupts disabled for a moment) */
		  __disable_irq();
		  strcpy(string_copy, validString);
		  letters_copy = letterCount;
		  validStringReady = 0;
		  __enable_irq();

		  sprintf(message, "Valid string: %s, lower-case: %d, upper-case: %d\r\n",
				  string_copy, letters_copy.small_letter, letters_copy.capital_letter);
		  USART2_SendString(message);
	  }
  }
  /* USER CODE END 3 */
}


void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);

  if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0)
  {
  Error_Handler();  
  }
  LL_RCC_HSI_Enable();

   /* Wait till HSI is ready */
  while(LL_RCC_HSI_IsReady() != 1)
  {
    
  }
  LL_RCC_HSI_SetCalibTrimming(16);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {
  
  }
  LL_Init1msTick(8000000);
  LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
  LL_SetSystemCoreClock(8000000);
}

/*
 * Implementation of function processing data received via USART.
 */
void proccesDmaData(uint8_t sign)
{
	/* start sign - begin new string (also restarts unfinished string) */
	if(sign == STRING_START_SIGN)
	{
		stringReceiving = 1;
		stringSignCount = 0;
		stringLetters.small_letter = 0;
		stringLetters.capital_letter = 0;
		return;
	}

	/* no start sign yet - ignore received signs */
	if(!stringReceiving)
	{
		return;
	}

	stringSignCount++;

	/* end sign within 35 signs after "#" - valid string */
	if(sign == STRING_END_SIGN)
	{
		stringBuffer[stringSignCount - 1] = '\0';

		if(!validStringReady)		//previous result already sent
		{
			strcpy(validString, stringBuffer);
			letterCount = stringLetters;
			validStringReady = 1;
		}
		stringReceiving = 0;
		return;
	}

	/* 35 signs after "#" without "$" - discard data, wait for new "#" */
	if(stringSignCount >= STRING_MAX_SIGNS)
	{
		stringReceiving = 0;
		return;
	}

	/* store sign and count letters */
	stringBuffer[stringSignCount - 1] = (char)sign;

	if(sign >= 'a' && sign <= 'z')
	{
		stringLetters.small_letter++;
	}
	else if(sign >= 'A' && sign <= 'Z')
	{
		stringLetters.capital_letter++;
	}
}


/*
 * Sends text via USART2 with DMA.
 * Waits until previous DMA transmission is finished (channel 7 is disabled in its TC interrupt).
 */
static void USART2_SendString(const char *text)
{
	uint8_t length = (uint8_t)strlen(text);

	while(LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_7));

	memcpy(txBuffer, text, length);
	USART2_PutBuffer(txBuffer, length);
}

void Error_Handler(void)
{

}

#ifdef  USE_FULL_ASSERT

void assert_failed(char *file, uint32_t line)
{ 

}

#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
