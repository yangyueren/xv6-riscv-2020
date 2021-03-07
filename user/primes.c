#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

/*
解题思路：
buffer的作用是保存这轮不能被筛除的数
*/

int
main(int argc, char *argv[])
{

    int fd[2];

    int buffer[36];
    int cnt = 0;
    for (int i = 2; i < 36; i++)
    {
        buffer[cnt++] = i;
    }
    
    while (cnt > 0)
    {   
        pipe(fd);
        if(fork() == 0)
        {
            close(fd[1]);
            int base = 0;
            cnt = -1;
            int v;
            while (read(fd[0], &v, sizeof(v)))
            {
                if(cnt == -1){
                    base = v;
                    cnt = 0;
                }else if(v % base != 0){
                    buffer[cnt++] = v;
                }
            }
            close(fd[0]);
            printf("prime %d\n", base);
        }
        else
        {
            close(fd[0]);
            for (int i = 0; i < cnt; i++)
            {
                write(fd[1], &buffer[i], sizeof(buffer[i]));
                
            }
            close(fd[1]);
            wait(0);
            break;

        }
    }
    
  
    
      
  
  exit(0);
}
