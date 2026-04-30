/** @file lc3vm.c
 * @brief LC-3 VM Implementation
 *
 * @author Student Name
 * @note   cwid: 123456
 * @date   Spring 2024
 * @note   ide:  g++ 8.2.0 / GNU Make 4.2.1
 *
 * Implementation of LC-3 VM/Microarchitecture simulator.  Functions
 * to fetch-decode-execute LC-3 encoded machine instructions.
 * Support functions for the microcode to decode instructions, addresses
 * and simulate registers, datapath and ALU operations.
 */
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "lc3vm.h"

/// declarations of memory and registers used in the microprogram simulator
uint16_t mem[UINT16_MAX + 1] = {0};
// memory map the registers starting at address 0xFFF0, maps PSR to 0xFFFC and MCR to 0xFFFE
// uint16_t reg[RCNT] = {0};
uint16_t* reg = &mem[0xFFF0];
// iomap maps memory starting at 0xFE00 for the Keyboard KBSR KBDR and Display DSR DDR maped registers
uint16_t* iomap = &mem[0xFE00];
uint16_t PC_START = 0x3000;

/** @brief memory read, transfer from memory
 *
 * Given a 16 bit address (from 0x0 to UINT_MAX=65535), access
 * the simulated memory and read and return the indicated value.
 * No error checking is done here, though the parameter type is
 * limited to 16 bits so in theory no illegal memory access should
 * be possible from this function.
 *
 * @param address The memory address to read and transfer data from.
 *   This parameter is an unsigned 16 bit integer, all addresses
 *   are constrained to be 16 bits in size in LC-3.
 *
 * @returns uint16_t Retuns a 16 bit result.  The result may be interpreted
 *   later as something other than an unsigned integer, but this function
 *   simply reads and returns the 16 bits stored at the indicated address.
 */
uint16_t mem_read(uint16_t address)
{
  return mem[address];
}

/** @brief memory write, transfer to memory
 *
 * Given a 16 bit address and a 16 bit value, store the value in our
 * LC-3 memory at the indicated address.  No error checking is done here,
 * though the parameter type for both the address and value are limited
 * to 16 bits, so in theory no illegal memory access should be possible, nor
 * is it possible to provide too few or too many bits in the requested read
 * operation.
 *
 * @param address A unsigned 16 bit memory address, the target location where the
 *   given value should be written into our simulated memory.
 * @param value A unsigned 16 bit value, this value is not interpreted, it is simply
 *   stored where requested, it could actually be a signed number, or an ascii
 *   character, or some other type of data.
 */
void mem_write(uint16_t address, uint16_t val)
{
  mem[address] = val;
}

/** @brief sign extend bits
 *
 * Given a 16-bit value and a sign position, perform a twos-complement sign
 * extension on the original 16-bit value.  For example, if the bits given are
 *    0000 0000 0001 1010
 * This number is a 5 bit value in twos-complement (-6 in this case), but we need
 * to extend this result to the full 16 bits before we can add this value to
 * other twos-complement encoded values and get the expected result.  For a
 * 2's complement size of 5 bits, the result after sign extend should be:
 *    1111 1111 1111 1010 (which is -6 in twos-complement encoded in full 16 bits)
 *
 * @param bits The original bits that encode a twos-complement number but only int
 *    last n significant bits
 * @param size The size or number of bits in the unextended 2s complement number we are
 *   given.  A 5-bit 2's complement number like 10101 has bits numbers 0-4 and the bit
 *   at position 4 will be the sign bit that needs to be extended..
 *
 * @returns uint16_t Returns the bits with the sign bit extended to all higher
 *    bit positions, thus converting this to a full 16-bit twos-complement sigend
 *    value.
 */
uint16_t sign_extend(uint16_t bits, int size)
{
  // if bit at the sign position is 1, we have to extend a 1 to all bits higher
  // than the sign position
  if ((bits >> (size - 1)) & 0x1)
  {
    return bits | (0xFFFF << size);
  }
  // else the bit at the sign position is 0 so we need to create a mast with 0's in
  // the upper 16-size bit positions and & with the bits
  else
  {
    return bits & (0xFFFF >> (16 - size));
  }
}

