/* 
   TERMatrix 0.2
   Copyright 2006 Flavio Chierichetti
   Licence GPL 2
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#define TERMINAL_DEVICE "/dev/tty"
#define BS 102400

#define min(x, y) ((x) < (y) ? (x) : (y))

#define termCursorOff() printf("\x1b[?25l");
#define termMoveTo(r, c) printf("\x1b[%d;%dH", r + 1, c + 1)
#define termSetAttr(fore, back, attr) printf("\x1b[%d;%dm", attr, fore + 30)
#define termPutChar(c) printf("%c", c)
#define termErase() printf("\x1b[2J")
#define termReset() printf("\x1b%c", 'c')

typedef struct s {
  int start, end, n, speed, counter;
  char last;
  struct s *next;
} seq;

enum { usecDelay = 8000, speedMin = 6, speedMax = 16 , darkProb = 75 , newSeqProb = 20 , letterChangeProb = 25 };

enum { black = 0, red, green, yellow, blue, magenta, cyan, white };
enum { normalAttr = 1 , boldAttr = 22 };

void initTerm(int signum);
void initSigActions();
void print(int signum);
void end(int signum);

char *V = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz<>()[]{}.,:;!@#$%^&*-_=+|?/`~'\"\\";
int Length, R, C;
seq **S;

char buf[BS];

int ofw, ofr;
char pipein;

char *quotes[] = {
  "Wake up, Neo.",
  "There is no spoon.",
  "Follow the white rabbit.",
  "What is the matrix?",
  "Welcome to the real world.",
  "Don't think you are. Know you are.",
  "It is inevitable.",
  "Only human.",
  "Deja vu...",
  NULL
};

int main() {
  initTerm(0);

  srand(time(NULL) % RAND_MAX);

  Length = strlen(V);
  pipein = !isatty(STDIN_FILENO);

  initSigActions();

  while (1)
  {
    char ch = 0;
    if (pipein)
    {
      if (read(STDIN_FILENO, &ch, 1))
      {
        if (ch < 0) ch *= -1;
        if (ch >= 0 && ch < 32) ch += 32;
        if (ch == 127) ch -= 1;
        buf[ofw] = ch;
        ofw++;
        ofw %= BS;
      } 
    }
    else pause();
  }

  return 0;
}

char get_char()
{
  char co;

  if (pipein)
  {
    if (buf[ofr] != 0)
    {
      co = buf[ofr];
      buf[ofr] = 0;
      ofr++;
      ofr %= BS;
      return co;
    } 
    else return ' ';
  }
  else return V[rand() % Length];  
}

void initTerm(int signum) {
  int fd;
  struct winsize ws;
  int i;

  if ( (fd = open(TERMINAL_DEVICE, O_RDWR)) == -1 )
    exit(1);

  if ( ioctl(fd, TIOCGWINSZ, &ws) != 0 )
    exit(1);

  close(fd);

  R = ws.ws_row;
  C = ws.ws_col;

  /* If this function is called more than once, there would be a little memory leak here */

  S = (seq**) calloc (C, sizeof(seq*));

  for ( i = 0 ; i < C ; i++ )
    S[i] = NULL;

  termCursorOff();
  //termSetAttr(green, black, normalAttr);
  //termSetAttr(red, black, normalAttr);
  termErase();
}

void initSigActions() {
  struct sigaction sigintAction, sigalrmAction, sigwinchAction;
  struct itimerval timer;

  sigfillset(&sigwinchAction.sa_mask);
  sigwinchAction.sa_flags = 0;
  sigwinchAction.sa_handler = initTerm;
  sigaction(SIGWINCH, &sigwinchAction, NULL);

  sigfillset(&sigalrmAction.sa_mask);
  sigalrmAction.sa_flags = 0;
  sigalrmAction.sa_handler = print;
  sigaction(SIGALRM, &sigalrmAction, NULL);

  sigfillset(&sigintAction.sa_mask);
  sigintAction.sa_flags = 0;
  sigintAction.sa_handler = end;
  sigaction(SIGINT, &sigintAction, NULL);

  timer.it_interval.tv_sec = timer.it_value.tv_sec = 0;
  timer.it_interval.tv_usec = timer.it_value.tv_usec = usecDelay;
  setitimer(ITIMER_REAL, &timer, NULL); 
}

void print(int signum) {
  int c, r;

  for ( c = 0 ; c < C ; c++ )
    if ( S[c] ) 
    {
      S[c]->counter = (S[c]->counter + 1) % S[c]->speed;
      
      if ( S[c]->counter == 0 ) 
      {
        for ( r = S[c]->start ; r < min(R, S[c]->end) ; r++ ) 
          if ( rand() % 100 < letterChangeProb || r == S[c]->end - 1 ) 
          {
            if ( rand() % 100 < darkProb )
              termSetAttr(green, black, boldAttr);
            else
              termSetAttr(red + (rand() % 7), black, normalAttr);
              //termSetAttr(yellow, black, normalAttr);
            
            termMoveTo(r, c);
            termPutChar(get_char());
	        }

        if ( S[c]->end == R )
          S[c]->end++;

        if ( S[c]->n == 0 ) 
        {
          termMoveTo(S[c]->start, c);
          termPutChar(' ');
          
          S[c]->start++;
        }
	
        if ( S[c]->end < R ) 
        {
          S[c]->last = get_char();

          termSetAttr(white, black, normalAttr);
          termMoveTo(S[c]->end, c);
          termPutChar(S[c]->last);
              
          S[c]->end++;
        }
	
	      if ( S[c]->n > 0 )
	        S[c]->n--;

	      if ( S[c]->start == S[c]->end ) 
        {
	        free(S[c]); 
          S[c] = NULL;
      	}
      }
    } else
      if ( rand() % 100 < newSeqProb ) 
      {
        S[c] = (seq*) malloc ( sizeof(seq) );

        S[c]->start = 0;
        S[c]->end = 0;
        S[c]->speed = rand() % (speedMax - speedMin + 1) + speedMin;
        S[c]->counter = 0;
        S[c]->n = rand() % R + 1;
        
        S[c]->last = 0;
      }

  fflush(stdout);
}

void end(int signum) {
  int i;

  termReset();

  for ( i = 0 ; quotes[i] != NULL ; i++ );
  i = rand() % i;
 
  printf("%s\n", quotes[i]);

  exit(0);
}
