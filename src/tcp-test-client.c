#include "socket_layer.h"
#include "util.h"

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define WHERE_ERROR "Error in tcp client:"
#define ANY_PORT 0
#define SMALL_RESPONSE "Small length"
#define LONG_RESPONSE "Long length "
#define DELIMITER "|\r"
#define END_OF_FILE_DELIMITER "\r\n"

int error_handler_for_client(const char* error_message, FILE* file, int socket_file_descriptor, int error_code)
{
    if(file != NULL) fclose(file);
    if(socket_file_descriptor >= 0) close(socket_file_descriptor);
    if(error_message) perror(error_message);
    return error_code;
}

int wait_for_tcp(int waiting_socket, size_t bytes_expected, char* buffer_to_store)
{
    M_REQUIRE_NON_NULL(buffer_to_store);

    char* buffer = NULL;
    char* output = NULL;

    if((buffer = calloc(bytes_expected, sizeof(char))) == NULL) return ERR_OUT_OF_MEMORY;
    if((output = calloc(bytes_expected, sizeof(char))) == NULL) return ERR_OUT_OF_MEMORY;

    char* iter = output;
    int n = bytes_expected;

    while(n > 0) {
        memset(buffer, 0, bytes_expected);

        int bytes_received = tcp_read(waiting_socket, buffer, n);
        if(bytes_received == -1) {
            free(buffer);
            free(output);
            return ERR_IO;
        }

        strncpy(iter, buffer, bytes_received);
        iter += bytes_received;

        n -= bytes_received;
    }

    strncpy(buffer_to_store, output, bytes_expected);
    free(buffer);
    free(output);
    return ERR_NONE;
}

int main(int argc, char** argv)
{
    argc--;
    argv++;
    // skip the first argument

    if(argc < 2) return error_handler_for_client(WHERE_ERROR " not enough arguments\n", NULL, -1,  ERR_NOT_ENOUGH_ARGUMENTS);
    if(argc > 2) return error_handler_for_client(WHERE_ERROR " invalid command\n", NULL, -1, ERR_INVALID_COMMAND);

    M_REQUIRE_NON_NULL(argv);
    M_REQUIRE_NON_NULL(argv[0]);
    M_REQUIRE_NON_NULL(argv[1]);

    int ret = 0;

    uint16_t port = atouint16(*(argv++));
    char filename[FILENAME_MAX + 1] = {0};
    strncpy(filename, *(argv), FILENAME_MAX + 1);

    FILE* file = NULL;
    if((file = fopen(filename, "rb")) == NULL
       || fseek(file, 0, SEEK_END) == -1)
        return error_handler_for_client("Error in tcp client: ERR_IO\n", file, -1, ERR_IO);

    long size = ftell(file);
    fclose(file);
    file = NULL;

    if(size > 2048) return error_handler_for_client(WHERE_ERROR " size too big\n",file, -1, ERR_INVALID_ARGUMENT);

    //socket init for client
    int socket_file_descriptor = -1;
    if((socket_file_descriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return error_handler_for_client(WHERE_ERROR " Error: could not initialize the socket\n", file, socket_file_descriptor, ERR_IO);

    struct sockaddr_in socket_address_client;
    zero_init_var(socket_address_client);
    socket_address_client.sin_family = AF_INET;
    socket_address_client.sin_port = htons(ANY_PORT); //what port to include ? If the client sends a request, the server does not need to know
    // the port before.
    socket_address_client.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    struct sockaddr_in socket_address_server;
    zero_init_var(socket_address_server);
    socket_address_server.sin_family = AF_INET;
    socket_address_server.sin_port = htons(port);
    socket_address_server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if(bind(socket_file_descriptor, (const struct sockaddr*) &socket_address_client, sizeof(socket_address_client)) == -1)
        return error_handler_for_client(WHERE_ERROR " Error: could not bind the socket\n", file, socket_file_descriptor, ERR_IO);

    if(connect(socket_file_descriptor, (const struct sockaddr *) &socket_address_server, sizeof(socket_address_server)) == -1)
        return error_handler_for_client(WHERE_ERROR " Error: could not connect the socket\n", file, socket_file_descriptor, ERR_IO);
    else printf("Talking to %d\n", port);

    char size_str_buffer[13] = {0};
    snprintf(size_str_buffer, 13, "%d%s", size, DELIMITER);
    printf("Sending size %d\n", size);
    if(tcp_send(socket_file_descriptor, (char*) size_str_buffer, sizeof(size_str_buffer)) == -1)
        return error_handler_for_client(WHERE_ERROR " Error: could not send the contents of the size buffer\n", file, socket_file_descriptor, ERR_IO);

    char first_buffer[13] = {0};
    if((ret = wait_for_tcp(socket_file_descriptor, 13, first_buffer)) != ERR_NONE)
        return error_handler_for_client(WHERE_ERROR " Error: TCP wait error -- Small length not received\n", file, socket_file_descriptor, ret);

    if(!strncmp(first_buffer, SMALL_RESPONSE, 13)) printf("Server responded: %s\n", SMALL_RESPONSE);
    else if(!strncmp(first_buffer, LONG_RESPONSE, 13)) {
        printf("Server responded: %s\n", LONG_RESPONSE);
        close(socket_file_descriptor);
        return ERR_NONE; // idk if ERR_NONE because file too big, could choose something else tbh
    } else return error_handler_for_client(WHERE_ERROR "Error: server response not coherent!\n", file, socket_file_descriptor, ERR_IO);

    char filename_buffer[FILENAME_MAX+3] = {0};
    snprintf(filename_buffer, FILENAME_MAX+3, "%s%s", filename, END_OF_FILE_DELIMITER);
    printf("Sending %s\n", filename);
    if(tcp_send(socket_file_descriptor, filename_buffer, FILENAME_MAX + 2) == -1)
        return error_handler_for_client(WHERE_ERROR " Error: could not connect the socket -- filename not sent\n", file, socket_file_descriptor, ERR_IO);

    const char* server_ack = "Filename received";
    char second_buffer[18] = {0};
    if((ret = wait_for_tcp(socket_file_descriptor, 18, second_buffer)) != ERR_NONE)
        return error_handler_for_client(WHERE_ERROR " Error: TCP wait error --filename ack not received\n", file, socket_file_descriptor, ret);

    if(strncmp(second_buffer, server_ack, 18))
        return error_handler_for_client(WHERE_ERROR "Error: server did not ack filename!\n", file, socket_file_descriptor, ERR_IO);
    else printf("Accepted\n");

    close(socket_file_descriptor);

    printf("Done\n");

    return ERR_NONE;
}