/** @brief update condition register flags
 *
 * Given a destination register that was just modified by an LC-3 operation,
 * update the condition codes register pased on the value in the modified
 * register.  If the register is 0 then the FZ condition should be set.  If
 * the register contains a twos-complement negative signed number (e.g. bit 15
 * is a 1), then the FN condition should be set for a negative number.  Otherwise
 * the FP condition flag should be set for a positive number.
 *
 * @param modified_register A (symbolic enumerated type name of) a register that
 *   was just modified by an operation and needs to have the condition code flags
 *   updated as a side effect of the operation just performed.
 */
void update_flags(enum registr r)
{
  // make sure conditions are clear before setting needed flag
  reg[PSR] &= 0xFFF8;

  // Can easily test for 0 condition, if the register was 0 then set FZ
  if (reg[r] == 0)
  {
    reg[PSR] |= FZ;
  }
  // otherwise if the result is negative, e.g. bit 15 is set, then set FN
  else if (reg[r] >> 15)
  {
    reg[PSR] |= FN;
  }
  // otherwise the result was not 0 or negative, so must be FP
  else
  {
    reg[PSR] |= FP;
  }
}

/** @brief add operation
 *
 * Add two values together and store result in destination register.
 * In one version, when the FIMM flag at bit 5 is 0, there are two
 * source registers to add to get the result.  When the FIMM flag at
 * bit 5 is 1, then the least significant 5 bits are interpreted as an
 * immediate twos-complement signed value that is added to the source
 * register to get the result.  Don't forget that we need to perform
 * sign extension if we have an immediate value in the instruction.
 *
 * Although all values in registers are uint16_t (unsigned 16 bit
 * integers), we can cheat a bit and just use the C + operator to do
 * the addition.  This is because, adding the bits of twos-complement
 * encoded values gives the correct result.  The only issue is that
 * the result may no longer fit into 16-bit twos complement.  If it
 * overflows the results is nonsensical.  Some architectures have
 * additional condition codes that would detect and be set if this
 * type of overflow occurs.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 *   instruction.
 */
void add(uint16_t i)
{
  // if the immediate flag is set, extract 5-bit immediate values as the second
  // operand, sign extend it to 16 bits, and add it to the source register for
  // result
  if (FIMM(i))
  {
    reg[DR(i)] = reg[SR1(i)] + SEXTIMM(i);
  }
  // otherwise use indicated two source registers as the operands for the addition
  else
  {
    reg[DR(i)] = reg[SR1(i)] + reg[SR2(i)];
  }

  // update condition flags based on the addition just performed
  update_flags(DR(i));
}

/** @brief logical AND operation
 *
 * Compute the logical AND of 2 16 bit values and store the result in
 * the destination register.  In one version, when the FIMM flag at bit
 * 5 is 0, there are two source registers to AND together to get the
 * result.  When the FIMM flag at bit 5 is 1, then the least significant
 * 5 bits are extracted.  For consistency we interpret as a signed number,
 * and perform sign extension to create a 16 bit value from the 5 immediate
 * bits in the instruction.
 *
 * We can use the bitwise and operator '&' to perform this operation for
 * both versions of our operation.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 *   instruction.
 */
void andlc(uint16_t i)
{
  // if the immediate flag is set, extract 5-bit immediate values as the
  // second operand, sign extend it to 16 bits, and add it to the source
  // register for result
  if (FIMM(i))
  {
    reg[DR(i)] = reg[SR1(i)] & SEXTIMM(i);
  }
  // otherwise use indicated two source registers as the operands for the AND
  // operation
  else
  {
    reg[DR(i)] = reg[SR1(i)] & reg[SR2(i)];
  }

  // update condition flags based on the addition just performed
  update_flags(DR(i));
}

/** @brief logical NOT operation
 *
 * Perform a logical NOT on the indicated source register and save the
 * result into the destination register.  We will use the C logical not
 * operator '~' to perform the actual operation.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 *   instruction.
 */
void notlc(uint16_t i)
{
  reg[DR(i)] = ~reg[SR1(i)];
  update_flags(DR(i));
}

/** @brief load RPC + offset
 *
 * Load a value from memory calculated as some offset from the current
 * RPC program counter location.  The low 9 bits of the instruction are
 * interpreted as a signed twos-complement number (and need to be sign
 * extended).  This offset is added to the RPC to calculate an address in
 * memory.  Because we use 9 signed bits, the offset from the current
 * RPC ranges from +256 to -257.  The value in the calculated address is
 * transfered from memory into the destination register specified in
 * the instruction.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 */
void ld(uint16_t i)
{
  reg[DR(i)] = mem_read(reg[RPC] + PCOFF9(i));
  update_flags(DR(i));
}

