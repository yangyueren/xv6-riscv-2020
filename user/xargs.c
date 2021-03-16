
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char *args[MAXARG];
  int ini_arg_num = 0;
  char buf[256] = {0};
//   printf("argc %d\n", argc);
  for(int i=1; i<argc; ++i){
      if(strcmp(argv[i], "-n") == 0 && i==1){
          ++i;
      }else{
          args[ini_arg_num++] = argv[i];
        //   printf("%d %s\n", ini_arg_num-1, argv[i]);
      }
  }
  /*
  这里有问题 无法解析\n
  $ echo "1\n2" | xargs -n 1 echo line
    "1\n2"
    line "1\n2"

  */
  while(gets( buf, sizeof(buf))){
      if(strlen(buf) < 1) break;
    //   printf("%d\n%s", strlen(buf), buf);
      buf[strlen(buf)-1] = 0; //清除gets读入的\n
    //   printf("%s\n", buf);
      int arg_num = ini_arg_num;
      char *p = buf;
      while (*p)
      {
          while ((*p == ' ') && *p) *p++ = 0;
          if(*p) args[arg_num++] = p;
          while ((*p != ' ') && *p) p++;
      }
      if(arg_num >= MAXARG) fprintf(2, "too many args\n");
      if(arg_num < 1) fprintf(2, "too few args for xargs\n");
      args[arg_num] = 0;
      if(fork() == 0){
        //   printf("%s\n", args[0]);
        //   printf("%s %s %s\n", args[1], args[2], args[3]);
        exec(args[0], args);
        exit(0);
      }else{
          wait(0);
      }
      
  }
  exit(0);
}
