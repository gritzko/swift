#include "swift.h"

using namespace swift;

#define HTTPGW_MAX_CLIENT 128

enum {
    HTTPGW_RANGE=0,
    HTTPGW_MAX_HEADER=1
};
const char * HTTPGW_HEADERS[HTTPGW_MAX_HEADER] = {
    "Content-Range"
};


struct http_gw_t {
    int      id;
    uint64_t offset;
    uint64_t tosend;
    int      transfer;
    SOCKET   sink;
    char*    headers[HTTPGW_MAX_HEADER];
} http_requests[HTTPGW_MAX_CLIENT];


int http_gw_reqs_open = 0;
int http_gw_reqs_count = 0;

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
        dprintf("%s @%i closed http connection %i\n",tintstr(),req->id,sock);
        for(int i=0; i<HTTPGW_MAX_HEADER; i++)
            if (req->headers[i]) {
                free(req->headers[i]);
                req->headers[i] = NULL;
            }
        *req = http_requests[--http_gw_reqs_open];
    }
    swift::close_socket(sock);
    swift::Datagram::Listen3rdPartySocket(sckrwecb_t(sock));
}


void HttpGwMayWriteCallback (SOCKET sink) {
    http_gw_t* req = HttpGwFindRequest(sink);
    uint64_t complete = swift::SeqComplete(req->transfer);
    if (complete>req->offset) { // send data
        char buf[1<<12];
        uint64_t tosend = std::min((uint64_t)1<<12,complete-req->offset);
        size_t rd = pread(req->transfer,buf,tosend,req->offset); // hope it is cached
        if (rd<0) {
            HttpGwCloseConnection(sink);
            return;
        }
        int wn = send(sink, buf, rd, 0);
        if (wn<0) {
            print_error("send fails");
            HttpGwCloseConnection(sink);
            return;
        }
        dprintf("%s @%i sent %ib\n",tintstr(),req->id,(int)wn);
        req->offset += wn;
        req->tosend -= wn;
    } else {
        if (req->tosend==0) { // done; wait for new request
            dprintf("%s @%i done\n",tintstr(),req->id);
            sckrwecb_t wait_new_req
            //  (req->sink,HttpGwNewRequestCallback,NULL,HttpGwCloseConnection);
              (req->sink,NULL,NULL,NULL);
            HttpGwCloseConnection(sink);
            swift::Datagram::Listen3rdPartySocket (wait_new_req);
        } else { // wait for data
            dprintf("%s @%i waiting for data\n",tintstr(),req->id);
            sckrwecb_t wait_swift_data(req->sink,NULL,NULL,HttpGwCloseConnection);
            swift::Datagram::Listen3rdPartySocket(wait_swift_data);
        }
    }
}


void HttpGwSwiftProgressCallback (int transfer, bin64_t bin) {
    dprintf("%s @A pcb: %s\n",tintstr(),bin.str());
    for (int httpc=0; httpc<http_gw_reqs_open; httpc++)
        if (http_requests[httpc].transfer==transfer)
            if ( (bin.base_offset()<<10) <= http_requests[httpc].offset &&
                  ((bin.base_offset()+bin.width())<<10) > http_requests[httpc].offset  ) {
                dprintf("%s @%i progress: %s\n",tintstr(),http_requests[httpc].id,bin.str());
                sckrwecb_t maywrite_callbacks
                        (http_requests[httpc].sink,NULL,
                         HttpGwMayWriteCallback,HttpGwCloseConnection);
                Datagram::Listen3rdPartySocket (maywrite_callbacks);
            }
}


void HttpGwFirstProgressCallback (int transfer, bin64_t bin) {
    if (bin!=bin64_t(0,0)) // need the first packet
        return;
    swift::RemoveProgressCallback(transfer,&HttpGwFirstProgressCallback);
    swift::AddProgressCallback(transfer,&HttpGwSwiftProgressCallback,0);
    for (int httpc=0; httpc<http_gw_reqs_open; httpc++) {
        http_gw_t * req = http_requests + httpc;
        if (req->transfer==transfer && req->tosend==0) { // FIXME states
            uint64_t file_size = swift::Size(transfer);
            char response[1024];
            sprintf(response,
                "HTTP/1.1 200 OK\r\n"\
                "Connection: keep-alive\r\n"\
                "Content-Type: video/ogg\r\n"\
                /*"X-Content-Duration: 32\r\n"*/\
                "Content-Length: %lli\r\n"\
                "Accept-Ranges: none\r\n"\
                "\r\n",
                file_size);
            send(req->sink,response,strlen(response),0);
            req->tosend = file_size;
            dprintf("%s @%i headers_sent size %lli\n",tintstr(),req->id,file_size);
        }
    }
    HttpGwSwiftProgressCallback(transfer,bin);
}


