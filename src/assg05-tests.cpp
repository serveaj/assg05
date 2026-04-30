/** @file assg05-tests.cpp
 * @brief Unit tests for OS assg 05
 *
 * @author Student Name
 * @note   cwid: 123456
 * @date   Spring 2024
 * @note   ide:  VS Code Editor / IDE ; g++ 8.2.0 / GNU Make 4.2.1
 *
 * Unit tests for assignment 03, implementaiton of LC-3
 * microarchitecture/VM simulator.
 */
#include "catch.hpp"

#include <iostream>
using namespace std;

#define TEST
#include "lc3vm.h"

#define task1_1
#define task1_2
#define task2
#define task3
#define task4_1
#define task4_2
#define task5
#undef task6_1
#undef task6_2
#undef task7_1
#undef task7_2
#undef task7_3
#undef task7_4

/**
 * @brief Task 1: Test privilege and priority functions in PSR
 */
#ifdef task1_1
TEST_CASE("Task 1: PSR user and supervisor privilege", "[task1]")
{
  // clear all PSR bits to 0 before we test
  reg[PSR] &= 0x0;

  // condition flags should not change when we change priority and/or privilege level
  reg[R0] = 0x01;
  update_flags(R0); // FP now set in PSR
  CHECK(RCND == FP);

  // should be in supervisor mode currently since all bits were 0
  CHECK_FALSE(is_user_mode());

  // set to user mode
  user_mode();
  CHECK(is_user_mode());

  // set back to supervisor mode
  supervisor_mode();
  CHECK_FALSE(is_user_mode());

  // RCND should not have been effected
  CHECK(RCND == FP);
}
#endif // task1_1

#ifdef task1_2
TEST_CASE("Task 1: PSR priority level", "[task1]")
{
  // clear all PSR bits to 0 before we test
  reg[PSR] &= 0x0;

  // condition flags should not change when we change priority and/or privilege level
  reg[R0] = 0x01;
  update_flags(R0); // FP now set in PSR
  CHECK(RCND == FP);
  user_mode();
  CHECK(is_user_mode());

  // priority level should be 0 since we cleared the PSR to start test
  CHECK(priority() == 0x00);

  // check setting priority to different levels
  set_priority(0x1);
  CHECK(priority() == 0x1);

  set_priority(0x3);
  CHECK(priority() == 0x3);

  set_priority(0x5);
  CHECK(priority() == 0x5);

  set_priority(0x7);
  CHECK(priority() == 0x7);

  // other PSR bits like user mode and status flags should not be effected
  CHECK(RCND == FP);
  CHECK(is_user_mode());

  // changing the supervisor bit shouldn't effect the priority
  supervisor_mode();
  CHECK_FALSE(is_user_mode());
  CHECK(priority() == 0x7);
  CHECK(RCND == FP);

  // set condition to Negative
  reg[R0] = 0x8000;
  update_flags(R0);

  // change priority some more
  set_priority(0x6);
  CHECK(priority() == 0x6);

  set_priority(0x4);
  CHECK(priority() == 0x4);

  set_priority(0x2);
  CHECK(priority() == 0x2);

  // condition flags and privilege should not have changed
  CHECK_FALSE(is_user_mode());
  CHECK(RCND == FN);
}
#endif // task1_2

/**
 * @brief Task 2: Test push and pop stack manipulation routines
 */
