#include <ctype.h>
#include <errno.h>    
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

// Stores the original terminal state so we can restore it when the program exits.
struct termios orig_termios;

// Error handling function. Prints the descriptive error message and exits the program.
void die(const char *s) {
  perror(s);
  exit(1);
}

// Restores the terminal attributes to their original state.
void disableRawMode() {
  // TCSAFLUSH applies the change after all pending output is written to the terminal,
  // and discards any unread input before applying.
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}

// Switches the terminal from its default "cooked" mode to "raw" mode.
void enableRawMode() {
  // 1. Read current terminal attributes into orig_termios
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
  
  // 2. Register disableRawMode to run automatically when the program exits (e.g., via return or exit())
  atexit(disableRawMode);
  
  // Create a copy of the original settings to modify
  struct termios raw = orig_termios;
  
  // --- Input Flags (c_iflag) ---
  // IXON: Disables Ctrl-S (pause transmission) and Ctrl-Q (resume transmission).
  // ICRNL: Disables the terminal's habit of translating carriage returns (Ctrl-M / '\r') into newlines ('\n').
  // BRKINT, INPCK, ISTRIP: Miscellaneous traditional flags turned off to ensure purely raw input.
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  
  // --- Output Flags (c_oflag) ---
  // OPOST: Disables output processing (like automatically adding '\r' before '\n' on the output side).
  raw.c_oflag &= ~(OPOST);
  
  // --- Control Flags (c_cflag) ---
  // CS8: Sets the character size to 8 bits per byte.
  raw.c_cflag |= (CS8);
  
  // --- Local Flags (c_lflag) ---
  // ECHO: Disables echoing of characters you type.
  // ICANON: Disables canonical mode so we read byte-by-byte instead of line-by-line.
  // ISIG: Disables Ctrl-C (SIGINT) and Ctrl-Z (SIGTSTP) from sending signals.
  // IEXTEN: Disables Ctrl-V (literal character insertion).
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);  
  
  // --- Control Characters (c_cc) ---
  // VMIN: Sets the minimum number of bytes needed before read() can return. (0 = return immediately)
  raw.c_cc[VMIN] = 0;
  // VTIME: Sets the maximum amount of time to wait before read() returns. (10 = 1 second / 10 tenths of a second)
  raw.c_cc[VTIME] = 10;
  
  // Apply the modified struct to the terminal immediately.
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int main() {
  enableRawMode();
  
  while (1) {
    char c = '\0';
    
    // Read 1 byte from standard input into 'c'.
    // If read returns -1 (an error) AND the error isn't EAGAIN (which just means timeout occurred in Cygwin/older systems), die.
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
    
    // iscntrl tests if a character is a control character (ASCII 0-31 and 127).
    if (iscntrl(c)) {
      // Print just the ASCII value of the control character.
      // We use \r\n because we disabled OPOST, meaning \n no longer automatically drops us to the beginning of the next line.
      printf("%d\r\n", c);
    } else {
      // Print the ASCII value and the visible character itself.
      printf("%d ('%c')\r\n", c, c);
    }
    
    // Break the loop and trigger atexit(disableRawMode) if the user presses 'q'.
    if (c == 'q') break;
  }
  
  return 0;
}