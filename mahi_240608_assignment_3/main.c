#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define QUIT_TIMES 2


struct termios original_terminal;



typedef struct editorRow {

    int size;

    char *chars;

} editorRow;



struct editorConfig {

    int cx;
    int cy;


    int rowOffset;

    int screenRows;
    int screenCols;


    int numberOfRows;

    editorRow *rows;


    char *filename;


    int dirty;

};


struct editorConfig editor;


void editorInsertRowAt(
    int index,
    char *text,
    int length
);


enum editorKey {

    BACKSPACE = 127,

    ENTER = 13,

    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN

};


/*
 Append buffer stores a complete terminal frame
 before writing it to the screen
*/
struct appendBuffer {

    char *buffer;
    int length;

};


#define APPEND_INIT {NULL, 0}


void append(
    struct appendBuffer *ab,
    const char *string,
    int length
) {

    char *new_buffer = realloc(
        ab->buffer,
        ab->length + length
    );


    if (new_buffer == NULL)
        return;


    memcpy(
        &new_buffer[ab->length],
        string,
        length
    );


    ab->buffer = new_buffer;

    ab->length += length;

}



void freeAppendBuffer(struct appendBuffer *ab) {

    free(ab->buffer);

}



void die(const char *message) {

    perror(message);
    exit(1);

}



void disableRawMode() {

    if (tcsetattr(
        STDIN_FILENO,
        TCSAFLUSH,
        &original_terminal
    ) == -1) {

        die("tcsetattr");

    }

}



void enableRawMode() {

    if (tcgetattr(
        STDIN_FILENO,
        &original_terminal
    ) == -1) {

        die("tcgetattr");

    }


    atexit(disableRawMode);


    struct termios raw = original_terminal;


    raw.c_iflag &= ~(IXON | ICRNL);

    raw.c_oflag &= ~(OPOST);

    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);


    if (tcsetattr(
        STDIN_FILENO,
        TCSAFLUSH,
        &raw
    ) == -1) {

        die("tcsetattr");

    }

}



int editorReadKey() {

    char c;


    if (read(STDIN_FILENO, &c, 1) == -1) {

        die("read");

    }


    if (c == '\x1b') {

        char sequence[2];


        if (read(STDIN_FILENO, &sequence[0], 1) != 1)
            return '\x1b';


        if (read(STDIN_FILENO, &sequence[1], 1) != 1)
            return '\x1b';



        if (sequence[0] == '[') {

            switch(sequence[1]) {

                case 'A': return ARROW_UP;

                case 'B': return ARROW_DOWN;

                case 'C': return ARROW_RIGHT;

                case 'D': return ARROW_LEFT;

            }

        }


        return '\x1b';

    }


    return c;

}







void editorRowDeleteChar(editorRow *row, int position) {


    if (position < 0 || position >= row->size) {

        return;

    }


    memmove(
        &row->chars[position],
        &row->chars[position + 1],
        row->size - position
    );


    row->size--;

}



void editorDeleteChar() {


    if (editor.cy == editor.numberOfRows) {

        return;

    }


    if (editor.cx == 0) {

        return;

    }


    editorRowDeleteChar(
        &editor.rows[editor.cy],
        editor.cx - 1
    );


    editor.cx--;


    editor.dirty = 1;

}



void editorRowInsertChar(editorRow *row, int position, int character) {


    if (position < 0 || position > row->size) {

        position = row->size;

    }


    row->chars = realloc(
        row->chars,
        row->size + 2
    );


    memmove(
        &row->chars[position + 1],
        &row->chars[position],
        row->size - position + 1
    );


    row->size++;


    row->chars[position] = character;

}




void editorInsertNewLine() {


    editorRow *row = &editor.rows[editor.cy];


    char *rightSide = malloc(row->size - editor.cx + 1);


    memcpy(
        rightSide,
        &row->chars[editor.cx],
        row->size - editor.cx
    );


    rightSide[row->size - editor.cx] = '\0';


    int rightLength = row->size - editor.cx;


    editorInsertRowAt(
        editor.cy + 1,
        rightSide,
        rightLength
    );


    row = &editor.rows[editor.cy];


    row->size = editor.cx;

    row->chars[row->size] = '\0';


    free(rightSide);


    editor.cy++;

    editor.cx = 0;

}



void editorInsertChar(int character) {


    if (editor.cy == editor.numberOfRows) {

        return;

    }


    editorRowInsertChar(
        &editor.rows[editor.cy],
        editor.cx,
        character
    );


    editor.cx++;


    editor.dirty = 1;

}



void editorInsertRowAt(int index, char *text, int length) {


    if (index < 0 || index > editor.numberOfRows) {

        return;

    }


    editor.rows = realloc(
        editor.rows,
        sizeof(editorRow) * (editor.numberOfRows + 1)
    );


    memmove(
        &editor.rows[index + 1],
        &editor.rows[index],
        sizeof(editorRow) * (editor.numberOfRows - index)
    );


    editor.rows[index].size = length;


    editor.rows[index].chars = malloc(length + 1);


    memcpy(
        editor.rows[index].chars,
        text,
        length
    );


    editor.rows[index].chars[length] = '\0';


    editor.numberOfRows++;

}



void editorInsertRow(char *text, int length) {

    editorInsertRowAt(
        editor.numberOfRows,
        text,
        length
    );

}





