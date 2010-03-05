#include "swift.h"

#define MAX_HTTP_CLIENT 128

struct http_gw_t {
    uint64_t offset;
    uint64_t tosend;
    int      transfer;
    SOCKET   sink;
} http_clients[MAX_HTTP_CLIENT];

void HttpGwErrorCallback (SOCKET sink) {

}

void HttpGwMayWriteCallback (SOCKET sink) {
    // if have data => write
    // otherwise, change mask
    if (not_enough_data)
        swift::Listen3rdPartySocket(http_conns[httpc].sink,NULL,NULL,ErrorCallback);
    if (all_done)
        swift::Listen3rdPartySocket(http_conns[httpc].sink,NewRequestCallback,NULL,ErrorCallback);
}


void SwiftProgressCallback (int transfer, bin64_t bin) {
    for (int httpc=0; httpc<conn_count; httpc++)
        if (http_conns[httpc].transfer==transfer) {
            // check mask
            if (bin==http_conns[httpc].offset)
                Listen3rdPartySocket(http_conns[httpc].sink,NULL,MayWriteCallback,ErrorCallback);
        }
}



void HttpGwNewRequestCallback (SOCKET http_conn){
    // read headers - the thrilling part
    // we surely do not support pipelining => one requests at a time
    fgets();
    // HTTP request line
    sscanf();
    // HTTP header fields
    sscanf();
    // incomplete header => screw it
    fprintf("400 Incomplete header\r\n");
    close();
    // initiate transmission
    // write response header
    http_clients[i].offset = 0;
    http_clients[i].tosend = 10000;
    http_clients[i].transfer = file;
    http_clients[i].sink = conn;
}


// be liberal in what you do, be conservative in what you accept
void HttpGwNewConnectionCallback (SOCKET serv) {
    Address client_address;
    SOCKET conn = accept (serv, & (client_address.addr), sizeof(struct sockaddr_in));
    if (conn==INVALID_SOCKET) {
        print_error("client conn fails");
        return;
    }
    make_socket_nonblocking(conn);
    // submit 3rd party socket to the swift loop
    socket_callbacks_t install(conn,HttpGwNewRequestCallback,HttpGwMayWriteCallback,HttpGwErrorCallback);
    swift::Listen3rdPartySocket(install);
}


void HttpGwError (SOCKET serv) {
    print_error("error on http socket");
}


SOCKET InstallHTTPGateway (Address bind_to) {
    SOCKET fd;
    #define gw_ensure(x) { if (!(x)) { print_error("http binding fails"); close_socket(fd); return INVALID_SOCKET; } }
    gw_ensure ( (fd=socket(AF_INET, SOCK_STREAM, 0)) != INVALID_SOCKET );
    gw_ensure ( 0==bind(fd, (sockaddr*)&(bind_to.addr), sizeof(struct sockaddr_in)) );
    gw_ensure (make_socket_nonblocking(fd));
    gw_ensure ( 0==listen(fd,8) );
    socket_callbacks_t install(sock,HttpGwNewConnectionCallback,NULL,HttpGwError);
    gw_ensure (swift::Listen3rdPartySocket(install));
}
