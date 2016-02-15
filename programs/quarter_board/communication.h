#ifndef _COMMUNICATION_H_
#define _COMMUNICATION_H_

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include <inttypes.h>

#define CHAR_BIT_LENGTH 4
#define VALUE_CHAR_LENGTH 4

#define COMMAND_CHAR_LENGTH 4
#define COMMAND_BEGIN 0
#define COMMAND_END 3
#define COMMAND_ID_MASK 0x7ff

#define MAX_DATA_CHAR_LENGTH 10
#define OPERATION_BIT_LENGTH 11
#define RW_BIT 1

#define COMMAND_CHAR_0_READ 0
#define COMMAND_CHAR_1_READ 1
#define COMMAND_CHAR_2_READ 2
#define COMMAND_CHAR_3_READ 3
#define SENDING_STATE 4
#define PROCESSING_STATE 5

#define NO_OPERATION 0
#define PROCESS_DATA_FROM_MASTER 1
#define SEND_DATA_TO_MASTER 2

extern thread_t *communicationp;
void communication_init(void);
void communication_thread(thread_t *msgThread);
uint8_t value_to_char(uint8_t character);
uint8_t char_to_value(uint8_t character);

typedef struct {
    uint16_t operation;
    uint8_t data[MAX_DATA_CHAR_LENGTH];
} message_data_t;

#endif
