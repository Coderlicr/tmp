#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>  // Include this header for malloc and free

#define IMAGE_BUFFER "/dev/top_module"  // Example path, adjust based on your setup
#define IMAGE_WIDTH 640
#define IMAGE_HEIGHT 480
#define IMAGE_SIZE (IMAGE_WIDTH * IMAGE_HEIGHT * 2) // Assuming RGB565 format

void save_image(const char *filename, uint8_t *data, size_t size) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file for writing");
        return;
    }
    fwrite(data, 1, size, file);
    fclose(file);
}

int main() {
    int buffer_fd = open(IMAGE_BUFFER, O_RDONLY);
    if (buffer_fd < 0) {
        perror("Failed to open image buffer");
        return 1;
    }

    uint8_t *image_data = malloc(IMAGE_SIZE);
    if (!image_data) {
        perror("Failed to allocate memory for image data");
        close(buffer_fd);
        return 1;
    }

    // Read image data from the buffer
    ssize_t bytes_read = read(buffer_fd, image_data, IMAGE_SIZE);
    if (bytes_read < 0) {
        perror("Failed to read image data");
        free(image_data);
        close(buffer_fd);
        return 1;
    }

    if (bytes_read != IMAGE_SIZE) {
        fprintf(stderr, "Unexpected image size: expected %d bytes, got %zd bytes\n", IMAGE_SIZE, bytes_read);
        free(image_data);
        close(buffer_fd);
        return 1;
    }

    save_image("captured_image.raw", image_data, bytes_read);

    free(image_data);
    close(buffer_fd);

    printf("Image saved as captured_image.raw\n");
    return 0;
}
