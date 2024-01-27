#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <poll.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/uinput.h>

#define UINPUT_DEVICE   "/dev/uinput"

// https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

struct termios orig_termios;

struct key_chord {
    int code;
    bool shift;
    bool ctrl;
    bool alt;
};

bool global_shift = false,
     global_ctrl  = false,
     global_alt   = false;

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
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

char readChar() {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1)
        die("read");
    return c;
}

char readIfReady() {
    struct pollfd fd = { .fd = STDIN_FILENO, .events = POLLIN };
    if (poll (&fd, 1, 0) == 1)
        return readChar();
    return 0x00;
}

int createKeyboardDevice() {
    // https://stackoverflow.com/questions/40676172/how-to-generate-key-strokes-events-with-the-input-subsystem
    int fd_key_emulator;
    struct uinput_user_dev dev_fake_keyboard = {
        .name = "kb-emulator",
        .id = {.bustype = BUS_USB, .vendor = 0x01, .product = 0x01, .version = 1}
    };

    // Open the uinput device
    fd_key_emulator = open(UINPUT_DEVICE, O_WRONLY | O_NONBLOCK);
    if (fd_key_emulator < 0) {
        fprintf(stderr, "Error in opening uinput: %s\n", strerror(errno));
        return -1;
    }

    // Enable all of the events we will need
    if (ioctl(fd_key_emulator, UI_SET_EVBIT, EV_KEY)
        || ioctl(fd_key_emulator, UI_SET_EVBIT, EV_SYN)) {
        fprintf(stderr, "Error in ioctl sets: %s\n", strerror(errno));
        return -1;
    }
    for (int code = 1; code <= 120; code++) {
        if (ioctl(fd_key_emulator, UI_SET_KEYBIT, code)) {
            fprintf(stderr, "Error in ioctl set for code %d: %s\n", code, strerror(errno));
        }
    }

    // Write the uinput_user_dev structure into uinput file descriptor
    if (write(fd_key_emulator, &dev_fake_keyboard, sizeof(struct uinput_user_dev)) < 0) {
        fprintf(stderr, "Error in write(): uinput_user_dev struct into uinput file descriptor: %s\n", strerror(errno));
        return -1;
    }

    // Create the device via an IOCTL call
    if (ioctl(fd_key_emulator, UI_DEV_CREATE)) {
        fprintf(stderr, "Error in ioctl : UI_DEV_CREATE : %s\n", strerror(errno));
        return -1;
    }

    return fd_key_emulator;
}

int handleCommandSeq(struct key_chord *chord){
    char c;
    while (true) {
        printf("Type `q` to quit, `r` to resume, or `Ctrl-b` to emit `Ctrl-b`.\n");
        printf("Type `s` to turn shift %s, `c` to turn ctrl %s, or `a` to turn alt %s.\n",
               global_shift ? "OFF" : "ON", global_ctrl ? "OFF" : "ON",
               global_alt ? "OFF" : "ON");
        c = readChar();
        switch (c) {
          case 'q':
            return -1;
          case 'r':
            return 0;
          case 0x02:
            chord->code = KEY_B;
            chord->shift = chord->alt = false;
            chord->ctrl = true;
            return 1;
          case 's':
            global_shift = !global_shift;
            return 0;
          case 'c':
            global_ctrl = !global_ctrl;
            return 0;
          case 'a':
            global_alt = !global_alt;
            return 0;
        }
    }
}

bool escapeSeq(char c, struct key_chord *chord) {
    // https://github.com/snaptoken/kilo-src/blob/propagate-highlight/kilo.c#L162
    if (c != 0x1b)
        return false;

    char one = readIfReady(),
         two = readIfReady();
    if (!one)
        return false;
    if (!two) {
        printf("Unexpected two character escape sequence: %x %x\n", c, one);
        return false;
    }
    chord->shift = chord->ctrl = chord->alt = false;
    if (one == '[') {
        if ('0' <= two && two <= '9') {
            char three = readIfReady();
            if (!three) {
                printf("Unexpected three character escape sequence: %x %x %x\n",
                       c, one, two);
                return false;
            }
            if (three == '~') {
                switch (two) {
                  case '1':
                    chord->code = KEY_HOME;
                    return true;
                  case '2':
                    chord->code = KEY_INSERT;
                    return true;
                  case '3':
                    chord->code = KEY_DELETE;
                    return true;
                  case '4':
                    chord->code = KEY_END;
                    return true;
                  case '5':
                    chord->code = KEY_PAGEUP;
                    return true;
                  case '6':
                    chord->code = KEY_PAGEDOWN;
                    return true;
                  case '7':
                    chord->code = KEY_HOME;
                    return true;
                  case '8':
                    chord->code = KEY_END;
                    return true;
                  default:
                    printf("Unexpected four character escape sequence: %x %x %x %x\n",
                        c, one, two, three);
                    return false;
                }
            }
        } else {
            switch (two) {
              case 'A':
                chord->code = KEY_UP;
                return true;
              case 'B':
                chord->code = KEY_DOWN;
                return true;
              case 'C':
                chord->code = KEY_RIGHT;
                return true;
              case 'D':
                chord->code = KEY_LEFT;
                return true;
              case 'H':
                chord->code = KEY_HOME;
                return true;
              case 'F':
                chord->code = KEY_END;
                return true;
            }
        }
    } else if (one == 'O') {
        switch(two) {
          case 'H':
            chord->code = KEY_HOME;
            return true;
          case 'F':
            chord->code = KEY_END;
            return true;
        }
    }
    printf("Unexpected three character escape sequence: %x %x %x\n",
            c, one, two);
    return false;
}

