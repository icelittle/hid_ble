
#include "cube_hal.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define divceil(x, n) (((x) + (n) - 1) / (n))
#define min(x, y) ((x) < (y) ? (x) : (y))
#define _BV(bit) (1 << (bit))

#define OUTPUT	true
#define INPUT	false
	
#define UP		0
#define DOWN	1

static uint8_t *states = NULL;
static int nrows = 6, ncols = 14;

static uint8_t scode[84] = {
	4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 
	14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 
	24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 
	34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 
	44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 
	54, 55};
static uint8_t* last_scancode = NULL;

static uint8_t key_map[32] = {0};
static uint8_t six_keys[8] = {0};

uint8_t matrix[6][14] = {
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13},
	{14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
	{28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41},
	{42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55},
	{56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69},
	{70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83},
};

uint8_t rows[] = {0, 1, 2, 3, 4, 5};
uint8_t cols[] = {6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

typedef struct {
	GPIO_TypeDef* port;
	uint16_t	  pin;
}hid_pin_t;

hid_pin_t hid_pins[20] = {
	{GPIOA, GPIO_PIN_3},//r1
	{GPIOA, GPIO_PIN_2},//r2
	{GPIOC, GPIO_PIN_6},//r3
	{GPIOB, GPIO_PIN_10},//r4
	{GPIOC, GPIO_PIN_3},//r5
	{GPIOB, GPIO_PIN_9},//r6

	{GPIOA, GPIO_PIN_0},//c1
	{GPIOA, GPIO_PIN_1},//c2
	{GPIOA, GPIO_PIN_8},//c3
	{GPIOB, GPIO_PIN_0},//c4
	{GPIOC, GPIO_PIN_13},//c5
	{GPIOB, GPIO_PIN_8},//c6
	{GPIOC, GPIO_PIN_5},//c7
	{GPIOC, GPIO_PIN_7},//c8
	{GPIOB, GPIO_PIN_12},//c9
	{GPIOB, GPIO_PIN_13},//c10
	{GPIOB, GPIO_PIN_14},//c11
	{GPIOB, GPIO_PIN_15},//c12
	{GPIOA, GPIO_PIN_10},//c13
	{GPIOA, GPIO_PIN_9},//c14
};

static bool is_pressed(uint8_t row, uint8_t col)
{
	const uint8_t pos = row*ncols + col;
	const uint8_t byte = pos / 8;
	const uint8_t bit = pos & 0x07;
	return (states[byte] >> bit) & 0x01;
}

static void set_state(uint8_t row, uint8_t col, bool state)
{
	const uint8_t pos = row*ncols + col;
	const uint8_t byte = pos / 8;
	const uint8_t bit = pos & 0x07;
	if (state)
		states[byte] |= (1 << bit);
	else
		states[byte] &= ~(1 << bit);
}

static void IO_config(uint8_t pin_num, bool dir)
{
  GPIO_InitTypeDef  GPIO_InitStruct;
  GPIO_InitStruct.Pin = hid_pins[pin_num].pin;
  GPIO_InitStruct.Mode = (dir == INPUT ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT_PP);
  GPIO_InitStruct.Pull = (dir == INPUT ? GPIO_PULLDOWN : GPIO_NOPULL);
  GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
  
  HAL_GPIO_Init(hid_pins[pin_num].port, &GPIO_InitStruct);
  
  if(dir != INPUT) {
  	HAL_GPIO_WritePin(hid_pins[pin_num].port, hid_pins[pin_num].pin, GPIO_PIN_RESET); 
  }
}

static void IO_set(uint8_t pin_num, GPIO_PinState pin_state)
{
  return HAL_GPIO_WritePin(hid_pins[pin_num].port, hid_pins[pin_num].pin, pin_state);
}

static bool IO_get(uint8_t pin_num)
{
  return (HAL_GPIO_ReadPin(hid_pins[pin_num].port, hid_pins[pin_num].pin) == GPIO_PIN_SET);
}

static void HID_set_scancode_state(uint8_t code, bool state)
{
	uint8_t byte_no = code / 8;
	uint8_t bit_no = code & 0x07;
	if (state == false)
		key_map[byte_no] &= ~_BV(bit_no);
	else
		key_map[byte_no] |= _BV(bit_no);
	/* The part below is not tested! */
	if (state == true) {
		uint8_t pos = 2;
		for (; pos < 8 && six_keys[pos] != 0; ++pos)
			;
		if (pos < 8)
			six_keys[pos] = code;
	} else {
		uint8_t pos = 2;
		for (; pos < 8 && six_keys[pos] != code; ++pos)
			;
		if (pos < 8)
			six_keys[pos] = 0;
	}
}

static void LAYOUT_set_key_state(uint8_t key, bool event)
{
	if (event == DOWN) {
		last_scancode[key] = scode[key];
	}
	HID_set_scancode_state(last_scancode[key], event);
}

void MATRIX_init()
{
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	
	for (uint8_t i = 0; i < nrows; ++i) {
		IO_config(rows[i], OUTPUT);
	}
	for (uint8_t i = 0; i < ncols; ++i) {
		IO_config(cols[i], INPUT);
	}
	
	states = malloc(divceil(nrows*ncols, 8));
	memset(states, 0, divceil(nrows*ncols, 8));
	last_scancode = malloc(nrows*ncols);
}

bool MATRIX_scan()
{
	bool changed = false;
	/* scan the matrix */
	for (uint8_t i = 0; i < nrows; ++i) {
		IO_set(rows[i], GPIO_PIN_SET);
		for (uint8_t j = 0; j < ncols; ++j) {
			bool state = IO_get(cols[j]);
			if (state == is_pressed(i, j))
				continue;
			changed = true;
			set_state(i, j, state);
			LAYOUT_set_key_state(matrix[i][j], state);
		}
		IO_set(rows[i], GPIO_PIN_RESET);
	}
	return changed;
}

uint8_t* get_key_buff()
{
	return six_keys;
}

