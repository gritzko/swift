#include "swift.h"

using namespace swift;

#define HTTPGW_MAX_CLIENT 128

enum {
    HTTPGW_RANGE=0,
    HTTPGW_MAX_HEADER=1
};
char * HTTPGW_HEADERS[HTTPGW_MAX_HEADER] = {
    "Content-Range"
};


struct http_gw_t {
    uint64_t offset;
    uint64_t tosend;
    int      transfer;
    SOCKET   sink;
    char*    headers[HTTPGW_MAX_HEADER];
} http_requests[HTTPGW_MAX_CLIENT];


int http_gw_reqs_open = 0;

void HttpGwNewRequestCallback (SOCKET http_conn);
void HttpGwNewRequestCallback (SOCKET http_conn);

http_gw_t* HttpGwFindRequest (SOCKET sock) {
    for(int i=0; i<http_gw_reqs_open; i++)
        if (http_requests[i].sink==sock)
            return http_requests+i;
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
    swift::close_socket(sock);
}
 

void HttpGwMayWriteCallback (SOCKET sink) {
    http_gw_t* req = HttpGwFindRequest(sink);
    uint64_t complete = swift::SeqComplete(req->transfer);
    if (complete>req->offset) { // send data
        char buf[1<<12];
        uint64_t tosend = std::min((uint64_t)1<<12,complete-req->offset);
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
        if (swift::IsComplete(req->transfer)) { // done; wait for new request
            socket_callbacks_t wait_new_req(req->sink,HttpGwNewRequestCallback,NULL,HttpGwCloseConnection);
            swift::Listen3rdPartySocket (wait_new_req);
        } else { // wait for data
            socket_callbacks_t wait_swift_data(req->sink,NULL,NULL,HttpGwCloseConnection);
            swift::Listen3rdPartySocket(wait_swift_data);
        }
    }
}


void HttpGwSwiftProgressCallback (int transfer, bin64_t bin) {
    for (int httpc=0; httpc<http_gw_reqs_open; httpc++)
        if (http_requests[httpc].transfer==transfer)
            if ( (bin.offset()<<10) == http_requests[httpc].offset ) {
                socket_callbacks_t maywrite_callbacks
                        (http_requests[httpc].sink,NULL,HttpGwMayWriteCallback,HttpGwCloseConnection);
                Listen3rdPartySocket (maywrite_callbacks);
            }
}


void HttpGwFirstProgressCallback (int transfer, bin64_t bin) {
    printf("200 OK\r\n");
    printf("Content-Length: value\r\n");
    swift::RemoveProgressCallback(transfer,&HttpGwFirstProgressCallback);
    swift::AddProgressCallback(transfer,&HttpGwSwiftProgressCallback);
    HttpGwSwiftProgressCallback(transfer,bin);
}


void HttpGwNewRequestCallback (SOCKET http_conn){
    http_gw_t* req = http_requests + http_gw_reqs_open++;
    req->sink = http_conn;
    // read headers - the thrilling part
    // we surely do not support pipelining => one request at a time
    #define HTTPGW_MAX_REQ_SIZE 1024
    char buf[HTTPGW_MAX_REQ_SIZE+1];
    int rd = recv(http_conn,buf,HTTPGW_MAX_REQ_SIZE,0);
    if (rd<=0) { // if conn is closed by the peer, rd==0
        HttpGwCloseConnection(http_conn);
        return;
    }
    buf[rd] = 0;
    // HTTP request line
    char* reqline = strtok(buf,"\r\n");
    char method[16], url[512], version[16], crlf[5];
    if (4!=sscanf(reqline,"%16s %512s %16s%4[\n\r]",method,url,version,crlf)) {
        HttpGwCloseConnection(http_conn);
        return;
    }
    // HTTP header fields
    char* headerline;
    while (headerline=strtok(NULL,"\n\r")) {
        char header[128], value[256];
        if (3!=sscanf(headerline,"%120[^: \r\n]: %250[^\r\n]%4[\r\n]",header,value,crlf)) {
            HttpGwCloseConnection(http_conn);
            return;
        }
        for(int i=0; i<HTTPGW_MAX_HEADER; i++)
            if (0==strcasecmp(HTTPGW_HEADERS[i],header) && !req->headers[i])
                req->headers[i] = strdup(value);
    }
    // parse URL
    char * hashch=strtok(url,"/"), hash[41];
    while (hashch && (1!=sscanf(hashch,"%40[0123456789abcdefABCDEF]",hash) || strlen(hash)!=40))
        hashch = strtok(NULL,"/");
    if (strlen(hash)!=40) {
        HttpGwCloseConnection(http_conn);
        return;
    }
    // initiate transmission
    int file = swift::Open(hash,hash);
    // find/create transfer
    swift::AddProgressCallback(file,&HttpGwFirstProgressCallback);
    // write response header
    req->offset = 0;
    req->tosend = 10000;
    req->transfer = file;
    socket_callbacks_t install (http_conn,NULL,NULL,HttpGwCloseConnection);
    swift::Listen3rdPartySocket(install);
}


// be liberal in what you do, be conservative in what you accept
void HttpGwNewConnectionCallback (SOCKET serv) {
    Address client_address;
    socklen_t len;
    SOCKET conn = accept 
        (serv, (sockaddr*) & (client_address.addr), &len);
    if (conn==INVALID_SOCKET) {
        print_error("client conn fails");
        return;
    }
    make_socket_nonblocking(conn);
    // submit 3rd party socket to the swift loop
    socket_callbacks_t install
        (conn,HttpGwNewRequestCallback,NULL,HttpGwCloseConnection);
    swift::Listen3rdPartySocket(install);
}


void HttpGwError (SOCKET s) {
    print_error("everything fucked up");
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
    socket_callbacks_t install(fd,HttpGwNewConnectionCallback,NULL,HttpGwError);
    gw_ensure (swift::Listen3rdPartySocket(install));
}
