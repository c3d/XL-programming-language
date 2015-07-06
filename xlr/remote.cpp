// ****************************************************************************
//  remote.cpp                                                     Tao project
// ****************************************************************************
//
//   File Description:
//
//     Implementation of a simple socket-based transport for XL programs
//
//
//
//
//
//
//
//
// ****************************************************************************
//  (C) 2015 Christophe de Dinechin <christophe@taodyne.com>
//  (C) 2015 Taodyne SAS
// ****************************************************************************

#include "remote.h"
#include "runtime.h"
#include "serializer.h"
#include "main.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <string>
#include <sstream>
#include <ext/stdio_filebuf.h>


XL_BEGIN


// ============================================================================
// 
//   Utilities for the code below
// 
// ============================================================================

static Tree *xl_read_tree(int sock)
// ----------------------------------------------------------------------------
//   Read a tree directly from the socket
// ----------------------------------------------------------------------------
{
    int fd = dup(sock);         // stdio_filebuf closes its fd in dtor
    __gnu_cxx::stdio_filebuf<char> filebuf(fd, std::ios::in);
    std::istream is(&filebuf);
    return Deserializer::Read(is);
}


static void xl_write_tree(int sock, Tree *tree)
// ----------------------------------------------------------------------------
//   Write a tree directly into the socket
// ----------------------------------------------------------------------------
{
    int fd = dup(sock);         // stdio_filebuf closes its fd in dtor
    __gnu_cxx::stdio_filebuf<char> filebuf(fd, std::ios::out);
    std::ostream os(&filebuf);
    Serializer::Write(os, tree);
}



// ============================================================================
//
//    Simple program exchange over TCP/IP
//
// ============================================================================

static int xl_send(text host, Tree *code)
// ----------------------------------------------------------------------------
//   Send the text for the given body to the target host, return open fd
// ----------------------------------------------------------------------------
{
    // Compute port number
    int port = XL_DEFAULT_PORT;
    size_t found = host.rfind(':');
    if (found != std::string::npos)
    {
        text portText= host.substr(found+1);
        port = atoi(portText.c_str());
        if (!port)
        {
            std::cerr << "xl_tell: Port '" << portText << " is invalid, "
                      << "using " << XL_DEFAULT_PORT << "\n";
            port = XL_DEFAULT_PORT;
        }
        host = host.substr(0, found);
    }

    // Open socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "xl_tell: Error opening socket: "
                  << strerror(errno) << "\n";
        return -1;
    }

    // Resolve server name
    struct hostent *server = gethostbyname(host.c_str());
    if (!server)
    {
        std::cerr << "xl_tell: Error resolving server '" << host << "': "
                  << strerror(errno) << "\n";
        return -1;
    }

    // Initialize socket
    sockaddr_in address = { 0 };
    address.sin_family = AF_INET;
    memcpy((char *) &address.sin_addr.s_addr,
           (char *) server->h_addr,
           server->h_length);
    address.sin_port = htons(port);

    // Connect
    if (connect(sock, (struct sockaddr *) &address, sizeof(address)) < 0)
    {
        std::cerr << "xl_tell: Error connecting to '"
                  << host << "' port " << port << ": "
                  << strerror(errno) << "\n";
        return -1;
    }

    // Write program to socket
    xl_write_tree(sock, code);

    return sock;
}


int xl_tell(text host, Tree *code)
// ----------------------------------------------------------------------------
//   Send the text for the given body to the target host
// ----------------------------------------------------------------------------
{
    IFTRACE(remote)
        std::cerr << "xl_tell: Telling " << host << ":\n"
                  << code << "\n";
    int sock = xl_send(host, code);
    if (sock < 0)
        return sock;
    close(sock);
    return 0;
}


Tree_p xl_ask(text host, Tree *code)
// ----------------------------------------------------------------------------
//   Send code to the target, wait for reply
// ----------------------------------------------------------------------------
{
    IFTRACE(remote)
        std::cerr << "xl_ask: Asking " << host << ":\n"
                  << code << "\n";
    int sock = xl_send(host, code);
    if (sock < 0)
        return xl_nil;
    
    Tree *result = xl_read_tree(sock);
    IFTRACE(remote)
        std::cerr << "xl_ask: Response from " << host << " was:\n"
                  << result << "\n";
    close(sock);

    return result;
}


