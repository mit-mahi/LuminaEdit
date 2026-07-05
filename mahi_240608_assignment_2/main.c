#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>


#define CTRL_KEY(k) ((k) & 0x1f)


enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN
};


struct editorConfig {

    int cx;
    int cy;

    int screenRows;
    int screenCols;

    struct termios originalTerminal;

};


struct editorConfig editor;


void die(const char *message) {

    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(message);

    exit(1);

}


void disableRawMode() {

    tcsetattr(
        STDIN_FILENO,
        TCSAFLUSH,
        &editor.originalTerminal
    );

}


void enableRawMode() {


    if (tcgetattr(
        STDIN_FILENO,
        &editor.originalTerminal
    ) == -1) {

        die("tcgetattr");

    }


    atexit(disableRawMode);


    struct termios raw = editor.originalTerminal;


    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    raw.c_oflag &= ~(OPOST);

    raw.c_cflag |= CS8;

    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);


    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;


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


    while (read(STDIN_FILENO, &c, 1) != 1);


    if (c == '\x1b') {


        char sequence[2];


        if (read(STDIN_FILENO, &sequence[0], 1) != 1)
            return '\x1b';


        if (read(STDIN_FILENO, &sequence[1], 1) != 1)
            return '\x1b';


        if (sequence[0] == '[') {


            switch(sequence[1]) {


                case 'A':
                    return ARROW_UP;

                case 'B':
                    return ARROW_DOWN;

                case 'C':
                    return ARROW_RIGHT;

                case 'D':
                    return ARROW_LEFT;

            }

        }

    }


    return c;

}



int getWindowSize(int *rows, int *cols) {


    struct winsize ws;


    if (
        ioctl(
            STDOUT_FILENO,
            TIOCGWINSZ,
            &ws
        ) == -1
    ) {

        return -1;

    }


    *cols = ws.ws_col;
    *rows = ws.ws_row;


    return 0;

}



struct appendBuffer {

    char *buffer;

    int length;

};


#define APPEND_INIT {NULL, 0}


void append(
    struct appendBuffer *ab,
    const char *s,
    int length
) {


    char *new = realloc(
        ab->buffer,
        ab->length + length
    );


    if (new == NULL)
        return;


    memcpy(
        &new[ab->length],
        s,
        length
    );


    ab->buffer = new;

    ab->length += length;

}



void freeAppendBuffer(struct appendBuffer *ab) {

    free(ab->buffer);

}



void editorRefreshScreen() {


    struct appendBuffer ab = APPEND_INIT;


    append(&ab, "\x1b[?25l", 6);

    append(&ab, "\x1b[H", 3);


    for (int i = 0; i < editor.screenRows; i++) {


        append(&ab, "~", 1);


        if (i < editor.screenRows - 1) {

            append(&ab, "\r\n", 2);

        }

    }


    char buffer[32];


    snprintf(
        buffer,
        sizeof(buffer),
        "\x1b[%d;%dH",
        editor.cy + 1,
        editor.cx + 1
    );


    append(&ab, buffer, strlen(buffer));


    append(&ab, "\x1b[?25h", 6);


    write(
        STDOUT_FILENO,
        ab.buffer,
        ab.length
    );


    freeAppendBuffer(&ab);

}



void processKeyPress() {


    int c = editorReadKey();


    switch(c) {


        case CTRL_KEY('q'):

            write(STDOUT_FILENO, "\x1b[2J", 4);

            write(STDOUT_FILENO, "\x1b[H", 3);

            exit(0);


        case ARROW_UP:

            if (editor.cy != 0)
                editor.cy--;

            break;


        case ARROW_DOWN:

            editor.cy++;

            break;


        case ARROW_LEFT:

            if (editor.cx != 0)
                editor.cx--;

            break;


        case ARROW_RIGHT:

            editor.cx++;

            break;

    }

}



void initEditor() {


    editor.cx = 0;

    editor.cy = 0;


    if (
        getWindowSize(
            &editor.screenRows,
            &editor.screenCols
        ) == -1
    ) {

        die("window size");

    }

}



int main() {


    enableRawMode();

    initEditor();


    while (1) {


        editorRefreshScreen();

        processKeyPress();


    }


    return 0;

}
