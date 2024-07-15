#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"


wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

#define MAX_SIZE 8192


void prepare_http_response(int fd, char *path, struct stat *st);

void send_file_content(int fd, char *path);

void handle_file_open_error();

void handle_memory_allocation_error(int file);

void serve_file(int fd, char *path, struct stat *st) {
    prepare_http_response(fd, path, st);
    send_file_content(fd, path);
}

void prepare_http_response(int fd, char *path, struct stat *st) {
    long size = st->st_size;
    char *content_size = malloc(MAX_SIZE * sizeof(char));
    if (content_size == NULL) {
        handle_memory_allocation_error(-1); // Passing -1 as no file is open yet
        return;
    }
    sprintf(content_size, "%ld", size);

    http_start_response(fd, 200);
    http_send_header(fd, "Content-Type", http_get_mime_type(path));
    http_send_header(fd, "Content-Length", content_size);
    http_end_headers(fd);

    free(content_size);
}

void send_file_content(int fd, char *path) {
    int file = open(path, O_RDONLY);
    if (file == -1) {
        handle_file_open_error();
        return;
    }

    void *buffer = malloc(MAX_SIZE);
    if (buffer == NULL) {
        handle_memory_allocation_error(file);
        return;
    }

    size_t read_size;
    while ((read_size = read(file, buffer, MAX_SIZE)) > 0) {
        http_send_data(fd, buffer, read_size);
    }

    close(file);
    free(buffer);
}

void handle_file_open_error() {
    // Implement error handling for file open failure
}

void handle_memory_allocation_error(int file) {
    if (file != -1) {
        close(file);
    }
    // Implement error handling for memory allocation failure
}


void send_http_response_headers(int fd);

void list_directory_contents(int fd, char *path);

void handle_directory_error(int fd);

void serve_directory(int fd, char *path) {
    send_http_response_headers(fd);
    list_directory_contents(fd, path);
}

void send_http_response_headers(int fd) {
    http_start_response(fd, 200);
    http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
    http_end_headers(fd);
}

void list_directory_contents(int fd, char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        handle_directory_error(fd);
        return;
    }

    char *string = malloc(MAX_SIZE);
    if (string == NULL) {
        closedir(dir);
        // You can also add error handling for memory allocation failure if needed
        return;
    }

    struct dirent *dirent;
    while ((dirent = readdir(dir)) != NULL) {
        snprintf(string, MAX_SIZE, "<a href='./%s'>%s</a><br>\n", dirent->d_name, dirent->d_name);
        http_send_string(fd, string);
    }

    free(string);
    closedir(dir);
}

void handle_directory_error(int fd) {
    // Implement error handling for directory open failure
    // For example, send a 404 or 500 HTTP response
}


int validate_request(struct http_request *request, int fd);

char *construct_full_path(struct http_request *request);

void handle_regular_file(int fd, char *path);

void handle_directory_request(int fd, char *path);

void send_http_error_response(int fd, int status_code);

void handle_files_request(int fd) {
    struct http_request *request = http_request_parse(fd);
    if (!validate_request(request, fd)) return;

    char *path = construct_full_path(request);
    struct stat file_stat;
    if (stat(path, &file_stat) == -1) {
        send_http_error_response(fd, 404);
        free(path);
        return;
    }

    if (S_ISREG(file_stat.st_mode)) {
        handle_regular_file(fd, path);
    } else if (S_ISDIR(file_stat.st_mode)) {
        handle_directory_request(fd, path);
    } else {
        send_http_error_response(fd, 404);
    }

    free(path);
    close(fd);
}

int validate_request(struct http_request *request, int fd) {
    if (request == NULL || request->path[0] != '/' || strstr(request->path, "..") != NULL) {
        send_http_error_response(fd, request == NULL || request->path[0] != '/' ? 400 : 403);
        return 0;
    }
    return 1;
}

char *construct_full_path(struct http_request *request) {
    char *path = malloc(strlen(server_files_directory) + strlen(request->path) + 1); // +1 for null terminator
    strcpy(path, server_files_directory);
    strcat(path, request->path);
    return path;
}

void handle_regular_file(int fd, char *path) {
    struct stat file_stat;
    if (stat(path, &file_stat) == 0) {
        serve_file(fd, path, &file_stat);
    }
}

