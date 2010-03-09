#include "swift.h"

#define HTTPGW_MAX_CLIENT 128
enum {
    HTTPGW_RANGE,
    HTTPGW_MAX_HEADER
};


struct http_gw_t {
    uint64_t offset;
    uint64_t tosend;
    int      transfer;
    SOCKET   sink;
    char*    headers[HTTP_MAX_HEADER];
} http_requests[HTTP_MAX_CLIENT];


int http_gw_reqs_open = 0;


http_gw_t* HttpGwFindRequest (SOCKET sock) {
    for(int i=0; i<http_gw_reqs_open; i++)
        if (http_requests[i].sink==sock)
            return http_requests[i];
    return NULL;
}


void HttpGwCloseConnection (SOCKET sock) {
    http_gw_t* req = HttpGwFindRequest(sock);
    if (req) {
        for(int i=0; i<HTTPGW_MAX_HEADER; i++)
            if (req->headers[i]) {
                free(req->headers[i]);
                req->headers[i] = NULL;
            }
        *req = http_requests[http_gw_reqs_open--];
    }
    close_socket(sock);
}
 

void HttpGwMayWriteCallback (SOCKET sink) {
    http_gw_t* req = HttpGwFindRequest(sink);
    uint64_t complete = swift::SeqComplete(http_requests[reqi].transfer);
    if (complete>req->offset) { // send data
        char buf[1LL<<12];
        uint64_t tosend = std::min(1LL<<12,complete-req->offset);
        size_t rd = read(req->transfer,buf,tosend); // hope it is cached
        if (rd<0) {
            HttpGwCloseConnection(sink);
            return;
        }
        size_t wn = send(sink, buf, rd, 0);
        if (wn<0) {
            HttpGwCloseConnection(sink);
            return;
        }
        req->offset += wn;
        req->tosend -= wn;
    } else {
        if (swift::IsComplete(http_requests[reqi].transfer))  // done; wait for new request
            swift::Listen3rdPartySocket
                (http_conns[httpc].sink,NewRequestCallback,NULL,ErrorCallback);
        else  // wait for data
            swift::Listen3rdPartySocket(request.sink,NULL,NULL,ErrorCallback);
    }
}


void SwiftProgressCallback (int transfer, bin64_t bin) {
    for (int httpc=0; httpc<conn_count; httpc++)
        if (http_conns[httpc].transfer==transfer) {
            if (bin.offset()<<10==http_conns[httpc].offset)
                swift::Listen3rdPartySocket
                (http_conns[httpc].sink,NULL,MayWriteCallback,ErrorCallback);
        }
}


void HttpGwNewRequestCallback (SOCKET http_conn){
    // read headers - the thrilling part
    // we surely do not support pipelining => one request at a time
    #define HTTPGW_MAX_REQ_SIZE 1024
    char buf[HTTPGW_MAX_REQ_SIZE+1];
    int rd = recv(http_conn,buf,HTTPGW_MAX_REQ_SIZE,0);
    if (rd<=0) { // if conn is closed by the peer, rd==0
        HttpGwCloseRequest(http_conn);
        return;
    }
    buf[rd] = 0;
    // HTTP request line
    char* reqline = strtok(buf,"\r\n");
    char method[16], url[512], version[16], crlf[5];
    if (4!=sscanf(reqline,"%16s %512s %16s%4[\n\r]",method,url,version,crlf)) {
        HttpGwCloseRequest(http_conn);
        return;
    }
    // HTTP header fields
    char* headerline;
    while (headerline=strtok(NULL,"\n\r")) {
        char header[128], value[256];
        if (3!=sscanf(headerline,"%120[^: \r\n]: %250[^\r\n]%4[\r\n]",header,value,crlf)) {
            HttpGwCloseRequest(http_conn);
            return;
        }
        for(int i=0; i<HTTPGW_HEADER_COUNT; i++)
            if (0==strcasecmp(HTTPGW_HEADERS[i],header) && !http_requests[reqi])
                http_requests[reqi] = strdup(value);
    }
    // initiate transmission
    // parse URL
    // find/create transfer
    SwiftProgressCallback;
    // write response header
    sprintf("200 OK\r\n");
    sprintf("Header: value\r\n");
    http_clients[i].offset = 0;
    http_clients[i].tosend = 10000;
    http_clients[i].transfer = file;
    http_clients[i].sink = conn;
}


// be liberal in what you do, be conservative in what you accept
void HttpGwNewConnectionCallback (SOCKET serv) {
    Address client_address;
    SOCKET conn = accept 
        (serv, & (client_address.addr), sizeof(struct sockaddr_in));
    if (conn==INVALID_SOCKET) {
        print_error("client conn fails");
        return;
    }
    make_socket_nonblocking(conn);
    // submit 3rd party socket to the swift loop
    socket_callbacks_t install
        (conn,HttpGwNewRequestCallback,HttpGwMayWriteCallback,HttpGwErrorCallback);
    swift::Listen3rdPartySocket(install);
}


void HttpGwError (SOCKET serv) {
    print_error("error on http socket");
}


SOCKET InstallHTTPGateway (Address bind_to) {
    SOCKET fd;
    #define gw_ensure(x) { if (!(x)) { \
    print_error("http binding fails"); close_socket(fd); \
    return INVALID_SOCKET; } }
    gw_ensure ( (fd=socket(AF_INET, SOCK_STREAM, 0)) != INVALID_SOCKET );
    gw_ensure ( 0==bind(fd, (sockaddr*)&(bind_to.addr), sizeof(struct sockaddr_in)) );
    gw_ensure (make_socket_nonblocking(fd));
    gw_ensure ( 0==listen(fd,8) );
    socket_callbacks_t install(sock,HttpGwNewConnectionCallback,NULL,HttpGwError);
    gw_ensure (swift::Listen3rdPartySocket(install));
}