#ifdef task2
TEST_CASE("Task 2: push and pop manipulation routines", "[task2]")
{
  // by convention R6 is used to hold the stack pointer and
  // the supervisor stack starts just below user space
  reg[R6] = 0x3000;
  supervisor_mode();

  // test pushing values, after push top of stack address in R6 was updated and
  // top of stack has the indicated value pushed on to it.
  push(0xdead); // at bottom at 0x2FFF
  CHECK(reg[R6] == 0x2FFF);
  CHECK(mem_read(reg[R6]) == 0xdead);

  push(0xbeef); // 0x2FFE
  CHECK(reg[R6] == 0x2FFE);
  CHECK(mem_read(reg[R6]) == 0xbeef);

  push(0xf00d); // 0x2FFD
  CHECK(reg[R6] == 0x2FFD);
  CHECK(mem_read(reg[R6]) == 0xf00d);

  push(0x1ea7); // at top at 0x2FFC
  CHECK(reg[R6] == 0x2FFC);
  CHECK(mem_read(reg[R6]) == 0x1ea7);

  CHECK(mem_read(0x2FFF) == 0xdead);
  CHECK(mem_read(0x2FFE) == 0xbeef);
  CHECK(mem_read(0x2FFD) == 0xf00d);
  CHECK(mem_read(0x2FFC) == 0x1ea7);

  // test pop and push together
  // pop off top 2 items
  pop();
  pop();
  CHECK(reg[R6] == 0x2FFE);
  CHECK(mem_read(reg[R6]) == 0xbeef);

  push(0x1234);
  CHECK(reg[R6] == 0x2FFD);
  CHECK(mem_read(reg[R6]) == 0x1234);

  pop();
  pop();
  CHECK(reg[R6] == 0x2FFF);
  CHECK(mem_read(reg[R6]) == 0xdead);

  // empty the stack
  pop();
  CHECK(reg[R6] == 0x3000);
}
#endif // task2

/**
 * @brief Task 3: Implement is_running() using MCR master control register, MCR is
 *   mapped to address 0xFFFE, and bit 15 of the MCR is used as the RUN latch.
 *   The fetch/execute cycle checks the clock enable bit / latch and
 *   the machine halts if the latch becomes 0.
 */
#ifdef task3
TEST_CASE("Task 3: MCR run latch mapping and manipulations", "[task3]")
{
  // The MCR is in supivisor I/O memory mapped pages, need supervisor privilege to modify
  // set PSR, privilege, priority and flags, but we will invoke the HALT trap as a user
  reg[PSR] &= 0x0000;
  user_mode();
  set_priority(0x03);
  reg[RPC] = 0x3000; // current PC of running user program
  reg[R0] = 0x8000;
  update_flags(R0); // set FN
  CHECK(reg[PSR] == 0x8304);

  // LC3 should initialize the MCR RUN latch to 1, so should be running
  enable_clock();
  CHECK(is_running());
  CHECK(mem[MCR_ADDR] == 0x8000);
  CHECK(reg[MCR] == 0x8000);
  CHECK(is_user_mode());
  CHECK(reg[PSR] == 0x8304);

  disable_clock();
  CHECK_FALSE(is_running());
  CHECK(mem[MCR_ADDR] == 0x0000);
  CHECK(reg[MCR] == 0x0000);
  CHECK(is_user_mode());
  CHECK(reg[PSR] == 0x8304);
}
#endif

/**
 * @brief Task 4: Implement the `trap()` and `rti()` machine
 *   instructions using the new push/pop and user/supervisor
 *   mode functions.
 */
