/*

 tinymudserver - an example MUD server

 Author: 	Nick Gammon <nick@gammon.com.au>
          http://www.gammon.com.au/ 

 Date:		6th January 2003

 This program is placed in the public domain.

 You may use it as a starting point for writing your own MUD server.

 I would appreciated being credited in the event that you use this code.

 Please do not email questions directly to me, send questions/comments to my
 forum at:

   http://www.gammon.com.au/forum/ 

 Because the program is in the public domain, there is no warranty for the program,
 to the extent permitted by applicable law.

 The author provides the program "as is" without warranty of any kind, either
 expressed or implied, including, but not limited to, the implied warranties of
 merchantability and fitness for a particular purpose.

 The entire risk as to the quality and performance of the program is with you.

 Should the program prove defective, you assume the cost of all necessary servicing,
 repair or correction.

 To compile without using the makefile:

   gcc tinymudserver.cpp -o tinymudserver -g -Wall
 
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <netinet/in.h>
#include <arpa/inet.h>

/* stl includes for string handling and lists */

#include <string>
#include <list>

#define UMIN(a, b)              ((a) < (b) ? (a) : (b))
#define UMAX(a, b)              ((a) > (b) ? (a) : (b))
#define VERSION "1.0"						/* server version */

/* NB - if you get errors on the "accept" line remove the typedef below */
typedef int socklen_t;

using namespace std;			/* some stl operations will fail if we don't do this */

/* change this stuff to customise behaviour (eg. connection port) */

#define PORT 							4000								/* port to connect to */

/* every MESSAGE_INTERVAL seconds the message TICK_MESSAGE is sent to all connected players */

#define MESSAGE_INTERVAL   30		/* seconds between tick messages */

/* This is the time the "select" waits before timing out. */

#define COMMS_WAIT_SEC  	0    								/* time to wait in seconds */
#define COMMS_WAIT_USEC 	500000    					/* time to wait in microseconds */

/* messages sent to the player - customise these or translate into other languages */

#define INITIAL_STRING 			"\nWelcome to the Tiny MUD Server version " VERSION "\n"  
#define FINAL_STRING 				"See you next time!\n"  				
#define TELL_NAME      			"Enter your name ...  "
#define ALREADY_CONNECTED   "%s is already connected.\n"
#define TELL_PASSWORD     	"Enter your password ... "
#define PASSWORD_INCORRECT  "That password is incorrect.\n"
#define WELCOME	            "Welcome back, %s!\n\n"
#define LOOK_STRING         "You are standing in a large, sombre room, with no exits.\n"
#define IN_THE_ROOM					"You also see "
#define SAY_WHAT					  "Say what?\n"
#define YOU_SAY						  "You say, \"%s\"\n"
#define SOMEONE_SAYS			  "%s says, \"%s\"\n"
#define TELL_WHOM						"Tell whom?\n"
#define TELL_WHAT						"Tell %s what?\n"
#define NOT_SELF						"You cannot do that to yourself\n"
#define YOU_TELL						"You tell %s, \"%s\"\n"
#define SOMEONE_TELLS       "%s tells you, \"%s\"\n"
#define NOT_CONNECTED				"%s is not connected.\n"
#define HUH								  "Huh?\n"
#define TICK_MESSAGE		    "You hear creepy noises ...\n"
#define SHUTDOWN            "\n\n** Game closed by system operator\n\n"

/* We use -1 to indicate no socket is connected */

#define NO_SOCKET						-1

static int bStopNow = 0;			/* set by signal handler */

time_t tLastMessage;							/* time we last sent a periodic message */

/* socket for accepting new connections */
static int iControl = NO_SOCKET;

/* converts a string to const char * */
#define str(arg) arg.c_str ()

typedef list<string> tStringList;
typedef tStringList::iterator tStringIterator;

/* connection states - add more to have more complex connection dialogs */
enum
{
  eAwaitingName,
  eAwaitingPassword,
  ePlaying,			/* this is the normal 'connected' mode  */
};