void handle_directory_request(int fd, char *path) {
    struct stat file_stat;
    char *index_path = malloc(strlen(path) + strlen("/index.html") + 1); // +1 for null terminator
    strcpy(index_path, path);
    strcat(index_path, "/index.html");

    if (stat(index_path, &file_stat) == 0) {
        serve_file(fd, index_path, &file_stat);
    } else {
        serve_directory(fd, path);
    }

    free(index_path);
}

void send_http_error_response(int fd, int status_code) {
    http_start_response(fd, status_code);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
}


typedef struct proxy_status {
    int src_socket;
    int dst_socket;
    pthread_cond_t *cond;
    int alive;
} proxy_status;


struct proxy_status *create_proxy_status(int src, int dst, int alive, pthread_cond_t *cond) {
    proxy_status *proxy_status = (struct proxy_status *) malloc(sizeof(proxy_status));
    proxy_status->src_socket = src;
    proxy_status->dst_socket = dst;
    proxy_status->alive = alive;
    proxy_status->cond = cond;

    return proxy_status;
}

void send_to_client(int dst, int src) {
    void *buffer = malloc(MAX_SIZE);
    size_t size;
    while ((size = read(src, buffer, MAX_SIZE)) > 0)
        http_send_data(dst, buffer, size);

    free(buffer);
}

void *run_proxy(void *args) {
    proxy_status *pstatus = (proxy_status *) args;
    send_to_client(pstatus->dst_socket, pstatus->src_socket);
    pstatus->alive = 0;
    pthread_cond_signal(pstatus->cond);
    return NULL;
}

/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */


proxy_status *create_proxy_status(int src_fd, int dst_fd, int alive, pthread_cond_t *cond);

// Forward declarations for our new helper functions
int setup_connection(struct sockaddr_in *target_address, int *target_fd);

void handle_connection_error(int fd, int target_fd);

void setup_proxy_threads(int fd, int target_fd);

void handle_proxy_request(int fd) {
    struct sockaddr_in target_address;
    int target_fd;
    if (setup_connection(&target_address, &target_fd) != 0) {
        handle_connection_error(fd, target_fd);
        return;
    }

    setup_proxy_threads(fd, target_fd);
}

int setup_connection(struct sockaddr_in *target_address, int *target_fd) {
    memset(target_address, 0, sizeof(*target_address));
    target_address->sin_family = AF_INET;
    target_address->sin_port = htons(server_proxy_port);

    struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

    *target_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (*target_fd == -1) {
        fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
        return -1;
    }

    if (target_dns_entry == NULL) {
        fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
        return -2;
    }

    char *dns_address = target_dns_entry->h_addr_list[0];
    memcpy(&target_address->sin_addr, dns_address, sizeof(target_address->sin_addr));

    if (connect(*target_fd, (struct sockaddr *) target_address, sizeof(*target_address)) < 0) {
        return -3;
    }

    return 0; // Success
}

void handle_connection_error(int fd, int target_fd) {
    http_request_parse(fd);
    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
    if (target_fd != -1) {
        close(target_fd);
    }
    close(fd);
}

void setup_proxy_threads(int fd, int target_fd) {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    int err;

    proxy_status *request_status = create_proxy_status(fd, target_fd, 1, &cond);
    proxy_status *response_status = create_proxy_status(target_fd, fd, 1, &cond);

    if (!request_status || !response_status) {
        close(target_fd);
        close(fd);
        exit(ENOMEM);
    }

    pthread_t request_thread, response_thread;
    pthread_mutex_lock(&mutex);
    err = pthread_create(&request_thread, NULL, run_proxy, request_status);
    if (err != 0) {
        close(target_fd);
        close(fd);
        exit(err);
    }

    err = pthread_create(&response_thread, NULL, run_proxy, response_status);
    if (err != 0) {
        pthread_cancel(request_thread);
        close(target_fd);
        close(fd);
        exit(err);
    }

    while (request_status->alive && response_status->alive) {
        pthread_cond_wait(&cond, &mutex);
    }

    pthread_mutex_unlock(&mutex);
    pthread_join(request_thread, NULL);
    pthread_join(response_thread, NULL);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    free(request_status);
    free(response_status);

    close(target_fd);
    close(fd);
}


