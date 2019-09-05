#include <stdio.h>
#include <unistd.h>

int main(){
  char in;
  ssize_t bytes = read(STDIN_FILENO, &in, 1);
  if(bytes != -1)
    printf("%c\n", in);
  return 0;
}
