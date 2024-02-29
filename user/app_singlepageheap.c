/*
 * Below is the given application for lab2_challenge2_singlepageheap.
 * This app performs malloc memory.
 */

#include "user_lib.h"
//#include "util/string.h"

typedef unsigned long long uint64;

char* strcpy(char* dest, const char* src) {
  char* d = dest;
  while ((*d++ = *src++))
    ;
  return dest;
}
int main(void) {
  
  char str[20] = "hello, world!!!";
  char *m = (char *)better_malloc(100); // 0~136 (100 roundup 8 to 104, add size of memblock 32, total 136)
  char *p = (char *)better_malloc(50); // 136~224 (50 roundup 8 to 56, add size of memblock 32, total 88, 136 + 88=224)
  if((uint64)p - (uint64)m > 512 ){
    printu("you need to manage the vm space precisely!");
    exit(-1);
  }
  better_free((void *)m); // free 0~136

  strcpy(p,str);
  printu("%s\n",p);
  char *n = (char *)better_malloc(50); // 224~312, following the Worst Fit Strategy
  
  // @assert n - m == 224

  if(n - m != 224)
  {
    printu("your malloc is not complete.\n");
    exit(-1);
  }
//  else{
//    printu("0x%lx 0x%lx\n", m, n);
//  }
  exit(0);
  return 0;
}