/*---------------------------------------------- */
/*  player class - holds details about each connected player */
/*---------------------------------------------- */

class tPlayer
{
public:
  int s;							/* socket they connected on */
  int connstate;			/* connection state */
  string playername;	/* player name */

  tStringList outbuf;	/* pending output */
  string inbuf;				/* pending input */
  string address;			/* address player is from */
  int port; 					/* port they connected on */

  tPlayer ()	/* constructor */
    {
    s = NO_SOCKET;						/* no socket yet */
    connstate = eAwaitingName;	/* new player needs name */
    port = 0;
    };
  
  ~tPlayer ()	/* destructor */
    {
    printf ("Deleting player, socket %i\n", s);
    if (s != NO_SOCKET)	/* close connection if active */
      close (s);
    };
};

/* we will use an stl list of players */
typedef list <tPlayer*> tPlayerList;
typedef tPlayerList::iterator tPlayerListIterator;

/* here is the actual list */
tPlayerList playerlist;		/* list of all connected players */

/* a couple of forward declaration */
void ProcessWrite (tPlayer * p);
void DoLook (tPlayer * p);

/* get rid of leading and trailing spaces from a string */

void Trim (string & s)
{
  string::size_type iPastSpace = s.find_first_not_of (' ');
  if (iPastSpace == string::npos)
    s = "";  /* string only contains spaces, make it empty */
  else
    {
    string::size_type iLastNonSpace = s.find_last_not_of (' ');
    string::size_type iLength = iLastNonSpace - iPastSpace + 1;
    s = s.substr (iPastSpace, iLength);
    }
}

/* find a player by name */

tPlayer * FindPlayer (const char * name)
{
  for (tPlayerListIterator listiter = playerlist.begin (); listiter != playerlist.end (); listiter++)
    {
    tPlayer * p = *listiter;
    if (p->s != NO_SOCKET &&				/* don't if not connected */
      p->connstate == ePlaying &&		/* only send if playing (eg. entered name etc.) */
      p->playername == name)		/* correct player? */
      return p;	/* found them */
    }

  return NULL;
}	/* end of FindPlayer */

/* set up comms - get ready to listen for connection */