#ifdef task4_1
TEST_CASE("Task 4: `trap()` instruction implementation", "[task4]")
{
  // load in the trap service table to start of memory 0x0000 - 0x00FF
  char trap_table[] = "./progs/trap-vector-table.obj";
  cout << "Task 4 part 1: loading trap-vector-table.obj for task 4 tests" << endl;
  ld_img(trap_table);
  supervisor_mode();
  CHECK(mem_read(0x0020) == 0x03E0); // spot check some service routine addresses from load
  CHECK(mem_read(0x0025) == 0x0520);

  // first check trap called while in user mode
  // set PSR, stack pointers and PC
  reg[PSR] &= 0x0000;
  user_mode();
  set_priority(0x03);
  reg[RPC] = 0x3333; // current PC of running user program
  reg[R0] = 0x8000;
  update_flags(R0); // set FN
  CHECK(reg[PSR] == 0x8304);
  reg[R6] = 0xFE00;  // user stack starts here
  reg[SSP] = 0x3000; // supervisor stack starts here

  // invoke a trap 25 HALT from user mode.  On a trap we expect
  trap(0x25);
  // 1. we are now in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. R6 is now the supervisor stack pointer and USP has previous user stack pointer
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFE);
  // 3. The RPC and the PSR were pushed onto the stack
  CHECK(mem_read(0x2FFF) == 0x3333); // RPC should be on bottom of stack
  // the old PSR was 0x8304 = 1000 0011 0000 0100 for user mode, priority 3, FN set
  CHECK(mem_read(0x2FFE) == 0x8304);
  // 4. We invoked a HALT trap, so PC should now be in the HALT service routine
  CHECK(reg[RPC] == 0x0520);
  // 5. current PSR should still have same priority and condition flags, just in supervisor mode
  CHECK(reg[PSR] == 0x0304);

  // Now we are in supervisor mode in the TRAP service routine,
  // have the HALT trap invoke say the PUTS trap, to for example output a message to console before halting
  trap(0x22);
  // 1. we should still be in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. The top of the supervisor stack should now have 2 more items on it
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFC);
  // 3. The RPC of the HALT routine and the PSR where pushed on the supervisor stack now
  CHECK(mem_read(0x2FFD) == 0x0520); // RPC of HALT to return to is on stack
  // the PSR should not have changed in this trap, should still be what it was before and is now on stack
  CHECK(mem_read(0x2FFC) == 0x0304);
  // 4. We invoked a PUTS service routine, so PC should now be in the PUTS service routine
  CHECK(reg[RPC] == 0x0460);
  // 5. current PSR should still have same priority and condition flags, just in supervisor mode
  CHECK(reg[PSR] == 0x0304);
}
#endif // task4_1

#ifdef task4_2
TEST_CASE("Task 4: `rti()` instruction implementation", "[task4]")
{
  // load in the trap service table to start of memory 0x0000 - 0x00FF
  char trap_table[] = "./progs/trap-vector-table.obj";
  cout << "Task 4 part 2: loading trap-vector-table.obj for task 4 tests" << endl;
  ld_img(trap_table);
  CHECK(mem_read(0x0020) == 0x03E0); // spot check some service routine addresses from load
  CHECK(mem_read(0x0025) == 0x0520);

  // NOTE: we redo all of the previous tests to set up 2 trap invocations on
  // the system stack to test returns from

  // first check trap called while in user mode
  // set PSR, stack pointers and PC
  reg[PSR] &= 0x0000;
  user_mode();
  set_priority(0x03);
  reg[RPC] = 0x3333; // current PC of running user program
  reg[R0] = 0x8000;
  update_flags(R0); // set FN
  CHECK(reg[PSR] == 0x8304);
  reg[R6] = 0xFE00;  // user stack starts here
  reg[SSP] = 0x3000; // supervisor stack starts here

  // invoke a trap 25 HALT from user mode.  On a trap we expect
  trap(0x25);
  // 1. we are now in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. R6 is now the supervisor stack pointer and USP has previous user stack pointer
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFE);
  // 3. The RPC and the PSR were pushed onto the stack
  CHECK(mem_read(0x2FFF) == 0x3333); // RPC should be on bottom of stack
  // the old PSR was 0x8304 = 1000 0011 0000 0100 for user mode, priority 3, FN set
  CHECK(mem_read(0x2FFE) == 0x8304);
  // 4. We invoked a HALT trap, so PC should now be in the HALT service routine
  CHECK(reg[RPC] == 0x0520);
  // 5. current PSR should still have same priority and condition flags, just in supervisor mode
  CHECK(reg[PSR] == 0x0304);

  // Now we are in supervisor mode in the TRAP service routine,
  // have the HALT trap invoke say the PUTS trap, to for example output a message to console before halting
  trap(0x22);
  // 1. we should still be in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. The top of the supervisor stack should now have 2 more items on it
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFC);
  // 3. The RPC of the HALT routine and the PSR where pushed on the supervisor stack now
  CHECK(mem_read(0x2FFD) == 0x0520); // RPC of HALT to return to is on stack
  // the PSR should not have changed in this trap, should still be what it was before and is now on stack
  CHECK(mem_read(0x2FFC) == 0x0304);
  // 4. We invoked a PUTS service routine, so PC should now be in the PUTS service routine
  CHECK(reg[RPC] == 0x0460);
  // 5. current PSR should still have same priority and condition flags, just in supervisor mode
  CHECK(reg[PSR] == 0x0304);

  // Now test the RTI return from trap or interrupt.  We first return from our current call in PUTS
  uint16_t i = 0x8000; // RTI instruction, not really needed but this is what would invoke rti() if fetch/executed
  rti(i);
  // 1. We returned back to the HALT, should still be in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. The stack had the top PC and PSR popped off of it now
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFE);
  // 3. We are back to the RPC of the HALT routine,
  CHECK(reg[RPC] == 0x0520);
  // 4. PSR should not have changed, we are still in supervisor mode, priority 3, FN set
  CHECK(reg[PSR] == 0x0304);

  // Now we test RTI from supervisor mode back to user mode, if the HALT were to perform a RTI
  // (which it doesn't actually do...)
  rti(i);
  // 1. We returned back to userland that originally called HALT so no longer in supervisor mode
  CHECK(is_user_mode());
  // 2. The stack SSP should have been saved, and R6 should have the user USP restored to it
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[SSP] == 0x3000);
  CHECK(reg[R6] == 0xFE00);
  // 3. We are back to the RPC of original user program that called the first service routine
  CHECK(reg[RPC] == 0x3333);
  // 4. PSR is back to what it was when we started calling the service routine
  CHECK(reg[PSR] == 0x8304);
}
#endif // task4_2

