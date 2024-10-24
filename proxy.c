#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "cache.h"


#define ISVALIDSOCKET(s) ((s) >=0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)
#define BUFFER_SIZE 4096
#define MAX_REQUEST_SIZE 2047
#define MAX_CONTENT_SIZE 10*1024*1024
#define MAX_PORT_LEN 200

struct server_info{
        char *hostname;
        char *port;
        char *path;
        char *full_path;
};

struct client_info {
    socklen_t address_length;
    struct sockaddr_storage address;
    SOCKET socket;
    char request[BUFFER_SIZE];
    int received;
    //char *file_requested;
    
};
struct server_info* get_server_details(struct client_info* new_client);
const char *get_client_address(struct client_info *ci) {
    static char address_buffer[100];
    getnameinfo((struct sockaddr*)&ci->address,
            ci->address_length,
            address_buffer, sizeof(address_buffer), 0, 0,
            NI_NUMERICHOST);
    return address_buffer;
}

int get_max_age(char *file_content);
char* fetch_file(char* hostname,char* port, char* path);
void add_file_age(char **file_content, int curr_age);
void drop_client(struct client_info *client) {
    
    assert(client);
    CLOSESOCKET(client->socket);

    
   
            free(client);
        

    return;
}



SOCKET create_socket(const char *port) {
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(0, port, &hints, &bind_address);

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family,
            bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen,
                bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    return socket_listen;
}



void send_400(struct client_info *client) {
    const char *c400 = "HTTP/1.1 400 Bad Request\r\n"
        "Connection: close\r\n"
        "Content-Length: 11\r\n\r\nBad Request";
    send(client->socket, c400, strlen(c400), 0);
    drop_client(client);
}

void send_404(struct client_info *client) {
    const char *c404 = "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n"
        "Content-Length: 9\r\n\r\nNot Found";
    send(client->socket, c404, strlen(c404), 0);
    drop_client(client);
}

