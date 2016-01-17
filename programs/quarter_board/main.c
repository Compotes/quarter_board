#include "ch.h"
#include "hal.h"

#include <icu_lld.h>
#include "chprintf.h"
#include <inttypes.h>

#include "communication.h"

#define MAX_PULSE_LENGTH 3000
#define MIN_PULSE_LENGTH 400
#define NOT_SEEING_VALUE 3000
#define QUANTITY_OF_SENSORS 2

#define UNUSED(arg) (void)(arg)

int32_t sensors_values[QUANTITY_OF_SENSORS];

thread_t *commanderp;
thread_t *ir_measurementp;

static void read_pulse(ICUDriver *icup) {
    uint8_t i = 0;
    int32_t value;

    if (icup == &ICUD1) {
        i = 0;
    } else if (icup == &ICUD3) {
        i = 1;
    }
    
    value = icuGetWidthX(icup);

    if (value > MIN_PULSE_LENGTH && value < MAX_PULSE_LENGTH) {
        sensors_values[i] = value;
    } else {
        sensors_values[i] = NOT_SEEING_VALUE;
    }
}

void execute_master_command(uint16_t command_id, uint8_t *buff) {
    switch (command_id) {
        case 0xAA:  
            led(GREEN, buff[0]);
            led(RED, buff[1]);
            break;

        default:    
            break;
    }

}

uint8_t *send_data_command(uint16_t command_id, uint8_t data_length, uint8_t *buff) {
    uint32_t values = 0;
    uint8_t i;
    switch (command_id) {
        case 0xFF:  
            values = sensors_values[0] | (sensors_values[1] << 16);
            sensors_values[0] = NOT_SEEING_VALUE;
            sensors_values[1] = NOT_SEEING_VALUE;
            led(GREEN, 1);
            led(RED, 0);
            for(i = 0; i < data_length; i++) {
                buff[i] = (values >> (4 * i)) & 0xf;
            }
            break;

        default:    
            break;           
    }
    return buff;
}

THD_WORKING_AREA(waCommanderThread, 128);
THD_FUNCTION(CommanderThread, arg) {
    UNUSED(arg);

    thread_t *serialp;
    message_data_t *messagep;
    uint16_t cmd, command_id;
    uint8_t data_buff[MAX_DATA_CHAR_LENGTH];
    uint8_t data_length, i, rw_bit;

    while (1) {
        serialp = chMsgWait();
        messagep = (message_data_t *)chMsgGet(serialp);
        
        cmd = messagep->operation;

        command_id = cmd & COMMAND_ID_MASK;
        rw_bit = ((cmd >> OPERATION_BIT_LENGTH) & 1) + 1;
        data_length = cmd >> (OPERATION_BIT_LENGTH + RW_BIT);
        
        if (rw_bit == PROCESS_DATA_FROM_MASTER) {
            for (i = 0; i < data_length; i++) {
                data_buff[i] = messagep->data[i];
            }
            execute_master_command(command_id, data_buff);
        } else {
            send_data_command(command_id, data_length, data_buff);
            for (i = 0; i < data_length; i++) {
                messagep->data[i] = data_buff[i];
            }
        }

        chMsgRelease(serialp, (msg_t)messagep);
    }
    chRegSetThreadName("commander");
    chThdSleepMilliseconds(100);
}

static const ICUConfig icucfg_1 = {
    ICU_INPUT_ACTIVE_HIGH,
    2000000, // 2 MHz timer
    read_pulse, // pulse received
    NULL,
    NULL,
    ICU_CHANNEL_1,
    0
};

static const ICUConfig icucfg_2 = {
   ICU_INPUT_ACTIVE_HIGH,
   2000000, // 2 MHz timer
   read_pulse, // pulse received
   NULL,
   NULL,
   ICU_CHANNEL_2,
   0
};

int main(void) {    
    halInit();
    chSysInit();

    uint8_t i;
    
    communication_init();

    /*
     * ICUDriver 1 setup
     */
    icuInit();
    icuStart(&ICUD1, &icucfg_2);
    icuStartCapture(&ICUD1);
    icuEnableNotifications(&ICUD1); 
    palSetPadMode(GPIOA,  9, PAL_MODE_ALTERNATE(2));

    /*
     * ICUDriver 3 setup
     */
    icuStart(&ICUD3, &icucfg_1);
    icuStartCapture(&ICUD3);
    icuEnableNotifications(&ICUD3);
    palSetPadMode(GPIOA, 6, PAL_MODE_ALTERNATE(1));

    for (i = 0; i < QUANTITY_OF_SENSORS; i++) {
        sensors_values[i] = NOT_SEEING_VALUE;
    }

    /*
     * Threads setup
     */
    commanderp = chThdCreateStatic(waCommanderThread, sizeof(waCommanderThread), 
                                   NORMALPRIO, CommanderThread, NULL);

    communication_thread(commanderp);

    while (1) {
        chThdSleepMilliseconds(100);
    }
}
