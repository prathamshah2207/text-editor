/*** includes ***/
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/
struct editorConfig {
	int screenrows;
	int screencols;
	struct termios original_termios;
};

struct editorConfig E;

/*** terminal ***/

//  this is for error handling
void die(const char *s) {
	// this makes sure if we run into some error, we dont throw out garbage but clear everything and then throw error at the top of screen and exit
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	// perror reads from global errno variable and the description error message and provides it. Exit 1 then exits the program with exit status 1, meaning failure
	perror(s);
	exit(1);
}

// disables raw mode - to be done one exit
void disableRawMode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.original_termios) == -1) die("tcsetattr");
}

// This enables raw typing mode by manually changing all echo attributes
void enableRawMode() {
	// get all attributes into original_termios struct to save them
	if (tcgetattr(STDIN_FILENO, &E.original_termios) == -1) die("tcgetattr");

	atexit(disableRawMode);
	
	// a new raw struct with the same attributes as original
	struct termios raw = E.original_termios;

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

// gets the window size in rows and columns and stores them in the passed pointers of the struct
int getWindowSize(int *rows, int *columns) {
	struct winsize ws;

	//ioctl() will place the number of columns wide and the number of rows high the terminal is into the given winsize struct
	//ioctl stands for Input/Output Control
	//TIOCGWINSZ stands for Terminal IOCtl Get Window SiZe
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		//on ioctl failure or if somehow theres no columns on the display then we return -1
		return -1;
	} else {
		*columns = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** input ***/

// checks for key entries and simulate such outputs for specific entries
void editorProcessKeypress() {
	// call key reading function to get the read key
	char c = editorReadKey();

	switch (c) {
		// checks of ctrl+q was pressed to go for exit sequence of the program
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
	}
}

/*** output ***/

// draw tilde(~) on the left side of the screen on all columns after the end of file
void editorDrawRows() {
	int y;
	for (y=0; y < E.screenrows; y++) {
		write(STDOUT_FILENO, "~\r\n", 3);
	}
}

// clears the current cli screen
void editorRefreshScreen() {

	// writes the clear screen escape sequence in the terminal
	// 4 is for letting the terminal know it will get a 4 bytes writing
	// \x1b is the escape character 27 (1 byte)
	// the [2J (3 bytes) is command after the excape sequence
	// J command clears the screen, 2 clears the ENTIRE screen. by default is 0 meaning clearing cursor to the end and 1 means top to the cursor
	write(STDOUT_FILENO, "\x1b[2J", 4);

	// H command positions the cursor and it can take upto 2 args for row and column position.
	// eg. if want to position at 80x24 we could write <esc>[24;80H
	// by default it start from 1;1 and that is the top left corner of screen not 0,0
	write(STDOUT_FILENO, "\x1b[H", 3);

	editorDrawRows();

	write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** init ***/

// runs the getWindowSize function to get the window size parameters stored globally and die on error
void initEditor() {
	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
	enableRawMode();
	initEditor();

	// read input from the user until it gets input of q
	while (1) {
		editorRefreshScreen();
		editorProcessKeypress();
	}
	return 0;
}
