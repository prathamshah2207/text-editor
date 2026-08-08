/*** includes ***/
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct termios original_termios;

/*** terminal ***/

//  this is for error handling
void die(const char *s) {
	// perror reads from global errno variable and the description error message and provides it. Exit 1 then exits the program with exit status 1, meaning failure
	perror(s);
	exit(1);
}

// disables raw mode - to be done one exit
void disableRawMode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios) == -1) die("tcsetattr");
}

// This enables raw typing mode by manually changing all echo attributes
void enableRawMode() {
	// get all attributes into original_termios struct to save them
	if (tcgetattr(STDIN_FILENO, &original_termios) == -1) die("tcgetattr");

	atexit(disableRawMode);
	
	// a new raw struct with the same attributes as original
	struct termios raw = original_termios;

	// modify raw by hand
	// c_lflags means local flags
	// IXON comes from control char ctrl+c and ctrl+q for off and on respectively
	// ICRNL turns off ctrl+M where CR mean carriage return (13,'\r') and NL mean new line
	// some other miscellaneous flags
	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	// turn off the output flag for \n translated into \r\n meaning carriage movese the cursor to beginning and then newline moving cursor down
	raw.c_oflag &= ~(OPOST);
	// misc flag
	raw.c_cflag |= (CS8);
	// ICANON turns off canonical mode(line-by-line to byte-by-byte)
	// ISIG turns off ctrl+c ctrl+z signals
	// IEXTEN turns off ctrl+v but it wont work for a emulated linux terminal as the upper windows terminal will catch ctrl+v as paste command
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

	// c_cc means control characters, an array of bytes that control various terminal settings
	// VMIN sets min # of bytes of input needed before read() can return
	// VTIME sets the max amount of time to wait before read() returns
	// in windows, VTIME wont be cared by the machine and still block for input.
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	
	// passing the modified raw to tcsetattr
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

// reads input and by waiting for a keypress and then return it
char editorReadKey() {
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read");
	}
	return c;
}

/*** input ***/

// checks for key entries and simulate such outputs for specific entries
void editorProcessKeypress() {
	// call key reading function to get the read key
	char c = editorReadKey();

	switch (c) {
		// checks of ctrl+q was pressed to exit the program
		case CTRL_KEY('q'):
			exit(0);
			break;
	}
}

/*** init ***/

int main() {
	enableRawMode();

	// read input from the user until it gets input of q
	while (1) {
		editorProcessKeypress();
	}
	return 0;
}
