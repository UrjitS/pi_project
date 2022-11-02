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
static void process_response();

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
        dataPacket.data_flag = 1;
        // Ack flag set to 0
        dataPacket.ack_flag = 0;
        // Alternate sequence number
        sequence = !sequence;
        dataPacket.sequence_flag = sequence;
        // Get data
        buffer[bytesRead-1] = '\0';
        // Dynamic memory for data to send and fill with what was read into the buffer.
        dataPacket.data = malloc(BUF_SIZE);
        dataPacket.data = buffer;

        // Serialize struct
        bytes = dp_serialize(&dataPacket, &size);
        // Send to server by using Socket FD.
        write_bytes(to_fd, bytes, size, server_addr);
        // Read socket FD until response from server is available, deserialize packet info and
        //  display response.
        read_bytes(to_fd, bytes, size, server_addr, sequence);
        process_response();
    }

    // If the reading returns an error, leave with error.
    if(bytesRead == -1)
    {
        fatal_errno(__FILE__, __func__ , __LINE__, errno, 3);
    }

    // Free memory used for buffer.
    free(buffer);
}

/**
 * For sending by writing to socket FD.
 * @param fd Socket FD.
 * @param bytes the bytes to read.
 * @param size the size of bytes to read.
 * @param server_addr Network address of the server to send to.
 */
static void write_bytes(int fd, const uint8_t *bytes, size_t size, struct sockaddr_in server_addr)
{

    ssize_t nWrote;

    // Sending the data to server machine.
    nWrote = sendto(fd, bytes, size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

    options_process_close(nWrote);

    // Display bytes and ACK/SEQ of packet sent.
    printf("Wrote %ld bytes\n", nWrote);

}

/**
 * Display ACK/SEQ of the data packet sent over.
 * @param dataPacket the data packet to make UDP reliable and hold data.
 */
static void process_response(void) {
    printf("Received Ack \n");
}

/**
 *  Read from socket FD and deserialize data packet.
 * @param fd Socket FD.
 * @param bytes The bytes to read.
 * @param size the size of bytes to read.
 * @param server_addr the network address of the server.
 * @return Data packet pointer of deserialized packet.
 */
static struct data_packet * read_bytes(int fd, const uint8_t *bytes, size_t size, struct sockaddr_in server_addr, int seq)
{
    struct sockaddr from_addr;
    char data[BUF_SIZE];
    ssize_t nRead;
    socklen_t from_addr_len;
    printf("\n Waiting \n");
    from_addr_len = sizeof (struct sockaddr);

    // Read from the socket FD and get bytes read.
    nRead = recvfrom(fd, data, BUF_SIZE, 0, &from_addr, &from_addr_len);

    // Re-send if -1
    if(nRead == -1)
    {
        write_bytes(fd, bytes, size, server_addr);
    }

    // Return the data packet from the serialized information sent over.
    struct data_packet * dataPacket = dp_deserialize(nRead, data);
    if (dataPacket->sequence_flag != seq) {
        read_bytes(fd, bytes, size, server_addr, seq);
    }
}

/**
 * Serialize the data packet.
 * @param x Pointer to the data packet to send to server.
 * @param size Size of the data packet.
 * @return Serialized data packet to send to server.
 */
static uint8_t *dp_serialize(const struct data_packet *x, size_t *size)
{

    uint8_t *bytes;
    size_t count;
    size_t len;
    int data_flag_number;
    int ack_flag_number;
    int sequence_flag_number;

    len = strlen(x->data);

    // Dynamic memory for serialize data packet.
    *size = sizeof(x->data_flag) + sizeof(x->ack_flag) + sizeof(x->sequence_flag) + len;
    bytes = malloc(*size);

    // Make network byte order
    data_flag_number = htons(x->data_flag);
    // ACK/SEQ for reliable UDP converted to network bytes.
    ack_flag_number = htons(x->ack_flag);
    sequence_flag_number = htons(x->sequence_flag);

    count = 0;

    // Writing of serialized sequence to be sent over to server. Includes
    // the ACK, SEQ, and Data to be sent over.
    memcpy(&bytes[count], &data_flag_number, sizeof(data_flag_number));
    count += sizeof(data_flag_number);

    memcpy(&bytes[count], &ack_flag_number, sizeof(ack_flag_number));
    count += sizeof(ack_flag_number);

    memcpy(&bytes[count], &sequence_flag_number, sizeof(sequence_flag_number));
    count += sizeof(sequence_flag_number);

    memcpy(&bytes[count], x->data, len);

    return bytes;
}

/**
 * Deserialize the data packet received.
 * @param nRead Number of bytes to read.
 * @param data_buffer Data  buffer to read from.
 * @return
 */
static struct data_packet *dp_deserialize(ssize_t nRead, char * data_buffer)
{
    struct data_packet * dataPacketRecieved = malloc(sizeof(struct data_packet));
    size_t count;

    memset(dataPacketRecieved, 0, sizeof(struct data_packet));
    count = 0;

    memcpy(&dataPacketRecieved->data_flag, &data_buffer[count], sizeof(dataPacketRecieved->data_flag));
    count += sizeof(dataPacketRecieved->data_flag);

    memcpy(&dataPacketRecieved->ack_flag, &data_buffer[count], sizeof(dataPacketRecieved->ack_flag));
    count += sizeof(dataPacketRecieved->ack_flag);

    memcpy(&dataPacketRecieved->sequence_flag, &data_buffer[count], sizeof(dataPacketRecieved->sequence_flag));

    dataPacketRecieved->data_flag = ntohs(dataPacketRecieved->data_flag);
    dataPacketRecieved->ack_flag = ntohs(dataPacketRecieved->ack_flag);
    dataPacketRecieved->sequence_flag = ntohs(dataPacketRecieved->sequence_flag);

    return dataPacketRecieved;
}
