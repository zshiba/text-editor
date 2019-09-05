#include <stdbool.h>
#include <stdio.h>

int main(){
  bool isDone = false;
  while(!isDone){
    int c = getchar();
    if(c != EOF)
      printf("%c\n", c);
    else
      isDone = true;
  }
  return 0;
}
