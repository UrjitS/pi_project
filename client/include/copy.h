#ifndef OPEN_COPY_H
#define OPEN_COPY_H

#include <unistd.h>
#include <netinet/in.h>

// Custom struct for confirmation and sequence between client/server.
struct data_packet {
    int data_flag;
    int ack_flag;
    int sequence_flag;
    int clockwise;
    int counter_clockwise;
    char *data;
};
/**
 * For sending information to another machine.
 * @param from_fd File Descriptor of source.
 * @param to_fd  File Descriptor of Destination.
 * @param dataPacket Data Packet for making connection reliable.
 * @param server_addr Socket address of destination address.
 */
void copy(int from_fd, int to_fd, struct sockaddr_in server_addr);

#endif //OPEN_COPY_H
