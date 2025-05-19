#include <stdio.h>
#ifndef VARIABLES_H
#define VARIABLES_H

extern volatile int block_recv;
extern volatile int expected_opcode;
extern volatile int msg_received;
extern struct msgBuffer expected_msg;

void init_variables();
void set_block_recv(int value);
void set_expected_opcode(int value);
void set_msg_received(int value);
void set_expected_msg(struct msgBuffer msg);
struct msgBuffer get_expected_msg();
int get_block_recv();
int get_expected_opcode();
int get_msg_received();

#endif // VARIABLES_H