Nick Gammon originally [shared this in 2003](http://www.gammon.com.au/forum/?id=2209), and then later went on to [release an improved version](http://www.gammon.com.au/forum/?id=4496).

He maintains the code here: https://github.com/nickgammon/tinymudserver

I created this repository for historical reasons, and also for demonstrating the basics of a very simple MUD server. I felt like it was easier to digest in his original, single file form. There is even more to be learned from his current repository, though.

========================================================
Tiny Mud Server (tinymudserver)
========================================================

Author: Nick Gammon
Email:  nick@gammon.com.au
Date:   8th January 2003
Web:    www.gammon.com.au

Post questions, comments, bug reports to the forum at:

        http://www.gammon.com.au/forum/

COPYRIGHT

 This program is placed in the public domain. 

 You may use it as a starting point for writing your own MUD server.

CREDIT

 I would appreciated being credited in the event that you use this code.

NO WARRANTY

 Because the program is in the public domain, there is no warranty for the program,
 to the extent permitted by applicable law.

 The author provides the program "as is" without warranty of any kind, either
 expressed or implied, including, but not limited to, the implied warranties of
 merchantability and fitness for a particular purpose.

 The entire risk as to the quality and performance of the program is with you.

 Should the program prove defective, you assume the cost of all necessary servicing,
 repair or correction.

INSTALLATION

 The simplest way to compile the program is to type "make". This should use the
 enclosed "Makefile" to compile and link. If this doesn't work, to compile without
 using the makefile:

   gcc tinymudserver.cpp -o tinymudserver -g -Wall

EXECUTION

 Run the server like this:

  ./tinymudserver &

CONNECTING

 The default behaviour is to listen for connections on port 4000 (change a define in 
 the code to alter this). To test the server you could connect to it like this:

  telnet localhost 4000

DESCRIPTION

 This program demonstrates a simple MUD (Multi-User Dungeon) server - in a single file. 

 It does the following:

 * Accepts multiple connections from players
 * Maintains a list of connected players
 * Asks players for a name and password (in this version the password is the name)
 * Implements the commands: quit, look, say, tell
 * Illustrates sending messages to a single player (eg. a tell) or all players
   (eg. a say)
 * Handles players disconnecting or quitting
 * Illustrates a "connection dialog" - players get asked their name, then their password.
 * Demonstrates using stl for lists and strings
 * Illustrates period messages using a timer (at present it just shows a message every
   30 seconds)

WHAT YOU COULD ADD

 As it stands the program is too simple to be used for a full MUD, however it could be the
 basis for writing your own. For instance, there is no file handling at present. You would
 want to add things like this:

 * Load/save player details to player files - I would suggest using XML for this
 * Load messages (eg. message-of-the-day)
 * Load room details (eg. area files) - again I suggest using XML
 * Code for moving from room to room, taking/dropping things, etc.
 * Fighting (if required)
 * Building/extending online
 * Logging events (eg. connections, disconnections, faults)
 * Colour
 * MCCP (compression)
 * MXP  (MUD Extension Protocol)
 * Telnet negotiation

