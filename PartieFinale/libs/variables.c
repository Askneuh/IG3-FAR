#include <stdio.h>
#include "variables.h"
#include "common.h"

volatile int block_recv = 0;
volatile int expected_opcode = 0;  // 0 = pas de blocage spécifique
struct msgBuffer expected_msg;     // Pour stocker le message attendu
volatile int msg_received = 0;     // Flag pour indiquer si le message a été reçu

void init_variables() {
    block_recv = 0;
    expected_opcode = 0;
    msg_received = 0;
}

void set_block_recv(int value) {
    block_recv = value;
}
void set_expected_opcode(int value) {
    expected_opcode = value;
}
void set_msg_received(int value) {
    msg_received = value;
}
void set_expected_msg(struct msgBuffer msg) {
    expected_msg.adClient = msg.adClient;
    expected_msg.opCode = msg.opCode;
    expected_msg.port = msg.port;
    expected_msg.msgSize = msg.msgSize;
    strcpy(expected_msg.username, msg.username);
    strcpy(expected_msg.msg, msg.msg);
}
struct msgBuffer get_expected_msg() {
    return expected_msg;
}
int get_block_recv() {
    return block_recv;
}
int get_expected_opcode() {
    return expected_opcode;
}
int get_msg_received() {
    return msg_received;
}
