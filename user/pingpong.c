#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc > 1){
      fprintf(2, "ping-pong doesn't need params\n");
      exit(1);
  }
  char v = 0;
  
  int parent[2];
  int child[2];
  pipe(parent);
  pipe(child);
  if(fork() == 0){
      //child
      close(child[1]);
      close(parent[0]);
      read(child[0], &v, 1);
      
      if(v==0){
          printf("%d: received ping\n", getpid());
      }else{
          exit(1);
      }
      v = 1;
      write(parent[1], &v, 1);
      close(child[0]);
      close(parent[1]);

  }else{
      //parent
      close(child[0]);
      close(parent[1]);
      write(child[1], &v, 1);
      
      read(parent[0], &v, 1);
      if(v==1){
          printf("%d: received pong\n", getpid());
      }else{
          exit(1);
      }
      close(parent[0]);
      close(child[1]);

  }
  
  exit(0);
}
