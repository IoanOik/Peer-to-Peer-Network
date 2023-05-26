#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/select.h>
#include <stdint.h>
#include <pthread.h>
#include "DataStructs/List/LinkedList.h"
#include "DataStructs/Node/Node.h"
#define N 255
#define PORT 4999
#define MAX_PEERS 10
#define pkt_size 2048
#define file_ASCII "#102105108101#"

enum msg_type
{
    Begin_file = 0,
    File = 1,
    Chat = 2,
    eof = 3
};

typedef struct chat_net_packet
{
    char payload[N];
    int vclk[MAX_PEERS];
} chat_net_packet;

typedef struct file_net_packet
{
    char filename[N];
    char payload[N];
    int vclk[MAX_PEERS];
} file_net_packet;

typedef struct Chatclock
{
    int clock[MAX_PEERS];
} Chatclock;

typedef struct Fileclock
{
    int clock[MAX_PEERS];
} Fileclock;

int stov(char *literal, int *arr);
void vtos(char *literal, int *arr, int size);
void setup_server_com();
void set_init();
int add_to_set(int fd);
void remove_from_set(int fd);
void init_fds(fd_set *set);
int max_fd();
int min_vector(int *a, int *b);
int get_new_id();
int compare_func(const void *a, const void *b);
void fifo_broadcast(LinkedList *list, enum msg_type type);
void fifo_broadcast_file(LinkedList *list, enum msg_type type);
void merge_clocks(int *clock_1, int *clock_2);

Chatclock server_chat_clock;
Fileclock server_file_clock;
int monitored_fd_set[MAX_PEERS];
int counter = 0;

int main(int argc, char const *argv[])
{
    for (int i = 0; i < MAX_PEERS; i++)
    {
        server_chat_clock.clock[i] = 0;
        server_file_clock.clock[i] = 0;
    }

    setup_server_com();

    return 0;
}

