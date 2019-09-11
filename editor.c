#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

typedef enum _Key{
  DELETE_LEFT = 127, //ASCII table value for DEL
  NEWLINE,
  UP,
  DOWN,
  RIGHT,
  LEFT,
  QUIT
} Key;

typedef struct _Row{
  int capacity;
  int size;
  char* raw;
} Row;

typedef struct _Buffer{
  int capacity;
  int size;
  Row** rows;
} Buffer;

typedef struct _Cursor{
  int row;
  int column;
} Cursor;

typedef struct _LineNumberPane{
  int offset;
  char format[4];
} LineNumberPane;

typedef struct _Window{
  int rows;
  int columns;
  LineNumberPane lineNumnerPane;
  char* frame;
} Window;

typedef enum _State{
  READY,
  RUNNING,
  DONE
} State;

typedef struct _Editor{
  State state;
  Window window;
  Cursor cursor;
  Buffer buffer;
} Editor;

Row* createEmptyRow(int capacity){
  Row* row = malloc(sizeof(Row));
  row->capacity = capacity;
  row->size = 0;
  row->raw = malloc(sizeof(char) * row->capacity);
  return row;
}

void setLineNumberOffsetBy(int bufferSize, LineNumberPane* pane){
  if(bufferSize == 0){
    pane->offset = 0;
    pane->format[0] = '%';
    pane->format[1] = 'd';
    pane->format[2] = '\0';
    pane->format[3] = '\0';
  }else{
    int offset = 1;
    int s = bufferSize;
    while(s / 10 > 0){
      ++offset;
      s /= 10;
    }
    if(9 < offset)
      offset = 9; //ad-hoc

    pane->offset = offset;
    pane->format[0] = '%';
    pane->format[1] = '0' + offset;
    pane->format[2] = 'd';
    pane->format[3] = '\0';
  }
}

Editor* createEditor(){
  Editor* editor = NULL;

  struct winsize ws;
  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1){
    editor = malloc(sizeof(Editor));
    editor->state = READY;

    editor->window.rows = ws.ws_row;
    editor->window.columns = ws.ws_col;
    editor->window.frame = malloc(sizeof(char) * ((editor->window.rows * editor->window.columns) + 1));
    editor->window.frame[0] = '\0';

    editor->cursor.column = 0;
    editor->cursor.row = 0;

    editor->buffer.capacity = editor->window.rows;
    editor->buffer.rows = malloc(sizeof(Row*) * editor->buffer.capacity);
    Row* row = createEmptyRow(editor->window.columns);
    editor->buffer.rows[0] = row;
    editor->buffer.size = 1;

    setLineNumberOffsetBy(editor->buffer.size, &(editor->window.lineNumnerPane));
  }else{
    perror("createEditor()");
  }
  return editor;
}

