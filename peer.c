#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "DataStructs/List/LinkedList.h"
#include "DataStructs/Node/Node.h"
#define N 255
#define ERROR -1
#define MAX_PEERS 10
#define pkt_size 2048

enum msg_type
{
    File = 1,
    Chat = 2
};

typedef struct msg_request
{
    char message[N];
    char vclk[N];
    int ticket;
} msg_request;

typedef struct file_msg_request
{
    char filename[N];
    char line[N];
    char vclk[N];
    int ticket;
} file_msg_request;

typedef struct Chatclock
{
    int clock[MAX_PEERS];
} Chatclock;

typedef struct Fileclock
{
    int clock[MAX_PEERS];
} Fileclock;

void *server_callback(void *args);
int client_communication(const char *server_ip, int server_port, int peer_port);
int handshake(int socket, int port_num);
void memory_cleanup(void *args);
void file_cleanup(void *args);
void peer_exit(pthread_t peer);
int stov(char *literal, int *arr);
void vtos(char *literal, int *arr, int size);
void clock_init(int *clock);
void add_event(int *clock);
void merge_clocks(int *clock_1, int *clock_2);
void *send_callback(void *args);
void *chat_handler_callback(void *args);
void *file_handler_callback(void *args);
void *recv_callback(void *args);
void send_file(int socket);
void fcpy(char *source_filename, char *dest_filename);
void flush_stdin();

LinkedList queue; // where the messages are stored

pthread_mutex_t lock_chat, lock_file, lock_queue, lock_console;
pthread_cond_t cv, cv_2, cv_queue;
pthread_t send_tid, recv_tid, chat_tid, file_tid;

Chatclock chatclock;
Fileclock fileclock;

int id, choice;
int permission = 0; // conditional variable

int main(int argc, char const *argv[])
{

    if (argc != 3)
    {
        printf(" You need to enter server's ip and port number\n");
        exit(0);
    }
    pthread_mutex_init(&lock_chat, NULL);
    pthread_mutex_init(&lock_file, NULL);
    pthread_mutex_init(&lock_queue, NULL);
    pthread_mutex_init(&lock_console, NULL);
    pthread_cond_init(&cv, NULL);
    pthread_cond_init(&cv_2, NULL);
    pthread_cond_init(&cv_queue, NULL);
    clock_init(chatclock.clock);
    clock_init(fileclock.clock);

    int port_num;
    printf("Enter port number: \n");
    scanf("%d", &port_num);
    flush_stdin();
    // pthread_t server_thread;
    // pthread_create(&server_thread, NULL, server_callback, (void *)(intptr_t)port_num);
    queue = ll_constructor();
    client_communication(argv[1], atoi(argv[2]), port_num);
    while (1)
    {
        printf("\n\n--------MENU--------\n1)Type '1' to send a text message\n2)Type '2' to share an updated text file\n3)Type '3' to exit\n\n");
        scanf("%d", &choice);
        flush_stdin();
        pthread_mutex_lock(&lock_console);
        permission = 1;
        pthread_cond_signal(&cv);
        pthread_cond_wait(&cv_2, &lock_console);
        if (choice == 3)
            break;
    }
    pthread_mutex_unlock(&lock_console);
    pthread_mutex_destroy(&lock_chat);
    pthread_mutex_destroy(&lock_queue);
    pthread_mutex_destroy(&lock_file);
    pthread_mutex_destroy(&lock_console);
    pthread_cond_destroy(&cv);
    pthread_cond_destroy(&cv_2);
    pthread_cond_destroy(&cv_queue);
    // peer_exit(server_thread);

    return 0;
}

void *server_callback(void *args)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    int port = (int)(intptr_t)args;
    // printf("port -> %d\n", port);
    char incoming_msg[pkt_size], text[N], filename[N];
    struct sockaddr_in server_address, client_address;
    int addr_len = sizeof(struct sockaddr);
    int master_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (master_socket_fd == ERROR)
    {
        perror("Error on master socket\n");
        pthread_exit(0);
    }
    pthread_cleanup_push(file_cleanup, (void *)(intptr_t)master_socket_fd);

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(master_socket_fd, (struct sockaddr *)&server_address, sizeof(struct sockaddr)) == ERROR)
    {
        perror("Binding problem!");
        pthread_exit(0);
    }

    listen(master_socket_fd, 5);

    while (1)
    {
        printf("SERVER: Waiting for a client connection!\n");
        int client_handler_fd = accept(master_socket_fd, (struct sockaddr *)&client_address, &addr_len);
        if (client_handler_fd == ERROR)
        {
            perror("Accept error!");
            pthread_exit(0);
        }
        pthread_cleanup_push(file_cleanup, (void *)(intptr_t)client_handler_fd);
        printf("SERVER: Client connected!\n");

        memset(incoming_msg, 0, sizeof(incoming_msg));
        if (recv(client_handler_fd, incoming_msg, sizeof(incoming_msg), 0) == ERROR)
        {
            perror("Error with the receiving function!");
            pthread_exit(0);
        }
        sscanf(incoming_msg, "%s %s", text, filename);

        printf("SERVER: %s says-> %s\n", inet_ntoa(client_address.sin_addr), incoming_msg);
        close(client_handler_fd);
        pthread_cleanup_pop(0);
    }
    pthread_cleanup_pop(0);
}