/** @brief load indirect
 *
 * Load a value from memory using indirect addressing.  The same
 * initial address is calculated from the PCOffset9 low order bits of
 * this instruction, giving a first target address that is +256 or
 * -257 distance from the current RPC.  However the value in that
 * location is then interpreted as another memory address, which will
 * be fetched and then loaded into the destination register.
 * Thus two memory access reads are performed by this operation.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 */
void ldi(uint16_t i)
{
  reg[DR(i)] = mem_read(mem_read(reg[RPC] + PCOFF9(i)));
  update_flags(DR(i));
}

/** @brief load base + relative offset
 *
 * This instruction uses SR1 as a base address.  The value in this register is
 * treated as and address and the low 6 bits are treated as an offset which is
 * a twos-complement signed number.  So offset values can range from
 * +32 to -32 from the base address in the base register.  This offset is added
 * to the base address and the value from this memory location is fetched
 * and stored in the destination register.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 */
void ldr(uint16_t i)
{
  reg[DR(i)] = mem_read(reg[SR1(i)] + OFF6(i));
  update_flags(DR(i));
}

/** @brief load effective address
 *
 * Despite this functions name, a memory access is not performed.
 * This function calculates an effective address that may be useful to
 * load further values.  The low order 9 bits are treated as a
 * twos-complement signed value and added to the current RPC.  This
 * calculated address is simply saved into the indicated destination
 * register (e.g. we do not perform a memory read here, only
 * calculate an address).
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 */
void lea(uint16_t i)
{
  reg[DR(i)] = reg[RPC] + PCOFF9(i);
}

/** @brief store to PC + offset
 *
 * Store a value into memory from a source register.  The location where the
 * value is stored is calculated from an offset relative to the current RPC
 * value.  The low 9 bits of the instruction are treated as an signed
 * twos-complement number for the offset, thus values from +0xFF to -0X100
 * can be stored relative to the current RPC location.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 */
void st(uint16_t i)
{
  mem_write(reg[RPC] + PCOFF9(i), reg[DR(i)]);
}

/** @brief store indirect
 *
 * Similar to the basic store, but with an extra level of indirection.
 * The PCOFF9 low order bits provide an offset in the range of -0x100
 * to 0xFF relative to the PC.  This address is first read to obtain
 * the indirect address.  This indirect fetched address is the
 * ultimate location where the source register for this instruction is
 * stored.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 */
void sti(uint16_t i)
{
  mem_write(mem_read(reg[RPC] + PCOFF9(i)), reg[DR(i)]);
}

/** @brief store offset relative to base address
 *
 * This instruction has a register with a base address, and the low 6
 * bits are interpreted as a twos-complement signed number that is
 * added to the base address to get the actuall address for storing
 * the value.  The source register of this instruction contains the
 * value that will be stored to the calculated base + offset address.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 */
void str(uint16_t i)
{
  mem_write(reg[SR1(i)] + OFF6(i), reg[DR(i)]);
}

/** @brief jump unconditionally
 *
 * Jump unconditionally to a 16 bit address.  The jump
 * destination is held in the indicated base
 * register (in the SR1 bits) of the given instruction.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 */
void jmp(uint16_t i)
{
  reg[RPC] = reg[SR1(i)];
}

/** @brief conditional branch
 *
 * Perform a conditional branch.  First we extract the
 * N,Z,P bits from positions 11-9 of the instruction.  These
 * indicate which conditions (if any) we should branch on,
 * N negative, Z zero, P positive.  These conditions need to
 * be checked agains the current condition flags in the PSR
 * register.  If the condition to branch is met, we use the low
 * 9 bits to perform a PC relative branch to the new location
 * in the code.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 */
void br(uint16_t i)
{
  // test if the condition flags meet conditions specified in instruction, and
  // branch to offset9 from RPC if they do.
  if (RCND & FCND(i))
  {
    reg[RPC] += PCOFF9(i);
  }
}

/** @brief jump to/from subtroutine
 *
 * This microcode handles both jump into a subroutine and return
 * from subroutine, which may appear as different opcodes jsr and
 * jsrr respectively in the assembly.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 */
void jsr(uint16_t i)
{
  // save return address, no matter if this is a jump into a
  // new subroutine or we are about to return from this subroutine.
  reg[R7] = reg[RPC];

  // if the flow bit i is 1 then this is a jump into a new subroutine,
  // so get the low 11 bits and jump to location relative to current RPC
  if (FL(i))
  {
    reg[RPC] = reg[RPC] + PCOFF11(i);
  }
  // otherwise this is a return, so get return address from indicated
  // base register
  else
  {
    reg[RPC] = reg[BR(i)];
  }
}

