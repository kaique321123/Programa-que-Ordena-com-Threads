#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>

// Definição do tamanho de cada registro no arquivo
#define TAMANHO_REGISTRO 100
#define TAMANHO_CHAVE sizeof(int) // Tamanho da chave (4 bytes)

// Estrutura para passar dados para as threads
typedef struct {
    void *array;              // Ponteiro para a partição do array a ser ordenado
    size_t num_registros;     // Número de registros na partição
    size_t tamanho_registro;  // Tamanho de cada registro
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
    qsort(dados->array, dados->num_registros, dados->tamanho_registro, dados->cmp); // Ordena a partição
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
    // Copia o resultado final de volta para o array original
    memcpy(array, espaco_trabalho, total_qtd * tamanho_registro);

    free(espaco_trabalho); // Libera o espaço de trabalho
}

// Função principal para ordenar o array em paralelo
void ordenar_em_paralelo(void *array, size_t num_registros, size_t tamanho_registro, 
                         int (*cmp)(const void *, const void *), int num_threads) {
    if (num_threads > num_registros) {
        num_threads = num_registros; // Garante que o número de threads não seja maior que os registros
    }

    size_t tamanho_particao = num_registros / num_threads; // Tamanho básico de cada partição
    size_t restante = num_registros % num_threads; // Registros extras a distribuir

    pthread_t threads[num_threads];           // array de threads
    DadosThread dados[num_threads];           // Dados para cada thread

    char *inicio = array;                     // Ponteiro para o início do array
    size_t offset = 0;                        // Controle da próxima partição

    // Cria as threads para ordenar cada partição
    for (int i = 0; i < num_threads; i++) {
        size_t tamanho_atual = tamanho_particao + (restante > 0 ? 1 : 0);
        restante--;

        dados[i].array = (char *)inicio + offset * tamanho_registro;  // Corrige o cálculo do ponteiro
        dados[i].num_registros = tamanho_atual;
        dados[i].tamanho_registro = tamanho_registro;
        dados[i].cmp = cmp;

        pthread_create(&threads[i], NULL, ordenar_particao, &dados[i]);

        offset += tamanho_atual;
    }

    // Aguarda a conclusão de todas as threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Mescla as partições iterativamente
    size_t step = 1;
    while (step < num_threads) {
        for (int i = 0; i + step < num_threads; i += 2 * step) {
            char *esquerda = dados[i].array;
            char *direita = dados[i + step].array;

            size_t qtd_esquerda = dados[i].num_registros;
            size_t qtd_direita = dados[i + step].num_registros;

            merge(esquerda, qtd_esquerda, qtd_direita, tamanho_registro, cmp);

            // Atualiza o tamanho da partição mesclada
            dados[i].num_registros = qtd_esquerda + qtd_direita;
        }
        step *= 2;
    }
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        // Verifica se os argumentos foram passados corretamente
        fprintf(stderr, "Uso: %s <entrada> <saída> <threads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *arquivo_entrada = argv[1];  // Nome do arquivo de entrada
    const char *arquivo_saida = argv[2];    // Nome do arquivo de saída
    int num_threads = atoi(argv[3]);        // Número de threads especificado pelo usuário

    // Abrir arquivo de entrada
    int fd_entrada = open(arquivo_entrada, O_RDONLY);
    if (fd_entrada < 0) err(EXIT_FAILURE, "Erro ao abrir o arquivo de entrada");

    // Obtém o tamanho do arquivo
    off_t tamanho_arquivo = lseek(fd_entrada, 0, SEEK_END);
    lseek(fd_entrada, 0, SEEK_SET);

    // Calcula o número de registros no arquivo
    size_t num_registros = tamanho_arquivo / TAMANHO_REGISTRO;

    // Mapeia o arquivo de entrada para a memória
    void *registros = mmap(NULL, tamanho_arquivo, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd_entrada, 0);
    if (registros == MAP_FAILED) err(EXIT_FAILURE, "Erro ao mapear o arquivo de entrada");

    // Se o tamanho do arquivo estiver entre 50MB e 70MB, ajusta o número de threads para 1
    if (tamanho_arquivo >= 50 * 1024 * 1024 && tamanho_arquivo <= 70 * 1024 * 1024) {
        num_threads = 1;
        printf("Arquivo entre 50MB e 70MB. Número de threads ajustado para 1.\n");
    } else if (num_threads == 0 || num_threads > 8) {
        num_threads = 8;
        printf("Número de threads ajustado automaticamente para %d.\n", num_threads);
    }

    // Ordena os registros
    ordenar_em_paralelo(registros, num_registros, TAMANHO_REGISTRO, comparar_registros, num_threads);

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