int client_communication(const char *server_ip, int server_port, int peer_port)
{
    int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int addr_len = sizeof(struct sockaddr);
    struct sockaddr_in dest_address;
    char outgoing_msg[N];
    char vclk[N];
    char packet[pkt_size];

    dest_address.sin_family = AF_INET;
    dest_address.sin_port = htons(server_port);
    inet_aton(server_ip, &dest_address.sin_addr);

    if (connect(socket_fd, (struct sockaddr *)&dest_address, sizeof(struct sockaddr)) == ERROR)
    {
        perror("Connection error!");
        return ERROR;
    }
    printf("connected to server !\n");
    // performing the handshake
    id = handshake(socket_fd, peer_port);
    if (id == ERROR)
    {
        perror("Handshake failure!");
        return ERROR;
    }
    printf("%d\n", id);
    pthread_create(&send_tid, NULL, send_callback, (void *)(intptr_t)socket_fd);
    pthread_create(&recv_tid, NULL, recv_callback, (void *)(intptr_t)socket_fd);
    pthread_create(&chat_tid, NULL, chat_handler_callback, NULL);
    pthread_create(&file_tid, NULL, file_handler_callback, NULL);
    pthread_create(&send_tid, NULL, send_callback, (void *)(intptr_t)socket_fd);

    return 0;
}

int handshake(int socket, int port_num)
{
    char hello_msg[N], ack_msg[N];
    memset(hello_msg, 0, sizeof(hello_msg));
    memset(ack_msg, 0, sizeof(ack_msg));
    sprintf(hello_msg, "%d", port_num);
    if (send(socket, hello_msg, strlen(hello_msg) + 1, 0) == ERROR)
    {
        perror("Handshake sending error!");
        return ERROR;
    }
    if (recv(socket, ack_msg, sizeof(ack_msg), 0) == ERROR)
    {
        perror("Handshake receiving error!");
        return ERROR;
    }
    return atoi(ack_msg);
}

void *recv_callback(void *args)
{
    int socket_fd = (int)(intptr_t)args;
    msg_request request;
    file_msg_request file_request;
    char packet[pkt_size];
    int value;
    enum msg_type type;
    while (1)
    {
        memset(packet, 0, sizeof(packet));
        if (recv(socket_fd, packet, sizeof(packet), 0) == 0)
        {
            perror("Problem with receive callback!\n");
            exit(EXIT_FAILURE);
        }
        printf("PACKET RECEIVED: %s\n", packet);
        sscanf(packet, "%d", &value);
        type = (enum msg_type)value;
        if (type == Chat)
        {
            /* Peer received a chat message*/

            sscanf(packet, "%d %s %d %[^\n]", &value, request.message, &request.ticket, request.vclk);
            pthread_mutex_lock(&lock_queue);
            ll_push(&queue, &request, sizeof(msg_request));
        }
        else
        {
            /* Peer received a file sharing message */

            sscanf(packet, "%d %s %s %d %[^\n]", &value, file_request.filename, file_request.line, &file_request.ticket, file_request.vclk);
            pthread_mutex_lock(&lock_queue);
            ll_push(&queue, &file_request, sizeof(file_msg_request));
        }

        if (queue.length - 1 == 0)
        {
            pthread_cond_broadcast(&cv_queue);
            puts("RECEIVER: I send the signal");
        }
        pthread_mutex_unlock(&lock_queue);
    }
}