void printCharCode(char c) {
    if (iscntrl(c)) {
        printf("%x\n", c);
    } else {
        printf("%x ('%c')\n", c, c);
    }
}

bool carriageReturn(char c, struct key_chord *chord) {
    // carriageReturns are 0x0d == Ctrl-M, so we have to catch this before handling the
    // control character codes
    if (c != '\r')
        return false;

    chord->code = KEY_ENTER;
    chord->shift = chord->ctrl = chord->alt = false;
    return true;
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
        chord->code = KEY_BACKSPACE;
        return true;
    }
    return false;
}

void writeEventVals(int fd, unsigned short type, unsigned short code, signed int value) {
    struct input_event event = (struct input_event) {.type=type, .code=code, .value=value};
    gettimeofday(&event.time, NULL);
    write(fd, &event, sizeof(struct input_event));
}

void emitChord(int fd, struct key_chord *chord) {
    printf("Key %i, shift %i, ctrl %i, alt %i\n", chord->code, chord->shift,
            chord->ctrl, chord->alt);
    if (chord->ctrl || global_ctrl) {
        writeEventVals(fd, EV_KEY, KEY_LEFTCTRL, 1);
        writeEventVals(fd, EV_SYN, SYN_REPORT, 0);
    }
    if (chord->alt || global_alt) {
        writeEventVals(fd, EV_KEY, KEY_LEFTALT, 1);
        writeEventVals(fd, EV_SYN, SYN_REPORT, 0);
    }
    if (chord->shift || global_shift) {
        writeEventVals(fd, EV_KEY, KEY_LEFTSHIFT, 1);
        writeEventVals(fd, EV_SYN, SYN_REPORT, 0);
    }
    writeEventVals(fd, EV_KEY, chord->code, 1);
    writeEventVals(fd, EV_SYN, SYN_REPORT, 0);
    writeEventVals(fd, EV_KEY, chord->code, 0);
    writeEventVals(fd, EV_SYN, SYN_REPORT, 0);
    if (chord->shift || global_shift) {
        writeEventVals(fd, EV_KEY, KEY_LEFTSHIFT, 0);
        writeEventVals(fd, EV_SYN, SYN_REPORT, 0);
    }
    if (chord->alt || global_alt) {
        writeEventVals(fd, EV_KEY, KEY_LEFTALT, 0);
        writeEventVals(fd, EV_SYN, SYN_REPORT, 0);
    }
    if (chord->ctrl || global_ctrl) {
        writeEventVals(fd, EV_KEY, KEY_LEFTCTRL, 0);
        writeEventVals(fd, EV_SYN, SYN_REPORT, 0);
    }
}

int main() {
    enableRawMode();
    int fd_keyboard = createKeyboardDevice();
    if (fd_keyboard == -1) {
        fprintf(stderr, "Error creating emulated keyboard.\n");
        return -1;
    }

    while (1) {
        char c = readChar();
        struct key_chord chord;
        if (c == 0x02) { // Ctrl-B to spark command sequence
            int resp = handleCommandSeq(&chord);
            if (resp == -1)
                break;
            if (resp == 0)
                continue;
            emitChord(fd_keyboard, &chord);
        } else if (carriageReturn(c, &chord) || escapeSeq(c, &chord)
                   || alphaChord(c, &chord) || symbolChord(c, &chord)) {
            emitChord(fd_keyboard, &chord);
        } else {
            printCharCode(c);
        }
    }
    return 0;
}
