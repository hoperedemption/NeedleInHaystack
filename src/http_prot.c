#include "http_prot.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>

/**
* @brief This function tells us if the string haystack is prefixed by the strign needle.
* @param haystack (const char*) : the string to be tested
* @param neddle (const char*) : the prefix to be used
* @param needle_len (int) : the length of the needle
* @param haystack_len (int) : the length of haystack
* @return int : returns 0 if haystack is not prefixed with needle and 1 otherwise
*/
int match_prefix(const char* haystack, const char* needle, int needle_len, int haystack_len)
{
    M_REQUIRE_NON_NULL(haystack);
    M_REQUIRE_NON_NULL(needle);

    if(needle_len > haystack_len) return 0;
    return !strncmp(haystack, needle, needle_len);
}

int http_match_uri(const struct http_message *message, const char *target_uri)
{
    M_REQUIRE_NON_NULL(message);
    M_REQUIRE_NON_NULL(target_uri);
    M_REQUIRE_NON_NULL(message->uri.val);

    return match_prefix(message->uri.val, target_uri, strlen(target_uri), message->uri.len);
}

int http_match_verb(const struct http_string* method, const char* verb)
{
    M_REQUIRE_NON_NULL(method);
    M_REQUIRE_NON_NULL(verb);
    M_REQUIRE_NON_NULL(method->val);

    return !strncmp(method->val, verb, method->len) && verb[method->len] == '\0';
}

/**
* @brief This function is a garbage collector for our other functions.
* Nothing fancy, just frees the associated memory to the pointer, before returning
* the passed error code. Allows to avoid memory leaks.
 * @param return_value (int) : the error code the be returned
 * @param ptr1 (char*) : a pointer to free
 * @return (int) : returns the error code passed to this function
*/
int garbage_collector(int return_value, char* ptr1)
{
    if(ptr1) free(ptr1);
    return return_value;
}


/**
* @brief This struct returns the position of the next token in a sequence of string tokens
* separated by a delimiter, as well as the length of the current token
 * @param next_pos (const char*) : the position of the next token
 * @param len_return (int) : the length of the current token
*/
struct return_tokenize {
    const char* next_pos;
    int len_return;
};

/**
 * @brief This function is an adapted version of the strtok function of the original C library,
 * inspired by the case study we did in class. The function allows us to iterate over a stream of
 * characters haystack and separate the stream into tokens separated by a delimiter.
 * The function iterates over a char stream called "haystack" and stops as soon as it finds the sequence "needle" in the stream.
 * At this point it knows what the current token, as well as its length and can also calculate the position
 * of the next token in the sequence by leveraging the length of the delimiter.
 * The difference with the C strtok function, is that since we use http strings that are not by invariant
 * null terminated, original strtok might break the flow at a 0 byte (resulting in undefined behaviour), contrary
 * to our function.
 * Since this is a helper function, it has an implicit invariant: needles aren't empty!
  * @param haystack (const struct http_string*) : the complete string in which we are trying to find the next token and the value of the current one
 * @param needle (const char*) : the delimiter by which the tokens are separated
 * @param output (char**) : a pointer to the string containning the value of the token foudn at the end of the function.
 * @return (struct return_tokenize) : returns a struct containing the position of the next token (if any) and the length of the current one.
*/
struct return_tokenize strtokadapted(const struct http_string* haystack, const char* needle, char** output)
{
    *output = haystack->val;

    int len = 0;
    char const* iter = NULL;
    for(iter = (char const*) haystack->val, len = 0; len < haystack->len; ++iter, ++len) {
        char const* haystack_iter = iter;
        char const* needle_iter = needle;

        while(*needle_iter != '\0' && *haystack_iter == *needle_iter) {
            haystack_iter++;
            needle_iter++;
        }

        if(*needle_iter == '\0') break;
    }

    size_t token_len = len;
    char* next_pos = iter + strlen(needle);

    struct return_tokenize ret = {.next_pos = next_pos, .len_return = token_len};
    return ret;
}


int http_get_var(const struct http_string* url, const char* name, char* out, size_t out_len)
{
    M_REQUIRE_NON_NULL(url);
    M_REQUIRE_NON_NULL(name);
    M_REQUIRE_NON_NULL(out);

    size_t len_name = strlen(name);
    char* needle = NULL;
    if((needle = calloc(len_name + 2, sizeof(char))) == NULL) return garbage_collector(ERR_OUT_OF_MEMORY, NULL);
    strncpy(needle, name, len_name);
    needle[len_name] = '=';

    char* token = NULL;

    struct http_string iter_string = *url;

    struct return_tokenize beginning_of_parameters = {0};
    beginning_of_parameters = strtokadapted(&iter_string, "?", &token);
    if(beginning_of_parameters.len_return == url->len) return garbage_collector(ERR_INVALID_ARGUMENT, needle);

    iter_string.val = beginning_of_parameters.next_pos;
    iter_string.len = iter_string.len - beginning_of_parameters.len_return - 1;

    while(iter_string.len > 0) {
        beginning_of_parameters = strtokadapted(&iter_string, "&", &token);
        if(!memcmp(token, needle, len_name + 1)) {
            token += len_name + 1;
            size_t length_of_parameter = beginning_of_parameters.len_return - len_name - 1;
            if(length_of_parameter == 0 || length_of_parameter >= out_len) return garbage_collector(ERR_RUNTIME, needle);
            memcpy(out, token, length_of_parameter);
            return garbage_collector(length_of_parameter, needle);
        }
        iter_string.val = beginning_of_parameters.next_pos;
        long len_remaining = iter_string.len - beginning_of_parameters.len_return - 1;
        iter_string.len = MAX(0, len_remaining);
    }

    return garbage_collector(0, needle);
}

