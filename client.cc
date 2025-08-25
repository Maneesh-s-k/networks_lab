#include <iostream>
#include "client.h"
#include "common.h"
#include "config.h"
#include "logger.h"
#include "network.h"
#include "protocol.h"
#include "utils.h"              
#include "version.h"
#include <thread>
#include <chrono>   
#include <cstring>   // for std::memset
#include <cstdlib>   // for std::atoi
#include <unistd.h>  // for close() on Unix-like systems
#include <arpa/inet.h> // for inet_pton
#include <netinet/in.h> // for sockaddr_in
#include <sys/socket.h> // for socket functions
#include <fcntl.h>      // for fcntl()
#include <errno.h>     // for errno
#include <poll.h>     // for poll()

using namespace std;

class Client {
    public:
}