#ifdef task5
TEST_CASE("Task 5: trap and run latch test in start() fetch-decode-execute", "[task5]")
{
  // load in the trap service table to start of memory 0x0000 - 0x00FF
  char trap_table[] = "./progs/trap-vector-table.obj";
  cout << "Task 5: loading trap-vector-table.obj for task 5 tests" << endl;
  ld_img(trap_table);
  supervisor_mode();
  CHECK(mem_read(0x0020) == 0x03E0); // spot check some service routine addresses from load
  CHECK(mem_read(0x0025) == 0x0520);

  // load in the simple halt service routine for this task that when invoked should disable
  // the run latch, causing your modified `start()` function to exit
  char halt_trap[] = "./progs/task5-trap-halt.obj";
  cout << "Task 5: loading task5-trap-halt.obj for task 5 tests" << endl;
  ld_img(halt_trap);
  CHECK(mem_read(0x0520) == 0x3209); // spot check the halt trap is loaded, first instruction is ST R1, #9
  CHECK(mem_read(0x0528) == 0x8000); // RTI instruction at end of routine

  // put trap 0x25 1111 0000 0010 0101 in memory 3000
  mem_write(0x3000, 0xF025);

  // now we need to perform fetch/execute loop, which should halt
  // once the HALT trap turns of the RUN latch
  start(0x00);

  // after we halt, the following should be true
  // 1. we are not running of course, couldn't get here if that
  //    were not true
  CHECK_FALSE(is_running());
  // 2. we should still be in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 3. The PSR should be in supervisor mode, and FZ zero flag should be set because the HALT routine
  //     last AND instruction executed gives a 0 result when masking MCR
  CHECK(reg[PSR] == 0x0002);
  // 3. the RPC should have stoped at the HALT instruction that unlatched
  //    the RUN latch
  CHECK(reg[RPC] == 0x0526);
  // 4. we didn't return from the trap, so the system stack still has the return
  //  PC and return PSR on it
  CHECK(reg[R6] == 0x2FFE);
  CHECK(mem_read(0x2FFE) == 0x8000);
  CHECK(mem_read(0x2FFF) == 0x3001);
  CHECK(mem[MCR_ADDR] == 0x0000);
}
#endif // task5

