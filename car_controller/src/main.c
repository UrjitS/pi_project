#include "error.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bits/types/struct_timeval.h>
#include <bits/types/sig_atomic_t.h>
#include <wiringPi.h>

#define RightButtonPin 1
#define LeftButtonPin 0
#define BUF_SIZE 1024
#define DEFAULT_PORT 5020

// Custom struct for confirmation and sequence between car_controller/car_motors.
struct data_packet {
    int data_flag;
    int ack_flag;
    int sequence_flag;
    int clockwise;
    int counter_clockwise;
    char *data;
};

// Tracking Ip and port information for car_controller and car_motors.
struct options
{
    char *ip_client;
    char *ip_receiver;
    in_port_t port_receiver; // special type for output port.
    struct sockaddr_in server_addr; // special type for
    int fd_in;
};
static volatile sig_atomic_t running;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Prototypes of functions.
static void options_init(struct options *opts);
static void parse_arguments(int argc, char *argv[], struct options *opts);
static void options_process(struct options *opts);
static void cleanup(const struct options *opts);
static uint8_t *dp_serialize(const struct data_packet *x, size_t *size);
static struct data_packet *dp_deserialize(char *data_buffer);
static void write_bytes(int fd, const uint8_t *bytes, size_t size, struct sockaddr_in server_addr);
static struct data_packet * read_bytes(int fd, const uint8_t *bytes, size_t size, struct sockaddr_in server_addr, int seq);


int main(int argc, char *argv[])
{
    // Initiating our custom struct.
    struct options opts;
    struct data_packet dataPacket;
    char *buffer;
    buffer = malloc(BUF_SIZE);
    int sequence = 1;
    long offCounter = 0;
    long rightCounter = 10000;
    long leftCounter = 10000;
    uint8_t *bytes;
    size_t size;

    memset(&dataPacket, 0, sizeof(struct data_packet)); // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)

    // Initiating, parsing, and processing option struct for car_controller/car_motors information.
    options_init(&opts);
    parse_arguments(argc, argv, &opts);
    // option processing is also when socket connection is made.
    options_process(&opts);

    // If valid information for car_controller and sever, send data to car_motors.
    if(opts.ip_client && opts.ip_receiver)
    {
        if (wiringPiSetup() == -1) {
            printf("WiringPi failed \n");
            return EXIT_FAILURE;
        }

        pinMode(RightButtonPin, INPUT);
        pinMode(LeftButtonPin, INPUT);

        running = 1;

        // Continues loop to keep listening to self.
        while(running)
        {
            // Turn motors off if neither buttons are pressed
            if (digitalRead(RightButtonPin) == 1 && digitalRead(LeftButtonPin) == 1) {
                offCounter++;
                if (offCounter >= 100000) {
                    printf("Sending Off command\n");
                    // Send Off
                    // Construct data packet before using sento
                    // Data flag set to 1
                    dataPacket.data_flag = 1;
                    // Ack flag set to 0
                    dataPacket.ack_flag = 0;
                    // Alternate sequence number
                    sequence = !sequence;
                    dataPacket.sequence_flag = sequence;

                    dataPacket.clockwise = 0;
                    dataPacket.counter_clockwise = 0;

                    buffer[0] = '\0';
                    // Dynamic memory for data to send and fill with what was read into the buffer.
                    dataPacket.data = malloc(BUF_SIZE);
                    dataPacket.data = buffer;

                    // Serialize struct
                    bytes = dp_serialize(&dataPacket, &size);
                    // Send to car_motors by using Socket FD.
                    write_bytes(opts.fd_in, bytes, size, opts.server_addr);
                    read_bytes(opts.fd_in, bytes, size, opts.server_addr, sequence);
                }
            } else if (digitalRead(RightButtonPin) == 0 && digitalRead(LeftButtonPin) == 1) {
                rightCounter++;
                if (rightCounter >= 100000) {
                    printf("Sending Clockwise command\n");
                    // Send Right
                    // Construct data packet before using sento
                    // Data flag set to 1
                    dataPacket.data_flag = 1;
                    // Ack flag set to 0
                    dataPacket.ack_flag = 0;
                    // Alternate sequence number
                    sequence = !sequence;
                    dataPacket.sequence_flag = sequence;

                    dataPacket.clockwise = 1;
                    dataPacket.counter_clockwise = 0;

                    // Get data
                    buffer[0] = '\0';
                    // Dynamic memory for data to send and fill with what was read into the buffer.
                    dataPacket.data = malloc(BUF_SIZE);
                    dataPacket.data = buffer;

                    // Serialize struct
                    bytes = dp_serialize(&dataPacket, &size);
                    // Send to car_motors by using Socket FD.
                    write_bytes(opts.fd_in, bytes, size, opts.server_addr);
                    read_bytes(opts.fd_in, bytes, size, opts.server_addr, sequence);
                }
            } else if (digitalRead(LeftButtonPin) == 0 && digitalRead(RightButtonPin) == 1) {
                leftCounter++;
                if (leftCounter >= 100000) {
                    printf("Sending CounterClockwise command\n");
                    // Send Left
                    // Construct data packet before using sento
                    // Data flag set to 1
                    dataPacket.data_flag = 1;
                    // Ack flag set to 0
                    dataPacket.ack_flag = 0;
                    // Alternate sequence number
                    sequence = !sequence;
                    dataPacket.sequence_flag = sequence;

                    dataPacket.clockwise = 0;
                    dataPacket.counter_clockwise = 1;

                    // Get data
                    buffer[0] = '\0';
                    // Dynamic memory for data to send and fill with what was read into the buffer.
                    dataPacket.data = malloc(BUF_SIZE);
                    dataPacket.data = buffer;

                    // Serialize struct
                    bytes = dp_serialize(&dataPacket, &size);
                    // Send to car_motors by using Socket FD.
                    write_bytes(opts.fd_in, bytes, size, opts.server_addr);
                    read_bytes(opts.fd_in, bytes, size, opts.server_addr, sequence);
                }

            }

        }
    }

    // Clean up memory from option struct pointer.
    cleanup(&opts);
    return EXIT_SUCCESS;
}


