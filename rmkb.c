#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <linux/input.h>

// https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

struct termios orig_termios;

struct key_chord {
    int code;
    bool shift;
    bool ctrl;
    bool alt;
};

void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcgetattr");

    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

char readChar() {
    char c = '\0';
    while (!c)
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
            die("read");
    return c;
}

int handleCommandSeq(struct key_chord *chord){
    char c;
    while (true) {
        printf("Type `q` to quit, `c` to continue, or `Ctrl-b` to emit `Ctrl-b`.\r\n");
        c = readChar();
        switch (c) {
          case 'q':
            return -1;
          case 'c':
            return 0;
          case 0x02:
            chord->code = KEY_B;
            chord->shift = chord->alt = false;
            chord->ctrl = true;
            return 1;
        }
    }
}

void handleEscapeSeq() {
    char one = readChar(),
         two = readChar();
    printf("Escape code %x %x\r\n", one, two);
    return;
}

void printCharCode(char c) {
    if (iscntrl(c)) {
        printf("%x\r\n", c);
    } else {
        printf("%x ('%c')\r\n", c, c);
    }
}

int alphaCode(char c) {
    c = (c & 0x1f) | 0x40; // Make uppercase
    switch (c) {
      case 'A':
        return KEY_A;
      case 'B':
        return KEY_B;
      case 'C':
        return KEY_C;
      case 'D':
        return KEY_D;
      case 'E':
        return KEY_E;
      case 'F':
        return KEY_F;
      case 'G':
        return KEY_G;
      case 'H':
        return KEY_H;
      case 'I':
        return KEY_I;
      case 'J':
        return KEY_J;
      case 'K':
        return KEY_K;
      case 'L':
        return KEY_L;
      case 'M':
        return KEY_M;
      case 'N':
        return KEY_N;
      case 'O':
        return KEY_O;
      case 'P':
        return KEY_P;
      case 'Q':
        return KEY_Q;
      case 'R':
        return KEY_R;
      case 'S':
        return KEY_S;
      case 'T':
        return KEY_T;
      case 'U':
        return KEY_U;
      case 'V':
        return KEY_V;
      case 'W':
        return KEY_W;
      case 'X':
        return KEY_X;
      case 'Y':
        return KEY_Y;
      case 'Z':
        return KEY_Z;
    }
    return -1;
}

bool alphaChord(char c, struct key_chord *chord) {
    if ((0xe0 & c) == 0x20)
        return false;

    int code = alphaCode(c);
    if (code == -1)
        return false;

    chord->code = code;
    chord->ctrl = ((0xe0 & c) == 0x00);
    chord->shift = ((0xe0 & c) == 0x40);
    chord->alt = false;
    return true;
}