#ifdef task6_1
TEST_CASE("Task 6: clear KBSR on read of the KBDR value", "[task6]")
{
  // put a character in KBDR and set KBSR to 1 to indicate value ready to read;
  iomap[KBSR] = 0x8000;
  iomap[KBDR] = 0x1234;
  CHECK(mem[KBSR_ADDR] == 0x8000);
  CHECK(mem[KBDR_ADDR] == 0x1234);

  // now test that on a read of the KBDR the KBSR is cleared to 0, indicating no data is currently available
  CHECK(mem_read(KBDR_ADDR) == 0x1234);
  CHECK(iomap[KBSR] == 0x0000);
  CHECK(mem[KBSR_ADDR] == 0x0000);
}
#endif // task6_1

#ifdef task6_2
TEST_CASE("Task 6: clear DSR on write of the DDR value", "[task6]")
{
  // clear the DDR and set DSR to 1 to indicate can receive a new value to display
  iomap[DSR] = 0x8000;
  iomap[DDR] = 0x0000;
  CHECK(mem[DSR_ADDR] == 0x8000);
  CHECK(mem[DDR_ADDR] == 0x0000);

  // now test that on a write of the DDR the DSR is cleared to 0, indicating value is available for display
  // but has not yet been displayed
  mem_write(DDR_ADDR, 0xbeef);
  CHECK(iomap[DSR] == 0x0000);
  CHECK(mem[DSR_ADDR] == 0x0000);
  CHECK(iomap[DDR] == 0xbeef);
  CHECK(mem[DDR_ADDR] == 0xbeef);
}
#endif // task6_1

#ifdef task7_1
TEST_CASE("Task 7: implement `except()` microarchitecture function", "[task7]")
{
  // load in the expection vector service table to memory 0x0100 - 0x0103
  char exception_table[] = "./progs/exception-vector-table.obj";
  cout << "Task 7 part 1: loading exception-vector-table.obj for task 7 tests" << endl;
  ld_img(exception_table);
  CHECK(mem_read(0x0100) == 0x0600); // spot check some exception routine addresses from load
  CHECK(mem_read(0x0101) == 0x0640); // spot check some exception routine addresses from load
  CHECK(mem_read(0x0102) == 0x0680);

  // first check exception invoked while in user mode
  // set PSR, stack pointers and PC
  reg[PSR] &= 0x0000;
  user_mode();
  set_priority(0x03);
  reg[RPC] = 0x3333; // current PC of running user program
  reg[R0] = 0x8000;
  update_flags(R0); // set FN
  CHECK(reg[PSR] == 0x8304);
  reg[R6] = 0xFE00;  // user stack starts here
  reg[SSP] = 0x3000; // supervisor stack starts here

  // invoke an exception 0x02 access control violation in user mode.  On an exception we expect
  except(0x02);
  // 1. we are now in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. R6 is now the supervisor stack pointer and USP has previous user stack pointer
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFE);
  // 3. The RPC and the PSR were pushed onto the stack
  CHECK(mem_read(0x2FFF) == 0x3333); // RPC should be on bottom of stack
  // the old PSR was 0x8304 = 1000 0011 0000 0100 for user mode, priority 3, FN set
  CHECK(mem_read(0x2FFE) == 0x8304);
  // 4. We invoked a ACV exception, so PC should now be in the access control violation exception routine
  CHECK(reg[RPC] == 0x0680);
  // 5. current PSR should still have same priority and condition flags, just in supervisor mode
  CHECK(reg[PSR] == 0x0304);

  // Now we are in supervisor mode in the ACV exception service routine,
  // have a privilege mode exception occur
  except(0x00);
  // 1. we should still be in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. The top of the supervisor stack should now have 2 more items on it
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFC);
  // 3. The RPC of the previous exception and the PSR where pushed on the supervisor stack now
  CHECK(mem_read(0x2FFD) == 0x0680); // RPC of exception to return to is on stack
  // the PSR should not have changed in this trap, should still be what it was before and is now on stack
  CHECK(mem_read(0x2FFC) == 0x0304);
  // 4. We invoked a privilige mode exception, so PC should now be in the privilege mode exception service routine
  CHECK(reg[RPC] == 0x0600);
  // 5. current PSR should still have same priority and condition flags, just in supervisor mode
  CHECK(reg[PSR] == 0x0304);

  // Now test the RTI return from trap or interrupt.  We first return from our most recent instruction
  uint16_t i = 0x8000; // RTI instruction, not really needed but this is what would invoke rti() if fetch/executed
  rti(i);
  // 1. We returned back to the original access control violation, should still be in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. The stack had the top PC and PSR popped off of it now
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFE);
  // 3. We are back to the RPC of the original exception from ACV
  CHECK(reg[RPC] == 0x0680);
  // 4. PSR should not have changed, we are still in supervisor mode, priority 3, FN set
  CHECK(reg[PSR] == 0x0304);

  // Now we test RTI from supervisor mode back to user mode, if an RTI from the ACV occurs
  rti(i);
  // 1. We returned back to userland that originally called HALT so no longer in supervisor mode
  CHECK(is_user_mode());
  // 2. The stack SSP should have been saved, and R6 should have the user USP restored to it
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[SSP] == 0x3000);
  CHECK(reg[R6] == 0xFE00);
  // 3. We are back to the RPC of original user program that called the first service routine
  CHECK(reg[RPC] == 0x3333);
  // 4. PSR is back to what it was when we started calling the service routine
  CHECK(reg[PSR] == 0x8304);
}
#endif // task7_1

