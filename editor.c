#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

typedef enum _State{
  READY,
  RUNNING,
  DONE
} State;

typedef struct _Editor{
  State state;
  unsigned int bufferCapacity;
  unsigned int bufferSize;
  char* buffer;
} Editor;

Editor* createEditor(){
  Editor* editor = malloc(sizeof(Editor));
  editor->state = READY;
  editor->bufferCapacity = 256;
  editor->bufferSize = 0;
  editor->buffer = malloc(sizeof(char) * editor->bufferCapacity);
  return editor;
}

void resetScreen(){
  printf("\x1b[2J"); //clear screen
  printf("\x1b[H"); //move cursor to home (top-left)
}

void update(Editor* editor, int key){
  if(key == EOF || key == 'q'){
    editor->state = DONE;
  }else{
    if(editor->bufferSize < editor->bufferCapacity){
      editor->buffer[editor->bufferSize] = key;
      ++editor->bufferSize;
    }else{
      //ToDo: expand buffer
    }
  }
}

void draw(Editor* editor){
  resetScreen();
  for(unsigned int i = 0; i < editor->bufferSize; i++)
    printf("%c", editor->buffer[i]);
}

void start(Editor* editor){
  resetScreen();
  editor->state = RUNNING;

  while(editor->state == RUNNING){
    int key = getchar();
    update(editor, key);
    draw(editor);
  }
}

struct termios* createRawModeSettinsFrom(struct termios* terminalIOMode){
  struct termios* raw = malloc(sizeof(struct termios));
  memcpy(raw, terminalIOMode, sizeof(struct termios));

  //(input mode) BRKINT: (for SIGINT)
  //             ICRNL: for CTRL+M
  //             INPCK: (for input parity check)
  //             ISTRIP: (for strip character)
  //             IXON: for CTRL+S and CTRL+Q
  raw->c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

  //(output mode) OPOST: to turn off post-processing features
  raw->c_oflag &= ~(OPOST);

  //(control mode) CS8: (to set CSIZE, Character size, to be 8 bits.)
  raw->c_cflag |= (CS8);

  //(local mode) ECHO: to turn off echoing
  //             ICANON: to read inputs byte-by-byte
  //             IEXTEN: for CTRL+V
  //             ISIG: for CTRL+C
  raw->c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  //(control chars) VMIN: to wait to read 1 byte
  //                VTIME: to wait 0.X sec
  raw->c_cc[VMIN] = 1;
  //raw->c_cc[VTIME] = 1;
  return raw;
}

int main(){
  struct termios original;
  if(tcgetattr(STDIN_FILENO, &original) != -1){
    struct termios* raw = createRawModeSettinsFrom(&original);
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, raw) != -1){
      free(raw);

      Editor* editor = createEditor();
      start(editor);
      free(editor);

      if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &original) == -1)
        perror("tcsetattr (original)");
      resetScreen();
    }else{
      perror("tcsetattr (raw)");
    }
  }else{
    perror("tcgetattr (original)");
  }
  return 0;
}