//KOOOOOOOOOOOOOSE

void *th_handle(void *args) {
    void (*func)(int) = args;
    while (1) {
        int fd = wq_pop(&work_queue);
        func(fd);
        close(fd);
    }
}


/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
int setup_server_socket(int *socket_number);

void handle_new_connection(int server_socket, void (*request_handler)(int));

void init_thread_pool(void (*request_handler)(int));

void serve_forever(int *socket_number, void (*request_handler)(int)) {
    *socket_number = setup_server_socket(socket_number);
    if (*socket_number == -1) return;

    wq_init(&work_queue);
    init_thread_pool(request_handler);

    while (1) {
        handle_new_connection(*socket_number, request_handler);
    }

    shutdown(*socket_number, SHUT_RDWR);
    close(*socket_number);
}

int setup_server_socket(int *socket_number) {
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(server_port);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Failed to create a new socket");
        return -1;
    }

    int socket_option = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &socket_option, sizeof(socket_option)) == -1) {
        perror("Failed to set socket options");
        close(sock);
        return -1;
    }

    if (bind(sock, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        perror("Failed to bind on socket");
        close(sock);
        return -1;
    }

    if (listen(sock, 1024) == -1) {
        perror("Failed to listen on socket");
        close(sock);
        return -1;
    }

    printf("Listening on port %d...\n", server_port);
    return sock;
}

void handle_new_connection(int server_socket, void (*request_handler)(int)) {
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);
    int client_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_address_length);

    if (client_socket < 0) {
        perror("Error accepting socket");
        return;
    }

    printf("Accepted connection from %s on port %d\n", inet_ntoa(client_address.sin_addr),
           ntohs(client_address.sin_port));

    if (num_threads != 0) {
        wq_push(&work_queue, client_socket);
    } else {
        request_handler(client_socket);
        close(client_socket);
    }
}

void init_thread_pool(void (*request_handler)(int)) {
    pthread_t pthread[num_threads];
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pthread[i], NULL, th_handle, (void *) request_handler);
    }
}

int server_fd;

void signal_callback_handler(int signum) {
    printf("Caught signal %d: %s\n", signum, strsignal(signum));
    printf("Closing socket %d\n", server_fd);
    if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
    exit(0);
}

char *USAGE =
        "Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
        "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_callback_handler);
    signal(SIGPIPE, SIG_IGN);

    /* Default settings */
    server_port = 8000;
    void (*request_handler)(int) = NULL;

    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp("--files", argv[i]) == 0) {
            request_handler = handle_files_request;
            free(server_files_directory);
            server_files_directory = argv[++i];
            if (!server_files_directory) {
                fprintf(stderr, "Expected argument after --files\n");
                exit_with_usage();
            }
        } else if (strcmp("--proxy", argv[i]) == 0) {
            request_handler = handle_proxy_request;

            char *proxy_target = argv[++i];
            if (!proxy_target) {
                fprintf(stderr, "Expected argument after --proxy\n");
                exit_with_usage();
            }

            char *colon_pointer = strchr(proxy_target, ':');
            if (colon_pointer != NULL) {
                *colon_pointer = '\0';
                server_proxy_hostname = proxy_target;
                server_proxy_port = atoi(colon_pointer + 1);
            } else {
                server_proxy_hostname = proxy_target;
                server_proxy_port = 80;
            }
        } else if (strcmp("--port", argv[i]) == 0) {
            char *server_port_string = argv[++i];
            if (!server_port_string) {
                fprintf(stderr, "Expected argument after --port\n");
                exit_with_usage();
            }
            server_port = atoi(server_port_string);
        } else if (strcmp("--num-threads", argv[i]) == 0) {
            char *num_threads_str = argv[++i];
            if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
                fprintf(stderr, "Expected positive integer after --num-threads\n");
                exit_with_usage();
            }
        } else if (strcmp("--help", argv[i]) == 0) {
            exit_with_usage();
        } else {
            fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
            exit_with_usage();
        }
    }

    if (server_files_directory == NULL && server_proxy_hostname == NULL) {
        fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                        "                      \"--proxy [HOSTNAME:PORT]\"\n");
        exit_with_usage();
    }

    serve_forever(&server_fd, request_handler);

    return EXIT_SUCCESS;
}