/**
 * For sending by writing to socket FD.
 * @param fd Socket FD.
 * @param bytes the bytes to read.
 * @param size the size of bytes to read.
 * @param server_addr Network address of the car_motors to send to.
 */
static void write_bytes(int fd, const uint8_t *bytes, size_t size, struct sockaddr_in server_addr)
{

    ssize_t nWrote;

    // Sending the data to car_motors machine.
    nWrote = sendto(fd, bytes, size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // Display bytes and ACK/SEQ of packet sent.
    printf("Wrote %ld bytes\n", nWrote);

}

/**
 *  Read from socket FD and deserialize data packet.
 * @param fd Socket FD.
 * @param bytes The bytes to read.
 * @param size the size of bytes to read.
 * @param server_addr the network address of the car_motors.
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
    struct data_packet * dataPacket = dp_deserialize(data);
    if (dataPacket->sequence_flag != seq) {
        read_bytes(fd, bytes, size, server_addr, seq);
    }
}

/**
 * Serialize the data packet.
 * @param x Pointer to the data packet to send to car_motors.
 * @param size Size of the data packet.
 * @return Serialized data packet to send to car_motors.
 */
static uint8_t *dp_serialize(const struct data_packet *x, size_t *size)
{

    uint8_t *bytes;
    size_t count;
    size_t len;
    int data_flag_number;
    int ack_flag_number;
    int sequence_flag_number;
    int clockwise;
    int counter_clockwise;

    len = strlen(x->data);

    // Dynamic memory for serialize data packet.
    *size = sizeof(x->data_flag) + sizeof(x->ack_flag) + sizeof(x->sequence_flag) + sizeof(x->clockwise) + sizeof(x->counter_clockwise) + len;
    bytes = malloc(*size);

    // Make network byte order
    data_flag_number = htons(x->data_flag);
    // ACK/SEQ for reliable UDP converted to network bytes.
    ack_flag_number = htons(x->ack_flag);
    sequence_flag_number = htons(x->sequence_flag);
    clockwise = htons(x->clockwise);
    counter_clockwise = htons(x->counter_clockwise);

    count = 0;

    // Writing of serialized sequence to be sent over to car_motors. Includes
    // the ACK, SEQ, and Data to be sent over.
    memcpy(&bytes[count], &data_flag_number, sizeof(data_flag_number));
    count += sizeof(data_flag_number);

    memcpy(&bytes[count], &ack_flag_number, sizeof(ack_flag_number));
    count += sizeof(ack_flag_number);

    memcpy(&bytes[count], &sequence_flag_number, sizeof(sequence_flag_number));
    count += sizeof(sequence_flag_number);

    memcpy(&bytes[count], &clockwise, sizeof(clockwise));
    count += sizeof(clockwise);

    memcpy(&bytes[count], &counter_clockwise, sizeof(counter_clockwise));
    count += sizeof(counter_clockwise);

    memcpy(&bytes[count], x->data, len);

    return bytes;
}

/**
 * Deserialize the data packet received.
 * @param data_buffer Data  buffer to read from.
 * @return
 */
static struct data_packet *dp_deserialize(char *data_buffer)
{
    struct data_packet * pDataPacket = malloc(sizeof(struct data_packet));
    size_t count;

    memset(pDataPacket, 0, sizeof(struct data_packet));
    count = 0;

    memcpy(&pDataPacket->data_flag, &data_buffer[count], sizeof(pDataPacket->data_flag));
    count += sizeof(pDataPacket->data_flag);

    memcpy(&pDataPacket->ack_flag, &data_buffer[count], sizeof(pDataPacket->ack_flag));
    count += sizeof(pDataPacket->ack_flag);

    memcpy(&pDataPacket->sequence_flag, &data_buffer[count], sizeof(pDataPacket->sequence_flag));

    memcpy(&pDataPacket->clockwise, &data_buffer[count], sizeof(pDataPacket->clockwise));

    memcpy(&pDataPacket->counter_clockwise, &data_buffer[count], sizeof(pDataPacket->counter_clockwise));

    pDataPacket->data_flag = ntohs(pDataPacket->data_flag);
    pDataPacket->ack_flag = ntohs(pDataPacket->ack_flag);
    pDataPacket->sequence_flag = ntohs(pDataPacket->sequence_flag);
    pDataPacket->clockwise = ntohs(pDataPacket->clockwise);
    pDataPacket->counter_clockwise = ntohs(pDataPacket->counter_clockwise);

    return pDataPacket;
}


/**
 * Initiating the option struct.
 * @param opts Pointer to option struct.
 */
static void options_init(struct options *opts)
{
    // Dynamic memory
    memset(opts, 0, sizeof(struct options)); // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)

    // Default values for file descriptor standard input.
    opts->fd_in = STDIN_FILENO;

    // Default value for Default output port.
    opts->port_receiver = DEFAULT_PORT;
}

/**
 * Take in arguments from the command line.
 * @param argc number of arguments.
 * @param argv array of arguments.
 * @param opts custom struct option pointer.
 */
static void parse_arguments(int argc, char *argv[], struct options *opts)
{
    int c;

    // While valid option is passed.
    while((c = getopt(argc, argv, ":c:o:")) != -1)   // NOLINT(concurrency-mt-unsafe)
    {
        switch(c)
        {

            // For setting IP of car_controller.
            case 'c':
            {
                if (inet_addr(optarg) == ( in_addr_t)(-1)) {
                    options_process_close(-1);
                }
                printf("Listening on ip address: %s \n", optarg);
                opts->ip_client = optarg;
                break;
            }

            // For setting IP of car_motors.
            case 'o':
            {
                if (inet_addr(optarg) == ( in_addr_t)(-1)) {
                    options_process_close(-1);
                }
                printf("Sending to ip address: %s \n", optarg);
                opts->ip_receiver = optarg;
                break;
            }

            case ':':
            {
                fatal_message(__FILE__, __func__ , __LINE__, "\"Option requires an operand\"", 5); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
            }
            case '?':
            {
                fatal_message(__FILE__, __func__ , __LINE__, "\n\nUnknown Argument Passed: Please use from the following...\n'c' for setting car_controller IP.\n"
                                                             "'o' for setting output IP.\n"
                                                             "'p' for port (optional).", 6); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
            }
            default:
            {
                assert("should not get here");
            }
        }
    }

}

/**
 * Process the option struct with car_controller/car_motors information.
 * @param opts
 */
static void options_process(struct options *opts)
{

    // Only process if valid IP for car_controller is given.
    if(opts->ip_client)
    {
        errno = 0;

        // Special address types for holding car_controller and car_motors addresses.
        struct sockaddr_in addr;
        struct sockaddr_in to_addr;
        int bindResult;

        // Setting up the socket file descriptor.
        opts->fd_in = socket(AF_INET, SOCK_DGRAM, 0);

        // Error check creation of socket FD.
        options_process_close(opts->fd_in);

        // Setting up the car_controller address information for connecting over the network.
        addr.sin_family = AF_INET;
        addr.sin_port = htons(DEFAULT_PORT);
        // convert char dot notation of car_controller IP to network bytes.
        addr.sin_addr.s_addr = inet_addr(opts->ip_client);

        // check for error when getting network byte from Client IP.
        if(to_addr.sin_addr.s_addr ==  (in_addr_t)-1)
        {
            perror("inet_addr: Client Ip could not convert from dot notation to network bytes");
            options_process_close(-1);
        }

        // Setting wait time for data packet exchange.
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        // Setting socket options for initiated socket FD and its reception/send time.
        setsockopt(opts->fd_in, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                   sizeof timeout);
        setsockopt(opts->fd_in, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                   sizeof timeout);

        // Assigning address name to the socket FD.
        bindResult = bind(opts->fd_in, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

        // If socket FD to port binding fails, display error message and leave program.
        options_process_close(bindResult);


        // Setting address information for car_motors side.
        to_addr.sin_family = AF_INET;
        to_addr.sin_port = htons(opts->port_receiver);
        // convert char dot notation of destination IP to network bytes.
        to_addr.sin_addr.s_addr = inet_addr(opts->ip_receiver);

        // If car_motors IP could not be converted to network bytes, print error message and leave program.
        if(to_addr.sin_addr.s_addr ==  (in_addr_t)-1)
        {
            options_process_close(-1);
        }
        opts->server_addr = to_addr;
    }

}

/**
 * Close Socket FD.
 * @param opts pointer to option struct containing car_controller/car_motors address information.
 */
static void cleanup(const struct options *opts)
{
    if(opts->ip_client)
    {
        close(opts->fd_in);
    }
}