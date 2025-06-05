/**
 * @file imgfs_server_service.h
 * @brief Service layer to connect imgfs_server and imgFS backend.
 *
 * @author Konstantinos Prasopoulos
 */

#pragma once

#include "http_prot.h"

#define BASE_FILE "index.html"
#define DEFAULT_LISTENING_PORT 8000

int server_startup (int argc, char** argv);

void server_shutdown (void);

int handle_http_message(struct http_message* msg, int connection);

static int reply_error_msg(int connection, int error);

static int reply_302_msg(int connection);

int handle_http_message(struct http_message* msg, int connection);

int handle_list_call(int connection);

int handle_read_call(struct http_string* uri, int connection);

int handle_delete_call(struct http_message* msg, int connection);

int handle_insert_call(struct http_message* msg, int connection);