/** @brief return from trap / interrupt
 *
 * Return from a trap or interrupt.  We expect to be in
 * supervisor mode to use this instruction.  We take the
 * PC and PSR values from the supervisor stack and restore them.
 * If we are returning back to user mode, we also update the
 * stack to the user stack.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.
 */
void rti(uint16_t i) {}

/** @brief reserved
 *
 * Reserved/unused opcode (1101).  We generate an illegal opcode
 * exception if an attempt is made to run this instruction.
 *
 * @param i The instruction.  The bits of the instruction we are
 *   executing.  We need all of the bits so that we can extract the
 *   destination and source register operands, and to extract the
 *   second source register or the immediate value encoded in the
 */
void res(uint16_t i) {}

/** @brief trap instruction
 *
 * The trap service vector is in low 8 bits 7-0 of the instruction.
 * The trap service vector indexes into the trap vector table, that
 * exists in supervisor memory from 0x0000 - 0x00FF.  These contain the
 * address of the trap service routine in supervisor memory that should
 * be invoked.
 *
 * @param i The instruction.  The bits of the trap instruction we are
 *   executing.  The low 7 bits i[7:0] contain the trap service vector
 *   index to be invoked.
 */
void trap(uint16_t i) {}

/**
 * LC-3 instruction microcode store / lookup table.  Need to define array
 * of function pointers with all (microcode) functions inserted in
 * correct order so that index corresponds with the instruction opcode.
 * Execution happens by extracting opcode from instruction after fetch, and
 * looking up and invoking the (microcode) instruction function from this
 * lookup table.
 */
op_ex_f op_ex[NUMOPS] = {/*0000*/ br,
  /*0001*/ add,
  /*0010*/ ld,
  /*0011*/ st,
  /*0100*/ jsr,
  /*0101*/ andlc,
  /*0110*/ ldr,
  /*0111*/ str,
  /*1000*/ rti,
  /*1001*/ notlc,
  /*1010*/ ldi,
  /*1011*/ sti,
  /*1100*/ jmp,
  /*1101*/ res,
  /*1110*/ lea,
  /*1111*/ trap};

/** @brief initialize LC-3 simulator
 *
 * Initialize the starting state of the LC-3 simulator.
 * Make sure that the RPC, SSP, USP, and MCR all have
 * good initial starting state values.  Also
 * initialize the terminal and keyboard input to simulate
 * hardware driven I/O in the simulator.
 *
 * @param offset Normal starting RPC is 0x3000, but we can specify
 *   a 16-bit (signed) offset from this location and start there instead
 *   in this routine.
 */
void init(uint16_t offset)
{
  // initialize all registers to 0
  for (enum registr r = R0; r < RCNT; r++)
  {
    reg[r] = 0x0000;
  }

  // set R6, USP and SSP to traditional location of stacks
  reg[R6] = 0xFE00;
  reg[USP] = 0xFE00;
  reg[SSP] = 0x3000;

  // set initial RPC program counter location
  reg[RPC] = PC_START + offset;

  // set MCR/PSR, e.g. enable the clock, set priority to 0 and
  // start in user mode
  // enable_clock();
  // user_mode();

  // initialize memory mapped status registers
  iomap[KBSR] = 0x0000; // 0 indicates no key is available yet for a program to read
  iomap[DSR] = 0x8000;  // 1 indicates display is ready to receive a character

  // setup terminal input to be nonblocking and not echo characters
  struct termios t;
  tcgetattr(STDIN_FILENO, &t);
  t.c_lflag &= ~ICANON;
  t.c_lflag &= ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &t);

  // set read to non blocking to simulate memory mapped input
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

/** @brief check and update device status
 *
 * At end of each fetch-decode-execute cycle, check
 * the keyboard and display memory mapped devices and
 * simulate updating status of devices.  We are using hooks
 * to the actual linux/unix system calls to simulate our
 * memory mapped I/O here.
 *
 * For the keyboard device, we perform a non blocking read
 * of standard input.  If we find a character available, we
 * set KBSR to 1 to indicate a character is ready to be
 * consumbed by a program, and put the character into KBDR.
 *
 * For the display device, If a program desires a character
 * to be displayed, it puts it in DDR and set the DSR to 0.
 * So if DSR is 0 we write the character to standard
 * output, then reset DSR to 1 to indicate we are ready
 * to receive another character for display from a user
 * program.
 */
