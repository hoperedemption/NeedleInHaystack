#include "socket_layer.h"
#include "util.h"

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

/**
 * @brief This function handles an error if something bad happens during the execution of the TCP protocol. It closes the socket and displays an error.
 * @param error_message (const char*) : the error message to display
 * @param socket_fd (int) : the socket to be closed
 * @param error_code (int) : the error code to return
 * @return (int) : it returns the error code passed.
*/
int error_handler_for_tcp(const char* error_message, int socket_fd, int error_code)
{
    if(socket_fd >= 0) close(socket_fd);
    perror(error_message);
    return error_code;
}

/**
 * @brief This function initialises a server with a given port. For that it initilaises its listening socket and binds it to the process running the server. Then,
 * the socket is ready to listen. 
 * @param port (uint16_t) : the port on which the listenning socket needs to live
 * @return (int) : returns ERR_IO if an error occurs during the initialisation of the server. Otherwise, if everything went fine, it returns the file descriptor 
 * of the listenning socket. 
*/
int tcp_server_init(uint16_t port)
{
    int socket_file_descriptor = 0;
    if((socket_file_descriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return error_handler_for_tcp("Error: could not initialize the socket\n", socket_file_descriptor, ERR_IO);
    int opt = 1;
    if(setsockopt(socket_file_descriptor, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(int)) == -1)
        return error_handler_for_tcp("Error: could not initialize the socket\n", socket_file_descriptor, ERR_IO);

    struct sockaddr_in socket_address;
    zero_init_var(socket_address);
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(port);
    socket_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if(bind(socket_file_descriptor, (const struct sockaddr*) &socket_address, sizeof(socket_address)) == -1)
        return error_handler_for_tcp("Error: could not bind the socket\n", socket_file_descriptor, ERR_IO);
    if(listen(socket_file_descriptor, 32) == -1)
        return error_handler_for_tcp("Error: in listening to new connections\n", socket_file_descriptor, ERR_IO);

    return socket_file_descriptor;
}

int tcp_accept(int passive_socket)
{
    return accept(passive_socket, NULL, NULL);
}

ssize_t tcp_read(int active_socket, char* buf, size_t buflen)
{
    M_REQUIRE_NON_NULL(buf);
    if(active_socket < 0) return ERR_INVALID_ARGUMENT;

    return recv(active_socket, buf, buflen, 0);
}

/**
 * @brief A wrap-up funtion for the send (2) function of C, that takes care of sending over TCP.
*/
ssize_t tcp_send(int active_socket, const char* response, size_t response_len)
{
    M_REQUIRE_NON_NULL(response);
    if(active_socket < 0) return ERR_INVALID_ARGUMENT;

    return send(active_socket, response, response_len, 0);
}
