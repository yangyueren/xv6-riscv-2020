#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// extract b from ./a/b
void
extractname(char *path, char *buf)
{
    char *p = path + strlen(path);
    while(*--p != '/'){

    }
    strcpy(buf, p+1);
}

void
find(char *path, char *name)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  if((fd = open(path, 0)) < 0){
      fprintf(2, "find: cannot open %s\n", path);
        return;
  }
  //fstat是查看该fd的元信息
  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }
  switch(st.type){
  case T_FILE:
    extractname(path, buf);
    if(strcmp(buf, name) == 0){
        printf("%s\n", path);
    }
    
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    //文件夹也是一个file，从里面会逐个读取文件夹下的内容，每次读一个struct dirent大小
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
    
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("find: cannot stat %s\n", buf);
        continue;
      }
    //   printf("debug %s\n", buf);
      /* 出现递归
        debug ./.
        debug ././.
        debug ./././.
        debug ././././.
      */
      char *check = buf + strlen(buf);
      while(*check != '/') check--;
      check++;
      if(strcmp(check, ".") == 0) continue;
      if(strcmp(check, "..") == 0) continue;

      find(buf, name);
    //   printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }
  close(fd);

}


int
main(int argc, char *argv[])
{

  if(argc < 3){
    fprintf(2, "find need at least 3 params\n");
    exit(1);
  }
  find(argv[1], argv[2]);
  exit(0);
}
