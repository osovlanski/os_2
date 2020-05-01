#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char **argv)
{
  int i;

  if(argc < 3){
    printf(2, "usage: kill pid... signal\n");
    exit();
  }
  for(i=1; i<argc-1; i++)
    kill(atoi(argv[i]),atoi(argv[argc-1]));
  exit();
}
