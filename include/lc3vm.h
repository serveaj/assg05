/** @file lc3vm.h
 * @brief LC-3 VM API
 *
 * @author Student Name
 * @note   cwid: 123456
 * @date   Spring 2024
 * @note   ide:  g++ 8.2.0 / GNU Make 4.2.1
 *
 * Header include file for LC-3 simulator API/functions.
 */
#include <stdbool.h>
#include <stdint.h>

#ifndef LC3VM_H
#define LC3VM_H

#define NUMOPS (16)

// operand extraction macros
#define OPC(i) ((i) >> 12)

#define SR2(i) ((i) & 0x7)
#define SR1(i) (((i) >> 6) & 0x7)
#define DR(i) (((i) >> 9) & 0x7)

#define SEXTIMM(i) (sign_extend((i), 5))
#define OFF6(i) (sign_extend((i), 6))
#define PCOFF9(i) (sign_extend((i), 9))
#define PCOFF11(i) (sign_extend((i), 11))

#define FIMM(i) ((i >> 5) & 0x1)
#define FCND(i) (((i) >> 9) & 0x7)
#define BR(i) (((i) >> 6) & 0x7)
#define FL(i) (((i) >> 11) & 1)
#define TRP(i) ((i) & 0xFF)

#define RCND ((reg[PSR]) & 0x7)

typedef void (*op_ex_f)(uint16_t i);

// device and register memory mappings
#define PSR_ADDR 0xFFFC
#define MCR_ADDR 0xFFFE
enum registr
{
  R0 = 0, // 0xFFF0
  R1,
  R2,
  R3,
  R4,
  R5,
  R6,
  R7,
  SSP,
  USP,
  RPC,
  NR1,
  PSR, // 0xFFFC
  NR2,
  MCR, // 0xFFFE
  RCNT
};

enum flags
{
  FP = 1 << 0,
  FZ = 1 << 1,
  FN = 1 << 2
};

#define KBSR_ADDR 0xFE00
#define KBDR_ADDR 0xFE02
#define DSR_ADDR 0xFE04
#define DDR_ADDR 0xFE06
enum iodev
{
  KBSR = 0, // 0xFE00
  NI1,
  KBDR, // 0xFE02
  NI2,
  DSR, // 0xFE04
  NI3,
  DDR, // 0xFE06
  IOCNT
};

// If we are creating tests, make all declarations extern C so can
// work with catch2 C++ framework
#ifdef TEST
extern "C" {
#endif

extern bool running;
extern uint16_t mem[];
extern uint16_t* iomap;
extern uint16_t* reg;
extern uint16_t PC_START;

// Defining and accessing memory
uint16_t mem_read(uint16_t address);
void mem_write(uint16_t address, uint16_t val);

// Sign extension function
uint16_t sign_extend(uint16_t bits, int size);

// Update condition code flags
void update_flags(enum registr r);

// microarchitecture opcode implementations
void add(uint16_t i);
void andlc(uint16_t i);
void notlc(uint16_t i);

void ld(uint16_t i);
void ldi(uint16_t i);
void ldr(uint16_t i);
void lea(uint16_t i);

void st(uint16_t i);
void sti(uint16_t i);
void str(uint16_t i);

void jmp(uint16_t i);
void br(uint16_t i);
void jsr(uint16_t i);

void rti(uint16_t i);
void res(uint16_t i);

void trap(uint16_t i);

// loading and running programs using fetch-decode-execute cycle
void init(uint16_t offset);
void check_device_status();
void start(uint16_t offset);
void ld_img(char* fname);

// declare your assignment 05 functions here, and put the implementations
// at the bottom of the lc3vm.c file

bool is_user_mode(void); // test if system is in user mode
void user_mode(void); // switch the system into usr mode
void supervisor_mode(void); // switch the system into supervisor mode


// task 2 stack manipulation function declarations here

// task 3 MCR clock latch manipulation

// task 7 exceptions

#ifdef TEST
} // end extern C for C++ test runner
#endif

#endif // LC3VM_H