const char *get_content_type(const char* path) {
    const char *last_dot = strrchr(path, '.');
    if (last_dot) {
        if (strcmp(last_dot, ".css") == 0) return "text/css";
        if (strcmp(last_dot, ".csv") == 0) return "text/csv";
        if (strcmp(last_dot, ".gif") == 0) return "image/gif";
        if (strcmp(last_dot, ".htm") == 0) return "text/html";
        if (strcmp(last_dot, ".html") == 0) return "text/html";
        if (strcmp(last_dot, ".ico") == 0) return "image/x-icon";
        if (strcmp(last_dot, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".jpg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".js") == 0) return "application/javascript";
        if (strcmp(last_dot, ".json") == 0) return "application/json";
        if (strcmp(last_dot, ".png") == 0) return "image/png";
        if (strcmp(last_dot, ".pdf") == 0) return "application/pdf";
        if (strcmp(last_dot, ".svg") == 0) return "image/svg+xml";
        if (strcmp(last_dot, ".txt") == 0) return "text/plain";
    }

    return "application/octet-stream";
}


void serve_resource(struct client_info *client, const char *file_content) {

        printf("serve_resource %s\n", get_client_address(client));


    size_t content_length = strlen(file_content);
     #define BSIZE 1024
//     char buffer[BSIZE];

//     // Send HTTP headers
//     sprintf(buffer, "HTTP/1.1 200 OK\r\n");
//     send(client->socket, buffer, strlen(buffer), 0);

//     sprintf(buffer, "Connection: close\r\n");
//     send(client->socket, buffer, strlen(buffer), 0);

//     sprintf(buffer, "Content-Length: %zu\r\n", content_length);  // Send content length
//     send(client->socket, buffer, strlen(buffer), 0);

// //     sprintf(buffer, "Content-Type: %s\r\n", content_type);  // Send content type
// //     send(client->socket, buffer, strlen(buffer), 0);

//     sprintf(buffer, "\r\n");  // End of headers
//     send(client->socket, buffer, strlen(buffer), 0);

    // Send the content in chunks
    size_t remaining = content_length;
    const char *p = file_content;

    while (remaining > 0) {
        size_t chunk_size = (remaining > BSIZE) ? BSIZE : remaining;
        send(client->socket, p, chunk_size, 0);
        p += chunk_size;
        remaining -= chunk_size;
    }

    drop_client(client); 

}


void send_request(SOCKET s, char *hostname, char *port, char *path) {
    char buffer[2048];

    sprintf(buffer, "GET %s HTTP/1.1\r\n", path);
    sprintf(buffer + strlen(buffer), "Host: %s:%s\r\n", hostname, port);
    sprintf(buffer + strlen(buffer), "Connection: close\r\n");
    //sprintf(buffer + strlen(buffer), "User-Agent: honpwc web_get 1.0\r\n");
    sprintf(buffer + strlen(buffer), "\r\n");

    send(s, buffer, strlen(buffer), 0);
    printf("Sent Headers:\n%s", buffer);
}


SOCKET connect_to_host(char *hostname, char *port) {


    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;
    struct addrinfo *peer_address;
    if (getaddrinfo(hostname, port, &hints, &peer_address)) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    fprintf(stderr,"Configuring remote address for host: %s, port: %s\n", hostname, port);  
    
    // Use strncmp to compare the port with "9049"
//     if (strncmp(port, "9049", 4) == 0) {
//         printf("Port is correct: %s\n", port);
//     } else {
//         printf("Port is incorrect: %s (expected 9049)\n", port);
//     }

    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];
    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
            address_buffer, sizeof(address_buffer),
            service_buffer, sizeof(service_buffer),
            NI_NUMERICHOST);
    printf("%s %s\n", address_buffer, service_buffer);

    printf("Creating socket...\n");
    SOCKET server;
    server = socket(peer_address->ai_family,
            peer_address->ai_socktype, peer_address->ai_protocol);
    if (!ISVALIDSOCKET(server)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    printf("Connecting...\n");
    if (connect(server,
                peer_address->ai_addr, peer_address->ai_addrlen)) {
        fprintf(stderr, "connect() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    freeaddrinfo(peer_address);

    printf("Connected.\n\n");

    return server;
}









int main(int argc,char *argv[]){

        if (argc != 2) {
        fprintf(stderr, "Usage: %s <port_number>\n", argv[0]);
        return 1;
    }

    const char *port_no = argv[1];

    //Create proxy server socket

    SOCKET proxy_server = create_socket(port_no);
    Cache ch = Cache_new(10);


    while(1){
    printf("Waiting for connection...\n");

    struct client_info *new_client = calloc(1,sizeof(*new_client));

    assert(new_client!=NULL);

     new_client->socket = accept(proxy_server,
                    (struct sockaddr*) &(new_client->address),
                    &(new_client->address_length));
   
     if(!ISVALIDSOCKET(new_client->socket)){

         fprintf(stderr,"accept() failed.(%d)\n",GETSOCKETERRNO());
                return 1;
     }

       printf("New connection .\n");
                    
                    //get_client_address(new_client)



         int r = recv(new_client->socket,
                        new_client->request + new_client->received,
                        MAX_REQUEST_SIZE - new_client->received, 0);
        fprintf(stderr,"Received request successfully\n");
                if (r < 1) {
                    printf("Unexpected disconnect from %s.\n",
                            get_client_address(new_client));
                    drop_client(new_client);

                } else {
                    new_client->received += r;
                     fprintf(stderr,"HERE 1\n");
                    new_client->request[new_client->received] = 0;
                     fprintf(stderr,"HERE 2\n");
                     assert(new_client);
                     fprintf(stderr,"New client exits\n");
                     fprintf(stderr,"Request is \n");
                     fprintf(stderr,"%s",new_client->request);
                    struct server_info* server = get_server_details(new_client);
                     fprintf(stderr,"HERE 3\n");
                     if(server==NULL){
                        fprintf(stderr,"NULL SERVER\n");
                     }
                     fprintf(stderr,"Hostname is %s\n",server->hostname);
                      fprintf(stderr,"Port is %s\n",server->port);
                       fprintf(stderr,"Path is %s\n",server->path);
                    char *file_requested = server->full_path;
                    for (char *p = file_requested; *p; ++p) {
                                if (*p == ':' || *p == '/') {
                                                   *p = '_';}
                    }
                                                                        
                                                                                        
                    fprintf(stderr,"File requested is %s\n",file_requested);
                    if(exists_in_cache(ch,file_requested) && !isStale(ch,file_requested)){
                        
                         fprintf(stderr,"debug1\n");
                        char *file_content = return_file_content(ch,file_requested);
                         fprintf(stderr,"debug2\n");
                        int curr_age = get_file_age(ch,file_requested);
                         fprintf(stderr,"debug3\n");
                        add_file_age(&file_content,curr_age);
                         fprintf(stderr,"debug4\n");
                        serve_resource(new_client,file_content);
                         fprintf(stderr,"debug5\n");
                         // update state in cache
                         update_LRU(ch,file_requested);
                    }
                    else{
                    char *file_content = fetch_file(server->hostname,server->port,server->path);
                    FILE *file = fopen(file_requested,"w");
                    assert(file);
                    if(file){
                        fprintf(file,"%s",file_content);
                        fclose(file);
                    }
                    int a = get_max_age(file_content);
                    int age;
                    if(a>=0){
                                age = a;

                    }
                    else{
                        age = 3600;
                    }
                    Cache_put(ch,file_requested,age); // either stale-(should be handle by case?) or never in cache-(handled by cache put vacant)
                     fprintf(stderr,"HERE 4\n");
                    serve_resource(new_client,file_content);
                     fprintf(stderr,"HERE 5\n");
                     
                     }

              
                }
                

    }

                return 0;



}

struct server_info* get_server_details(struct client_info* new_client) {
    fprintf(stderr, "Here10\n");

    char *q = strstr(new_client->request, "\r\n\r\n");
    if (q) {
        *q = 0;

        if (strncmp("GET ", new_client->request, 4)) {
            send_400(new_client);
            return NULL;
        }

        char *hostname = NULL;
        char *port = NULL;
        char *host_header = strstr(new_client->request, "Host: ");
        char *path = NULL;

        if (host_header) {
            host_header += 6;  //"Host: "
            char *end_host = strstr(host_header, "\r\n");
            if (end_host) *end_host = 0;

            char *colon = strstr(host_header, ":");
            if (colon) {
                *colon = 0;
                port = malloc(MAX_PORT_LEN);
                assert(port);
                strncpy(port, colon + 1, MAX_PORT_LEN - 1);
                port[MAX_PORT_LEN - 1] = '\0';
            } else {
                port = strdup("80");  // Deflt 80
            }

            hostname = strdup(host_header);
        } else {
            send_400(new_client);
            return NULL;
        }

        // Extract URL
        char *url_start = strstr(new_client->request, "http://");
        if (url_start) {
            path = strchr(url_start + 7, '/');
            if (!path) {
                send_400(new_client);
                return NULL;
            }

            char *end_path = strstr(path, " ");
            if (end_path) {
                *end_path = 0;
            } else {
                send_400(new_client);
                return NULL;
            }
        }

        struct server_info* server_details = malloc(sizeof(struct server_info));
        assert(server_details);
        server_details->hostname = hostname;
        fprintf(stderr,"The hostname is %s",server_details->hostname);
        server_details->path = path;
        fprintf(stderr,"The path is %s",server_details->path);
        server_details->port = port;
          fprintf(stderr,"The port is %s",server_details->port);

        int combined_len = strlen(hostname) + strlen(path) + strlen(port) + 10;
        char *full_path = malloc(combined_len);
        snprintf(full_path, combined_len, "%s:%s/%s", hostname, port, path);
        server_details->full_path = full_path;

        return server_details;
    }




    return NULL;
}


char* fetch_file(char* hostname, char* port, char* path) {
    fprintf(stderr, "h1\n");

    SOCKET server = connect_to_host(hostname, port);
    fprintf(stderr, "h2\n");

    send_request(server, hostname, port, path);
    fprintf(stderr, "h3\n");

    char *response = malloc(MAX_CONTENT_SIZE);
    if (!response) {
        fprintf(stderr, "Failed to allocate memory\n");
        return NULL;
    }

    char *p = response;
    int bytes_received;
    int remaining = MAX_CONTENT_SIZE;
    int total_received = 0;

    
    while ((bytes_received = recv(server, p, remaining, 0)) > 0) {
        total_received += bytes_received;
        p += bytes_received;
        remaining -= bytes_received;
    }

    if (bytes_received < 0) {
        fprintf(stderr, "Error receiving response\n");
        free(response);
        return NULL;
    }

    response[total_received] = '\0';  
    fprintf(stderr, "h5\n");

    CLOSESOCKET(server);
    fprintf(stderr, "h6\n");

    return response;
}


int get_max_age(char *file_content){

         char *cache_control = strstr(file_content, "Cache-Control: ");
    if (cache_control) {
        
        char *max_age = strstr(cache_control, "max-age=");
        if (max_age) {
            
            max_age += 8;
           
            int age = strtol(max_age, NULL, 10);
            return age;
        }
    }
    return -1;        

}

void add_file_age(char **file_content, int curr_age) {
    
    char *header_end = strstr(*file_content, "\r\n");
    if (header_end) {
        
        char age_header[50];  // Enough space for "Age: N\r\n"
        snprintf(age_header, sizeof(age_header), "Age: %d\r\n", curr_age);

        
        size_t new_content_len = strlen(*file_content) + strlen(age_header) + 1;

        
        char *new_content = realloc(*file_content, new_content_len);
        if (new_content) {
           
            memmove(new_content + (header_end - *file_content) + 2 + strlen(age_header), 
                    header_end + 2, 
                    strlen(header_end + 2) + 1);  

           
            memcpy(new_content + (header_end - *file_content) + 2, age_header, strlen(age_header));

           
            *file_content = new_content;
        }
    }
}

