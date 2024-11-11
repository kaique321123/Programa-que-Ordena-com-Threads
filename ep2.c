#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#define RECORD_SIZE 100 // Tamanho do registro em bytes

typedef struct {
    int key;            // Chave do registro (4 bytes)
    char data[96];      // Dados do registro (96 bytes)
} Record;

typedef struct {
    Record* records;
    int start;
    int end;
    int thread_id;      // ID da thread
    pthread_mutex_t* mutex; // Mutex para sincronizar a fusão
} ThreadArgs;

int compare(const void* a, const void* b) {
    int keyA = ((Record*)a)->key;
    int keyB = ((Record*)b)->key;
    return (keyA > keyB) - (keyA < keyB);
}

void* sort_section(void* args) {
    ThreadArgs* targs = (ThreadArgs*) args;
    int n = targs->end - targs->start;

    // Ordena a seção
    qsort(targs->records + targs->start, n, sizeof(Record), compare);

    return NULL;
}

void merge_sections(Record* records, int n, int sections, int* section_sizes, pthread_mutex_t* mutex) {
    Record* temp = malloc(n * sizeof(Record)); // Array temporário para fusão
    if (temp == NULL) {
        perror("Erro ao alocar memória temporária para fusão");
        exit(EXIT_FAILURE);
    }

    int* indices = malloc(sections * sizeof(int)); // Índices atuais para cada seção
    if (indices == NULL) {
        perror("Erro ao alocar memória para índices");
        free(temp);
        exit(EXIT_FAILURE);
    }

    // Inicializa os índices de cada seção e calcula o ponto final de cada uma
    for (int i = 0; i < sections; i++) {
        indices[i] = (i == 0) ? 0 : indices[i - 1] + section_sizes[i - 1];
    }

    int total = 0;
    while (total < n) {
        int minIndex = -1;
        Record minRecord;

        // Encontra o menor registro entre as seções, respeitando o tamanho de cada seção
        for (int i = 0; i < sections; i++) {
            if (indices[i] < ((i == 0) ? section_sizes[i] : indices[i - 1] + section_sizes[i]) &&
                (minIndex == -1 || records[indices[i]].key < minRecord.key)) {
                minIndex = i;
                minRecord = records[indices[i]];
                }
        }

        if (minIndex != -1) {
            temp[total++] = minRecord;
            indices[minIndex]++;
        }
    }

    // Protege a fusão usando o mutex para evitar conflito de escrita
    pthread_mutex_lock(mutex);
    for (int i = 0; i < n; i++) {
        records[i] = temp[i];
    }
    pthread_mutex_unlock(mutex);

    free(temp);
    free(indices);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <entrada> <saída> <threads>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* input_file = argv[1];
    const char* output_file = argv[2];
    int num_threads = atoi(argv[3]);

    // Validar o número de threads
    if (num_threads < 1) {
        fprintf(stderr, "O número de threads deve ser pelo menos 1.\n");
        exit(EXIT_FAILURE);
    }

    int fd = open(input_file, O_RDONLY);
    if (fd < 0) {
        perror("Erro ao abrir arquivo de entrada");
        exit(EXIT_FAILURE);
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("Erro ao obter tamanho do arquivo");
        close(fd);
        exit(EXIT_FAILURE);
    }

    size_t file_size = sb.st_size;
    if (file_size == 0) {
        fprintf(stderr, "Arquivo de entrada está vazio.\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    int record_count = file_size / RECORD_SIZE;
    // Ajustar o número de threads se for maior que o número de registros
    if (num_threads > record_count) {
        num_threads = record_count;
    }

    Record* records = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);

    if (records == MAP_FAILED) {
        perror("Erro ao mapear arquivo");
        exit(EXIT_FAILURE);
    }

    pthread_t threads[num_threads];
    ThreadArgs args[num_threads];
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    int section_size = record_count / num_threads;
    int remainder = record_count % num_threads;

    // Distribuir os registros de forma justa entre as threads
    for (int i = 0; i < num_threads; i++) {
        args[i].records = records;

        // Calcular o início e o fim de cada thread com base na divisão
        args[i].start = i * section_size + (i < remainder ? i : remainder);

        // Ajustar o fim da última thread para cobrir todos os registros
        if (i == num_threads - 1) {
            args[i].end = record_count; // Garante que a última thread vá até o final
        } else {
            args[i].end = args[i].start + section_size + (i < remainder ? 1 : 0);
        }

        // Exibir o intervalo de registros para depuração
        printf("Thread %d: Ordenando registros de %d a %d (tamanho: %d registros)\n", i, args[i].start, args[i].end - 1, args[i].end - args[i].start);

        // Criação das threads
        if (pthread_create(&threads[i], NULL, sort_section, &args[i]) != 0) {
            perror("Erro ao criar thread");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    int* section_sizes = malloc(num_threads * sizeof(int));
    for (int i = 0; i < num_threads; i++) {
        section_sizes[i] = args[i].end - args[i].start;
    }

    // Chama merge_sections com section_sizes
    merge_sections(records, record_count, num_threads, section_sizes, &mutex);

    free(section_sizes);

    // Estrutura para armazenar o tempo
    struct timeval start_time, end_time;

    // Começando a contagem do tempo
    gettimeofday(&start_time, NULL);

    int out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (out_fd < 0) {
        perror("Erro ao abrir arquivo de saída");
        munmap(records, file_size);
        exit(EXIT_FAILURE);
    }

    if (write(out_fd, records, file_size) != file_size) {
        perror("Erro ao escrever no arquivo de saída");
    }
    fsync(out_fd);
    close(out_fd);
    munmap(records, file_size);

    // Terminando a contagem do tempo
    gettimeofday(&end_time, NULL);

    // Calculando o tempo gasto em segundos
    double time_taken = (end_time.tv_sec - start_time.tv_sec) +
                        (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
    printf("Tempo de execução: %f segundos\n", time_taken);

    return 0;
}
