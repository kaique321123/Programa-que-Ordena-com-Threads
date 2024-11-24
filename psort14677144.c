#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>

// Definição do tamanho de cada registro no arquivo
#define RECORD_SIZE 100
#define KEY_SIZE sizeof(int) // Tamanho da chave (4 bytes)

// Estrutura para passar dados para as threads
typedef struct {
    void *array;              // Ponteiro para a partição do vetor a ser ordenada
    size_t num_records;       // Número de registros na partição
    size_t record_size;       // Tamanho de cada registro
    int (*cmp)(const void *, const void *); // Função de comparação
} DadosThread;

// Função para comparar registros com base na chave
int comparar_registros(const void *a, const void *b) {
    int chave_a = *(int *)a; // Extrai a chave do primeiro registro
    int chave_b = *(int *)b; // Extrai a chave do segundo registro
    return (chave_a - chave_b); // Retorna a diferença (ordem crescente)
}

// Função executada por cada thread para ordenar uma partição usando qsort
void *ordenar_particao(void *arg) {
    DadosThread *dados = (DadosThread *)arg; // Obtém os dados da partição
    qsort(dados->array, dados->num_records, dados->record_size, dados->cmp); // Ordena a partição
    return NULL;
}

// Função para juntar duas partições já ordenadas
void merge(void *array, size_t esquerda_qtd, size_t direita_qtd, size_t tamanho_registro, int (*cmp)(const void *, const void *)) {
    size_t total_qtd = esquerda_qtd + direita_qtd; // Total de registros que falta juntar
    char *espaco_trabalho = malloc(total_qtd * tamanho_registro); // Aloca espaço para o resultado
    if (!espaco_trabalho) err(EXIT_FAILURE, "Erro ao alocar memória para mesclagem");

    char *esquerda = array; // Início da partição esquerda
    char *direita = esquerda + esquerda_qtd * tamanho_registro; // Início da partição direita
    char *destino = espaco_trabalho; // Ponteiro para o espaço de trabalho

    // Compara e copia registros das duas partições para o espaço de trabalho
    while (esquerda_qtd > 0 && direita_qtd > 0) {
        if (cmp(esquerda, direita) <= 0) {
            memcpy(destino, esquerda, tamanho_registro); // Copia o menor registro
            esquerda += tamanho_registro;
            esquerda_qtd--;
        } else {
            memcpy(destino, direita, tamanho_registro);
            direita += tamanho_registro;
            direita_qtd--;
        }
        destino += tamanho_registro;
    }

    // Copia os registros restantes da partição esquerda (se houver)
    memcpy(destino, esquerda, esquerda_qtd * tamanho_registro);
    // Copia os registros restantes da partição direita (se houver)
    memcpy(destino + esquerda_qtd * tamanho_registro, direita, direita_qtd * tamanho_registro);
    // Copia o resultado final de volta para o vetor original
    memcpy(array, espaco_trabalho, total_qtd * tamanho_registro);

    free(espaco_trabalho); // Libera o espaço de trabalho
}

// Função principal para ordenar o vetor em paralelo
void ordenar_em_paralelo(void *array, size_t num_registros, size_t tamanho_registro, int (*cmp)(const void *, const void *), int num_threads) {
    if (num_threads <= 1) {
        // Se apenas uma thread, usa qsort diretamente
        qsort(array, num_registros, tamanho_registro, cmp);
        return;
    }

    size_t meio = num_registros / 2; // Divide o vetor ao meio
    char *segunda_metade = (char *)array + meio * tamanho_registro; // Início da segunda metade

    pthread_t thread; // Thread para ordenar a segunda metade
    DadosThread dados = {segunda_metade, num_registros - meio, tamanho_registro, cmp}; // Dados para a thread

    pthread_create(&thread, NULL, ordenar_particao, &dados); // Cria a thread
    qsort(array, meio, tamanho_registro, cmp); // Ordena a primeira metade na thread principal

    pthread_join(thread, NULL); // Aguarda a conclusão da thread
    merge(array, meio, num_registros - meio, tamanho_registro, cmp); // Mescla as duas partes ordenadas
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        // Verifica se os argumentos foram passados corretamente
        fprintf(stderr, "Uso: %s <entrada> <saída> <threads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *arquivo_entrada = argv[1];  // Nome do arquivo de entrada
    const char *arquivo_saida = argv[2];   // Nome do arquivo de saída
    int num_threads = atoi(argv[3]);       // Número de threads especificado pelo usuário

    // Abrir arquivo de entrada
    int fd_entrada = open(arquivo_entrada, O_RDONLY);
    if (fd_entrada < 0) err(EXIT_FAILURE, "Erro ao abrir o arquivo de entrada");

    // Obtém o tamanho do arquivo
    off_t tamanho_arquivo = lseek(fd_entrada, 0, SEEK_END);
    lseek(fd_entrada, 0, SEEK_SET);

    // Calcula o número de registros no arquivo
    size_t num_registros = tamanho_arquivo / RECORD_SIZE;

    // Mapeia o arquivo de entrada para a memória
    void *registros = mmap(NULL, tamanho_arquivo, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd_entrada, 0);
    if (registros == MAP_FAILED) err(EXIT_FAILURE, "Erro ao mapear o arquivo de entrada");

    // Ajusta o número de threads se for 0
    if (num_threads == 0) {
        num_threads = num_registros; // Cada registro será processado por uma thread
        printf("Número de threads ajustado automaticamente para %d (baseado no número de registros)\n", num_threads);
    }

    // Ordena os registros
    ordenar_em_paralelo(registros, num_registros, RECORD_SIZE, comparar_registros, num_threads);

    // Escrever o arquivo de saída
    int fd_saida = open(arquivo_saida, O_CREAT | O_RDWR, 0644);
    if (fd_saida < 0) err(EXIT_FAILURE, "Erro ao criar o arquivo de saída");

    // Define o tamanho do arquivo de saída
    if (ftruncate(fd_saida, tamanho_arquivo) < 0) err(EXIT_FAILURE, "Erro ao redimensionar o arquivo de saída");

    // Mapeia o arquivo de saída para a memória
    void *mapa_saida = mmap(NULL, tamanho_arquivo, PROT_WRITE, MAP_SHARED, fd_saida, 0);
    if (mapa_saida == MAP_FAILED) err(EXIT_FAILURE, "Erro ao mapear o arquivo de saída");

    // Copia os dados ordenados para o arquivo de saída
    memcpy(mapa_saida, registros, tamanho_arquivo);
    msync(mapa_saida, tamanho_arquivo, MS_SYNC); // Garante que os dados sejam gravados no disco

    // Desmapeia e fecha os arquivos
    munmap(mapa_saida, tamanho_arquivo);
    close(fd_saida);

    munmap(registros, tamanho_arquivo);
    close(fd_entrada);

    return EXIT_SUCCESS;
}
