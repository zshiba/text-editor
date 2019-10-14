#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

typedef enum _Key{
  DELETE_LEFT = 127, //ASCII table value for DEL
  DELETE_RIGHT,
  DELETE_RIGHT_HALF,
  NEWLINE,
  UP,
  DOWN,
  RIGHT,
  LEFT,
  RIGHTMOST,
  LEFTMOST,
  ACTIVATE_REGION,
  COPY_REGION,
  CUT_REGION,
  PASTE,
  CANCEL_COMMAND,
  QUIT
} Key;

typedef struct _Point{
  int row;
  int column;
} Point;

typedef struct _Region{
  bool isActive;
  Point mark;
  Point point;
  Point* head;
  Point* tail;
} Region;

typedef struct _Row{
  int capacity;
  int size;
  char* raw;
  bool isEnabled;
} Row;

typedef struct _Buffer{
  int capacity;
  int size;
  Row** rows;
  Region region;
} Buffer;

typedef struct _Clip{
  Row* row;
  struct _Clip* next;
} Clip;

typedef struct _Clipboard{
  Clip* head;
} Clipboard;

typedef struct _Cursor{
  int row;
  int column;
} Cursor;

typedef struct _Scroll{
  int row;
  int column;
} Scroll;

typedef struct _LineNumberPane{
  int offset;
  char format[4];
} LineNumberPane;

typedef struct _StatusPane{
  int rows;
  int columns;
  int capacity;
  char* message;
} StatusPane;

typedef struct _Window{
  int rows;
  int columns;
  LineNumberPane lineNumnerPane;
  StatusPane statusPane;
  Scroll scroll;
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
  Clipboard clipboard;
} Editor;

//(message: null-terminated required)
void setMessage(char* message, StatusPane* statusPane){
  if(message == NULL){
    statusPane->message[0] = '\0';
  }else{
    int i = 0;
    while(message[i] != '\0' && i < statusPane->capacity - 1){
      statusPane->message[i] = message[i];
      ++i;
    }
    statusPane->message[i] = '\0';
  }
}

void clearMessage(StatusPane* statusPane){
  setMessage(NULL, statusPane);
}

void mark(Region* region, int row, int column){
  region->isActive = true;
  region->mark.row = row;
  region->mark.column = column;
  region->point.row = row;
  region->point.column = column;
  region->head = &(region->mark);
  region->tail = &(region->point);
}

void activateRegion(Editor* editor){
  Region* region = &(editor->buffer.region);
  int r = editor->cursor.row;
  int c = editor->cursor.column;
  mark(region, r, c);
}

void point(Region* region, int row, int column){
  region->point.row = row;
  region->point.column = column;

  if(region->point.row < region->mark.row){
    region->head = &(region->point);
    region->tail = &(region->mark);
  }else if(region->mark.row < region->point.row){
    region->head = &(region->mark);
    region->tail = &(region->point);
  }else{ //region->mark.row == region->point.row
    if(region->point.column < region->mark.column){
      region->head = &(region->point);
      region->tail = &(region->mark);
    }else{
      region->head = &(region->mark);
      region->tail = &(region->point);
    }
  }
}

void pointRegion(Editor* editor){
  Region* region = &(editor->buffer.region);
  if(region->isActive){
    int r = editor->cursor.row;
    int c = editor->cursor.column;
    point(region, r, c);
  }
}

void deactivateRegion(Editor* editor){
  Region* region = &(editor->buffer.region);
  if(region->isActive){
    region->isActive = false;
    region->mark.row = 0;
    region->mark.column = 0;
    region->point.row = 0;
    region->point.column = 0;
    region->head = NULL;
    region->tail = NULL;
  }
}

void clearClipboard(Clipboard* clipboard){
  Clip* current = clipboard->head;
  while(current != NULL){
    Clip* clip = current;
    current = clip->next;
    free(clip->row->raw);
    free(clip->row);
    free(clip);
  }
  clipboard->head = NULL;
}

