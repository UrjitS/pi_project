#include "conversion.h"
#include "error.h"
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <wiringPi.h>

#define MotorPin1       0
#define MotorPin2       2
#define MotorEnable     3
#define BUF_LEN 1024
#define DEFAULT_PORT 5020


struct options
{
    char *ip_server;
    in_port_t server_port;
    int fd_in;
};
struct server_information
{
    char * struct_message_data;
    ssize_t bytes_read_from_socket;
    struct sockaddr from_addr;
    char * previous_message;
    int previous_sequence_number;
};


struct data_packet {
    int data_flag;
    int ack_flag;
    int sequence_flag;
    int clockwise;
    int counter_clockwise;
    char *data;
};

static volatile sig_atomic_t running;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static struct data_packet *dp_deserialize(ssize_t nRead, char * data_buffer);
static void read_bytes(int fd, struct server_information *serverInformation);
static void send_ack_packet(const struct data_packet * dataPacket, struct sockaddr * from_addr, int fd);
static void options_init(struct options *opts, struct server_information *serverInformation);
static void parse_arguments(int argc, char *argv[], struct options *opts);
static void options_process(struct options *opts);
static void cleanup(const struct options *opts, struct server_information *serverInformation);
static void process_packet(const struct data_packet * dataPacket, struct server_information * serverInformation);
static uint8_t *dp_serialize(const struct data_packet *ackPacket, size_t *size);
static void write_bytes(int fd, const uint8_t *bytes, size_t size, struct sockaddr_in server_addr);
static void options_process_close(int result_number);

int main(int argc, char *argv[])
{
    struct options opts;
    struct server_information serverInformation;
    struct data_packet *dataPacket;

    options_init(&opts, &serverInformation);
    parse_arguments(argc, argv, &opts);
    options_process(&opts);

    // If server IP is given, run loop to listen to self.
    if(opts.ip_server)
    {
        if (wiringPiSetup() == -1) {
            printf("WiringPi failed \n");
            return EXIT_FAILURE;
        }

        pinMode(MotorPin1, OUTPUT);
        pinMode(MotorPin2, OUTPUT);
        pinMode(MotorEnable, OUTPUT);

        running = 1;

        // Continues loop to keep listening to self.
        while(running)
        {
            read_bytes(opts.fd_in, &serverInformation);
            dataPacket = dp_deserialize(serverInformation.bytes_read_from_socket, serverInformation.struct_message_data);
            process_packet(dataPacket, &serverInformation);
            send_ack_packet(dataPacket, &serverInformation.from_addr, opts.fd_in);
        }
    }
    cleanup(&opts, &serverInformation);
    return EXIT_SUCCESS;
}

/**
 * Process Packet once it has been deserialized.
 * @param dataPacket Data packet deserialized and sent from another machine.
 * @param serverInformation Pointer to struct for server side information.
 */
static void process_packet(const struct data_packet * dataPacket, struct server_information * serverInformation) {
    printf("Processing packet \n");

    // Confirm it is a new packet to be processed before processing.
    if (dataPacket->data_flag && !dataPacket->ack_flag) {
        if (serverInformation->previous_sequence_number != dataPacket->sequence_flag) {
            // Update server side information.
            serverInformation->previous_sequence_number = dataPacket->sequence_flag;
            serverInformation->previous_message = malloc(strlen(dataPacket->data));

            // Update previous message sent by the other machine.
            memmove(serverInformation->previous_message, dataPacket->data, strlen(dataPacket->data));
            printf("Data Flag: %d \n", dataPacket->data_flag);
            printf("Ack: %d \n", dataPacket->ack_flag);
            printf("Seq: %d \n", dataPacket->sequence_flag);
            printf("Clock: %d \n", dataPacket->clockwise);
            printf("CClock: %d \n", dataPacket->counter_clockwise);
            printf("Data: %s \n", dataPacket->data);

            if (dataPacket->clockwise == 1 && dataPacket->counter_clockwise == 0) {
                printf("Clockwise\n");
                digitalWrite(MotorEnable, HIGH);
                digitalWrite(MotorPin1, HIGH);
                digitalWrite(MotorPin2, LOW);
                sleep(1);
            }

            if (dataPacket->counter_clockwise && dataPacket->clockwise == 0) {
                printf("Anti-clockwise\n");
                digitalWrite(MotorEnable, HIGH);
                digitalWrite(MotorPin1, LOW);
                digitalWrite(MotorPin2, HIGH);
                sleep(1);
            }
            if (dataPacket->clockwise == 0 && dataPacket->counter_clockwise == 0) {
                digitalWrite(MotorEnable, LOW);
            }

        }
    }

}

