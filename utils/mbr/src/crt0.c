/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright (c) 2003  Marcus R. Brown <mrbrown@0xd6.org>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Ensures the __start entrypoint is placed at 0x100000
#
*/

#include <kernel.h>
#include <startup.h>
#include <stdnoreturn.h> // noreturn

extern char *_end;
extern char *_heap_size;
extern char *_fbss;
extern char *_stack;
extern char *_stack_size;

__attribute__((weak)) void _init();
__attribute__((weak)) void _fini();

int main(int argc, char **argv);

static void _main();

static struct sargs args;
static struct sargs_start *args_start;

/*
 * First function to be called by the loader
 * This function sets up the stack and heap.
 * DO NOT USE THE STACK IN THIS FUNCTION!
 */
void __start(struct sargs_start *pargs) __attribute__((section("._start")));
void __start(struct sargs_start *pargs) {
  asm volatile("# Clear bss area       \n"
               ".set noat              \n"
               "la   $2, _fbss         \n"
               "la   $3, _end          \n"
               "1:                     \n"
               "sltu   $1, $2, $3      \n"
               "beq   $1, $0, 2f       \n"
               "nop                    \n"
               "sq   $0, ($2)          \n"
               "addiu   $2, $2, 16     \n"
               "j   1b                 \n"
               "nop                    \n"
               "2:                     \n"
               "                       \n"
               "# Save first argument  \n"
               "sw     %1, %0          \n"
               "                       \n"
               "# SetupThread          \n"
               "la     $4, _gp         \n"
               "la     $5, _stack      \n"
               "la     $6, _stack_size \n"
               "la     $7, args	    \n"
               "la     $8, ExitThread  \n"
               "move   $gp, $4         \n"
               "addiu  $3, $0, 60      \n"
               "syscall                \n"
               "move   $sp, $2         \n"
               "                       \n"
               "# Jump to _main        \n"
               "j      %2              \n"
               ".set at              \n"
               : /* No outputs. */
               : "m"(args_start), "r"(pargs), "Csy"(_main)
               : "1", "2", "3", "4", "5", "6", "7", "8");
}

/*
 * Intermediate function between _start and main, stack can be used as normal.
 */
static void _main() {
  int retval;
  struct sargs *pa;

  // initialize heap
  SetupHeap(&_end, (int)&_heap_size);

  // writeback data cache
  FlushCache(0);

  // Initialize the kernel (Apply necessary patches).
  _InitSys();

  // Use arguments sent through start if sent (by ps2link for instance)
  pa = &args;
  if (args.argc == 0 && args_start != NULL && args_start->args.argc != 0)
    pa = &args_start->args;

  // Enable interruts
  EI();

  // call global constructors (weak)
  if (_init)
    _init();

  // call main
  retval = main(pa->argc, pa->argv);

  // call global destructors (weak)
  if (_fini)
    _fini();

  // Exit
  Exit(retval);
}

noreturn void _exit(int status) {
  // call global destructors (weak)
  if (_fini)
    _fini();

  // Exit
  Exit(status);
}