Row* createEmptyRow(int capacity){
  Row* row = malloc(sizeof(Row));
  row->capacity = capacity;
  row->size = 0;
  row->raw = malloc(sizeof(char) * row->capacity);
  row->isEnabled = false;
  return row;
}

void setLineNumberOffsetBy(int bufferSize, LineNumberPane* pane){
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

Editor* createEditor(){
  Editor* editor = NULL;

  struct winsize ws;
  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1){
    editor = malloc(sizeof(Editor));
    editor->state = READY;

    editor->window.rows = ws.ws_row;
    editor->window.columns = ws.ws_col;
    editor->window.scroll.row = 0;
    editor->window.scroll.column = 0;
    editor->window.statusPane.rows = 2;
    editor->window.statusPane.columns = editor->window.columns;
    editor->window.statusPane.capacity = editor->window.columns;
    editor->window.statusPane.message = malloc(sizeof(char) * editor->window.statusPane.capacity);
    clearMessage(&(editor->window.statusPane));
    editor->window.frame = malloc(sizeof(char) * ((editor->window.rows * editor->window.columns) + 1));
    editor->window.frame[0] = '\0';

    editor->cursor.column = 0;
    editor->cursor.row = 0;

    editor->buffer.capacity = editor->window.rows;
    editor->buffer.rows = malloc(sizeof(Row*) * editor->buffer.capacity);
    Row* row = createEmptyRow(editor->window.columns);
    editor->buffer.rows[0] = row;
    editor->buffer.size = 1;

    deactivateRegion(editor);

    editor->clipboard.head = NULL;

    setLineNumberOffsetBy(editor->buffer.size, &(editor->window.lineNumnerPane));
  }else{
    perror("createEditor()");
  }
  return editor;
}

void dispose(Editor* editor){
  clearClipboard(&(editor->clipboard));
  for(int i = 0; i < editor->buffer.size; i++){
    free(editor->buffer.rows[i]->raw);
    free(editor->buffer.rows[i]);
  }
  free(editor->buffer.rows);
  free(editor->window.statusPane.message);
  free(editor->window.frame);
  free(editor);
}

void resetScreen(){
  printf("\x1b[2J"); //clear screen
  printf("\x1b[H"); //move cursor to home (top-left)
}

int readKey(){
  const char CTRL = 0x1f; //(0001 1111)
  int c = getchar();
  switch(c){
    case 8: //BS backspace or ctrl-h
    case 127: //DEL
      c = DELETE_LEFT;
      break;

    case (CTRL & 'd'): //ctrl-d
      c = DELETE_RIGHT;
      break;

    case 9: //TAB, \t and ctrl-i
      //ToDo
      break;

    case 10: //LF line feed, \n and ctrl-j
    case 13: //CR carriage return, \r and ctrl-m
      c = NEWLINE;
      break;

    case (CTRL & 'e'): //ctrl-e
      c = RIGHTMOST;
      break;

    case (CTRL & 'a'): //ctrl-a
      c = LEFTMOST;
      break;

    case (CTRL & 'f'): //ctrl-f
      c = RIGHT;
      break;

    case (CTRL & 'b'): //ctrl-b
      c = LEFT;
      break;

    case (CTRL & 'n'): //ctrl-n
      c = DOWN;
      break;

    case (CTRL & 'p'): //ctrl-p
      c = UP;
      break;

    case (CTRL & 'k'): //ctrl-k
      c = DELETE_RIGHT_HALF;
      break;

    case '\x1b': //ESC
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
        }else if(c2 == 'w'){ //alt-w
          c = COPY_REGION;
        }
      }
      break;

    case (CTRL & 'g'): //ctrl-g
      c = CANCEL_COMMAND;
      break;

    case (CTRL & ' '): //ctrl-space
      c = ACTIVATE_REGION;
      break;

    case (CTRL & 'w'): //ctrl-w
      c = CUT_REGION;
      break;

    case (CTRL & 'y'): //ctrl-y
      c = PASTE;
      break;

    case EOF:
    case (CTRL & 'q'): //ctrl-q
      c = QUIT;
      break;

    default:
      break;
  }
//fprintf(stderr, "%x\n", c);
  return c;
}