#ifdef task7_2
TEST_CASE("Task 7: invoke privilege mode exception", "[task7]")
{
  // load in the expection vector service table to memory 0x0100 - 0x0103
  char exception_table[] = "./progs/exception-vector-table.obj";
  cout << "Task 7 part 2: loading exception-vector-table.obj for task 7 tests" << endl;
  ld_img(exception_table);
  supervisor_mode();
  CHECK(mem_read(0x0100) == 0x0600); // spot check some exception routine addresses from load
  CHECK(mem_read(0x0101) == 0x0640); // spot check some exception routine addresses from load
  CHECK(mem_read(0x0102) == 0x0680);

  // set registers, priority, RPC and put us in user mode
  reg[PSR] &= 0x0000;
  user_mode();
  set_priority(0x03);
  reg[RPC] = 0x3333; // current PC of running user program
  reg[R0] = 0x8000;
  update_flags(R0); // set FN
  CHECK(reg[PSR] == 0x8304);
  reg[R6] = 0xFE00;  // user stack starts here
  reg[SSP] = 0x3000; // supervisor stack starts here

  // cause rti() to be called
  uint16_t i = 0x8000; // rti instruction
  rti(i);

  // should cause exception, so we should
  // 1. be in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. The top of the supervisor stack should now have 2 more items on it
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFE);
  // 3. The RPC where we were when exception occured and the PSR are on the system stack
  CHECK(mem_read(0x2FFF) == 0x3333); // RPC of location where excption occurred
  // the PSR should not have changed in this trap, should still be what it was before and is now on stack
  CHECK(mem_read(0x2FFE) == 0x8304);
  // 4. We caused a privilege mode exception so should be in privilege mode exception service routine
  CHECK(reg[RPC] == 0x0600);
  // 5. current PSR should still have same priority and condition flags, just in supervisor mode
  CHECK(reg[PSR] == 0x0304);
}
#endif // task7_2

