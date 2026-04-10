/*
 * Userspace program that communicates with the vga_ball device driver
 * through ioctls
 *
 * Stephen A. Edwards
 * Columbia University
 */

#include <stdio.h>
#include <stdlib.h>
#include "top_module.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#define VGA_BALL_MAX_X 239
#define VGA_BALL_MAX_Y 239
#define VGA_BALL_MIN_X 15
#define VGA_BALL_MIN_Y 15

#define SERVER_IP "127.0.0.1"
#define PORT 9999

#define BUFFER_SIZE 640*480 + 1 
#define READ_PIXELS 640*480

int vga_ball_fd;
int cam_fd;
int sockfd;

/* Read and print the background color */
void read_vga_ball()
{
    VGA_BALL_ARG vla;

    if (ioctl(vga_ball_fd, VGA_BALL_READ, &vla))
    {
        perror("ioctl(VGA_BALL_READ) failed");
        return;
    }
    printf("Ball position: %d %d\n", vla.position.x, vla.position.y);
    }

/* Set the background color */
void set_vga_ball(const VGA_BALL_ARG *arg)
{
    VGA_BALL_ARG vla = *arg;

    
    if (ioctl(vga_ball_fd, VGA_BALL_WRITE, &vla))
    {
        perror("ioctl(VGA_BALL_SET_BACKGROUND) failed");
        return;
    }
}
void read_frame(char* buf)
{
    unsigned int info;
    unsigned int hcount, vcount;
    unsigned int address;
    unsigned char pixel;

    while (1) {
        if (ioctl(cam_fd, CAMERA_READ, &info)) {
            perror("ioctl(CAMERA_READ) failed");
            return;
        }

        // Extract fields from the info
        hcount = (info >> 29) & 0x3FF;      // Extract bits 29-38 (10 bits for hcount)
        vcount = (info >> 19) & 0x3FF;      // Extract bits 19-28 (10 bits for vcount)
        pixel = info & 0xFF;                // Extract bits 0-7 for the pixel value (dout)

        // Calculate the address (assuming a raster scan order)
        address = vcount * 640 + hcount;

        if (address < READ_PIXELS) {
            buf[address] = pixel;
        }

        // Check for end of field
        if (info & (1 << 18)) {
            break; // Exit the loop when end of field is encountered
        }
    }
}


void send_frame(const char* buffer) {
    int bytes_sent = 0;
    int total_bytes = BUFFER_SIZE;

    while (bytes_sent < total_bytes) {
        int n = send(sockfd, buffer + bytes_sent, total_bytes - bytes_sent, 0);
        if (n < 0) {
            perror("send failed");
            exit(EXIT_FAILURE);
        }
        bytes_sent += n;
    }
    printf("Frame sent. Bytes sent: %d\n", bytes_sent);
}

int main()
{
    VGA_BALL_ARG vla;
    int i, j;

    static const char filename[] = "/dev/top_module";
    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE] = {0};

    // Initialize TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    // Filling server information
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("VGA ball Userspace program started\n");

    if ((vga_ball_fd = open(filename, O_RDWR)) == -1)
    {
        printf("here 1");
        fprintf(stderr, "could not open %s\n", filename);
        close(sockfd);
        return -1;
    }

    printf("initial state: ");
    read_vga_ball();
    i = 0;
    vla.position.x = 100;
    vla.position.y = 20;
    int x_dir = 0;
    int y_dir = 1;
    set_vga_ball(&vla);
    read_vga_ball();

    while (1)
    {
        // adjust ball settings
        if (x_dir)
        {
            vla.position.x = (vla.position.x - 1 + VGA_BALL_MAX_X) % VGA_BALL_MAX_X;
            if (vla.position.x == VGA_BALL_MIN_X)
                x_dir = 0;
        }
        else
        {
            vla.position.x = (vla.position.x + 1) % VGA_BALL_MAX_X;
            if (vla.position.x == VGA_BALL_MAX_X - 1)
                x_dir = 1;
        }
        if (y_dir)
        {
            vla.position.y = (vla.position.y - 1 + VGA_BALL_MAX_Y) % VGA_BALL_MAX_Y;
            if (vla.position.y == VGA_BALL_MIN_Y)
                y_dir = 0;
        }
        else
        {
            vla.position.y = (vla.position.y + 1) % VGA_BALL_MAX_Y;
            if (vla.position.y == VGA_BALL_MAX_Y - 1)
                y_dir = 1;
        }

        // system call
        read_vga_ball();
        set_vga_ball(&vla);

        read_frame(buffer);
        send_frame(buffer);

        usleep(200000);  // Adjust sleep time for your application's needs

        // increment
        i++;
    }

    // Cleanup
    close(cam_fd);
    close(vga_ball_fd);
    close(sockfd);
    printf("VGA BALL Userspace program terminating\n");
    return 0;
}