void scroll(Editor* editor){
  Cursor* cursor = &(editor->cursor);
  Window* window = &(editor->window);
  Scroll* scroll = &(window->scroll);

  int verticalOffset = window->statusPane.rows;
  if(cursor->row < scroll->row) //scroll upward
    scroll->row = cursor->row;
  else if((cursor->row + 1) > scroll->row + (window->rows - verticalOffset)) //scroll downward
    scroll->row = (cursor->row + 1) - (window->rows - verticalOffset);

  int horizontalOffset = window->lineNumnerPane.offset;
  if(cursor->column < scroll->column) //scroll left
    scroll->column = cursor->column;
  else if((cursor->column + 1) > scroll->column + (window->columns - horizontalOffset)) //scroll right
    scroll->column = (cursor->column + 1) - (window->columns - horizontalOffset);
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
  if(editor->cursor.row == editor->buffer.size - 1){
    int r = editor->cursor.row;
    Row* row = editor->buffer.rows[r];
    editor->cursor.column = row->size;
  }else{
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

void moveCursorToRightmost(Editor* editor){
  int r = editor->cursor.row;
  Row* row = editor->buffer.rows[r];
  editor->cursor.column = row->size;
}

void moveCursorToLeftmost(Editor* editor){
  editor->cursor.column = 0;
}

void removeRow(int at, Buffer* buffer){
  if(0 <= at && at < buffer->size){
    Row* row = buffer->rows[at];
    for(int i = at; i < buffer->size - 1; i++)
      buffer->rows[i] = buffer->rows[i + 1];
    --buffer->size;
    free(row->raw);
    free(row);
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
  int r = editor->cursor.row;
  Row* row = editor->buffer.rows[r];
  if(!row->isEnabled)
    row->isEnabled = true;
  if(key == NEWLINE){
    Row* second = partition(row, editor->cursor.column);
    if(0 < second->size || r < editor->buffer.size - 1)
      second->isEnabled = true;
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
      removeRow(r, &(editor->buffer));

      //"previouse" became the last row and is empty
      if(r - 1 == editor->buffer.size - 1 && previous->size == 0)
        previous->isEnabled = false;

      //move cursor to the pinned location
      --editor->cursor.row;
      editor->cursor.column = pin;

      setLineNumberOffsetBy(editor->buffer.size, &(editor->window.lineNumnerPane));
    }
  }else{
    for(int i = c - 1; i < row->size; i++)
      row->raw[i] = row->raw[i + 1];
    --row->size;

    moveCursorLeft(editor);
  }
}

void deleteRightCharacter(Editor* editor){
  int r = editor->cursor.row;
  int c = editor->cursor.column;
  Row* row = editor->buffer.rows[r];
  if(c == row->size){
    if(r != editor->buffer.size - 1){
      Row* next = editor->buffer.rows[r + 1];
      append(next, row);
      removeRow(r + 1, &(editor->buffer));

      setLineNumberOffsetBy(editor->buffer.size, &(editor->window.lineNumnerPane));
    }
  }else{
    for(int i = c; i < row->size - 1; i++)
      row->raw[i] = row->raw[i + 1];
    --row->size;
  }
  //"row" is the last row and is empty
  if(r == editor->buffer.size - 1 && row->size == 0)
    row->isEnabled = false;
}

void deleteRightHalf(Editor* editor){
  int r = editor->cursor.row;
  int c = editor->cursor.column;
  Row* row = editor->buffer.rows[r];
  if(c == row->size){
    if(r != editor->buffer.size - 1){
      Row* next = editor->buffer.rows[r + 1];
      append(next, row);
      removeRow(r + 1, &(editor->buffer));

      setLineNumberOffsetBy(editor->buffer.size, &(editor->window.lineNumnerPane));
    }
  }else{
    row->size = c;
  }
  //"row" is the last row and is empty
  if(r == editor->buffer.size - 1 && row->size == 0)
    row->isEnabled = false;
}

void copyRegion(Editor* editor){
  Buffer* buffer = &(editor->buffer);
  Region* region = &(buffer->region);
  Clipboard* clipboard = &(editor->clipboard);
  if(region->isActive){
    clearClipboard(clipboard);

    Clip* current = clipboard->head;
    Point* head = region->head;
    Point* tail = region->tail;
    for(int r = head->row; r <= tail->row; r++){
      Row* original = buffer->rows[r];
      int start;
      int end;
      if(r == head->row){
        start = head->column;
        if(r == tail->row)
          end = tail->column;
        else
          end = original->size;
      }else if(r == tail->row){
        start = 0;
        end = tail->column;
      }else{
        start = 0;
        end = original->size;
      }
      Row* copy = createEmptyRow(original->capacity);
      int i = 0;
      for(int c = start; c < end; c++){
        copy->raw[i] = original->raw[c];
        ++copy->size;
        ++i;
      }
      Clip* clip = malloc(sizeof(Clip));
      clip->row = copy;
      clip->next = NULL;
      if(current == NULL)
        clipboard->head = clip;
      else
        current->next = clip;
      current = clip;
    }
  }
}

void deleteRegion(Editor* editor){
  Buffer* buffer = &(editor->buffer);
  Region* region = &(buffer->region);
  Cursor* cursor = &(editor->cursor);

  if(region->isActive){
    Point* head = region->head;
    Point* tail = region->tail;
    if(head->row == tail->row){
      if(head->column != tail->column){
        Row* row = buffer->rows[head->row];
        int start = head->column;
        int end = tail->column;
        int n = row->size - end;
        for(int i = 0; i < n; i++)
          row->raw[start + i] = row->raw[end + i];
        row->size -= (end - start);
      }
    }else{
      Row* first = buffer->rows[head->row];
      Row* last = buffer->rows[tail->row];
      Row* row = createEmptyRow(first->capacity + last->capacity);
      for(int i = 0; i < head->column; i++){
        row->raw[i] = first->raw[i];
        ++row->size;
      }
      for(int i = tail->column; i < last->size; i++){
        row->raw[row->size] = last->raw[i];
        ++row->size;
      }
      row->isEnabled = true;

      for(int i = head->row; i <= tail->row; i++){
        Row* r = buffer->rows[i];
        free(r->raw);
        free(r);
      }
      buffer->rows[head->row] = row;

      int m = buffer->size - (tail->row + 1);
      for(int i = 0; i < m; i++)
        buffer->rows[(head->row + 1) + i] = buffer->rows[(tail->row + 1) + i];
      buffer->size -= (tail->row - head->row);
    }
    //move cursor to the begining of the region
    cursor->row = head->row;
    cursor->column = head->column;
  }
}

void pasteFromClipboard(Editor* editor){
  Clipboard* clipboard = &(editor->clipboard);
  if(clipboard->head != NULL){
    int c = editor->cursor.column;
    int r = editor->cursor.row;
    Row* second = partition(editor->buffer.rows[r], c);

    Clip* clip = clipboard->head;
    while(clip != NULL){
      Row* row = clip->row;
      for(int i = 0; i < row->size; i++)
        insert(row->raw[i], editor);

      if(clip->next == NULL){
        r = editor->cursor.row;
        Row* current = editor->buffer.rows[r];
        append(second, current);
        free(second->raw);
        free(second);
      }else{
        insert(NEWLINE, editor);
      }

      clip = clip->next;
    }
  }
}

/*
//for debug
void dumpClipboard(Clipboard* clipboard){
  Clip* current = clipboard->head;
  while(current != NULL){
    Row* row = current->row;
    for(int c = 0; c < row->size; c++){
      fprintf(stderr, "%c", row->raw[c]);
    }
    fprintf(stderr, "\r\n");
    current = current->next;
  }
}
*/

void update(Editor* editor, int key){
  Region* region = &(editor->buffer.region);
  StatusPane* statusPane = &(editor->window.statusPane);

  switch(key){
    case QUIT:
      editor->state = DONE;
      break;

    case DELETE_LEFT:
      if(region->isActive){
        deleteRegion(editor);
        deactivateRegion(editor);
        setMessage("(delete region)", statusPane); //ad-hoc for demo
      }else{
        deleteLeftCharacter(editor);
        setMessage("(delete left)", statusPane); //ad-hoc for demo
      }
      break;

    case DELETE_RIGHT:
      if(region->isActive){
        deleteRegion(editor);
        deactivateRegion(editor);
        setMessage("(delete region)", statusPane); //ad-hoc for demo
      }else{
        deleteRightCharacter(editor);
        setMessage("(delete right)", statusPane); //ad-hoc for demo
      }
      break;

    case UP:
      moveCursorUp(editor);
      if(region->isActive)
        pointRegion(editor);
      setMessage("(up)", statusPane); //ad-hoc for demo
      break;

    case DOWN:
      moveCursorDown(editor);
      if(region->isActive)
        pointRegion(editor);
      setMessage("(down)", statusPane); //ad-hoc for demo
      break;

    case RIGHT:
      moveCursorRight(editor);
      if(region->isActive)
        pointRegion(editor);
      setMessage("(right)", statusPane); //ad-hoc for demo
      break;

    case LEFT:
      moveCursorLeft(editor);
      if(region->isActive)
        pointRegion(editor);
      setMessage("(left)", statusPane); //ad-hoc for demo
      break;

    case RIGHTMOST:
      moveCursorToRightmost(editor);
      if(region->isActive)
        pointRegion(editor);
      setMessage("(right most)", statusPane); //ad-hoc for demo
      break;

    case LEFTMOST:
      moveCursorToLeftmost(editor);
      if(region->isActive)
        pointRegion(editor);
      setMessage("(left most)", statusPane); //ad-hoc for demo
      break;

    case DELETE_RIGHT_HALF:
      if(region->isActive)
        deactivateRegion(editor);
      deleteRightHalf(editor);
      setMessage("(delete right half)", statusPane); //ad-hoc for demo
      break;

    case CANCEL_COMMAND:
      if(region->isActive)
        deactivateRegion(editor);
      setMessage("(cancel)", statusPane); //ad-hoc for demo
      break;

    case ACTIVATE_REGION:
      activateRegion(editor);
      setMessage("(activate region)", statusPane); //ad-hoc for demo
      break;

    case COPY_REGION:
      if(region->isActive){
        copyRegion(editor);
        deactivateRegion(editor);
      }
      setMessage("(copy region)", statusPane); //ad-hoc for demo
//dumpClipboard(&(editor->clipboard)); //for debug
      break;

    case CUT_REGION:
      if(region->isActive){
        copyRegion(editor);
        deleteRegion(editor);
        deactivateRegion(editor);
      }
      setMessage("(cut region)", statusPane); //ad-hoc for demo
      break;

    case PASTE:
      if(region->isActive){
        deleteRegion(editor);
        deactivateRegion(editor);
      }
      pasteFromClipboard(editor);
      setMessage("(paste)", statusPane); //ad-hoc for demo
      break;

    default:
      if(region->isActive){
        deleteRegion(editor);
        deactivateRegion(editor);
      }
      insert(key, editor);
      setMessage("(insert)", statusPane); //ad-hoc for demo
      break;
  }
  scroll(editor);
}

void draw(Editor* editor){
  int horizontalOffset = editor->window.lineNumnerPane.offset;
  int verticalOffset = editor->window.statusPane.rows;
  char* format = editor->window.lineNumnerPane.format;
  Region* region = &(editor->buffer.region);
             //ToDo: use snprintf()
             //ToDo: window needs to hold frame capacity
  int f = 0; //ToDo: expand frame when needed
  char* frame = editor->window.frame;

  f += sprintf(frame + f, "\x1b[?25l"); //hide cursor
  f += sprintf(frame + f, "\x1b[H"); //move cursor to home (top-left)

  bool doneRenderingRegion = !(region->isActive);

  for(int wr = 0; wr < editor->window.rows - verticalOffset; wr++){
    int r = wr + editor->window.scroll.row;
    if(r < editor->buffer.size){
      Row* row = editor->buffer.rows[r];
      if(row->isEnabled){
        bool isCurrentRow;
        if(r == editor->cursor.row)
          isCurrentRow = true;
        else
          isCurrentRow = false;

        //line number pane
        f += sprintf(frame + f, "\x1b[90m"); //90:bright black (foreground)
        f += sprintf(frame + f, format, r + 1);
        f += sprintf(frame + f, "\x1b[0m"); //0: reset

        //highlight current line
        if(isCurrentRow)
          f += sprintf(frame + f, "\x1b[48;5;18m"); //48:(background), 5:(indexed color), 18:(color code)

        bool isRenderingRegion = false;
        for(int wc = 0; wc < editor->window.columns - horizontalOffset; wc++){
          int c = wc + editor->window.scroll.column;

          if(!doneRenderingRegion){
            if(!isRenderingRegion){
              if((r == region->head->row && c == region->head->column) || (region->head->row < r && r <= region->tail->row)){
                isRenderingRegion = true;
                f += sprintf(frame + f, "\x1b[48;5;66m"); //48:(background), 5:(indexed color), 66:(color code)
              }
            }
            if(isRenderingRegion){
              if(r == region->tail->row && c == region->tail->column){
                if(isCurrentRow)
                  f += sprintf(frame + f, "\x1b[48;5;18m"); //48:(background), 5:(indexed color), 18:(color code)
                else
                  f += sprintf(frame + f, "\x1b[0m"); //0:reset
                isRenderingRegion = false;
                doneRenderingRegion = true;
              }
            }
          }

          if(c < row->size){
            if(row->raw[c] == '\t' || iscntrl(row->raw[c])){
              char dummy;
              if(row->raw[c] == '\t')
                dummy = ' '; //ToDo:ad-hoc, 1 space for now
              else //ToDo:ad-hoc, non-printable (<= 31)
                dummy = '?';

              f += sprintf(frame + f, "\x1b[4m"); //4:underline
              f += sprintf(frame + f, "%c", dummy);
              f += sprintf(frame + f, "\x1b[0m"); //0:reset

              if(isCurrentRow)
                f += sprintf(frame + f, "\x1b[48;5;18m"); //highlight current line
            }else{
              frame[f] = row->raw[c];
              ++f;
            }
          }else{
            f += sprintf(frame + f, "\x1b[0K"); //clear rest of line
            break;
          }
        }
        isRenderingRegion = false;
        f += sprintf(frame + f, "\x1b[0m"); //end highlight current line
      }else{ //row is not enabled. Either the buffer is empty or the very last line of the buffer has not been enabled yet.
        for(int i = 0; i < editor->window.lineNumnerPane.offset; i++) //ad-hoc
          f += sprintf(frame + f, " "); //for line number part
        f += sprintf(frame + f, "\x1b[0K"); //clear rest of line
      }
    }else{
      f += sprintf(frame + f, "\x1b[2K"); //clear line
    }
    f += sprintf(frame + f, "\r\n");
  }

  //statu pane
  f += sprintf(frame + f, "\x1b[30;47m"); //30: black (foreground), 47:bright black (background)
  int offset = sprintf(frame + f, "(%d,%d) ", editor->cursor.row + 1, editor->cursor.column);
  f += offset;
  for(int i = 0; i < editor->window.statusPane.columns - offset; i++)
    f += sprintf(frame + f, "-");
  f += sprintf(frame + f, "\x1b[0m"); //0: reset
  f += sprintf(frame + f, "\r\n");

  //message
  f += sprintf(frame + f, "%s", editor->window.statusPane.message);
  f += sprintf(frame + f, "\x1b[K"); //clear rest of line
  //f += sprintf(frame + f, "\x1b[J"); //clear rest of screen

  f += sprintf(frame + f, "\x1b[%d;%dH", editor->cursor.row - editor->window.scroll.row + 1, editor->cursor.column - editor->window.scroll.column + 1 + horizontalOffset); //move cursor
  f += sprintf(frame + f, "\x1b[?25h"); //show cursor
  frame[f] = '\0';

  printf("%s", frame);
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
      resetScreen();

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