void setup_server_com()
{
    struct sockaddr_in server_addr, peer_addr;
    int addr_len = sizeof(struct sockaddr);
    char buffer[pkt_size], message[N], filename[N], clk_literal[N];
    int peer_clk[MAX_PEERS];
    fd_set readfds;

    int master_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (master_socket_fd == -1)
    {
        perror("Error building master socket!");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(master_socket_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("Binding error!");
        exit(EXIT_FAILURE);
    }
    listen(master_socket_fd, 5);
    set_init();
    add_to_set(master_socket_fd);
    puts("server up and running!");
    while (1)
    {
        init_fds(&readfds);
        puts("blocked on select");
        select(max_fd() + 1, &readfds, NULL, NULL, NULL);
        puts("select unblocked");
        int peer_handler_fd, peer_id, peer_port;

        if (FD_ISSET(master_socket_fd, &readfds))
        {
            // New member to the chat-room
            puts("new client connected");
            char ack_message[N];
            memset(ack_message, 0, sizeof(ack_message));
            peer_handler_fd = accept(master_socket_fd, (struct sockaddr *)&peer_addr, &addr_len);
            if (peer_handler_fd == -1)
            {
                perror("Problem with accept!");
                exit(EXIT_FAILURE);
            }
            if (recv(peer_handler_fd, ack_message, sizeof(ack_message), 0) == -1)
            {
                perror("Problem with receiving!");
                exit(EXIT_FAILURE);
            }
            peer_port = atoi(ack_message);
            memset(ack_message, 0, sizeof(ack_message));
            sprintf(ack_message, "%d", get_new_id());
            if (send(peer_handler_fd, ack_message, strlen(ack_message) + 1, 0) == -1)
            {
                perror("Problem with sending!");
                exit(EXIT_FAILURE);
            }
            add_to_set(peer_handler_fd);
        }
        else
        {
            // Existing member sends a message for broadcast
            puts("Existing member sends a message for broadcast");
            int value;
            enum msg_type type;
            LinkedList chat_msg_list = ll_constructor();
            LinkedList file_msg_list = ll_constructor();

            for (int i = 0; i < MAX_PEERS; i++)
            {
                if (FD_ISSET(monitored_fd_set[i], &readfds))
                {
                    // we found the one who wants to broadcast
                    puts("we found the one who wants to broadcast");
                    printf("%d\n", i);
                    memset(buffer, 0, sizeof(buffer));
                    memset(message, 0, sizeof(message));
                    memset(clk_literal, 0, sizeof(clk_literal));
                    peer_id = i;
                    peer_handler_fd = monitored_fd_set[i];
                    if (recv(peer_handler_fd, buffer, sizeof(buffer), 0) == -1)
                    {
                        perror("Problem receiving the broadcast message!\n");
                        exit(EXIT_FAILURE);
                    }
                    sscanf(buffer, "%d", &value);

                    type = (enum msg_type)value;
                    if (type == Chat)
                    {
                        /* Peer sent a chat message */
                        chat_net_packet pkt;
                        sscanf(buffer, "%d %s %[^\n]", &value, message, clk_literal);
                        printf("PACKET: %s\n", buffer);
                        printf("CLOCK: %s\n", clk_literal);
                        stov(clk_literal, peer_clk);
                        for (int i = 0; i < MAX_PEERS; i++)
                        {
                            printf("%d\t", peer_clk[i]);
                        }

                        merge_clocks(peer_clk, server_chat_clock.clock);
                        if (strcmp(message, "exit") == 0)
                        {
                            remove_from_set(peer_handler_fd);
                            puts("Client exited");
                        }
                        printf("Hey\n");
                        memmove(pkt.payload, message, sizeof(message));
                        puts("after memmove");
                        //printf("%s\n", pkt.payload);
                        memcpy(pkt.vclk, server_chat_clock.clock, sizeof(server_chat_clock));
                        puts("after memcopy");
                        ll_push(&chat_msg_list, &pkt, sizeof(chat_net_packet));
                    }
                    else
                    {
                        /* Peer sent a file sharing message */
                        file_net_packet packet;
                        memset(filename, 0, sizeof(filename));
                        sscanf(buffer, "%d %s %s %[^\n]", &value, filename, message, clk_literal);
                        printf("RECEIVING PACKET: %s\n", buffer);
                        stov(clk_literal, peer_clk);
                        merge_clocks(peer_clk, server_file_clock.clock);
                        memmove(packet.payload, message, sizeof(message));
                        memmove(packet.filename, filename, strlen(filename) + 1);
                        puts("after memmove");
                        memcpy(packet.vclk, server_file_clock.clock, sizeof(server_file_clock));
                        puts("after memcopy");
                        ll_push(&file_msg_list, &packet, sizeof(file_net_packet));
                    }
                }
            }
            // The lists are completed and now we have to decide the boadcast queue
            ll_sort(&file_msg_list, compare_func);
            ll_sort(&chat_msg_list, compare_func);
            fifo_broadcast(&chat_msg_list, Chat);
            fifo_broadcast_file(&file_msg_list, File);
        }
    }
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

void set_init()
{
    for (int i = 0; i < MAX_PEERS; i++)
    {
        monitored_fd_set[i] = -1;
    }
}

int add_to_set(int fd)
{
    for (int i = 0; i < MAX_PEERS; i++)
    {
        if (monitored_fd_set[i] == -1)
        {
            monitored_fd_set[i] = fd;
            return 0;
        }
    }
    return -1;
}

void remove_from_set(int fd)
{
    for (int i = 0; i < MAX_PEERS; i++)
    {
        if (monitored_fd_set[i] == fd)
        {
            monitored_fd_set[i] = -1;
            server_chat_clock.clock[i] = 0;
            server_file_clock.clock[i] = 0;
            break;
        }
    }
}

void init_fds(fd_set *set)
{
    FD_ZERO(set);
    for (int i = 0; i < MAX_PEERS; i++)
    {
        if (monitored_fd_set[i] != -1)
            FD_SET(monitored_fd_set[i], set);
    }
}

int max_fd()
{
    int max = monitored_fd_set[0];
    for (int i = 1; i < MAX_PEERS; i++)
    {
        if (max < monitored_fd_set[i])
        {
            max = monitored_fd_set[i];
        }
    }
    return max;
}

int get_new_id()
{
    int i;
    for (i = 0; i < MAX_PEERS; i++)
    {
        if (monitored_fd_set[i] == -1)
            break;
    }
    return i - 1;
}

int min_vector(int *a, int *b)
{
    int a_counter = 0;
    int b_counter = 0;
    for (int i = 0; i < MAX_PEERS; i++)
    {
        if (a[i] < b[i])
            a_counter++;
        else if (a[i] > b[i])
            b_counter++;
    }
    if (a_counter == 0 && b_counter > 0)
        return 1;
    else if (b_counter == 0 && a_counter > 0)
        return -1;

    return 0;
}

int compare_func(const void *a, const void *b)
{
    chat_net_packet *pkt_1 = (chat_net_packet *)a;
    chat_net_packet *pkt_2 = (chat_net_packet *)b;
    return min_vector(pkt_1->vclk, pkt_2->vclk);
}

void fifo_broadcast(LinkedList *list, enum msg_type type)
{
    char buffer[pkt_size];
    char clock_literal[N];
    puts("Fifo broadcast started");

    for (int i = 0; i < list->length; i++)
    {
        counter++;
        chat_net_packet pkt = *(chat_net_packet *)ll_retrieve(list, i);
        memset(buffer, 0, sizeof(buffer));
        memset(clock_literal, 0, sizeof(clock_literal));
        vtos(clock_literal, pkt.vclk, MAX_PEERS);
        sprintf(buffer, "%d %s %d %s", type, pkt.payload, counter, clock_literal);
        printf("Packet to send: %s\n", buffer);
        for (int j = 1; j < MAX_PEERS; j++)
        {
            if (monitored_fd_set[j] != -1)
            {
                printf("Found connected clients: %d\n", j);
                if (send(monitored_fd_set[j], buffer, strlen(buffer) + 1, 0) == -1)
                {
                    perror("broadcast failed!");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    ll_destroy(list);
}

void fifo_broadcast_file(LinkedList *list, enum msg_type type)
{
    char buffer[pkt_size];
    char clock_literal[N];
    puts("Fifo broadcast started");

    for (int i = 0; i < list->length; i++)
    {
        counter++;
        file_net_packet pkt = *(file_net_packet *)ll_retrieve(list, i);
        memset(buffer, 0, sizeof(buffer));
        memset(clock_literal, 0, sizeof(clock_literal));
        vtos(clock_literal, pkt.vclk, MAX_PEERS);
        sprintf(buffer, "%d %s %s %d %s", type, pkt.filename, pkt.payload, counter, clock_literal);
        printf("Packet to send: %s\n", buffer);
        for (int j = 1; j < MAX_PEERS; j++)
        {
            if (monitored_fd_set[j] != -1)
            {
                printf("Found connected clients: %d\n", j);
                if (send(monitored_fd_set[j], buffer, strlen(buffer) + 1, 0) == -1)
                {
                    perror("broadcast failed!");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    ll_destroy(list);
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