static int active_children = 0;
static void child_died(int)
// ----------------------------------------------------------------------------
//    When a child dies, get its exit status
// ----------------------------------------------------------------------------
{
    while (int childPID = waitpid(-1, NULL, WNOHANG))
    {
        if (childPID < 0)
            break;
        IFTRACE(remote)
            std::cerr << "xl_listen: Child PID " << childPID << " died\n";
        active_children--;
    }
}


static sockaddr_in client = { 0 };


int xl_listen(Context *context, uint port)
// ----------------------------------------------------------------------------
//    Listen on the given port for sockets, evaluate programs when received
// ----------------------------------------------------------------------------
{
    // Open the socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "xl_listen: Error opening socket: "
                  << strerror(errno) << "\n";
        return -1;
    }
    
    int option = 1;
    if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR,
                    (char *)&option, sizeof (option)) < 0)
        std::cerr << "xl_listen: Error setting SO_REUSEADDR: "
                  << strerror(errno) << "\n";

    struct sockaddr_in address = { 0 };
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *) &address, sizeof(address)) < 0)
    {
        std::cerr << "xl_listen: Error binding to port " << port << ": "
                  << strerror(errno) << "\n";
        return -1;
    }

    // Listen to socket
    listen(sock, 5);

    // Make sure we get notified when a child dies
    signal(SIGCHLD, child_died);

    // Accept client
    int forking = MAIN->options.listen_forks;
    while (true)
    {
        // Block until we can accept more connexions (avoid fork bombs)
        while (forking && active_children >= forking)
        {
            IFTRACE(remote)
                std::cerr << "xl_listen: Too many children, waiting\n";
            if (int childPID = waitpid(-1, NULL, 0))
            {
                if (childPID > 0)
                {
                    IFTRACE(remote)
                        std::cerr << "xl_listen: Child " << childPID
                                  << " died, resuming\n";
                    active_children--;
                }
            }
        }

        // Accept input
        IFTRACE(remote)
            std::cerr << "xl_listen: Accepting input\n";
        socklen_t length = sizeof(client);
        int insock = accept(sock, (struct sockaddr *) &client, &length);
        if (insock < 0)
        {
            std::cerr << "xl_listen: Error accepting port " << port << ": "
                      << strerror(errno) << "\n";
            continue;
        }
        std::cerr << "xl_listen: Got incoming connexion\n";

        // Fork child for incoming connexion
        int pid = forking ? fork() : 0;
        if (pid == -1)
        {
            std::cerr << "xl_listen: Error forking child\n";
        }
        else if (pid)
        {
            IFTRACE(remote)
                std::cerr << "xl_listen: Forked pid " << pid << "\n";
            close(insock);
            active_children++;
        }
        else
        {
            // Read data from client
            Tree *code = xl_read_tree(insock);
            
            // Evaluate resulting code
            if (code)
            {
                IFTRACE(remote)
                    std::cerr << "xl_listen: Received code: " << code << "\n";
                Tree_p result = context->Evaluate(code);
                IFTRACE(remote)
                    std::cerr << "xl_listen: Evaluated as: " << result << "\n";
                xl_write_tree(insock, result);
                IFTRACE(remote)
                    std::cerr << "xl_listen: Response sent\n";
            }
            close(insock);

            if (forking)
            {
                IFTRACE(remote)
                    std::cerr << "xl_listen: Exiting PID " << getpid() << "\n";
                exit(0);
            }
        }
    }
        
}


int xl_reply(Tree *code)
// ----------------------------------------------------------------------------
//   Send code back to whoever invoked us
// ----------------------------------------------------------------------------
{
    // Open socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "xl_reply: Error opening socket: "
                  << strerror(errno) << "\n";
        return -1;
    }

    // Connect
    if (connect(sock, (struct sockaddr *) &client, sizeof(client)) < 0)
    {
        std::cerr << "xl_reply: Error replying to '"
                  << inet_ntoa(client.sin_addr)
                  << "' port " << ntohs(client.sin_port) << ": "
                  << strerror(errno) << "\n";
        return -1;
    }

    // Get program in serialized form
    std::ostringstream out;
    Serializer serialize(out);
    code->Do(serialize);
    text payload = out.str();

    // Write the payload
    uint sent = write(sock, payload.data(), payload.length());
    if (sent < payload.length())
    {
        std::cerr << "xl_tell: Error writing data: "
                  << strerror(errno) << "\n";
        return -1;
    }

    close(sock);
    return 0;
}
    



XL_END