void check_device_status()
{
  // memory mapped keyboard input handling
  // if KBSR bit 15 is 0 that means the machine is ready to read another
  // character
  if ((iomap[KBSR] & 0x8000) == 0)
  {
    // use linux system calls to do actual read of a key from standard input
    // need an asynchronous read, if no key is in buffer we do nothing
    char c;
    ssize_t num_read = read(STDIN_FILENO, &c, 1);

    // if we read a character, then update the keyboard data and status registers
    if (num_read == 1)
    {
      // set KBSR bit 15 to 1 to indicate character is available to read
      iomap[KBSR] |= 0x8000;

      // set the KBDR bits
      iomap[KBDR] = (uint16_t)c;
    }
  }

  // memory mapped display output handling
  // if DSR bit 15 is 0 that means a character is available to display
  if ((iomap[DSR] & 0x8000) == 0)
  {
    // use linux system calls to do actual write of a key to standard output
    // need to flush the write so we see result with no buffering
    char c = (char)iomap[DDR];
    ssize_t num_write = write(STDOUT_FILENO, &c, 1);
    fflush(stdout);

    if (num_write == 1)
    {
      // set DDR bit 15 to 1 to indicate ready to receive another character to write
      iomap[DSR] |= 0x8000;

      // reset DDR bits? not really necessary
      iomap[DDR] = 0;
    }
  }
}

/** @brief start/run LC-3 simulator
 *
 * Implement the main fetch-decode-execute loop.  The next
 * instruction is fetched based on the RPC.  We extract the
 * opcode using the OPC macro, and use the opcode execution
 * function lookup table to decode the instruction parameters and
 * complete the execution of the opcode.  This simulation
 * keeps fetching and executing instructions until a halt
 * trap service routine is invoked, which halts the simulation
 * from running.
 *
 * @param offset Normal starting RPC is 0x3000, but we can specify
 *   a 16-bit (signed) offset from this location and start there instead
 *   in this routine.
 */
void start(uint16_t offset)
{
  // set initial state of LC-3 simulator
  init(offset);

  // perform the fetch-decode-execute cycle while the
  // run clock/latch is enabled
  while (true) // needs to be modified to use is_running() once implemented
  {
    // fetch the next instruction from memory
    uint16_t i = mem_read(reg[RPC]);

    // increment the RPC in preparation for next fetch
    reg[RPC]++;

    // invoke opcode execution function to decode and execute
    op_ex[OPC(i)](i);

    // perform I/O and interrupt tasks before next fetch
    // check_device_status();
  }
}

/** @brief load an LC-3 machine instruction image
 *
 * This functions loads a LC-3 machine language file (binary)
 * from a file into memory.  We expect a simple binary/elf
 * format for LC-3 images here.  All LC-3 bin files have
 * the following expected format:
 *
 * section_address section_size
 * section_code
 * section_address section_size
 * section_code
 * ...
 *
 * All values are read in as 16 bit words.  Each section
 * specifies the address it is to be loaded into memory, and
 * the size of the section (again in 16-bit words).
 *
 * This method reads and loads in all sections found in the file.
 * There can be 1 or more sections in an LC-3 bin file.
 *
 * @param fname The name of the file to open and read the LC-3
 *   machine instructions from.  This is expected to be a binary file
 *   which reads 16 bit values and places them consecutively into
 *   the simulated memory.

 */
void ld_img(char* fname)
{
  FILE* in = fopen(fname, "rb");
  if (NULL == in)
  {
    fprintf(stderr, "Cannot open file %s.\n", fname);
    exit(1);
  }

  fprintf(stdout, "<ld_img> loading image file: <%s>\n", fname);

  // attempt to read in sections, and keep reading until we
  // detect the end of the file
  uint16_t section_num = 0x1;
  uint16_t section_address;
  uint16_t section_size;
  while (!feof(in))
  {
    // the section address and section size are the first
    // two words for each section, get those first
    fread(&section_address, sizeof(uint16_t), 1, in);
    // eof not detected until a read happens at end, so
    if (feof(in))
      break;
    fread(&section_size, sizeof(uint16_t), 1, in);

    // load in the code for this section which is of the indicated
    // size to the indicated address for the code
    uint16_t* p = mem + section_address;
    fread(p, sizeof(uint16_t), section_size, in);

    // display status of loading this section
    fprintf(stdout, "   section %03d: address: %04X size: %04X\n", section_num, section_address, section_size);
    section_num++;
  }
  fclose(in);
}