int InitComms (void)
  {
  struct sockaddr_in sa;

  /* Create the control socket */
  if ( (iControl = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
    perror ("creating control socket");
    return 1;
    }
  
  /* make sure socket doesn't block */
  if (fcntl( iControl, F_SETFL, FNDELAY ) == -1)
    {
    perror ("fcntl on control socket");
    return 1;
    }

  struct linger	ld;
  ld.l_onoff  = 0;
  ld.l_linger = 0;

  /* Don't allow closed sockets to linger */
  if (setsockopt( iControl, SOL_SOCKET, SO_LINGER,
                  (char *) &ld, sizeof ld ) == -1)
    {
    perror ("setsockopt");
    return 1;
    }
  
  sa.sin_family       = AF_INET;
  sa.sin_port	        = htons (PORT);
  sa.sin_addr.s_addr  = INADDR_ANY;		/* change to listen on a specific adapter */

  /* bind the socket to our connection port */
  if ( bind (iControl, (struct sockaddr *) &sa, sizeof sa) == -1)
    {
    perror ("bind");
    return 1;
    }
  
  /* wait for connections */

  if (listen (iControl, 3) == -1)
    {
    perror ("listen");
    return 1;
    }

  tLastMessage = time (NULL);
  
  return 0;
  }   /* end of InitComms */


/* close listening port */

void CloseComms (void)
  {

  fprintf (stderr, "Closing all comms connections.\n");

  /* close listening socket */
  if (iControl != NO_SOCKET)
    close (iControl);
  iControl = NO_SOCKET;
 
  } /* end of CloseComms */

/* SendBuffer - used for sending printf style strings */

char SendBuffer [1000];

void Send (tPlayer * p, const char * message, ...)
{
  if (p->s == NO_SOCKET)
    return;

  va_list ap;
  va_start (ap, message);
  int iSent = vsnprintf (SendBuffer, (sizeof SendBuffer) - 1, message, ap);
  va_end (ap);

  if (iSent == -1)
    {
    fprintf (stderr, "Message too long to be sent to player %i\n", p->s);
    return;
    }

  p->outbuf.push_back (SendBuffer);
}	/* end of Send */

/* send message to all connected players, excepting "ExceptThis" (which can be null) */

void SendToAll (tPlayer * ExceptThis, const char * message, ...)
{

  va_list ap;
  va_start (ap, message);
  int iSent = vsnprintf (SendBuffer, (sizeof SendBuffer) - 1, message, ap);
  va_end (ap);

  if (iSent == -1)
    {
    fprintf (stderr, "Message too long to be sent to all players\n");
    return;
    }

  for (tPlayerListIterator listiter = playerlist.begin (); listiter != playerlist.end (); listiter++)
    {
    tPlayer * p = *listiter;

    if (p != ExceptThis &&					/* ignore this player */
        p->s != NO_SOCKET &&				/* don't if not connected */
        p->connstate == ePlaying)		/* only send if playing (eg. entered name etc.) */
      p->outbuf.push_back (SendBuffer);
    
    }
}	/* end of SendToAll */

void ClosePlayer (tPlayer * p)
{
  /* close comms connection */
  if (p->s != NO_SOCKET)
    close (p->s);
  p->s = NO_SOCKET;
}	/* end of ClosePlayer */

void ProcessPlayerName (string & sLine, tPlayer * p)
{
  /* name can't be blank */
  if (sLine.empty ())
    {
    Send (p, TELL_NAME);
    return;
    }

  /* don't allow two of the same name */
  if (FindPlayer (str (sLine)))
    {
    Send (p, ALREADY_CONNECTED, str (sLine));
    Send (p, TELL_NAME);
    return;
    }
  
  /* in practice you would look up the player name on disk */
  /* you might also allow for 'new' to allow new players to be created */
  
  p->playername = sLine;
  p->connstate = eAwaitingPassword;
  Send (p, TELL_PASSWORD);
  
}	/* end of ProcessPlayerName */

void ProcessPlayerPassword (string & sLine, tPlayer * p)
{
  /* password can't be blank */
  if (sLine.empty ())
    {
    Send (p, TELL_PASSWORD);
    return;
    }

  /* for testing I am just looking for the same password as the name */

  /* in practice, you would store the password in the player file  */
  
  if (sLine != p->playername)
    {
    Send (p, PASSWORD_INCORRECT);
    Send (p, TELL_PASSWORD);
    return;
    }
  
  p->connstate = ePlaying;
  Send (p, WELCOME, str (p->playername));
  DoLook (p);		/* new player looks around */
  SendToAll (p, "Player %s has joined the game.\n", str (p->playername));
  /* log on console */
  printf ("Player %s has joined the game.\n", str (p->playername));

}	/* end of ProcessPlayerPassword */

/* split a line into the first word, and rest-of-the-line */

string GetWord (string & sLine)
{
  string word = sLine;
    
  /* find space after first word */
  string::size_type i = sLine.find (' ');
  
  if (i == string::npos)
    sLine = "";			/* not found - whole input string is the word */
  else
    {
    word = sLine.substr (0, i);
    sLine = sLine.substr (i + 1, string::npos);	/* get rest of line */
    }

  /* trim both the found word, and the rest of the line */
  Trim (word);
  Trim (sLine);

  /* return first word in line */
  return word;
  
}	/* end of GetWord */

/* quit */

void DoQuit (tPlayer * p)
  {
  /* if s/he finished connecting, tell others s/he has left */
  
  if (p->connstate == ePlaying)
    {
    Send (p, FINAL_STRING);
    ProcessWrite (p);		/* force message out */
    printf ("Player %s has left the game.\n", str (p->playername));
    SendToAll (p, "Player %s has left the game.\n", str (p->playername));   
    }	/* end of properly connected */

  ClosePlayer (p);
  }	/* end of DoQuit */

/* look */

void DoLook (tPlayer * p)
{
  Send (p, LOOK_STRING);

  /* list other players in the same room */
  /* in practice we would check the room number to be the same as ours */
  
  int iOthers = 0;
  for (tPlayerListIterator listiter = playerlist.begin (); listiter != playerlist.end (); listiter++)
    {
    tPlayer *otherp = *listiter;
    if (otherp != p &&	/* we don't see ourselves */
        otherp->connstate == ePlaying &&  /* must be connected */
        otherp->s != NO_SOCKET)  /* and not about to leave  */
      {
      if (iOthers++ == 0)
        Send (p, IN_THE_ROOM);
      else
        Send (p, ", ");
      Send (p, str (otherp->playername));
      }
    }		/* end of looping through all players */

  /* If we listed anyone, finish up the line with a period, newline */
  if (iOthers)
    Send (p, ".\n");

}	/* end of DoLook */

/* say <something> */

void DoSay (tPlayer * p, string sWhat)
{
  if (sWhat.empty ())
    Send (p, SAY_WHAT);
  else
    {
    Send (p, YOU_SAY, str (sWhat));
    SendToAll (p, SOMEONE_SAYS, str (p->playername), str (sWhat));
    }
}	/* end of DoSay */

/* tell <someone> <something> */

void DoTell (tPlayer * p, string sWhat)
{
  /* error if nothing after 'tell' */
  if (sWhat.empty ())
    {
    Send (p, TELL_WHOM);
    return;    
    }

  /* next word is who to tell it to */

  string who = GetWord (sWhat);

  if (sWhat.empty ())
    {
    Send (p, TELL_WHAT, str (who));
    return;
    }

  /* scan player list to find a player of this name */
  
  tPlayer * ptarget = FindPlayer (str (who));

  if (!ptarget)
    {
    Send (p, NOT_CONNECTED, str (who));
    return;
    }

  if (p == ptarget)
    {
    Send (p, NOT_SELF);
    return;
    }
  
  Send (p, YOU_TELL, str (who), str (sWhat));
  Send (ptarget, SOMEONE_TELLS, str (p->playername), str (sWhat));
  
}	/* end of DoTell */

/* process commands when player is connected */

void ProcessCommand (string & sLine, tPlayer * p)
{
  
  string command = GetWord (sLine);

  if (command == "quit")
    DoQuit (p);
  else if (command == "look")
    DoLook (p);
  else if (command == "say")
    DoSay (p, sLine);
  else if (command == "tell")
    DoTell (p, sLine);
  else
    Send (p, HUH);
  
}	/* end of ProcessCommand */

/* process player input - check connection state, and act accordingly */

void ProcessPlayerInput (string & sLine, tPlayer * p)
{

  /* get rid of carriage-return, if present */

  string::size_type i = sLine.find ('\r');
  if (i != string::npos)
    sLine.erase (i, 1);
  
  switch (p->connstate)
    {
    /* until we have name and password we must prompt them */
    case eAwaitingName:
      ProcessPlayerName (sLine, p);
      break;
      
    case eAwaitingPassword:
      ProcessPlayerPassword (sLine, p);
      break;

    /* if playing, everything they type is a command of some sort */
    case ePlaying:
      ProcessCommand (sLine, p);
      break;

    /* whoops! */
    default:
      fprintf (stderr, "Invalid connstate %i for connection %i\n",
                     p->connstate, p->s);
      break;
    }
}	/* end of ProcessPlayerInput */

/* new player has connected */

void ProcessNewConnection (void)
  {
  static struct sockaddr_in sa;
  socklen_t sa_len = sizeof sa;		

  int s;		/* incoming socket */

  /* loop until all outstanding connections are accepted */
  while (true)
    {
    s = accept ( iControl, (struct sockaddr *) &sa, &sa_len);

    /* a bad socket probably means no more connections are outstanding */
    if (s == NO_SOCKET)
      {

      /* blocking is OK - we have accepted all outstanding connections */
      if ( errno == EWOULDBLOCK )
        return;

      perror ("accept");
      return;
      }

    /* TODO: you might immediately close sockets if they are from an address
      which is not acceptable (eg. spammers) */

   
    /* here on successful accept - make sure socket doesn't block */
    
    /* make sure socket doesn't block */
    if (fcntl (s, F_SETFL, FNDELAY) == -1)
      {
      perror ("fcntl on player socket");
      return;
      }

    tPlayer * p = new tPlayer;

    p->s = s;
    p->address = inet_ntoa ( sa.sin_addr);
    p->port = ntohs (sa.sin_port);

    playerlist.push_back (p);
    
    printf ("New player accepted on socket %i, from address %s, port %i\n",
            s, str (p->address), p->port);

    Send (p, INITIAL_STRING);
    Send (p, TELL_NAME);
   
    
    } /* end of processing *all* new connections */

  } /* end of ProcessNewConnection */

void ProcessException (tPlayer * p)
{
  fprintf (stderr, "Exception on socket %i\n", p->s);

  /* signals can cause exceptions, don't get too excited. :) */
}	/* end of ProcessException */

/* Here when there is outstanding data to be read for this player */

void ProcessRead (tPlayer * p)
{
  int nRead;
  static char buf [1000];

  nRead = read(p->s, buf, sizeof(buf) - 1 );
  
  if (nRead == -1)
    {
    if (errno != EWOULDBLOCK)
      perror ("read from player");
    return;
    }

  if (nRead == 0)
    {
    fprintf (stderr, "Connection %i closed\n", p->s);
    DoQuit (p);		// tell other players he has gone
    return;
    }

  buf[nRead] = 0;   /* make sure null-terminated */

  p->inbuf += buf;		/* add to input buffer */

  /* try to extract lines from the input buffer */
  for ( ; ; )
    {
    string::size_type i = p->inbuf.find ('\n');
    if (i == string::npos)
      break;	/* no more at present */

    string sLine = p->inbuf.substr (0, i);	/* extract first line */
    p->inbuf = p->inbuf.substr (i + 1, string::npos);	/* get rest of string */

    Trim (sLine);	/* get rid of leading, trailing spaces */
    ProcessPlayerInput (sLine, p);  /* now, do something with it */
        
    }
    
}	/* end of ProcessRead */

/* Here when we can send stuff to the player. We are allowing for large
 volumes of output that might not be sent all at once, so whatever cannot
 go this time gets put into the list of outstanding strings for this player. */

void ProcessWrite (tPlayer * p)
{
  /* we will loop attempting to write all in buffer, until write blocks */
  while (p->s != NO_SOCKET && !p->outbuf.empty ())
    {

    /* get first outstanding string, remove from list */
    string outbuf = p->outbuf.front ();
    p->outbuf.pop_front ();
    int iLength = outbuf.length ();

    /* send to player */
    int nWrite = write (p->s, str (outbuf), iLength );

    /* check for bad write */
    if (nWrite < 0)
      {
      if (errno != EWOULDBLOCK )
        perror ("send to player");	/* some other error? */

      /* write would block - push back onto front of queue */
      p->outbuf.push_front (outbuf);
      return;
      }

    /* if partial write, put back on queue, and exit */
    if (nWrite < iLength)
      {
      p->outbuf.push_front (&outbuf [nWrite]);
      return;
      } /* end of partial write */

    } /* end of having write loop */

}		/* end of ProcessWrite */

/* main processing loop */

void MainLoop (void)
{

fd_set in_set;
fd_set out_set;
fd_set exc_set;
int iMaxdesc;

struct timeval timeout;
tPlayerListIterator listiter;
  
  /* loop processing input, output, events */

  do
    {

    /* We will go through this loop roughly every COMMS_WAIT_SEC/COMMS_WAIT_USEC
      seconds (at present 0.5 seconds).
      Thus, this is a good place to put periodic processing (eg. fights,
      weather, random events etc.).
      The example below just sends a message every MESSAGE_INTERVAL seconds.
      */
    
    /* send new command if it is time */
    if (time (NULL) > (tLastMessage + MESSAGE_INTERVAL))
      {
      SendToAll (NULL, TICK_MESSAGE);
      tLastMessage = time (NULL);
      }
  
    /* delete players who have closed their comms - have to do it outside other loops to avoid */
    /* access violations (iterating loops that have had items removed) */
    for (listiter = playerlist.begin (); listiter != playerlist.end (); )
      {
      tPlayer * p = *listiter;

      if (p->s == NO_SOCKET)
        {
        delete p;
        playerlist.erase (listiter);
        listiter = playerlist.begin ();		/* list iteration is no longer valid */
        }
      else
        listiter++;
      }	/* end of looping through players */
    
    /* get ready for "select" function ... */

    FD_ZERO (&in_set);
    FD_ZERO (&out_set);
    FD_ZERO (&exc_set);

    /* add our control socket */
    FD_SET (iControl, &in_set);
    iMaxdesc	= iControl;

    /* loop through all connections, adding them to the descriptor set */
    for (listiter = playerlist.begin (); listiter != playerlist.end (); listiter++)
      {
      tPlayer * p = *listiter;

      /* don't bother if connection is closed */
      if (p->s != NO_SOCKET)
        {
        iMaxdesc = UMAX (iMaxdesc, p->s);
        FD_SET( p->s, &in_set  );
        FD_SET( p->s, &exc_set );

        /* we are only interested in writing to sockets we have something for */
        if (!p->outbuf.empty ())
          FD_SET( p->s, &out_set );
        }	/* end of active player */

      }	/* end of looping through players */
    
    timeout.tv_sec = COMMS_WAIT_SEC;
    timeout.tv_usec = COMMS_WAIT_USEC;  /* time to wait before timing out */

    if (select (iMaxdesc + 1, &in_set, &out_set, &exc_set, &timeout) == 0)
      continue;		/* time  limit expired? - nothing to do this time */

    /* New connection on control port? */
    if (FD_ISSET (iControl, &in_set))
      ProcessNewConnection ();

    /* loop through all players */
    for (tPlayerListIterator listiter = playerlist.begin (); listiter != playerlist.end (); listiter++)
      {
      tPlayer * p = *listiter;
       
      /* handle exceptions */
      if (p->s != NO_SOCKET && FD_ISSET (p->s, &exc_set))
        ProcessException (p);

      /* look for ones we can read from, provided they aren't closed */
      if (p->s != NO_SOCKET && FD_ISSET (p->s, &in_set))
        ProcessRead (p);

      /* look for ones we can write to, provided they aren't closed */
      if (p->s != NO_SOCKET && FD_ISSET (p->s, &out_set))
        ProcessWrite (p);
 
      }   /* end of looping looking through all players */

    }  while (!bStopNow); 	/* end of looping processing input */

}		/* end of MainLoop */

/* Here when a signal is raised */

void bailout (int sig)
{
  printf ("**** Terminated by player on signal %i ****\n\n", sig);
  bStopNow = 1;
}	/* end of bailout */

int main (int argc, char* argv[])
{

	printf ("Tinymudserver version %s\n", VERSION);
  printf ("Accepting connections from port %i\n", PORT);

  /* standard termination signals */
  signal (SIGINT,  bailout);
  signal (SIGTERM, bailout);
  signal (SIGHUP,  bailout);

  /* initialise listening socket, exit if we can't */

  if (InitComms ())
    return 1;
  
  /* loop processing player input and other events */

  MainLoop ();

  /* tell them we have shut down */
  
  SendToAll (NULL, SHUTDOWN);

  /* wrap up */

  /* delete all players from list */
  for (tPlayerListIterator listiter = playerlist.begin (); listiter != playerlist.end (); listiter++)
    {
    tPlayer * p = *listiter;

    ProcessWrite (p);		/* force out closure message */
    delete p;
    }

  /* close listening port */
  CloseComms ();

	return 0;
}		/* end of main */
