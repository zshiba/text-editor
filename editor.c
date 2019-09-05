#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct _Editor{
} Editor;

Editor* createEditor(){
  Editor* editor = malloc(sizeof(Editor));
  return editor;
}

int main(){
  Editor* editor = createEditor();

  bool isDone = false;
  while(!isDone){
    int c = getchar();
    if(c != EOF)
      printf("%c\n", c);
    else
      isDone = true;
  }

  free(editor);
  return 0;
}
