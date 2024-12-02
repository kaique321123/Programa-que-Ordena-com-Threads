#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define RECORD_SIZE 100

struct registro {
    int key;
    unsigned char data[RECORD_SIZE - 4];
};

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("modo de uso:\n");
        printf("\t%s <nome do arquivo>\n", argv[0]);
        return -1;
    }
    char *nomearq = argv[1];
    struct stat file_info;

    int fd = open(nomearq, O_RDONLY);
    if (fd == -1) {
        perror("Nao foi possivel abrir o arquivo");
        exit(-1);
    }

    if (fstat(fd, &file_info) == -1) {
        close(fd);
        perror("Erro no fstat.");
        exit(-1);
    }

    off_t tamanho_b = file_info.st_size;
    double tamanho_k = 100.0 * tamanho_b;
    double tamanho_m = tamanho_k / 1024;
    double tamanho_g = tamanho_m / 1024;

    printf("Arquivo: %s\n", nomearq);
    printf("Tamanho: %ld bytes (%.2f KB, %.2f MB, %.2f GB)\n", tamanho_b, tamanho_k, tamanho_m, tamanho_g);

    if (tamanho_b % RECORD_SIZE != 0) {
        printf("Problema: tamanho do arquivo nao eh multiplo do tamanho do registro\n");
    }

    struct registro buf;
    int oldkey;
    int read_bytes;

    read_bytes = read(fd, &buf, RECORD_SIZE);
    if (read_bytes < RECORD_SIZE) {
        printf("Ha menos de um registro no arquivo\n");
    }
    oldkey = buf.key;
    read_bytes = read(fd, &buf, RECORD_SIZE);

    while (read_bytes == 100) {
        if (buf.key < oldkey) {
            printf("Ordem incorreta detectada, chaves %d %d\n", oldkey, buf.key);
        }

        oldkey = buf.key;
        read_bytes = read(fd, &buf, RECORD_SIZE);
    }

    close(fd);

    return 0;
}
