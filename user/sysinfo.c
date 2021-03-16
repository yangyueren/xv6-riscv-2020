#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/sysinfo.h"

int
main(int argc, char *argv[])
{
  if(argc > 1){
    fprintf(2, "usage: sysinfo\n");
    exit(1);
  }
  struct sysinfo info;
  
  sysinfo(&info);
  printf("free space %d bytes, not unused process %d\n", info.freemem, info.nproc);
  exit(0);
}
