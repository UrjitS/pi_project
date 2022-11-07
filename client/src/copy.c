#include "copy.h"
#include "error.h"
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>


#define BUF_SIZE 1024

static uint8_t *dp_serialize(const struct data_packet *x, size_t *size);
static struct data_packet *dp_deserialize(ssize_t nRead, char * data_buffer);
static void write_bytes(int fd, const uint8_t *bytes, size_t size, struct sockaddr_in server_addr);
static struct data_packet * read_bytes(int fd, const uint8_t *bytes, size_t size, struct sockaddr_in server_addr, int seq);
static void process_response(void);

/**
 * Function to send data packet from client to server.
 * @param from_fd The file descriptor to read from.
 * @param to_fd The socket FD.
 * @param dataPacket Data packet with data and ACK/SEQ for reliable UDP.
 * @param server_addr Server address in network bytes.
 */
void copy(int from_fd, int to_fd, struct sockaddr_in server_addr)
{
    char *buffer;
    ssize_t bytesRead;
    uint8_t *bytes;
    size_t size;
    int sequence = 1;

    buffer = malloc(BUF_SIZE);
    // Special data type for
    struct data_packet dataPacket;

    // dynamic memory for data packet to be sent over to server.
    memset(&dataPacket, 0, sizeof(struct data_packet)); // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)

    // If buffer could not make enough memory, leave with error.
    if(buffer == NULL)
    {
        fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
    }

    // Read from the client's file descriptor put into the buffer.
    // Keep reading bytes until zero is returned for nothing read.
    while((bytesRead = read(from_fd, buffer, BUF_SIZE)) > 0)
    {
        // Construct data packet before using sento
        // Data flag set to 1
//        dataPacket.data_flag = 1;
//        // Ack flag set to 0
//        dataPacket.ack_flag = 0;
//        // Alternate sequence number
//        sequence = !sequence;
//        dataPacket.sequence_flag = sequence;
//
//        dataPacket.clockwise = 1;
//        dataPacket.counter_clockwise = 0;
//
//        // Get data
//        buffer[bytesRead-1] = '\0';
//        // Dynamic memory for data to send and fill with what was read into the buffer.
//        dataPacket.data = malloc(BUF_SIZE);
//        dataPacket.data = buffer;
//
//        // Serialize struct
//        bytes = dp_serialize(&dataPacket, &size);
//        // Send to server by using Socket FD.
//        write_bytes(to_fd, bytes, size, server_addr);
//        // Read socket FD until response from server is available, deserialize packet info and
//        //  display response.
//        read_bytes(to_fd, bytes, size, server_addr, sequence);
//        process_response();
    }

    // If the reading returns an error, leave with error.
    if(bytesRead == -1)
    {
        fatal_errno(__FILE__, __func__ , __LINE__, errno, 3);
    }

    // Free memory used for buffer.
    free(buffer);
}
