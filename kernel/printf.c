//
// formatted console output -- printf, panic.
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

volatile int panicked = 0;

// lock to avoid interleaving concurrent printf's.
static struct {
  struct spinlock lock;
  int locking;
} pr;

static char digits[] = "0123456789abcdef";

static void
printint(int xx, int base, int sign)
{
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}

static void
printptr(uint64 x)
{
  int i;
  consputc('0');
  consputc('x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the console. only understands %d, %x, %p, %s.
void
printf(char *fmt, ...)
{
  va_list ap;
  int i, c, locking;
  char *s;

  locking = pr.locking;
  if(locking)
    acquire(&pr.lock);

  if (fmt == 0)
    panic("null fmt");

  va_start(ap, fmt);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(va_arg(ap, int), 10, 1);
      break;
    case 'x':
      printint(va_arg(ap, int), 16, 1);
      break;
    case 'p':
      printptr(va_arg(ap, uint64));
      break;
    case 's':
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&pr.lock);
}

void
panic(char *s)
{
  pr.locking = 0;
  printf("panic: ");
  printf(s);
  printf("\n");
  panicked = 1; // freeze uart output from other CPUs
  for(;;)
    ;
}

void
printfinit(void)
{
  initlock(&pr.lock, "pr");
  pr.locking = 1;
}

//0x0000000080002db8
//0x0000000080002c1c
//0x0000000080002906
//0x0000000000000012


/*
 *
 * -----------  tramploine
 * guard page
 * -----------
 * use here as kstack1
 * ----------- kstack1
 * guard page
 * -----------
 * kstack2
 * -----------kstack2
 *
 * -----------
 *
 * -----------
 *
 *
 * 在PGROUNDUP(fp)放的是从user space进入kernel space的返回地址和fp
    ra  0x0000000080002dcc
    fp  0x0000003fffff9fc0
    ra  0x0000000080002c30
    fp  0x0000003fffff9fe0
    ra  0x000000008000291a
    fp  0x0000003fffffa000
    ra  0x0000000000000012    ->    user space return address
    fp  0x0000000000002fe0

 *
 *
 *
 *
 */




void
backtrace(void)
{
  uint64 fp = r_fp();
//  printf("kstack %p\n", (void*)myproc()->kstack);
//  printf("fp %p\n", (void*)fp);
  uint64 lo = PGROUNDDOWN(fp);
  uint64 hi = PGROUNDUP(fp);
//  printf("fp lo %p, hi %p\n", (void*)lo, (void *)hi);
//  printf("kstack lo %p, hi %p\n", (void*)PGROUNDDOWN(myproc()->kstack), (void *)(PGROUNDUP(myproc()->kstack)));
  printf("backtrace:\n");
  while (fp >= lo && fp < hi){
      printf("%p\n", *(uint64*)(fp-8));
      fp = *((uint64*)(void*)(fp-16));

//      printf("fp %p\n", (void*)fp);
  }
  return;
}