/**
 * @brief Function similar in essence to strtokadapted, with the only major difference being the outputs and
 * the handling of edge cases, like an empty needle.
 * This function finds the next token in a stream of character given by message.
  * @param message (const char*) : the message stream of chars in which we find the current token to
  * be converted to http string and the position of the next one
 * @param delimiter (const char*) : the delimiter by which the tokens are separated
 * @param output (struct http_string*) : a pointer to the http_string structure containing the previous token
 * @return (static const char*) the position of the next token in the stream, after the delimiter
*/
static const char* get_next_token(const char* message, const char* delimiter, struct http_string* output)
{
    if(message == NULL || delimiter == NULL) return NULL;

    const char const* needle = (const char const*) delimiter;

    if(*needle == '\0') {
        size_t len_message = strlen(message);
        if(output != NULL) {
            output->val = message;
            output->len = len_message;
        }
        return message + len_message;
    }

    int len;
    char const* iter;
    for(iter = (char const*) message, len = 0; *iter != '\0'; ++iter, ++len) {
        char const* haystack_iter = iter;
        char const* needle_iter = needle;

        while(*needle_iter != '\0' && *haystack_iter == *needle_iter) {
            haystack_iter++;
            needle_iter++;
        }

        if(*needle_iter == '\0') break;
    }

    if(output != NULL) {
        output->val = message;
        output->len = len;
    }
    return iter + strlen(delimiter);
}
/**
 * @brief this function parses the header according to a conventiaonal delimiter, and allows us to determine the different headers composing it
 * and their values (for example the header content-length associated to 64). All of this data is then copied in output. Moreover,
 * this function returns a poiter to the beginning of the maybe present body (which is normally located at length of final delimiter + 2).
 * If the body does not exists it simply returns NULL.
 * @param header_start (const char*) : the header to parse
 * @param output (struct http_message*) : the struct to fill
 * @return (char*) : it returns the position of the body, if present. Otherwise it returns NULL.
*/
static const char* http_parse_headers(const char* header_start, struct http_message* output)
{
    _Static_assert(strcmp(HTTP_HDR_END_DELIM, HTTP_LINE_DELIM HTTP_LINE_DELIM) == 0, "HTTP_HDR_END_DELIM is not twice HTTP_LINE_DELIM");

    if(header_start == NULL || output == NULL) return NULL;

    struct http_string key;
    struct http_string value;

    struct http_header header;

    struct http_header headers[MAX_HEADERS] = {0};


    char const* next_key = header_start;
    char const* next_value = NULL;

    int nb_header;
    for(nb_header = 0; strncmp(next_key, HTTP_LINE_DELIM, 2) != 0 && nb_header < MAX_HEADERS; ++nb_header) {
        if((next_value = get_next_token(next_key, HTTP_HDR_KV_DELIM, &key)) == NULL) return NULL;
        header.key = key;
        if((next_key = get_next_token(next_value, HTTP_LINE_DELIM, &value)) == NULL) return NULL;
        header.value = value;

        headers[nb_header] = header;
    }

    if(strncmp(next_key, HTTP_LINE_DELIM, 2) != 0 && nb_header == MAX_HEADERS) return NULL;

    memcpy(output->headers, headers, nb_header * sizeof(struct http_header));
    output->num_headers = nb_header;

    return next_key + 2;
}


int http_parse_message(const char *stream, size_t bytes_received, struct http_message *out, int *content_len)
{

    M_REQUIRE_NON_NULL(stream);
    M_REQUIRE_NON_NULL(out);
    M_REQUIRE_NON_NULL(content_len);

    char const* iter = stream;
    if(strstr(iter, HTTP_HDR_END_DELIM) == NULL) return 0;

    struct http_string method;
    if((iter = (char const*) get_next_token(iter, " ", &method)) == NULL) return ERR_RUNTIME;
    out->method = method;

    struct http_string uri;
    if((iter = (char const*) get_next_token(iter, " ", &uri)) == NULL) return  ERR_RUNTIME;
    out->uri = uri;

    struct http_string protocol;
    if((iter = (char const*) get_next_token(iter, HTTP_LINE_DELIM, &protocol)) == NULL) return ERR_RUNTIME;
    if(!http_match_verb(&protocol, "HTTP/1.1")) return ERR_RUNTIME;

    ptrdiff_t number_of_bytes_header = 0;

    if((iter = http_parse_headers(iter, out)) == NULL) return ERR_RUNTIME;
    else number_of_bytes_header = (ptrdiff_t)((iter) - (stream));

    for(int i = 0; i < out->num_headers; ++i) {
        if(http_match_verb(&out->headers[i].key, "Content-Length")) {
            const char* val_temp_buffer = NULL;
            if((val_temp_buffer = calloc(out->headers[i].value.len + 1, sizeof(char))) == NULL) return ERR_OUT_OF_MEMORY;
            strncpy(val_temp_buffer, out->headers[i].value.val, out->headers[i].value.len);

            *content_len = atouint32(val_temp_buffer);
            free(val_temp_buffer);

            int body_len = (bytes_received - number_of_bytes_header);

            out->body.val = iter;
            out->body.len = body_len;

            if(body_len > *content_len) return ERR_RUNTIME;
            else if(body_len < *content_len) return 0;

            return 1;
        }
    }

    out->body.len = 0;
    out->body.val = NULL;
    *content_len = 0;
    *content_len = 0;
    return 1;
}
