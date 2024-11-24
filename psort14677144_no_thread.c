#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // Necessário para memcpy
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>  // Este cabeçalho é necessário para ftruncate e outros
#include <err.h>  // Necessário para a função err

#define RECORD_SIZE 100 // Tamanho de cada registro

// Função para comparar registros com base na chave
int comparar_registros(const void *a, const void *b) {
    int chave_a = *(int *)a; // Extrai a chave do primeiro registro
    int chave_b = *(int *)b; // Extrai a chave do segundo registro
    return (chave_a - chave_b); // Retorna a diferença (ordem crescente)
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <entrada> <saída>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *arquivo_entrada = argv[1];  // Nome do arquivo de entrada
    const char *arquivo_saida = argv[2];   // Nome do arquivo de saída

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

    // Ordena os registros usando qsort
    qsort(registros, num_registros, RECORD_SIZE, comparar_registros);

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
