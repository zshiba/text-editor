#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

typedef enum _Key{
  DELETE = 127, //ASCII table value
  NEWLINE,
  UP,
  DOWN,
  RIGHT,
  LEFT,
  QUIT
} Key;

typedef struct _Window{
  unsigned short rows;
  unsigned short columns;
} Window;

typedef enum _State{
  READY,
  RUNNING,
  DONE
} State;

typedef struct _Editor{
  State state;
  Window window;
  unsigned int cursorColumn;
  unsigned int cursorRow;
  unsigned int bufferCapacity;
  unsigned int bufferSize;
  char* buffer;
} Editor;

Editor* createEditor(){
  Editor* editor = NULL;

  struct winsize ws;
  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1){
    editor = malloc(sizeof(Editor));
    editor->state = READY;

    editor->window.rows = ws.ws_row;
    editor->window.columns = ws.ws_col;

    editor->cursorColumn = 0;
    editor->cursorRow = 0;

    editor->bufferCapacity = (editor->window.rows * editor->window.columns);
    editor->bufferSize = 0;
    editor->buffer = malloc(sizeof(char) * editor->bufferCapacity);
  }else{
    perror("createEditor()");
  }
  return editor;
}

void dispose(Editor* editor){
  free(editor->buffer);
  free(editor);
}

void resetScreen(){
  printf("\x1b[2J"); //clear screen
  printf("\x1b[H"); //move cursor to home (top-left)
}


int readKey(){
  int c = getchar();
  switch(c){
    case 8: //BS backspace
    case 127: //DEL
      c = DELETE;
      break;

    case 10: //LF line feed
    case 13: //CR carriage return
      c = NEWLINE;
      break;

    case '\x1b':
      {
        int c2 = getchar();
        if(c2 == '['){
          int c3 = getchar();
          if(c3 == 'A')
            c = UP;
          else if(c3 == 'B')
            c = DOWN;
          else if(c3 == 'C')
            c = RIGHT;
          else if(c3 == 'D')
            c = LEFT;
        }
      }
      break;

    case EOF:
    case ('q' & 0x1f): //CTRL: 0x1f (0001 1111)
      c = QUIT;
      break;

    default:
      break;
  }
  return c;
}

void update(Editor* editor, int key){
  if(key == QUIT){
    editor->state = DONE;
  }else if(key == DELETE){
    if(0 < editor->bufferSize){
      char c = editor->buffer[editor->bufferSize - 1];
      --editor->bufferSize;
      if(c == '\n' && 0 < editor->bufferSize && editor->buffer[editor->bufferSize - 1] == '\r')
        --editor->bufferSize;

      if(0 < editor->cursorColumn)
        --editor->cursorColumn;
    }
  }else if(key == UP){
    if(0 < editor->cursorRow)
      --editor->cursorRow;
  }else if(key == DOWN){
    if(editor->cursorRow < editor->window.rows - 1)
      ++editor->cursorRow;
  }else if(key == RIGHT){
    if(editor->cursorColumn < editor->window.columns - 1)
      ++editor->cursorColumn;
  }else if(key == LEFT){
    if(0 < editor->cursorColumn)
      --editor->cursorColumn;
  }else{
    if(editor->bufferSize < editor->bufferCapacity){
      if(key == NEWLINE){
        editor->buffer[editor->bufferSize] = '\r';
        ++editor->bufferSize;
        editor->buffer[editor->bufferSize] = '\n';
        ++editor->bufferSize;

        editor->cursorColumn = 0;
        ++editor->cursorRow;
      }else{
        editor->buffer[editor->bufferSize] = key;
        ++editor->bufferSize;

        if(editor->cursorColumn < editor->window.columns - 1){
          ++editor->cursorColumn;
        }else{
          editor->cursorColumn = 0;
          ++editor->cursorRow;
        }
      }
    }else{
      //ToDo: expand buffer
    }
  }
}

void draw(Editor* editor){
  resetScreen();
  editor->buffer[editor->bufferSize] = '\0'; //ToDo: ad-hoc
  printf("%s", editor->buffer);
  printf("\x1b[%d;%dH", editor->cursorRow + 1, editor->cursorColumn + 1);
}

void start(Editor* editor){
  resetScreen();
  editor->state = RUNNING;

  while(editor->state == RUNNING){
    int key = readKey();
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
      if(editor != NULL){
        start(editor);
        dispose(editor);
      }

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
