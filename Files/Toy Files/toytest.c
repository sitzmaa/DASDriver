#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#define TOY "/dev/toy"
#define TOYIOCTL _IO('Y',0)

int main() {
    int fd;
    char buffer[100];
    
    // Open the device file
    fd = open(TOY, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device file");
        return -1;
    }
    
    // Perform read operation
    if (read(fd, buffer, sizeof(buffer)) < 0) {
        perror("Read failed");
        close(fd);
        return -1;
    }
    printf("Read data: %s\n", buffer);
    
    // reset the buffer
    for (int i = 0; i < 30; i++) {
        buffer[i] = '\0';
    }
    // Perform write operation
    /* resets the reads*/
    write(fd, buffer, sizeof(buffer));

    int i = 1, rv;
    while((rv = read(fd, buffer, i)) == i) {
        if (rv <= 0) {
            perror("Read failed");
            close(fd);
            return -1;
        }
        printf("%s\n", buffer);
        // reset buffer
        for (int j = 0; j < i; j++) {
            buffer[j] = '\0';
        }
    }

    ioctl(fd, TOYIOCTL);

    
    // Close the toy driver
    close(fd);
    
    return 0;
}
