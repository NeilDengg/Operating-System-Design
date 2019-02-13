#include "userapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

void factorial (int fact){

	int val =1;
	while(fact--!=0){
		val *= fact ;
	}
}

int main(int argc, char* argv[])
{
  /*
  printf("%d\n", (int) sizeof(argv));
  if((int) sizeof(argv)<3){
    printf("Usage: ./userapp repeat_time number\n"); 
    exit(1);
  }
  */
    
	int pid = getpid();
	printf("\n=====I'm registering my process.\n");
	// FILE* file = fopen("")
	pid_t mypid = getpid();
	FILE *f = fopen ("/proc/mp1/status", "a");
	int written_bytes = fprintf(f, "%d\0", mypid);
	fclose(f);
	printf("My pid is %i, pid has been registered\n", mypid);

	printf("Factorial and call kernel starts, this test has %d rounds.\n", time_factorial);
	while(time_factorial--!=0){
		factorial(value_factorial);
//		call_kernel();
	}
	return 0;
}