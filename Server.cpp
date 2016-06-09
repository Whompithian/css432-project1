/* 
 * @file    Server.cpp
 *           Adapted from examples in uw1-320-lab.uwb.edu:~css432/examples/, by
 *           Joe McCarthy
 * @brief   This program sets up a server that expects a connection from one
 *           client. The 
 * @author  Brendan Sweeney, SID 1161836
 * @date    October 9, 2012
 */

#include <cstdlib>
#include <stdio.h>
#include <sys/types.h>          // socket, bind
#include <sys/socket.h>         // socket, bind, listen, inet_ntoa
#include <netinet/in.h>         // htonl, htons, inet_ntoa
#include <arpa/inet.h>          // inet_ntoa
#include <netdb.h>              // gethostbyname
#include <unistd.h>             // read, write, close
#include <string.h>             // memset
#include <netinet/tcp.h>        // SO_REUSEADDR
#include <sys/uio.h>            // writev
#include <signal.h>             // sigaction
#include <fcntl.h>              // fcntl
#include <sys/time.h>           // gettimeofday

using namespace std;


static int BUFSIZE = 1500;      // size of incoming data buffer
int        nreps;               // get command line argument to other functions
int        newSd;               // get transfer socket to other functions

/*
 * Trigger on SIGIO for connection to a client. Start a timer, read from a
 *  network into a buffer a specified number of repetitions, keeping count of
 *  the number of reads, then stop the timer. Print time elapsed to stdout.
 *  Close data socket and exit this program.
 * @param  sig  Not used.
 * @param  siginfo  Not used.
 * @param  context  Not used.
 * @pre    newSd has been connected as a receive socket; a client has requested
 *          to send data to this server.
 * @post   All sockets are closed and the program exits; output is written to
 *          stdout indicating the time spend receiving data (in microseconds).
 */
static void iohandler (int sig, siginfo_t *siginfo, void *context)
{
    // if iohandler() called, SIGIO was signalled, 
    // so process I/O (input, in this case), and close socket
    char    databuf[BUFSIZE];   // receive buffer
    int     count = 0;          // counter for reads performed
    timeval start, stop;        // timer for time spent to receive
    
    // start timer for data receive
    if (gettimeofday(&start, NULL) < 0)
    {
        fprintf(stderr, "iohandler(): start time read failure\n");
        exit(EXIT_FAILURE);
    } // end if (gettimeofday(&start...))
    
    // receive data for the expected number of repetitions, incrementing count
    for (int i = 0; i < nreps; ++i)
    {
        for (int nRead = 0;
            (nRead += read(newSd, databuf, BUFSIZE - nRead)) < BUFSIZE; 
            ++count);
    } // end for (; i < nreps;)
    
    // stop timer for data receive
    if (gettimeofday(&stop, NULL) < 0)
    {
        fprintf(stderr, "iohandler(): stop time read failure\n");
        exit(EXIT_FAILURE);
    } // end if (gettimeofday(&stop...))
    
    // send read count back to client
    write(newSd, &count, sizeof(int));
    fprintf(stdout, "data-receiving time = %d usec\n",
      // iostream.h conflicted with another include, so I didn't use it. As a
      //  result, the results of the time calculations have to be cast to int.
      static_cast<int>(
      (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec));
    
    // close data socket and exit program
    close(newSd);
    exit(EXIT_SUCCESS);
} // end iohandler(int, siginfo_t*, void*)

/*
 * Setup a socket and listen for connections. Once a connection is established,
 *  setup a socket for data transfer and set that socket to asynchronous to
 *  wait for SIGIO to be received. Call iohandler() to manage data transfer.
 * @param  argc  Expected value is ARG_COUNT (default 3).
 * @param  argv  Expected to list port, nreps.
 * @pre    The program has been passed the two stated parameters, both of which
 *          contain valid values; port is available on this host.
 * @post   All sockets are closed and the server exits; a printout to stdout
 *          tells how much time the receive took (in microseconds); a read
 *          count is sent to the client.
 * @return 0 if no errors were encountered.
 */
int main(int argc, char** argv)
{
    int         port;                   // a server port number
    int         serverSd;               // listen socket
    const int   ARG_COUNT = 3;          // check command line argument count
    const int   ON = 1;                 // for asynchronous connection switch
    const int   ACCEPT = 5;             // number of clients to allow in queue
    sockaddr_in acceptSockAddr;         // address of listen socket
    sockaddr_in newSockAddr;            // address of data socket
    socklen_t   newSockAddrSize = sizeof( newSockAddr );
    struct      sigaction ioaction;     // respond to SIGIO
    
    // only continue if argument count is correct
    if (argc != ARG_COUNT)
    {
        fprintf(stderr, "Usage: %s <port> <nreps>\n", argv[0]);
        exit(EXIT_FAILURE);
    } // end if(argc != ARG_COUNT)
    
    // only continue if port is within a valid range
    port = atoi(argv[1]);
    if (port < 1024 || port > 65535)
    {
        fprintf(stderr, "%s: port must be between 1024 and 65535\n", argv[0]);
        exit(EXIT_FAILURE);
    } // end if (port < 1024 || port > 65535)
    
    // only continue if nreps is greater than 0
    nreps = atoi(argv[2]);
    if (nreps < 1)
    {
        fprintf(stderr, "%s: nreps must be positive\n", argv[0]);
        exit(EXIT_FAILURE);
    } // end if (nreps < 1)
    
    // build address data structure
    memset((char *)&acceptSockAddr, '\0', sizeof(acceptSockAddr));
    acceptSockAddr.sin_family      = AF_INET;   // Address Family Internet
    acceptSockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    acceptSockAddr.sin_port        = htons(port);
    memset (&ioaction, '\0', sizeof(ioaction));
    
    // active open, ensure success before continuing
    if ((serverSd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "%s: socket failure", argv[0]);
        exit(EXIT_FAILURE);
    } // end if ((serverSd = socket(...)))
    
    // setup server socket, bind, and listen for client connection
    setsockopt(serverSd, SOL_SOCKET, SO_REUSEADDR, (char *)&ON, sizeof(int));
    bind(serverSd, (sockaddr *)&acceptSockAddr, sizeof(acceptSockAddr));
    listen(serverSd, ACCEPT);
    
    // sleep indefinitely
    while(true)
    {
        // establish data connection
        newSd = accept(serverSd, (sockaddr *)&newSockAddr, &newSockAddrSize);
        // use sa_sigaction field because handle has two additional parameters
        ioaction.sa_sigaction = &iohandler;
        // SA_SIGINFO field, sigaction() use sa_sigaction field, not sa_handler
        ioaction.sa_flags = SA_SIGINFO;
        if (sigaction(SIGIO, &ioaction, NULL) < 0)
        {
            fprintf(stderr, "%s: sigaction failure", argv[0]);
            exit(EXIT_FAILURE);
        } // end if (sigaction(...))
        
        // make asynchronous and sleep
        fcntl(newSd, F_SETOWN, getpid());
        fcntl(newSd, F_SETFL, FASYNC);
        sleep(10);
    } // end while(true)

    close(serverSd);
    return 0;
} // end main(int, char**)