#ifdef task7_3
TEST_CASE("Task 7: invoke illegal opcode exception", "[task7]")
{
  // load in the expection vector service table to memory 0x0100 - 0x0103
  char exception_table[] = "./progs/exception-vector-table.obj";
  cout << "Task 7 part 3: loading exception-vector-table.obj for task 7 tests" << endl;
  ld_img(exception_table);
  CHECK(mem_read(0x0100) == 0x0600); // spot check some exception routine addresses from load
  CHECK(mem_read(0x0101) == 0x0640); // spot check some exception routine addresses from load
  CHECK(mem_read(0x0102) == 0x0680);

  // set registers, priority, RPC and put us in user mode
  reg[PSR] &= 0x0000;
  user_mode();
  set_priority(0x03);
  reg[RPC] = 0x3333; // current PC of running user program
  reg[R0] = 0x8000;
  update_flags(R0); // set FN
  CHECK(reg[PSR] == 0x8304);
  reg[R6] = 0xFE00;  // user stack starts here
  reg[SSP] = 0x3000; // supervisor stack starts here

  // cause res() to be called
  uint16_t i = 0xD000; // reserved instruction
  res(i);

  // should cause exception, so we should
  // 1. be in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. The top of the supervisor stack should now have 2 more items on it
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFE);
  // 3. The RPC where we were when exception occured and the PSR are on the system stack
  CHECK(mem_read(0x2FFF) == 0x3333); // RPC of location where excption occurred
  // the PSR should not have changed in this trap, should still be what it was before and is now on stack
  CHECK(mem_read(0x2FFE) == 0x8304);
  // 4. We caused a illegal opcode exception so should be in illegal opcode exception service routine
  CHECK(reg[RPC] == 0x0640);
  // 5. current PSR should still have same priority and condition flags, just in supervisor mode
  CHECK(reg[PSR] == 0x0304);
}
#endif // task7_3