void HttpGwNewRequestCallback (SOCKET http_conn){
    http_gw_t* req = http_requests + http_gw_reqs_open++;
    req->id = ++http_gw_reqs_count;
    req->sink = http_conn;
    req->offset = 0;
    req->tosend = 0;
    dprintf("%s @%i new http request\n",tintstr(),req->id);
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
    if (3!=sscanf(reqline,"%16s %512s %16s",method,url,version)) {
        HttpGwCloseConnection(http_conn);
        return;
    }
    // HTTP header fields
    char* headerline;
    while (headerline=strtok(NULL,"\n\r")) {
        char header[128], value[256];
        if (2!=sscanf(headerline,"%120[^: ]: %250[^\r\n]",header,value)) {
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
    dprintf("%s @%i demands %s\n",tintstr(),req->id,hash);
    // initiate transmission
    Sha1Hash root_hash = Sha1Hash(true,hash);
    int file = swift::Find(root_hash);
    if (file==-1)
        file = swift::Open(hash,root_hash);
    req->transfer = file;
    if (swift::Size(file)) {
        HttpGwFirstProgressCallback(file,bin64_t(0,0));
    } else {
        swift::AddProgressCallback(file,&HttpGwFirstProgressCallback,0);
        sckrwecb_t install (http_conn,NULL,NULL,HttpGwCloseConnection);
        swift::Datagram::Listen3rdPartySocket(install);
    }
}


// be liberal in what you do, be conservative in what you accept
void HttpGwNewConnectionCallback (SOCKET serv) {
    Address client_address;
    socklen_t len;
    SOCKET conn = accept (serv, (sockaddr*) & (client_address.addr), &len);
    if (conn==INVALID_SOCKET) {
        print_error("client conn fails");
        return;
    }
    make_socket_nonblocking(conn);
    // submit 3rd party socket to the swift loop
    sckrwecb_t install
        (conn,HttpGwNewRequestCallback,NULL,HttpGwCloseConnection);
    swift::Datagram::Listen3rdPartySocket(install);
}


void HttpGwError (SOCKET s) {
    print_error("httpgw is dead");
    dprintf("%s @0 closed http gateway\n",tintstr());
    close_socket(s);
    swift::Datagram::Listen3rdPartySocket(sckrwecb_t(s));
}


#include <signal.h>
SOCKET InstallHTTPGateway (Address bind_to) {
    SOCKET fd;
    #define gw_ensure(x) { if (!(x)) { \
    print_error("http binding fails"); close_socket(fd); \
    return INVALID_SOCKET; } }
    gw_ensure ( (fd=socket(AF_INET, SOCK_STREAM, 0)) != INVALID_SOCKET );
    int enable = true;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (setsockoptptr_t)&enable, sizeof(int));
    //setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (setsockoptptr_t)&enable, sizeof(int));
    //struct sigaction act;
    //memset(&act,0,sizeof(struct sigaction));
    //act.sa_handler = SIG_IGN;
    //sigaction (SIGPIPE, &act, NULL); // FIXME
    signal( SIGPIPE, SIG_IGN );
    gw_ensure ( 0==bind(fd, (sockaddr*)&(bind_to.addr), sizeof(struct sockaddr_in)) );
    gw_ensure (make_socket_nonblocking(fd));
    gw_ensure ( 0==listen(fd,8) );
    sckrwecb_t install_http(fd,HttpGwNewConnectionCallback,NULL,HttpGwError);
    gw_ensure (swift::Datagram::Listen3rdPartySocket(install_http));
    dprintf("%s @0 installed http gateway on %s\n",tintstr(),bind_to.str());
    return fd;
}