/**
 * Send ACK to other machine to confirm their data packet was delivered.
 * @param dataPacket Data packet that was received.
 * @param from_addr The client's IP address.
 * @param fd Socket FD.
 */
static void send_ack_packet(const struct data_packet * dataPacket, struct sockaddr * from_addr, int fd) {
    uint8_t * bytes;
    size_t size;
    // Send Ack back to the server
    struct data_packet acknowledgement_packet;
    memset(&acknowledgement_packet, 0, sizeof(struct data_packet)); // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    // Construct acknowledgement packet before sending
    // Data flag set to 0
    acknowledgement_packet.data_flag = 0;
    // Ack flag set to 1
    acknowledgement_packet.ack_flag = 1;
    // Alternate sequence number
    acknowledgement_packet.sequence_flag = dataPacket->sequence_flag;

    acknowledgement_packet.clockwise = -1;
    acknowledgement_packet.counter_clockwise = -1;

    acknowledgement_packet.data = malloc(strlen(dataPacket->data));
    acknowledgement_packet.data = dataPacket->data;

    // Serialize
    bytes = dp_serialize(&acknowledgement_packet, &size);

    // Send Ack
    struct sockaddr_in *addr_in = (struct sockaddr_in *)from_addr;
    struct sockaddr_in to_addr;
    to_addr.sin_family = AF_INET;
    to_addr.sin_port = addr_in->sin_port;
    to_addr.sin_addr.s_addr = inet_addr(inet_ntoa(addr_in->sin_addr));

    // Write to Socket FD to send packet.
    write_bytes(fd, bytes, size, to_addr);
}

/**
 * Read data sent from another machine.
 * @param fd the Socket FD.
 * @param serverInformation Struct for holding serialized data and client information.
 */
static void read_bytes(int fd, struct server_information * serverInformation)
{
    struct sockaddr from_addr;
    char data[BUF_LEN];
    ssize_t nRead;
    socklen_t from_addr_len;

    from_addr_len = sizeof (struct sockaddr);
    nRead = recvfrom(fd, data, BUF_LEN, 0, &from_addr, &from_addr_len);

    if(nRead == -1)
    {
        printf("Could not read from socket");
        return;
    }
    serverInformation->bytes_read_from_socket = nRead;
    serverInformation->from_addr = from_addr;
    serverInformation->struct_message_data = malloc(strlen(data));
    serverInformation->struct_message_data = data;
}

/**
 * Write to Socket FD to send data to a different machine.
 * @param fd Socket FD.
 * @param bytes buffer to send.
 * @param size Number of bytes.
 * @param server_addr Server address.
 */
