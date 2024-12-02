# Ordenação Paralela de Registros

Este projeto implementa um programa em C para ordenação paralela de registros utilizando `pthreads`. A aplicação divide os dados de entrada entre múltiplas threads, ordena cada parte em paralelo e combina os resultados ordenados. A abordagem explora conceitos de concorrência para melhorar o desempenho em grandes volumes de dados.

---

## Decisões de Projeto

1. **Escolha Inicial do Algoritmo: Quick Sort**  
   - Inicialmente, utilizamos o Quick Sort pela sua eficiência média de O(n log n). No entanto, problemas como divisão desigual de dados e dificuldades de reorganização levaram à substituição do algoritmo.

2. **Mudança para Merge Sort**  
   - O Merge Sort foi escolhido devido à sua melhor adequação para paralelização. Sua estrutura recursiva e estabilidade garantiram uma implementação mais previsível e eficiente.

3. **Distribuição de Tarefas**  
   - Arquivos de entrada foram divididos proporcionalmente ao número de threads, com cada thread responsável por ordenar uma parte do vetor. Isso minimizou condições de corrida.

4. **Eliminação do Uso de Mutex**  
   - O design evitou acesso simultâneo ao vetor, removendo a necessidade de mutex e reduzindo a sobrecarga associada.

---

## Método de Medição

- **Execução com diferentes números de threads**: Testes realizados com 1 a 8 threads, em arquivos de diferentes tamanhos (10MB a 100MB).
- **Comparação com e sem threads**: Avaliou-se o impacto da paralelização no desempenho.
- **Repetição de medições**: Cada configuração foi executada 5 vezes para garantir confiabilidade estatística.
- **Ferramentas**:
  - **Python**: Automação de testes e geração de gráficos.
  - **C**: Implementação da lógica de ordenação com e sem threads.

---

## Resultados

1. **Impacto do Número de Threads**  
   - Ganhos significativos de desempenho com mais threads, estabilizando entre 4 a 6 threads.
   - Arquivos grandes (>64MB) mostraram maior impacto da paralelização.

2. **Comparação com e sem Threads**  
   - Arquivos pequenos (<10MB): Diferença marginal no desempenho.  
   - Arquivos grandes (100MB): Melhora acentuada com múltiplas threads.

3. **Lei de Amdahl**  
   - A análise confirmou um ponto de saturação, onde o aumento de threads não traz ganhos adicionais devido a partes não paralelizáveis do programa.

4. **Gráficos de Desempenho**  
   - Estão disponíveis para diferentes tamanhos de arquivos, destacando o impacto do número de threads.

![10MB Comparativo](IMAGENS_COMPARANDO_ORDENAÇÃO_COM_THREADS_E_SEM_THREADS/10mb_comparativo.png)  
![100MB Comparativo](IMAGENS_COMPARANDO_ORDENAÇÃO_COM_THREADS_E_SEM_THREADS/100mb_comparativo.png)

---

## Código Explicado

### Funcionalidades

1. **Divisão e Ordenação**  
   - O vetor de registros é dividido entre threads. Cada thread utiliza `qsort` para ordenar sua partição.

2. **Mesclagem (Merge)**  
   - As partições ordenadas são combinadas utilizando a técnica de merge, preservando a ordem.

3. **Controle de Threads**  
   - `pthread_create` e `pthread_join` gerenciam as threads, permitindo execução paralela e sincronização ao final.

### Trechos de Código

```c
// Estrutura para dados de threads
typedef struct {
    void *array;
    size_t num_registros;
    size_t tamanho_registro;
    int (*cmp)(const void *, const void *);
} DadosThread;

// Função de comparação de registros
int comparar_registros(const void *a, const void *b) {
    int chave_a = *(int *)a;
    int chave_b = *(int *)b;
    return (chave_a - chave_b);
}

// Função de ordenação paralela
void *ordenar_particao(void *arg) {
    DadosThread *dados = (DadosThread *)arg;
    qsort(dados->array, dados->num_registros, dados->tamanho_registro, dados->cmp);
    return NULL;
}

// Função de mesclagem
void merge(void *array, size_t esquerda_qtd, size_t direita_qtd, size_t tamanho_registro, int (*cmp)(const void *, const void *)) {
    // Implementação de merge
}