char *editorRowsToString(int *bufferLength) {


    int totalLength = 0;


    for (int i = 0; i < editor.numberOfRows; i++) {

        totalLength += editor.rows[i].size + 1;

    }


    *bufferLength = totalLength;


    char *buffer = malloc(totalLength);


    char *position = buffer;


    for (int i = 0; i < editor.numberOfRows; i++) {


        memcpy(
            position,
            editor.rows[i].chars,
            editor.rows[i].size
        );


        position += editor.rows[i].size;


        *position = '\n';

        position++;

    }


    return buffer;

}




void editorSave() {


    if (editor.filename == NULL) {

        return;

    }


    int length;


    char *buffer = editorRowsToString(&length);


    int fileDescriptor = open(
        editor.filename,
        O_RDWR | O_CREAT,
        0644
    );


    if (fileDescriptor != -1) {


        ftruncate(
            fileDescriptor,
            length
        );


        write(
            fileDescriptor,
            buffer,
            length
        );


        close(fileDescriptor);


        editor.dirty = 0;

    }


    free(buffer);

}



void editorOpen(char *filename) {


    editor.filename = strdup(filename);


    FILE *file = fopen(filename, "r");


    if (!file) {

        die("fopen");

    }


    char *line = NULL;

    size_t capacity = 0;

    ssize_t length;



    while ((length = getline(&line, &capacity, file)) != -1) {


        while (
            length > 0 &&
            (
                line[length - 1] == '\n' ||
                line[length - 1] == '\r'
            )
        ) {

            length--;

        }


        editorInsertRow(line, length);

    }


    free(line);

    fclose(file);

}



void getWindowSize() {

    struct winsize size;


    if (ioctl(
        STDOUT_FILENO,
        TIOCGWINSZ,
        &size
    ) == -1) {

        die("ioctl");

    }


    editor.screenRows = size.ws_row;
    editor.screenCols = size.ws_col;

}




void moveCursor(int key) {

    switch(key) {

        case ARROW_LEFT:

            if (editor.cx != 0)
                editor.cx--;

            break;


        case ARROW_RIGHT:

            if (editor.cx != editor.screenCols - 1)
                editor.cx++;

            break;


        case ARROW_UP:

            if (editor.cy != 0)
                editor.cy--;

            break;


        case ARROW_DOWN:

            if (editor.cy < editor.numberOfRows - 1)
                editor.cy++;

            break;

    }

}




void editorScroll() {


    if (editor.cy < editor.rowOffset) {

        editor.rowOffset = editor.cy;

    }


    if (editor.cy >= editor.rowOffset + editor.screenRows) {

        editor.rowOffset = editor.cy - editor.screenRows + 1;

    }

}




void editorDrawStatusBar(struct appendBuffer *ab) {


    append(ab, "\x1b[7m", 4);


    char status[80];


    int length = snprintf(
        status,
        sizeof(status),
        "%.20s - %d lines %s",
        editor.filename ? editor.filename : "[No Name]",
        editor.numberOfRows,
        editor.dirty ? "(modified)" : ""
    );


    if (length > editor.screenCols) {

        length = editor.screenCols;

    }


    append(
        ab,
        status,
        length
    );


    while (length < editor.screenCols) {

        append(ab, " ", 1);

        length++;

    }


    append(ab, "\x1b[m", 3);

}



void editorRefreshScreen() {


    editorScroll();


    struct appendBuffer ab = APPEND_INIT;


    append(&ab, "\x1b[?25l", 6);

    append(&ab, "\x1b[H", 3);

    append(&ab, "\x1b[J", 3);



    for (int i = 0; i < editor.screenRows - 1; i++) {


        int fileRow = i + editor.rowOffset;


        if (fileRow >= editor.numberOfRows) {

            append(&ab, "~", 1);

        }

        else {

            append(
                &ab,
                editor.rows[fileRow].chars,
                editor.rows[fileRow].size
            );

        }


        if (i < editor.screenRows - 1) {

            append(&ab, "\r\n", 2);

        }

    }



    editorDrawStatusBar(&ab);


    char cursorPosition[32];


    snprintf(
        cursorPosition,
        sizeof(cursorPosition),
        "\x1b[%d;%dH",
        (editor.cy - editor.rowOffset) + 1,
        editor.cx + 1
    );


    append(
        &ab,
        cursorPosition,
        strlen(cursorPosition)
    );



    append(&ab, "\x1b[?25h", 6);


    write(
        STDOUT_FILENO,
        ab.buffer,
        ab.length
    );


    freeAppendBuffer(&ab);

}



int main(int argc, char *argv[]) {


    enableRawMode();


    getWindowSize();


    editor.cx = 0;
    editor.cy = 0;


    editor.rowOffset = 0;


    editor.numberOfRows = 0;

    editor.rows = NULL;

    editor.filename = NULL;


    editor.dirty = 0;


    if (argc >= 2) {

        editorOpen(argv[1]);

    }



    int quitTimes = QUIT_TIMES;


    while (1) {


        editorRefreshScreen();


        int c = editorReadKey();


        if (c == 'q') {


            if (editor.dirty && quitTimes > 0) {


                quitTimes--;

                continue;

            }


            break;

        }


        quitTimes = QUIT_TIMES;


        switch(c) {


            case CTRL_KEY('s'):

                editorSave();

                break;


            case BACKSPACE:

                editorDeleteChar();

                break;


            case ENTER:

                editorInsertNewLine();

                break;


            case ARROW_LEFT:
            case ARROW_RIGHT:
            case ARROW_UP:
            case ARROW_DOWN:

                moveCursor(c);
                break;


            default:

                editorInsertChar(c);
                break;

        }

    }


    return 0;

}
