/* 
 * @file    Client.cpp
 *           Adapted from examples in uw1-320-lab.uwb.edu:~css432/examples/, by
 *           Joe McCarthy
 * @brief   This program is a network client designed to test connection speeds
 *           using various methods of data transmission. The server name and
 *           port are passed on the command line, as well as parameters for the
 *           type of transmission and the desired number of repetitions. Output
 *           tells how long the transfer took (in microseconds) and how many
 *           read operations the server had to perform.
 * @author  Brendan Sweeney, SID 1161836
 * @date    October 9, 2012
 */


#include <stdio.h>
#include <cstdlib>
#include <arpa/inet.h>          // inet_ntoa
#include <netdb.h>              // gethostbyname
#include <unistd.h>             // read, write, close
#include <string.h>             // memset
#include <sys/time.h>           // gettimeofday

using namespace std;

// The transfer type: multiple-write, write-v, and single-write
enum tranType {MULTIPLE = 1, WRITEV, SINGLE};

/**
 * Establish a connection to a server and send a data buffer over a network.
 *  Close the connection when done. Print time and read statistics to stdout.
 * @param  argc  Expected value is ARG_COUNT (default 7).
 * @param  argv  Expected to list port, nreps, nbufs, bufsize, serverIP, type.
 * @pre    The program has been passed the six stated parameters, all of which
 *          contain valid values; nreps x nbufs = 1500; the provided server is
 *          already listening on the specified port, expecting the specified
 *          number of transmit repetitions.
 * @post   All sockets are closed and the server exits with this program; a
 *          printout to stdout tells how long the transfer took and how many
 *          reads were performed by the server.
 * @return 0 if no errors were encountered.
 */
int main(int argc, char** argv)
{
    int         port;                   // a server port number
    int         nreps;                  // repetitions for sending
    int         nbufs;                  // number of data buffers
    int         bufsize;                // size of each data buffer (in bytes)
    char        *serverIp;              // server IP name
    tranType    type;                   // transfer scenario: 1, 2, or 3
    const int   ARG_COUNT = 7;          // verify command line argument count
    int         clientSd;               // for the client-side socket
    int         count = 0;              // read count from server
    struct hostent *host;               // for resolved server from host name
    timeval     start, lap, stop;       // timer checkpoints for output
    sockaddr_in sendSockAddr;           // address data structure
    
    // only continue if there are enough command line arguments
    if (argc != ARG_COUNT)
    {
        fprintf(stderr,
              "Usage: %s <port> <nreps> <nbufs> <bufsize> <serverIP> <type>\n",
              argv[0]);
        exit(EXIT_FAILURE);
    } // end if(argc != ARG_COUNT)
    
    // store arguments in local variables
    port     = atoi(argv[1]);
    nreps    = atoi(argv[2]);
    nbufs    = atoi(argv[3]);
    bufsize  = atoi(argv[4]);
    serverIp = argv[5];
    type     = static_cast<tranType>(atoi(argv[6]));    // type is tranType
    
    // initialize data buffer, clearing any garbage data, and resolve host
    char databuf[nbufs][bufsize];
    memset(databuf, 'B', sizeof(databuf));
    host = gethostbyname(serverIp);
    
    // only continue if host name could be resolved
    if (!host)
    {
        fprintf(stderr, "%s: unknown hostname: %s\n", argv[0], serverIp);
        exit(EXIT_FAILURE);
    } // end if (!host)
    
    // build address data structure
    memset((char *)&sendSockAddr, '\0', sizeof(sendSockAddr));
    sendSockAddr.sin_family = AF_INET;
    sendSockAddr.sin_addr.s_addr =
        inet_addr(inet_ntoa(*(struct in_addr *) *host->h_addr_list));
    sendSockAddr.sin_port = htons(port);
    
    // active open, ensure success before continuing
    if ((clientSd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "%s: socket failure\n", argv[0]);
        exit(EXIT_FAILURE);
    } // end if ((clientSd = socket(...)))
    
    // only continue if socket connection could be established
    if (connect(clientSd, (sockaddr *)&sendSockAddr, sizeof(sendSockAddr)) < 0)
    {
        fprintf(stderr, "%s: connect failure\n", argv[0]);
        close(clientSd);
        exit(EXIT_FAILURE);
    } // end if (connect(...))
    
    // start time before sending data
    if (gettimeofday(&start, NULL) < 0)
    {
        fprintf(stderr, "%s: start time read failure\n", argv[0]);
        exit(EXIT_FAILURE);
    } // end if (gettimeofday(&start...))
    
    // check write method and use the correct one nreps times
    if (type == MULTIPLE)
    {   // send bufsize worth of data in each of nbufs data buffers
        for (int i = 0; i < nreps; ++i)
        {
            for (int j = 0; j < nbufs; ++j)
            {
                write(clientSd, databuf[j], bufsize);
            } // end for (; j < nbufs;)
        } // end for (; i < nreps;)
    } // end (type == MULTIPLE)
    else if (type == WRITEV)
    {   // send data with i/o vectors
        struct iovec vector[nbufs];
        for (int i = 0; i < nreps; ++i)
        {
            for (int j = 0; j < nbufs; ++j)
            {
                vector[j].iov_base = databuf[j];
                vector[j].iov_len = bufsize;
            } // end for (; j < nbufs;)
            writev(clientSd, vector, nbufs);
        } // end for (; i < nreps;)
    } // end (type == WRITEV)
    else if (type == SINGLE)
    {   // send all data in databuf at once
        for (int i = 0; i < nreps; ++i)
        {
            write(clientSd, databuf, nbufs * bufsize);
        } // end for (; i < nreps;)
    } // end (type == SINGLE)
    else
    {
        // assuming input not intentionally bad; check here rather than sooner
        fprintf(stderr, "%s: invalid write type\n", argv[0]);
        close(clientSd);
        exit(EXIT_FAILURE);
    } // end else (type ==)
    
    // data transfer end time
    if (gettimeofday(&lap, NULL) < 0)
    {
        fprintf(stderr, "%s: lap time read failure\n", argv[0]);
        exit(EXIT_FAILURE);
    } // end if (gettimeofday(&lap...))
    
    // get read count from server, then close connection
    read(clientSd, &count, sizeof(int));
    close(clientSd);
    
    // end time after connection is terminated
    if (gettimeofday(&stop, NULL) < 0)
    {
        fprintf(stderr, "%s: stop time read failure\n", argv[0]);
        exit(EXIT_FAILURE);
    } // end if (gettimeofday(&stop...))
    
    // provide time statistics and read count for data transfer
    fprintf(stdout,
      "data-sending time = %d usec, round-trip time = %d usec, # reads = %d\n",
      // iostream.h conflicted with another include, so I didn't use it. As a
      //  result, the results of the time calculations have to be cast to int.
      static_cast<int>(
        (lap.tv_sec - start.tv_sec) * 1000000 + lap.tv_usec - start.tv_usec),
      static_cast<int>(
        (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec),
      count);
    
    return 0;   // success
} // end main(int, char**)