void *chat_handler_callback(void *args)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    int temp_clk[MAX_PEERS];
    int current_counter = 0;
    msg_request request;
    LinkedList list = ll_constructor();
    while (1)
    {
        puts("Chat handler tries to acquire the queue lock...");
        pthread_mutex_lock(&lock_queue);
        puts("Chat thread looping...");
        printf("QUEUE LENGTH -> %d\n", queue.length);
        while (queue.length == 0)
        {
            puts("chat waits");
            pthread_cond_wait(&cv_queue, &lock_queue);
            puts("Chat signal received!");
        }

        for (int i = 0; i < queue.length; i++)
        {
            if (ll_retrieve_size(&queue, i) == sizeof(msg_request))
            {
                /* We have a chat message */
                request = *(msg_request *)ll_retrieve(&queue, i);
                ll_push(&list, &request, sizeof(msg_request));
                ll_remove(&queue, i);
                printf("CHAT: Received text -> %s\n", request.message);
                printf("CHAT: Received vector clock -> %s\n", request.vclk);
                printf("CHAT: incoming counter -> %d\n", request.ticket);
            }
        }
        pthread_mutex_unlock(&lock_queue);
        sleep(1);

        for (int i = 0; i < list.length; i++)
        {
            msg_request temp = *(msg_request *)ll_retrieve(&list, i);
            if (temp.ticket == current_counter + 1)
            {
                printf("CHAT: Delivered: %s\n", temp.message);
                stov(temp.vclk, temp_clk);
                pthread_mutex_lock(&lock_chat);
                add_event(chatclock.clock);
                merge_clocks(temp_clk, chatclock.clock);
                pthread_mutex_unlock(&lock_chat);
                ll_remove(&list, i);
                current_counter++;
            }
        }
    }
}

void *file_handler_callback(void *args)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    int temp_clk[MAX_PEERS];
    int current_counter = 0;
    file_msg_request request;
    int fd;
    LinkedList list = ll_constructor();
    while (1)
    {
        puts("File handler tries to acquire the queue lock...");
        pthread_mutex_lock(&lock_queue);
        puts("File thread looping...");
        printf("QUEUE LENGTH -> %d\n", queue.length);
        while (queue.length == 0)
        {
            puts("file waits");
            pthread_cond_wait(&cv_queue, &lock_queue);
            puts("file signal received!");
        }
        for (int i = 0; i < queue.length; i++)
        {
            if (ll_retrieve_size(&queue, i) == sizeof(file_msg_request))
            {
                /* We have a file sharing message */
                request = *(file_msg_request *)ll_retrieve(&queue, i);
                ll_push(&list, &request, sizeof(file_msg_request));
                ll_remove(&queue, i);
                printf("Received text -> %s\n", request.line);
                printf("Received vector clock -> %s\n", request.vclk);
                printf("incoming counter -> %d\n", request.ticket);
            }
        }
        pthread_mutex_unlock(&lock_queue);
        sleep(1);

        for (int i = 0; i < list.length; i++)
        {
            file_msg_request temp = *(file_msg_request *)ll_retrieve(&list, i);
            if (temp.ticket == current_counter + 1)
            {
                stov(temp.vclk, temp_clk);
                pthread_mutex_lock(&lock_file);
                add_event(fileclock.clock);
                merge_clocks(temp_clk, fileclock.clock);
                fd = open(temp.filename, O_CREAT | O_APPEND | O_WRONLY);
                write(fd, temp.line, strlen(temp.line));
                write(fd, "\n", strlen("\n"));
                close(fd);
                pthread_mutex_unlock(&lock_file);
                ll_remove(&list, i);
                current_counter++;
            }
        }
    }
}

void *send_callback(void *args)
{
    int socket_fd = (int)(intptr_t)args;
    char packet[pkt_size], text[N], vclk[N];
    while (1)
    {
        memset(text, 0, sizeof(text));
        memset(vclk, 0, sizeof(vclk));
        memset(packet, 0, sizeof(packet));
        pthread_mutex_lock(&lock_console);
        while (permission != 1)
            pthread_cond_wait(&cv, &lock_console);
        if (choice == 1)
        {
            // sending a text message
            printf("Write your message:\n");
            fgets(text, sizeof(text), stdin);
            pthread_cond_signal(&cv_2);
            pthread_mutex_unlock(&lock_console);
            puts("Got a new message!");
            pthread_mutex_lock(&lock_chat);
            add_event(chatclock.clock);
            for (size_t i = 0; i < MAX_PEERS; i++)
            {
                printf("%d\t", chatclock.clock[i]);
            }
            vtos(vclk, chatclock.clock, MAX_PEERS);
            pthread_mutex_unlock(&lock_chat);

            sprintf(packet, "%d %s %s", Chat, text, vclk);
            printf("SENDING: %s", text);
            if (send(socket_fd, packet, strlen(packet) + 1, 0) == -1)
            {
                perror("Problem with send callback!\n");
                exit(EXIT_FAILURE);
            }
        }
        else if (choice == 2)
        {
            // sharing a txt file
            send_file(socket_fd);
        }
        else
        {
            /* exiting */
            sprintf(packet, "%d %s %s", Chat, "exit", vclk);
            if (send(socket_fd, packet, strlen(packet) + 1, 0) == -1)
            {
                perror("Problem with send callback!\n");
                exit(EXIT_FAILURE);
            }
            pthread_cancel(recv_tid);
            pthread_cancel(file_tid);
            pthread_cancel(chat_tid);
            close(socket_fd);
            pthread_cond_signal(&cv_2);
            pthread_mutex_unlock(&lock_console);
            break;
        }

        permission = 0;
    }
    return NULL;
}