/** @brief is user mode
 *
 * Bit 15 of the PSR determines if we are in supervisor or user mode.  This function
 * tests bit 15 of the PSR.  When it is 0 we are currently in supervisor mode, and
 * when it is 1 we are in  user mode.
 *
 * @returns bool True if we are in user mode (bit 15 is 1) and False if we are
 *   in supervisor mode (bit 15 is 0).
 */
bool is_user_mode(void)
{
  // make but 15 and test if if is non 0
  return (reg[PSR] & 0x8000) != 0;
}

/** @brief set user mode
 *
 * Set the machine into user mode.  This function sets bit 15 to be 1 to indicate
 * that we are now running in the less privileged user mode.
 */
void user_mode(void)
{
  // set bit 15 using OR
  reg[PSR] |= 0x8000;
}

/** @brief set supervisor mode
 *
 * Set the machine into supervisor mode.  This function sets bit 15 to be 0
 * to indicate that we are now running in the more privileged supervisor mode.
 */
void supervisor_mode(void)
{
  // clear bit 15 using And
  reg[PSR] &= ~0x8000;
}

/** @brief get priority
 *
 * Get and return the current priority set in the PSR.  The priority
 * is contained in bits PSR[10:8] of the PSR.
 *
 * @returns uint16_t Returns the normal uint16 type, though only the least
 *   significant 3 bits should have any value since only priority levels
 *   0 - 7 are possible
 */
uint16_t priority(void)
{
  // shift bits 8-10 down to 0-2 and mask out all but the least 3 bits
  return (reg[PSR] >> 8) & 0x7;
}

/** @brief set priority
 *
 * Set the priority in the PSR to the indicated priority level.  We
 * are passined in a full 16 bit value, but only the least 3 significant
 * bits should be used when setting the priority.
 *
 * @param p The new priority level to set.  This should be a value from 0-7,
 *   it is undefined what happens if a value not in this range is set for the
 *   priority.
 */
void set_priority(uint16_t p)
{
  // clear bits 8-10 while preserving other bits
  reg[PSR] &= 0xF8FF; // 1111 1000 1111 1111

  // insert the new priority by shifting it up to bits 8-10 and OR with PSR
  uint16_t new_p = (p & 0x7) << 8;

  // OR the shifted priority with the PSR
  reg[PSR] |= new_p;
  
}

/** @brief push value to current stack
 *
 * This method assumes `R6` holds the address of the current stack in use by running
 * programs.  This function pushes the indidcated value onto the top of the stack by
 * first decreasing the stack address in `R6` then writing the value to this memory
 * location.  This function is required to use `mem_write()` to perform the write
 * that pushes the value on top of the stack.
 *
 * @param value
 */
void push(uint16_t value)
{
  // decrement stack pointer in R6
  reg[R6]--;

  // write value to memory at new top of stack address in R6
  mem_write(reg[R6], value);
}

/** @brief pop top of current stack
 *
 * Pop off the top value of the current stack.  This method assumes
 * that `R6` holds the address of the top of the current stack in use
 * by the running program.
 */
void pop(void)
{
  // increment stack pointer in R6 to remove this value from the stack
  reg[R6]++;

}

/** @brief enable clock run bit
 *
 * Enable the clock run by setting the MCR run latch bit [15]
 * to 1.
 */

/** @brief disable clock run bit
 *
 * Disable the machine clock by setting the MCR run latch bit
 * [15] to 0.
 */

/** @brief test is clock running
 *
 * Check the MCR clock enable / run latch bit [15] to determine
 * if the system clock is currently enabled, and thus if the
 * machine is currently running or not.
 *
 * @returns bool True if the clock is currently enabled and thus the
 *   system is currently running, false if not.
 */

/** @brief exception
 *
 * The exception service vector is in low 8 bits 7-0 of the
 * instruction.  The exception service vector indexes into the
 * exception vector table, that exists in privileged memory from
 * 0x0100 - 0x0102.  The usual defined LC-3 exceptions include
 *
 * 0x00 privelege mode violation
 * 0x01 illegal opcode exception
 * 0x02 access control violation ACV
 *
 * @param i The exception vector.  For this function this simply holds
 *   the exception vector number we use to index into the exception service
 *   vector table.
 */
