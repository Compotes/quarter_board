#include "ch.h"
#include "hal.h"

#include "chprintf.h"
#include "communication.h"

#include <icu_lld.h>
#include <inttypes.h>

thread_t *communicationp;

message_data_t message;
message_data_t *msgp = &message;

uint8_t value_to_char(uint8_t character) {
    if (character > 9) {
       character = character - 10 + 'A';
    } else {
       character = character + '0';
    }
    return character;
}


uint8_t char_to_value(uint8_t character) {
    if (character >= 'A') {
        character = character + 10 - 'A';
    } else {
        character = character - '0';
    }
    return character;
}

THD_WORKING_AREA(waSerialThread, 128);
THD_FUNCTION(SerialThread, msgThread) {
    static uint16_t command;
    static uint8_t my_address;
    static uint8_t quantity_of_slaves;
    static uint8_t state = COMMAND_CHAR_0_READ;
    uint8_t char_counter = 0;
    uint8_t data_length = 0;
    uint8_t i;
    uint8_t received_char;
    uint8_t rw = 0;

    uint8_t *data_buffer = NULL;
    uint8_t *operation_buffer = NULL;

    while (1) {
        received_char = sdGet(&SD1);

        if ((received_char >> 7) & 1) {
            if ((received_char >> 6) & 1) {
                quantity_of_slaves = (received_char + my_address) & ~0xC0;
                
                if (operation_buffer == NULL) {
                    operation_buffer = chCoreAlloc(sizeof(uint8_t) * quantity_of_slaves *
                                                   MAX_DATA_CHAR_LENGTH);
                }

                if (data_buffer == NULL) {
                    data_buffer = chCoreAlloc(sizeof(uint8_t) * quantity_of_slaves * 
                                              MAX_DATA_CHAR_LENGTH);
                }

                received_char--;
            } else {
                my_address = received_char & ~0x80;
                received_char++;
            }

            sdPut(&SD1, received_char);

        } else if (received_char == 0x0D || received_char == 0x0A) {
            state = COMMAND_CHAR_0_READ; // communication reset - \r \n
            sdPut(&SD1, received_char);
            char_counter = 0;
            command = 0;
        } else if ((received_char >= '0' && received_char <= '9') || 
                   (received_char >= 'A' && received_char <= 'F')) {
            
            received_char = char_to_value(received_char);

            if (state <= COMMAND_END) {
                command |= (received_char << (state * CHAR_BIT_LENGTH));

                sdPut(&SD1, value_to_char(received_char));

                state++;

                if (state == COMMAND_END + 1) {
                    //commmand_received
                    for (i = 0; i < (quantity_of_slaves * MAX_DATA_CHAR_LENGTH); i++) {
                        operation_buffer[i] = NO_OPERATION;
                        data_buffer[i] = 0;
                    }

                    data_length = command >> (OPERATION_BIT_LENGTH + RW_BIT);
                    rw = ((command >> OPERATION_BIT_LENGTH) & 1) + 1;

                    msgp->operation = command;
                    
                    char_counter = 0;

                    if (rw == SEND_DATA_TO_MASTER) {
                        msgp = (message_data_t *)chMsgSend(msgThread, (msg_t)msgp);
                        
                        for (i = 0; i < data_length; i++) {
                            data_buffer[i + data_length * my_address] = msgp->data[i];
                            operation_buffer[i + data_length * my_address] = SEND_DATA_TO_MASTER;
                        }
                    }

                    if (rw == PROCESS_DATA_FROM_MASTER) {
                        state = PROCESSING_STATE;
                        
                        for (i = 0; i < data_length; i++) {
                            operation_buffer[i + data_length * my_address] = PROCESS_DATA_FROM_MASTER;
                        }
                    }
                }
            } else {
                if (state == SENDING_STATE || state == PROCESSING_STATE) {
                    switch (operation_buffer[char_counter]) {
                        case NO_OPERATION:              
                            sdPut(&SD1, value_to_char(received_char));
                            break;

                        case PROCESS_DATA_FROM_MASTER: 
                            data_buffer[char_counter] = received_char;
                            sdPut(&SD1, value_to_char(received_char));
                            break;

                        case SEND_DATA_TO_MASTER:       
                            sdPut(&SD1, value_to_char(data_buffer[char_counter]));
                            break;

                        default:                        
                            break;
                    }

                    char_counter++;
 
                    if (char_counter == quantity_of_slaves * data_length) {
                        if (state == PROCESSING_STATE) {
                            for (i = 0; i < data_length; i++) {
                                msgp->data[i] = data_buffer[i + data_length * my_address];
                            }
                            chMsgSend(msgThread, (msg_t)msgp);
                        }
                        state = COMMAND_CHAR_0_READ;
                        char_counter = 0;
                    }
                }
            }
        }
    }
}

static const SerialConfig serial_cfg = {
    115200, // baud rate
    0,
    0,
    0
};

void communication_init(void) {
    sdInit();
    sdStart(&SD1, &serial_cfg);
}

void communication_thread(thread_t *msgThread) {
    communicationp = chThdCreateStatic(waSerialThread, sizeof(waSerialThread), 
                                       NORMALPRIO, SerialThread, msgThread);
}