#ifdef task7_4
TEST_CASE("Task 7: invoke access control violation (ACV)", "[task7]")
{
  // load in the expection vector service table to memory 0x0100 - 0x0103
  char exception_table[] = "./progs/exception-vector-table.obj";
  cout << "Task 7 part 4: loading exception-vector-table.obj for task 7 tests" << endl;
  ld_img(exception_table);
  CHECK(mem_read(0x0100) == 0x0600); // spot check some exception routine addresses from load
  CHECK(mem_read(0x0101) == 0x0640); // spot check some exception routine addresses from load
  CHECK(mem_read(0x0102) == 0x0680);

  // set registers, priority, RPC and put us in user mode
  reg[PSR] &= 0x0000;
  user_mode();
  set_priority(0x03);
  reg[RPC] = 0x3333; // current PC of running user program
  reg[R0] = 0x8000;
  update_flags(R0); // set FN
  CHECK(reg[PSR] == 0x8304);
  reg[R6] = 0xFE00;  // user stack starts here
  reg[SSP] = 0x3000; // supervisor stack starts here

  // in user mode, write and read to unprivileged memory should succeed
  mem_write(0x3333, 0xbeef);
  CHECK(mem_read(0x3333) == 0xbeef);
  CHECK(is_user_mode());
  CHECK(reg[RPC] == 0x3333);

  // in user mode, an attempt to read from system space should cause access control violation
  CHECK(mem_read(0x00FF) == 0x0000);

  // should cause exception, so we should
  // 1. be in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. The top of the supervisor stack should now have 2 more items on it
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFE);
  // 3. The RPC where we were when exception occured and the PSR are on the system stack
  CHECK(mem_read(0x2FFF) == 0x3333); // RPC of location where excption occurred
  // the PSR should not have changed in this trap, should still be what it was before and is now on stack
  CHECK(mem_read(0x2FFE) == 0x8304);
  // 4. We caused a access control violation so should be in ACV exception service routine
  CHECK(reg[RPC] == 0x0680);
  // 5. current PSR should still have same priority and condition flags, just in supervisor mode
  CHECK(reg[PSR] == 0x0304);

  // reset registers to try a read now from device register space
  reg[PSR] &= 0x0000;
  user_mode();
  set_priority(0x03);
  reg[RPC] = 0x3333; // current PC of running user program
  reg[R0] = 0x8000;
  update_flags(R0); // set FN
  CHECK(reg[PSR] == 0x8304);
  reg[R6] = 0xFE00;  // user stack starts here
  reg[SSP] = 0x3000; // supervisor stack starts here

  // in user mode, an attempt to read from device register space should cause access control violation
  CHECK(mem_read(0xFFFF) == 0x0000);

  // should cause exception, so we should
  // 1. be in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. The top of the supervisor stack should now have 2 more items on it
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFE);
  // 3. The RPC where we were when exception occured and the PSR are on the system stack
  CHECK(mem_read(0x2FFF) == 0x3333); // RPC of location where excption occurred
  // the PSR should not have changed in this trap, should still be what it was before and is now on stack
  CHECK(mem_read(0x2FFE) == 0x8304);
  // 4. We caused a access control violation so should be in ACV exception service routine
  CHECK(reg[RPC] == 0x0680);
  // 5. current PSR should still have same priority and condition flags, just in supervisor mode
  CHECK(reg[PSR] == 0x0304);

  // reset registers to try a write now into system space
  reg[PSR] &= 0x0000;
  user_mode();
  set_priority(0x03);
  reg[RPC] = 0x3333; // current PC of running user program
  reg[R0] = 0x8000;
  update_flags(R0); // set FN
  CHECK(reg[PSR] == 0x8304);
  reg[R6] = 0xFE00;  // user stack starts here
  reg[SSP] = 0x3000; // supervisor stack starts here

  // in user mode, an attempt to write to system space should cause an access control violation
  mem_write(0x2fff, 0xbeef);

  // should cause exception, so we should
  // 1. be in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. The top of the supervisor stack should now have 2 more items on it
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFE);
  // 3. The RPC where we were when exception occured and the PSR are on the system stack
  CHECK(mem_read(0x2FFF) == 0x3333); // RPC of location where excption occurred
  // the PSR should not have changed in this trap, should still be what it was before and is now on stack
  CHECK(mem_read(0x2FFE) == 0x8304);
  // 4. We caused a access control violation so should be in ACV exception service routine
  CHECK(reg[RPC] == 0x0680);
  // 5. current PSR should still have same priority and condition flags, just in supervisor mode
  CHECK(reg[PSR] == 0x0304);

  // reset registers to try a write finally now into the device register space
  reg[PSR] &= 0x0000;
  user_mode();
  set_priority(0x03);
  reg[RPC] = 0x3333; // current PC of running user program
  reg[R0] = 0x8000;
  update_flags(R0); // set FN
  CHECK(reg[PSR] == 0x8304);
  reg[R6] = 0xFE00;  // user stack starts here
  reg[SSP] = 0x3000; // supervisor stack starts here

  // in user mode, an attempt to write to device register space should cause an access control violation
  mem_write(0xFE00, 0xbeef);

  // should cause exception, so we should
  // 1. be in supervisor mode
  CHECK_FALSE(is_user_mode());
  // 2. The top of the supervisor stack should now have 2 more items on it
  CHECK(reg[USP] == 0xFE00);
  CHECK(reg[R6] == 0x2FFE);
  // 3. The RPC where we were when exception occured and the PSR are on the system stack
  CHECK(mem_read(0x2FFF) == 0x3333); // RPC of location where excption occurred
  // the PSR should not have changed in this trap, should still be what it was before and is now on stack
  CHECK(mem_read(0x2FFE) == 0x8304);
  // 4. We caused a access control violation so should be in ACV exception service routine
  CHECK(reg[RPC] == 0x0680);
  // 5. current PSR should still have same priority and condition flags, just in supervisor mode
  CHECK(reg[PSR] == 0x0304);
}
#endif // task7_4