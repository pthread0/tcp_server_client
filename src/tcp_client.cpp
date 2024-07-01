
#include "../include/tcp_client.h"
#include "../include/common.h"

TcpClient::TcpClient() {
    _isConnected = false;
    _isClosed = true;
}

TcpClient::~TcpClient() {
    close();
}

pipe_ret_t TcpClient::connectTo(const std::string & address, int port, int srcPort) {
    try {
        initializeSocket();  // Ensure _sockfd is properly initialized
        if (srcPort >= 0 && srcPort <= 65535) {  // Corrected port range check
            bindToSourcePort(srcPort);  // Bind to source port if specified
        }
        setAddress(address, port);  // Set server address
    } catch (const std::runtime_error& error) {
        return pipe_ret_t::failure(error.what());  // Handle initialization errors
    }

    const int connectResult = connect(_sockfd.get(), (struct sockaddr *)&_server, sizeof(_server));
    if (connectResult == -1) {
        return pipe_ret_t::failure(strerror(errno));  // Handle connection failure
    }

    startReceivingMessages();  // Start receiving messages asynchronously if needed
    _isConnected = true;  // Update connection status
    _isClosed = false;  // Update closed status

    return pipe_ret_t::success();  // Return success
}

void TcpClient::bindToSourcePort(int srcPort) {
    struct sockaddr_in srcAddr;
    memset(&srcAddr, 0, sizeof(srcAddr));
    srcAddr.sin_family = AF_INET;
    srcAddr.sin_port = htons(srcPort);
    srcAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(_sockfd.get(), (struct sockaddr*)&srcAddr, sizeof(srcAddr)) < 0) {
        std::string errorMessage = "Failed to bind to source port: " + std::to_string(srcPort) + ", " + strerror(errno);
        throw std::runtime_error(errorMessage);
    }
}

void TcpClient::startReceivingMessages() {
    _receiveTask = new std::thread(&TcpClient::receiveTask, this);
}

void TcpClient::initializeSocket() {
    pipe_ret_t ret;

    _sockfd.set(socket(AF_INET , SOCK_STREAM , 0));
    const bool socketFailed = (_sockfd.get() == -1);
    if (socketFailed) {
        throw std::runtime_error(strerror(errno));
    }
}

void TcpClient::setAddress(const std::string& address, int port) {
    const int inetSuccess = inet_aton(address.c_str(), &_server.sin_addr);

    if(!inetSuccess) { // inet_addr failed to parse address
        // if hostname is not in IP strings and dots format, try resolve it
        struct hostent *host;
        struct in_addr **addrList;
        if ( (host = gethostbyname( address.c_str() ) ) == nullptr){
            throw std::runtime_error("Failed to resolve hostname");
        }
        addrList = (struct in_addr **) host->h_addr_list;
        _server.sin_addr = *addrList[0];
    }
    _server.sin_family = AF_INET;
    _server.sin_port = htons(port);
}


pipe_ret_t TcpClient::sendMsg(const char * msg, size_t size) {
    const size_t numBytesSent = send(_sockfd.get(), msg, size, 0);

    if (numBytesSent < 0 ) { // send failed
        return pipe_ret_t::failure(strerror(errno));
    }
    if (numBytesSent < size) { // not all bytes were sent
        char errorMsg[100];
        sprintf(errorMsg, "Only %lu bytes out of %lu was sent to client", numBytesSent, size);
        return pipe_ret_t::failure(errorMsg);
    }
    return pipe_ret_t::success();
}

void TcpClient::subscribe(const client_observer_t & observer) {
    std::lock_guard<std::mutex> lock(_subscribersMtx);
    _subscibers.push_back(observer);
}

/*
 * Publish incomingPacketHandler client message to observer.
 * Observers get only messages that originated
 * from clients with IP address identical to
 * the specific observer requested IP
 */
void TcpClient::publishServerMsg(const char * msg, size_t msgSize) {
    std::lock_guard<std::mutex> lock(_subscribersMtx);
    for (const auto &subscriber : _subscibers) {
        if (subscriber.incomingPacketHandler) {
            subscriber.incomingPacketHandler(msg, msgSize);
        }
    }
}

/*
 * Publish client disconnection to observer.
 * Observers get only notify about clients
 * with IP address identical to the specific
 * observer requested IP
 */
void TcpClient::publishServerDisconnected(const pipe_ret_t & ret) {
    std::lock_guard<std::mutex> lock(_subscribersMtx);
    for (const auto &subscriber : _subscibers) {
        if (subscriber.disconnectionHandler) {
            subscriber.disconnectionHandler(ret);
        }
    }
}

/*
 * Receive server packets, and notify user
 */
void TcpClient::receiveTask() {
    while(_isConnected) {
        const fd_wait::Result waitResult = fd_wait::waitFor(_sockfd);

        if (waitResult == fd_wait::Result::FAILURE) {
            throw std::runtime_error(strerror(errno));
        } else if (waitResult == fd_wait::Result::TIMEOUT) {
            continue;
        }

        char msg[MAX_PACKET_SIZE];
        const size_t numOfBytesReceived = recv(_sockfd.get(), msg, MAX_PACKET_SIZE, 0);

        if(numOfBytesReceived < 1) {
            std::string errorMsg;
            if (numOfBytesReceived == 0) { //server closed connection
                errorMsg = "Server closed connection";
            } else {
                errorMsg = strerror(errno);
            }
            _isConnected = false;
            publishServerDisconnected(pipe_ret_t::failure(errorMsg));
            return;
        } else {
            publishServerMsg(msg, numOfBytesReceived);
        }
    }
}

void TcpClient::terminateReceiveThread() {
    _isConnected = false;

    if (_receiveTask) {
        _receiveTask->join();
        delete _receiveTask;
        _receiveTask = nullptr;
    }
}

pipe_ret_t TcpClient::close(){
    if (_isClosed) {
        return pipe_ret_t::failure("client is already closed");
    }
    terminateReceiveThread();

    const bool closeFailed = (::close(_sockfd.get()) == -1);
    if (closeFailed) {
        return pipe_ret_t::failure(strerror(errno));
    }
    _isClosed = true;
    return pipe_ret_t::success();
}