static void write_bytes(int fd, const uint8_t *bytes, size_t size, struct sockaddr_in server_addr)
{

    ssize_t nWrote;

    nWrote = sendto(fd, bytes, size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(nWrote == -1)
    {
        printf("Could not write to socket");
        return;
    }

    printf("Sent ack\n");
    printf("\n");
    digitalWrite(MotorEnable, LOW);
}

/**
 * Deserialize data sent from another machine.
 * @param nRead Number of bytes.
 * @param data_buffer Buffer for the data.
 * @return Data packet that has been deserialized.
 */
static struct data_packet *dp_deserialize(ssize_t nRead, char * data_buffer)
{
    struct data_packet * x = malloc(sizeof(struct data_packet));
    size_t count;
    size_t len;

    memset(x, 0, sizeof(struct data_packet));
    count = 0;

    memcpy(&x->data_flag, &data_buffer[count], sizeof(x->data_flag));
    count += sizeof(x->data_flag);

    memcpy(&x->ack_flag, &data_buffer[count], sizeof(x->ack_flag));
    count += sizeof(x->ack_flag);

    memcpy(&x->sequence_flag, &data_buffer[count], sizeof(x->sequence_flag));
    count += sizeof(x->sequence_flag);

    memcpy(&x->clockwise, &data_buffer[count], sizeof(x->clockwise));
    count += sizeof(x->clockwise);

    memcpy(&x->counter_clockwise, &data_buffer[count], sizeof(x->counter_clockwise));
    count += sizeof(x->counter_clockwise);

    x->data_flag = ntohs(x->data_flag);
    x->ack_flag = ntohs(x->ack_flag);
    x->sequence_flag = ntohs(x->sequence_flag);
    x->clockwise = ntohs(x->clockwise);
    x->counter_clockwise = ntohs(x->counter_clockwise);

    len = nRead - count;

    x->data = malloc((len+1));
    memcpy(x->data, &data_buffer[count], len);
    x->data[len] = '\0';

    return x;
}

/**
 * Serialize data packet to be sent to another machine.
 * @param ackPacket ACK packet to send pack to client.
 * @param size size of serialized information.
 * @return Serialized data packet.
 */
static uint8_t *dp_serialize(const struct data_packet *ackPacket, size_t *size)
{
    uint8_t *bytes;
    size_t count;
    size_t len;
    int data_flag_number;
    int ack_flag_number;
    int sequence_flag_number;
    int clockwise;
    int counter_clockwise;

    len = strlen(ackPacket->data);

    *size = sizeof(ackPacket->data_flag) + sizeof(ackPacket->ack_flag) + sizeof(ackPacket->sequence_flag) + sizeof(ackPacket->clockwise) + sizeof(ackPacket->counter_clockwise) + len;
    bytes = malloc(*size);
    // Make network byte order
    data_flag_number = htons(ackPacket->data_flag);
    ack_flag_number = htons(ackPacket->ack_flag);
    sequence_flag_number = htons(ackPacket->sequence_flag);
    clockwise = htons(ackPacket->clockwise);
    counter_clockwise = htons(ackPacket->counter_clockwise);

    count = 0;

    // Write serialized data.
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

    memcpy(&bytes[count], ackPacket->data, len);

    return bytes;
}

/**
 * Initiate option and server information structs.
 * @param opts pointer to option struct.
 * @param serverInformation  pointer to server information struct.
 */
static void options_init(struct options *opts, struct server_information *serverInformation)
{
    //Dynamic memory for option ans server information structs.
    memset(opts, 0, sizeof(struct options)); // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    memset(serverInformation, 0, sizeof(struct server_information)); // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    serverInformation->previous_sequence_number = 1;
    char d_text[4] = "null";
//    serverInformation->previous_message = malloc(strlen(d_text));
    serverInformation->previous_message = d_text;
    opts->fd_in       = STDIN_FILENO;
    opts->server_port     = DEFAULT_PORT;
}

/**
 * Take in arguments from command line when running program.
 * @param argc Number of arguments.
 * @param argv Argument.
 * @param opts Pointer to option struct.
 */
static void parse_arguments(int argc, char *argv[], struct options *opts)
{
    int c;

    while((c = getopt(argc, argv, ":i:p:")) != -1)   // NOLINT(concurrency-mt-unsafe)
    {
        switch(c)
        {
            case 'i':
            {
                if (inet_addr(optarg) == ( in_addr_t)(-1)) {
                    options_process_close(-1);
                }
                printf("Listening on ip address: %s \n", optarg);
                opts->ip_server = optarg;
                break;
            }
            case 'p':
            {
                printf("Running on port: %s \n", optarg);
                opts->server_port = parse_port(optarg, 10); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                break;
            }
            case ':':
            {
                fatal_message(__FILE__, __func__ , __LINE__, "\"Option requires an operand\"", 5); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
            }
            case '?':
            {
                fatal_message(__FILE__, __func__ , __LINE__, "\n\nUnknown Argument Passed: Please use from the following...\n'c' for setting client IP.\n"
                                                             "'i' for setting server IP.\n"
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
 * Process option struct for network information.
 * @param opts Pointer to the option struct initialized.
 */
static void options_process(struct options *opts)
{

    if(opts->ip_server)
    {
        struct sockaddr_in addr;

        int result;
        int option;

        opts->fd_in = socket(AF_INET, SOCK_DGRAM, 0);

        options_process_close(opts->fd_in);

        addr.sin_family = AF_INET;
        addr.sin_port = htons(opts->server_port);
        addr.sin_addr.s_addr = inet_addr(opts->ip_server);

        options_process_close(addr.sin_addr.s_addr == (in_addr_t) -1);

        option = 1;

        setsockopt(opts->fd_in, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

        result = bind(opts->fd_in, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

        options_process_close(result);
    }
}

/**
 * Error handling for not option cannot be processed.
 * @param result_number
 */
static void options_process_close(int result_number) {
    if(result_number == -1)
    {
        printf("Could not process options\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * Clear memory for end of program.
 * @param opts Option struct for holding network information, close socket.
 * @param serverInformation Free memory for data still being held.
 */
static void cleanup(const struct options *opts, struct server_information *serverInformation)
{
    if(opts->ip_server)
    {
        close(opts->fd_in);
    }
    free(serverInformation->struct_message_data);
}