void file_cleanup(void *args)
{
    printf("Invoked\n");
    int fd = (int)(intptr_t)args;
    close(fd);
}

void memory_cleanup(void *args)
{
    printf("invoked\n");
    free(args);
}

void peer_exit(pthread_t peer)
{
    pthread_cancel(peer);
    pthread_join(peer, NULL);
}

int stov(char *literal, int *arr)
{
    int index = 0;
    char *token = strtok(literal, " ");
    while (token != NULL)
    {
        arr[index++] = atoi(token);
        token = strtok(NULL, " ");
    }
    return index;
}

void vtos(char *literal, int *arr, int size)
{
    int index = sprintf(literal, "%d", arr[0]);
    for (int i = 1; i < size; i++)
    {
        index += sprintf(&literal[index], "%s%d", " ", arr[i]);
        // index += sprintf(&literal[index], "%s", " ");
    }
}

void clock_init(int *clock)
{
    for (int i = 0; i < MAX_PEERS; i++)
    {
        clock[i] = 0;
    }
}

void add_event(int *clock)
{
    printf("Event counter -> %d\n", clock[id]);
    clock[id] = clock[id] + 1;
}

void merge_clocks(int *clock_1, int *clock_2)
{
    for (int i = 0; i < MAX_PEERS; i++)
    {
        if (clock_1[i] > clock_2[i])
        {
            clock_2[i] = clock_1[i];
        }
    }
}

void send_file(int socket)
{
    int bytes;
    char filename[N], data[N], buffer[pkt_size], vclk[N];
    memset(filename, 0, sizeof(filename));
    printf("Write the file name:\n");
    fgets(filename, sizeof(filename), stdin);
    pthread_cond_signal(&cv_2);
    pthread_mutex_unlock(&lock_console);
    memset(data, 0, sizeof(data));
    memset(buffer, 0, sizeof(buffer));
    memset(vclk, 0, sizeof(vclk));
    filename[strlen(filename) - 1] = '\0';
    fcpy(filename, "temp.txt");
    FILE *fp = fopen("temp.txt", "r");
    while (fgets(data, sizeof(data), fp) != NULL)
    {
        printf("scaning the file...\n");
        pthread_mutex_lock(&lock_file);
        add_event(fileclock.clock);
        vtos(vclk, fileclock.clock, MAX_PEERS);
        pthread_mutex_unlock(&lock_file);
        // data[strlen(data) - 1] = '\0';
        sprintf(buffer, " %d %s %s %s", File, filename, data, vclk);
        if ((bytes = send(socket, buffer, strlen(buffer) + 1, 0)) == ERROR)
        {
            perror("Error in file sending");
            exit(EXIT_FAILURE);
        }
        printf("%d\n", bytes);
        printf("FILE SENDING: %s\n", buffer);
        memset(data, 0, sizeof(data));
        memset(buffer, 0, sizeof(buffer));
        memset(vclk, 0, sizeof(vclk));
        //sleep(1);
    }
    fclose(fp);
    remove("temp.txt");
    printf("Finished with the file\n");
}

void fcpy(char *source_filename, char *dest_filename)
{
    FILE *source_fp = fopen(source_filename, "r");
    FILE *dest_fp = fopen(dest_filename, "w");
    char line[N];
    memset(line, 0, sizeof(line));
    while (fgets(line, sizeof(line), source_fp) != NULL)
    {
        fprintf(dest_fp, "%s", line);
        printf("%s\n", line);
        memset(line, 0, sizeof(line));
    }
    fclose(dest_fp);
    fclose(source_fp);
}

void flush_stdin()
{
    int c;
    do
    {
        c = getchar();
    } while (c != '\n' && c != EOF);
}