bool symbolChord(char c, struct key_chord *chord) {
    chord->shift = chord->ctrl = chord->alt = false;
    switch (c) {
      // Control chars 0x00-0x1f
      case 0x1b:
        chord->code = KEY_LEFTBRACE;
        chord->ctrl = true;
        return true;
      case 0x1c:
        chord->code = KEY_BACKSLASH;
        chord->ctrl = true;
        return true;
      case 0x1d:
        chord->code = KEY_RIGHTBRACE;
        chord->ctrl = true;
        return true;
      case 0x1e:
        chord->code = KEY_APOSTROPHE;
        chord->ctrl = true;
        return true;
      case 0x1f:
        chord->code = KEY_MINUS;
        chord->ctrl = true;
        return true;
      // Symbol chars 0x20-0x3f
      case ' ':
        chord->code = KEY_SPACE;
        return true;
      case '!':
        chord->code = KEY_1;
        chord->shift = true;
        return true;
      case '"':
        chord->code = KEY_APOSTROPHE;
        chord->shift = true;
        return true;
      case '#':
        chord->code = KEY_3;
        chord->shift = true;
        return true;
      case '$':
        chord->code = KEY_4;
        chord->shift = true;
        return true;
      case '%':
        chord->code = KEY_5;
        chord->shift = true;
        return true;
      case '&':
        chord->code = KEY_7;
        chord->shift = true;
        return true;
      case '\'':
        chord->code = KEY_APOSTROPHE;
        return true;
      case '(':
        chord->code = KEY_9;
        chord->shift = true;
        return true;
      case ')':
        chord->code = KEY_0;
        chord->shift = true;
        return true;
      case '*':
        chord->code = KEY_8;
        chord->shift = true;
        return true;
      case '+':
        chord->code = KEY_EQUAL;
        chord->shift = true;
        return true;
      case ',':
        chord->code = KEY_COMMA;
        return true;
      case '-':
        chord->code = KEY_MINUS;
        return true;
      case '.':
        chord->code = KEY_DOT;
        return true;
      case '/':
        chord->code = KEY_SLASH;
        return true;
      case '0':
        chord->code = KEY_0;
        return true;
      case '1':
        chord->code = KEY_1;
        return true;
      case '2':
        chord->code = KEY_2;
        return true;
      case '3':
        chord->code = KEY_3;
        return true;
      case '4':
        chord->code = KEY_4;
        return true;
      case '5':
        chord->code = KEY_5;
        return true;
      case '6':
        chord->code = KEY_6;
        return true;
      case '7':
        chord->code = KEY_7;
        return true;
      case '8':
        chord->code = KEY_8;
        return true;
      case '9':
        chord->code = KEY_9;
        return true;
      case ':':
        chord->code = KEY_SEMICOLON;
        chord->shift = true;
        return true;
      case ';':
        chord->code = KEY_SEMICOLON;
        return true;
      case '<':
        chord->code = KEY_COMMA;
        chord->shift = true;
        return true;
      case '=':
        chord->code = KEY_EQUAL;
        return true;
      case '>':
        chord->code = KEY_DOT;
        chord->shift = true;
        return true;
      case '?':
        chord->code = KEY_SLASH;
        chord->shift = true;
        return true;
      // Upper-case 0x40-0x5f
      case '@':
        chord->code = KEY_2;
        chord->shift = true;
        return true;
      case '[':
        chord->code = KEY_LEFTBRACE;
        return true;
      case '\\':
        chord->code = KEY_BACKSLASH;
        return true;
      case ']':
        chord->code = KEY_RIGHTBRACE;
        return true;
      case '^':
        chord->code = KEY_6;
        chord->shift = true;
        return true;
      case '_':
        chord->code = KEY_MINUS;
        chord->shift = true;
        return true;
      // Lower-case 0x60-0x7f
      case '`':
        chord->code = KEY_GRAVE;
        return true;
      case '{':
        chord->code = KEY_LEFTBRACE;
        chord->shift = true;
        return true;
      case '|':
        chord->code = KEY_BACKSLASH;
        chord->shift = true;
        return true;
      case '}':
        chord->code = KEY_RIGHTBRACE;
        chord->shift = true;
        return true;
      case '~':
        chord->code = KEY_GRAVE;
        chord->shift = true;
        return true;
      case 0x7f:
        chord->code = KEY_DELETE;
        return true;
    }
    return false;
}

void emitChord(struct key_chord *chord) {
    printf("Key %i, shift %i, ctrl %i, alt %i\r\n", chord->code, chord->shift,
            chord->ctrl, chord->alt);
}

int main() {
    enableRawMode();

    while (1) {
        char c = readChar();
        struct key_chord chord;
        if (c == 0x1b) {
            handleEscapeSeq();
        } else if (c == 0x02) { // Ctrl-B to spark command sequence
            int resp = handleCommandSeq(&chord);
            if (resp == -1)
                break;
            if (resp == 0)
                continue;
            emitChord(&chord);
        } else if (alphaChord(c, &chord) || symbolChord(c, &chord)) {
            emitChord(&chord);
        } else {
            printCharCode(c);
        }
    }
    return 0;
}
