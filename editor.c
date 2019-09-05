#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct _Editor{
} Editor;

Editor* createEditor(){
  Editor* editor = malloc(sizeof(Editor));
  return editor;
}

void start(Editor* editor){
  bool isDone = false;
  while(!isDone){
    int c = getchar();
    if(c == EOF || c == 'q')
      isDone = true;
    else
      printf("%c\n", c);
  }
}

int main(){
  Editor* editor = createEditor();
  start(editor);
  free(editor);
  return 0;
}