void dispose(Editor* editor){
  for(int i = 0; i < editor->buffer.size; i++){
    free(editor->buffer.rows[i]->raw);
    free(editor->buffer.rows[i]);
  }
  free(editor->buffer.rows);
  free(editor->window.frame);
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
      c = DELETE_LEFT;
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

void moveCursorUp(Editor* editor){
  if(0 < editor->cursor.row){
    --editor->cursor.row;
    int r = editor->cursor.row;
    Row* row = editor->buffer.rows[r];
    if(editor->cursor.column > row->size)
      editor->cursor.column = row->size;
  }
}

void moveCursorDown(Editor* editor){
  if(editor->cursor.row < editor->buffer.size - 1){
    ++editor->cursor.row;
    int r = editor->cursor.row;
    Row* row = editor->buffer.rows[r];
    if(editor->cursor.column > row->size)
      editor->cursor.column = row->size;
  }
}

void moveCursorRight(Editor* editor){
  int c = editor->cursor.column;
  int r = editor->cursor.row;
  Row* row = editor->buffer.rows[r];
  if(c < row->size){
    ++editor->cursor.column;
  }else if(c == row->size && r < editor->buffer.size - 1){
    ++editor->cursor.row;
    editor->cursor.column = 0;
  }
}

void moveCursorLeft(Editor* editor){
  int c = editor->cursor.column;
  int r = editor->cursor.row;
  if(0 < c){
    --editor->cursor.column;
  }else if(c == 0 && 0 < r){
    --editor->cursor.row;
    r = editor->cursor.row;
    Row* row = editor->buffer.rows[r];
    editor->cursor.column = row->size;
  }
}

void extend(Row* row){
  row->capacity *= 2; //ad-hoc
  char* extended = malloc(sizeof(char) * row->capacity);
  for(int i = 0; i < row->size; i++)
    extended[i] = row->raw[i];
  free(row->raw);
  row->raw = extended;
}

void add(char character, Row* row, int at){
  if(row->size >= row->capacity)
    extend(row);

  for(int i = row->size; i > at; i--)
    row->raw[i] = row->raw[i - 1];
  row->raw[at] = character;
  ++row->size;
}

void expand(Buffer* buffer){
  buffer->capacity *= 2; //ad-hoc
  Row** expanded = malloc(sizeof(Row*) * buffer->capacity);
  for(int i = 0; i < buffer->size; i++)
    expanded[i] = buffer->rows[i];
  free(buffer->rows);
  buffer->rows = expanded;
}

void inject(Row* row, Buffer* buffer, int at){
  if(buffer->size >= buffer->capacity)
    expand(buffer);

  for(int i = buffer->size; i > at; i--)
    buffer->rows[i] = buffer->rows[i - 1];
  buffer->rows[at] = row;
  ++buffer->size;
}

Row* partition(Row* row, int pivot){
  Row* second = createEmptyRow(row->capacity);
  int size = row->size - pivot;
  for(int i = 0; i < size; i++){
    second->raw[i] = row->raw[pivot + i];
    ++second->size;
  }
  row->size = pivot;
  return second;
}

void insert(int key, Editor* editor){
  Row* row = editor->buffer.rows[editor->cursor.row];
  if(key == NEWLINE){
    Row* second = partition(row, editor->cursor.column);
    inject(second, &(editor->buffer), editor->cursor.row + 1);

    //move cursor to the beginning of the injected row
    ++editor->cursor.row;
    editor->cursor.column = 0;

    setLineNumberOffsetBy(editor->buffer.size, &(editor->window.lineNumnerPane));
  }else{
    add((char)key, row, editor->cursor.column);

    moveCursorRight(editor);
  }
}

void append(Row* one, Row* to){
  while((to->size + one->size) > to->capacity){
    extend(to);
  }
  for(int i = 0; i < one->size; i++){
    to->raw[to->size] = one->raw[i];
    ++to->size;
  }
}

void deleteLeftCharacter(Editor* editor){
  int r = editor->cursor.row;
  int c = editor->cursor.column;
  Row* row = editor->buffer.rows[r];
  if(c == 0){
    if(r != 0){
      Row* previous = editor->buffer.rows[r - 1];
      int pin = previous->size;
      append(row, previous);

      Buffer* buffer = &(editor->buffer);
      for(int i = r; i < buffer->size - 1; i++)
        buffer->rows[i] = buffer->rows[i + 1];
      --buffer->size;
      free(row->raw);
      free(row);

      //move cursor to the pinned location
      --editor->cursor.row;
      editor->cursor.column = pin;

      setLineNumberOffsetBy(editor->buffer.size, &(editor->window.lineNumnerPane));
    }
  }else{
    for(int i = c; i < row->size - 1; i++)
      row->raw[i] = row->raw[i + 1];
    --row->size;

    moveCursorLeft(editor);
  }
}

void update(Editor* editor, int key){
  switch(key){
    case QUIT:
      editor->state = DONE;
      break;
    case DELETE_LEFT:
      deleteLeftCharacter(editor);
      break;
    case UP:
      moveCursorUp(editor);
      break;
    case DOWN:
      moveCursorDown(editor);
      break;
    case RIGHT:
      moveCursorRight(editor);
      break;
    case LEFT:
      moveCursorLeft(editor);
      break;
    default:
      insert(key, editor);
      break;
  }
}

void draw(Editor* editor){
  resetScreen();
  char offset = editor->window.lineNumnerPane.offset;
  char* format = editor->window.lineNumnerPane.format;
  int f = 0; //ToDo: expand frame when needed
  char* frame = editor->window.frame;
  for(int r = 0; r < editor->buffer.size; r++){
    //line number
    f += sprintf(frame + f, "\x1b[90m"); //90: dark gray
    f += sprintf(frame + f, format, r + 1);
    f += sprintf(frame + f, "\x1b[0m"); //0: reset

    //ToDo: replace this with sprintf (null-terminated required)
    Row* row = editor->buffer.rows[r];
    for(int c = 0; c < row->size; c++){
      frame[f] = row->raw[c];
      ++f;
    }
    f += sprintf(frame + f, "\r\n");
  }
  frame[f] = '\0';
  printf("%s", frame);
  printf("\x1b[%d;%dH", editor->cursor.row + 1, editor->cursor.column + 1 + offset);
}

void start(Editor* editor){
  editor->state = RUNNING;
  draw(editor);

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
