// Bibliotecas adicionadas
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <time.h>
#include <conio.h>

// Prioridades dos atendimentos
#define PRIORIDADE_1 1
#define PRIORIDADE_2 2
#define PRIORIDADE_3 3

#define MAX_PACIENTES 50               // Máximo de nomes no vetor de nomes para gerar pacientes novos nas fila
#define MAX_NOME 100                   // Tamanho máximo para cada nome de paciente (bem extrapolado, colocar nomes pequenos para melhor visualização)
#define MAX_LINE_LENGTH 1024           // Número máximo de pacientes na mesma coluna da tabela em CSV (bem extrapolado também)
#define MAX_COLUMNS 5                  // Quantidade de colunas da tabela em CSV
#define CSV_FILE "hospital.csv"        // Nome do arquivo .csv onde vamos montar a tabela

// Constantes alteráveis antes de rodar
#define TIME_ATEND 5000                // Tempo de cada antendimento
#define TIME_INTERRUP 10000            // Tempo de interrupção de tarefas
#define NUM_MEDICOS 1                  // Quantos médicos estão disponíveis para atender
#define DEADLINEP1 10000               // Tempo de deadline para pacientes de prioridade 1
#define DEADLINEP2 40000               // Tempo de deadline para pacientes de prioridade 2
#define DEADLINEP3 80000               // Tempo de deadline para pacientes de prioridade 3

// Variáveis globais para armazenar o WCRT de cada task
double wcrt_atendimentos = 0;
double wcrt_adicionar_paciente = 0;
double wcrt_interrupcoes = 0;
double wcrt_interrupcoes_manual = 0;

// Apelido para o mutex para manipulalo
SemaphoreHandle_t xMutexPaciente, xMutexPacienteAtendido, xMutexAtualizaCSV, xMutexPrioridade1, xMutexPrioridade2, XMutexPrioridade3;

// Apelido das Tasks para caso precise manipular elas
TaskHandle_t xTaskAdd, xHandle, xTaskInterrup, xTaskInterrup2;
TaskHandle_t xTasks[NUM_MEDICOS];      // Vetor de handles tarefas de atendimento

// Estrutura para representar um paciente da fila
typedef struct Paciente {
    char nome[MAX_NOME];               // Nome do paciente
    int prioridade;                    // Prioridade do paciente na fila
    TickType_t tempo_chegada;          // Tempo de chegada do paciente na fila
    unsigned long int tempo_espera;    // Tempo que o paciente eperou na fila para ser atendido
    struct Paciente *proximo;          // Conexão com próximo paciente na fila
} Paciente;

// Estrutura para representar um paciente atendido
typedef struct Paciente_atendido {
    char nome[MAX_NOME];               // Nome do paciente atendido
    int prioridade;                    // Prioridade do paciente que foi atendido
    TickType_t tempo_chegada;          // Tempo de chegada do paciente na fila
    unsigned long int tempo_espera;    // Tempo que o paciente eperou na fila para ser atendido
    struct Paciente_atendido *proximo; // Conexão com próximo paciente na fila de atendidos
} Paciente_atendido;

// Inicio das listas
Paciente *cabeca = NULL;
Paciente_atendido *cabeca_atendido = NULL;

// Fila de pacientes para cada prioridade
QueueHandle_t fila_prioridade_1;
QueueHandle_t fila_prioridade_2;
QueueHandle_t fila_prioridade_3;

// Vetor de nomes de pacientes iniciais (Modificar aqui para começar com quantos pacientes quiser e caso queria começar com nenhum, deixa apaenas o "")
char nomes_iniciais[][MAX_NOME] = {"", "Lara", "Scarlet", "Naruto", "Joaquim", "Steve", "Elijah"};
// char nomes_iniciais[][MAX_NOME] = {""};

// Vetor de prioridades de pacientes iniciais (Modificar aqui para começar com quantos pacientes quiser e caso queria começar com nenhum, deixa apaenas o 0)
char prioridades_iniciais[] = {0, 1, 1, 1, 1, 1, 1};
// char prioridades_iniciais[] = {0};

// Definição de todas as funções criadas
void inicializar_csv();
void inicializa_pacientes(FILE *file);
void adicionar_paciente(Paciente paciente);
void adicionar_paciente_atendido(Paciente paciente);
void remover_paciente(Paciente paciente);
void remover_paciente_atendido(Paciente paciente);
void atualizar_csv();
void atendimentos(void *pvParameters);
void evento_interrupcao(void *pvParameters);
void adicionar_paciente_aleatorio(void *pvParameters);
void envia_evento(char msg[MAX_NOME]);
void cria_evento_aleatorio(void *pvParameters);
void avalia();

// Função que inicializa o arquivo CSV onde a tabela será montada para ser mostrada
// na interface, casa não exista o arquivo, ele será criado, caso exista, será sobrescrito
void inicializar_csv() {
    FILE *file = fopen(CSV_FILE, "w");                     // Abre arquivo para escrever ou cria arquiv para escrever
    if (file != NULL) {
        fprintf(file, "Prior F;Fila;Prior A;Atendidos;Espera\n"); // Coloca o cabeçalho da tabela no arquivo
        inicializa_pacientes(file);                        // Coloca os pacientes iniciais
        fclose(file);
    } else {
        printf("Erro ao criar o arquivo CSV.\n");
    }
}

// Função que preenche o arquivo CSV e as filas com os pacientes iniciais, que já estão no hospital
// Alterar vetores das linhas 50 e 53 para anterar esses pacientes
void inicializa_pacientes(FILE *file) {
    int contador = sizeof(prioridades_iniciais);
    Paciente paciente;
    for(int i = 1; i < contador; i++){
        fprintf(file, "%d;%s;;;\n", prioridades_iniciais[i], nomes_iniciais[i]); // Adiciona no CSV
        strcpy(paciente.nome, nomes_iniciais[i]);                                // Adiciona nome para paciente
        paciente.prioridade = prioridades_iniciais[i];                           // Adiciona prioridade para paciente
        paciente.tempo_chegada = xTaskGetTickCount();                            // Registra o tempo de chegada
        adicionar_paciente(paciente);                                            // Adiciona paciente na lista de struct
        if (paciente.prioridade == PRIORIDADE_1){
            xQueueSend(fila_prioridade_1, &paciente, 0);                         // Adicionar paciente na fila de prioridade 1
        }
        else if (paciente.prioridade == PRIORIDADE_2){
            xQueueSend(fila_prioridade_2, &paciente, 0);                         // Adicionar paciente na fila de prioridade 2
        }
        else if (paciente.prioridade == PRIORIDADE_3){
            xQueueSend(fila_prioridade_3, &paciente, 0);                         // Adicionar paciente na fila de prioridade 3
        }
    }
    if (contador == 1){
        fprintf(file, ";;;;\n");                                                 // Adiciona no CSV
    }
}

// Função adiciona paciente na fila de pacientes da struct Paciente
void adicionar_paciente(Paciente paciente) {
    if (xSemaphoreTake(xMutexPaciente, portMAX_DELAY) == pdTRUE) {
        Paciente *novo = (Paciente *)malloc(sizeof(Paciente));    // Aloca memória para criar uma nova struct
        if (!novo) {
            printf("Erro ao alocar memória!\n");
            return;
        }
        strcpy(novo->nome, paciente.nome);                        // Preenche nome na struct
        novo->prioridade = paciente.prioridade;                   // Preenche prioridade na struct
        novo->tempo_chegada = xTaskGetTickCount();                // Registra o tempo de chegada
        novo->proximo = NULL;                                     // Marca próximo como nulo, não existente

        // Se não houver nenhum outro paciente, esse paciente é o primeiro, caso contrário, ele será colocado no final da fila
        if (cabeca == NULL) {
            cabeca = novo;
        } else {
            Paciente *atual = cabeca;
            while (atual->proximo != NULL) {
                atual = atual->proximo;
            }
            atual->proximo = novo;
        }
        // printf("Paciente %s adicionado com prioridade %d.\n", paciente.nome, paciente.prioridade);
        xSemaphoreGive(xMutexPaciente);
    } else {
        printf("Falha ao pegar o mutex\n");
    }
}

// Função adiciona paciente na fila de pacientes antendidos da struct Paciente_atendido
void adicionar_paciente_atendido(Paciente paciente) {
    if (xSemaphoreTake(xMutexPacienteAtendido, portMAX_DELAY) == pdTRUE) {
        Paciente_atendido *novo = (Paciente_atendido *)malloc(sizeof(Paciente_atendido));    // Aloca memória para criar uma nova struct
        if (!novo) {
            printf("Erro ao alocar memória!\n");
            return;
        }
        strcpy(novo->nome, paciente.nome);                                                   // Preenche nome na struct
        novo->prioridade = paciente.prioridade;                                              // Preenche prioridade na struct
        novo->tempo_chegada = paciente.tempo_chegada;                                        // Preenche o tempo de chegada
        novo->tempo_espera = paciente.tempo_espera;                                          // Preenche o tempo de espera
        novo->proximo = NULL;                                                                // Marca próximo como nulo, não existente

        // Se não houver nenhum outro paciente antendido, esse paciente é o primeiro atendido, caso contrário, ele será colocado no final da fila
        if (cabeca_atendido == NULL) {
            cabeca_atendido = novo;
        } else {
            Paciente_atendido *atual = cabeca_atendido;
            while (atual->proximo != NULL) {
                atual = atual->proximo;
            }
            atual->proximo = novo;
        }
        // printf("Paciente %s atendido com prioridade %d.\n", paciente.nome, paciente.prioridade);
        xSemaphoreGive(xMutexPacienteAtendido);
    } else {
        printf("Falha ao pegar o mutex\n");
    }
}

// Função remove paciente na fila de pacientes da struct Paciente
void remover_paciente(Paciente paciente) {
    if (cabeca == NULL) {                                   // Verifica se a fila não é nula
        printf("Lista vazia!\n");
        return;
    }

    Paciente *atual = cabeca;
    Paciente *anterior = NULL;

    // Procura até achar a posição do paciente que deseja retirar e quando achar exclui ele e liga o paciente anterior com o posterior
    while (atual != NULL) {
        if (strcmp(atual->nome, paciente.nome) == 0) {     // Verifica se encontrou o paciente
            if (anterior == NULL) {
                cabeca = atual->proximo;                   // Removendo no caso de ser o primeiro paciente da lista
            } else {
                anterior->proximo = atual->proximo;        // Removendo no caso de ser um paciente do meio ou final
            }
            free(atual);
            // printf("Paciente %s removido.\n", paciente.nome);
            return;
        }
        anterior = atual;
        atual = atual->proximo;
    }
    printf("Paciente %s não encontrado.\n", paciente.nome);
}

// Função remove paciente na fila de pacientes da struct Paciente
void remover_paciente_atendido(Paciente paciente) {
    if (cabeca_atendido == NULL) {                                   // Verifica se a fila não é nula
        printf("Lista vazia1!\n");
        return;
    }

    Paciente_atendido *atual = cabeca_atendido;
    Paciente_atendido *anterior = NULL;

    // Procura até achar a posição do paciente que deseja retirar e quando achar exclui ele e liga o paciente anterior com o posterior
    while (atual != NULL) {
        if (strcmp(atual->nome, paciente.nome) == 0) {     // Verifica se encontrou o paciente
            if (anterior == NULL) {
                cabeca_atendido = atual->proximo;          // Removendo no caso de ser o primeiro paciente da lista
            } else {
                anterior->proximo = atual->proximo;        // Removendo no caso de ser um paciente do meio ou final
            }
            free(atual);
            // printf("Paciente atendido %s removido.\n", paciente.nome);
            return;
        }
        anterior = atual;
        atual = atual->proximo;
    }
    printf("Paciente atendido %s não encontrado.\n", paciente.nome);
}

// Função atualiza o arquivo CSV com as filas de Pacientes e Pacientes Atendidos
void atualizar_csv() {
    if (xSemaphoreTake(xMutexAtualizaCSV, portMAX_DELAY) == pdTRUE) {
        Paciente *atual = cabeca;
        Paciente_atendido *atual_atendido = cabeca_atendido;

        FILE *file = fopen(CSV_FILE, "w");                               // Abre arquivo com "w" para sobrescreve o arquivo anterior
        if (file == NULL) {                                              // Verifica se o arquivo existe
            printf("Erro ao abrir o arquivo!\n");
            return;
        }

        fprintf(file, "Prior F;Fila;Prior A;Atendidos;Espera\n");        // Insere o cabeçalho com o nome das colunas

        // Enquanto houver pacientes em alguma fila, coloca no arquivo
        while (atual != NULL || atual_atendido != NULL) {
            if (atual != NULL && atual_atendido != NULL){                // Preenchendo linha quando há pacientes atendidos e não atendidos
                float tempo = (float) (atual_atendido->tempo_espera)/1000;
                fprintf(file, "%d;%s;%d;%s;%.3f\n", atual->prioridade, atual->nome, atual_atendido->prioridade, atual_atendido->nome,tempo);
                atual = atual->proximo;
                atual_atendido = atual_atendido->proximo;
            }
            else if(atual != NULL){                                     // Preenchendo linha quando há só pacientes não atendidos
                fprintf(file, "%d;%s;;;\n", atual->prioridade, atual->nome);
                atual = atual->proximo;
            }
            else{                                                       // Preenchendo linha quando há só pacientes atendidos
                float tempo = (float) (atual_atendido->tempo_espera)/1000;
                fprintf(file, ";;%d;%s;%.3f\n", atual_atendido->prioridade, atual_atendido->nome,tempo);
                atual_atendido = atual_atendido->proximo;
            }
        }

        fclose(file);

        // printf("Arquivo sobrescrito com sucesso!\n");
        xSemaphoreGive(xMutexAtualizaCSV);
    } else {
        printf("Falha ao pegar o mutex\n");
    }
}

// Função da Task de atendimentos
void atendimentos(void *pvParameters) {
    int medico = *(int *)pvParameters;                                              // Converte o ponteiro para inteiro
    vPortFree(pvParameters);                                                        // Libera a memória alocada
    char msg[100] = "Devido à demora, um paciente morreu: ";
    char espaco[] = ", ";
    char fim[] = " seg de espera.";
    char str[10];

    Paciente paciente;
    while (1) {
        TickType_t inicio = xTaskGetTickCount();                                    // Tempo inicial da iteração
        paciente.prioridade = 0;
        if (uxQueueMessagesWaiting(fila_prioridade_1) != 0) {                       // Verifica se a fila de prioridades 1 contém alguém
            if (xSemaphoreTake(xMutexPrioridade1, portMAX_DELAY) == pdTRUE) {
                xQueueReceive(fila_prioridade_1, &paciente, portMAX_DELAY);             // Retira um paciente da fila de prioridades 1 se houver algum
                // printf("Atendendo paciente de prioridade 1: %s\n", paciente.nome);
                xSemaphoreGive(xMutexPrioridade1);
            } else {
                printf("Falha ao pegar o mutex\n");
            }
        }
        else if (uxQueueMessagesWaiting(fila_prioridade_2) != 0) {                  // Verifica se a fila de prioridades 2 contém alguém
            if (xSemaphoreTake(xMutexPrioridade2, portMAX_DELAY) == pdTRUE) {
                xQueueReceive(fila_prioridade_2, &paciente, portMAX_DELAY);             // Retira um paciente da fila de prioridades 2 se houver algum
                // printf("Atendendo paciente de prioridade 2: %s\n", paciente.nome);
                xSemaphoreGive(xMutexPrioridade2);
            } else {
                printf("Falha ao pegar o mutex\n");
            }
        }
        else if (uxQueueMessagesWaiting(fila_prioridade_3) != 0){                   // Verifica se a fila de prioridades 3 contém alguém
            if (xSemaphoreTake(XMutexPrioridade3, portMAX_DELAY) == pdTRUE) {
                xQueueReceive(fila_prioridade_3, &paciente, portMAX_DELAY);             // Retira um paciente da fila de prioridades 3 se houver algum
                // printf("Atendendo paciente de prioridade 3: %s\n", paciente.nome);
                xSemaphoreGive(XMutexPrioridade3);
            } else {
                printf("Falha ao pegar o mutex\n");
            }
        }
        if (paciente.prioridade != 0){
            TickType_t tempo_atual = xTaskGetTickCount();                          // Registra o tempo atual
            TickType_t tempo_espera = tempo_atual - paciente.tempo_chegada;        // Calcula o tempo de espera
            // printf("Tempo de espera: %lu ms\n", tempo_espera * portTICK_PERIOD_MS);
            remover_paciente(paciente);                                            // Remove o paciente da fila de struct de Paciente
            vTaskDelay(pdMS_TO_TICKS(TIME_ATEND-1000));                                 // Tempo de atendimento
            paciente.tempo_espera = tempo_espera * portTICK_PERIOD_MS;             // Registra tempo de espera na struct
            if (paciente.prioridade == 1 && paciente.tempo_espera > DEADLINEP1){
                paciente.prioridade = 100;
                strcat(msg, paciente.nome);
                strcat(msg, espaco);
                float tempo = (float) paciente.tempo_espera/1000;
                sprintf(str, "%.3f", tempo);
                strcat(msg, str);
                strcat(msg, fim);
                strcpy(paciente.nome, msg);
                adicionar_paciente(paciente); 
                atualizar_csv();
                exit(0);
            }else if(paciente.prioridade == 2 && paciente.tempo_espera > DEADLINEP2){
                paciente.prioridade = 100;
                strcat(msg, paciente.nome);
                strcat(msg, espaco);
                float tempo = (float) paciente.tempo_espera/1000;
                sprintf(str, "%.3f", tempo);
                strcat(msg, str);
                strcat(msg, fim);
                strcpy(paciente.nome, msg);
                adicionar_paciente(paciente);
                atualizar_csv();
                exit(0);
            }else if(paciente.prioridade == 3 && paciente.tempo_espera > DEADLINEP3){
                paciente.prioridade = 100;
                strcat(msg, paciente.nome);
                strcat(msg, espaco);
                float tempo = (float) paciente.tempo_espera/1000;
                sprintf(str, "%.3f", tempo);
                strcat(msg, str);
                strcat(msg, fim);
                strcpy(paciente.nome, msg);
                adicionar_paciente(paciente);
                atualizar_csv();
                exit(0);
            }
            adicionar_paciente_atendido(paciente);                                 // Adiciona paciente na fila de struct de Paciente Atendido
            atualizar_csv();                                                       // Atualiza o arquivo CSV com as mudanças feitas nas filas de struct
        }

        // Calcula o tempo gasto na iteração atual
        TickType_t fim = xTaskGetTickCount();
        double tempo_gasto = (double)(fim - inicio) * portTICK_PERIOD_MS / 1000.0; // Converte para segundos

        // Atualiza o WCRT, se necessário
        if (tempo_gasto > wcrt_atendimentos) {
            wcrt_atendimentos = tempo_gasto;
            printf("Novo WCRT da task atendimentos: %f segundos\n", wcrt_atendimentos);
        }

        // Aguarda antes de atender outro paciente
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}

// Função de interrupção
void evento_interrupcao(void *pvParameters) {
    char *eve = (char *)pvParameters;                                     // Converte o parâmetro recebido
    Paciente paciente;                                                    // Cria nova struct Paciente
    char msg[] = "Evento: ";
    
    if (!strcmp(eve, "terremoto")){
        strcat(msg, "Houve um terremoto na cidade!");
        strcpy(paciente.nome, msg);                                       // Preenche o nome do paciente
        paciente.prioridade = 0;                                          // Preenche a prioridade do paciente
        adicionar_paciente(paciente);                                     // Adiciona o paciente na fila de struct Paciente
        adicionar_paciente_atendido(paciente);                            // Adiciona o paciente na fila de struct Paciente Atendido 
        atualizar_csv(paciente);                                          // Atualiza o arquivo CSV com a mudança feita na fila de struct
        vTaskSuspend(xTaskAdd);                                           // Suspende a TaskAdd
        for (int i = 0; i < NUM_MEDICOS; i++){                            // Suspende as tarefas de atendimento
            vTaskSuspend(xTasks[i]); 
        }
        vTaskDelay(pdMS_TO_TICKS(TIME_INTERRUP));                         // Aguarda antes de liberar TaskAdd
        vTaskResume(xTaskAdd);                                            // Retoma a TaskAdd
        for (int i = 0; i < NUM_MEDICOS; i++){                            // Retoma as tarefas de atendimento
            vTaskResume(xTasks[i]); 
        }
        remover_paciente_atendido(paciente);                              // Remove o paciente na fila de struct Paciente
        remover_paciente(paciente);                                       // Remove o paciente na fila de struct Paciente
    }
    else if(!strcmp(eve, "energia")){
        strcat(msg, "A energia foi cortada!");
        strcpy(paciente.nome, msg);                                       // Preenche o nome do paciente
        paciente.prioridade = 0;                                          // Preenche a prioridade do paciente
        adicionar_paciente_atendido(paciente);                            // Adiciona o paciente na fila de struct Paciente Atendido 
        atualizar_csv(paciente);                                          // Atualiza o arquivo CSV com a mudança feita na fila de struct
        for (int i = 0; i < NUM_MEDICOS; i++){                            // Suspende as tarefas de atendimento
            vTaskSuspend(xTasks[i]); 
        }
        vTaskDelay(pdMS_TO_TICKS(TIME_INTERRUP));                         // Aguarda antes de liberar TaskAdd
        for (int i = 0; i < NUM_MEDICOS; i++){                            // Retoma as tarefas de atendimento
            vTaskResume(xTasks[i]); 
        }
        remover_paciente_atendido(paciente);                              // Remove o paciente na fila de struct Paciente
    }
    else if(!strcmp(eve, "triagens")){
        strcat(msg, "Problemas na Triagem!");
        strcpy(paciente.nome, msg);                                       // Preenche o nome do paciente
        paciente.prioridade = 0;                                          // Preenche a prioridade do paciente
        adicionar_paciente(paciente);                                     // Adiciona o paciente na fila de struct Paciente
        atualizar_csv(paciente);                                          // Atualiza o arquivo CSV com a mudança feita na fila de struct
        vTaskSuspend(xTaskAdd);                                           // Suspende a TaskAdd
        vTaskDelay(pdMS_TO_TICKS(TIME_INTERRUP));                         // Aguarda antes de liberar TaskAdd
        vTaskResume(xTaskAdd);                                            // Retoma a TaskAdd
        remover_paciente(paciente);                                       // Remove o paciente na fila de struct Paciente
    }
    else{
        strcat(msg, "O Painel dos atendimentos travou!");
        strcpy(paciente.nome, msg);                                       // Preenche o nome do paciente
        paciente.prioridade = 0;                                          // Preenche a prioridade do paciente
        adicionar_paciente_atendido(paciente);                            // Adiciona o paciente na fila de struct Paciente Atendido 
        atualizar_csv(paciente);                                          // Atualiza o arquivo CSV com a mudança feita na fila de struct
        for (int i = 0; i < NUM_MEDICOS; i++){                            // Suspende as tarefas de atendimento
            vTaskSuspend(xTasks[i]); 
        }
        vTaskDelay(pdMS_TO_TICKS(TIME_INTERRUP));                         // Aguarda antes de liberar TaskAdd
        for (int i = 0; i < NUM_MEDICOS; i++){                            // Retoma as tarefas de atendimento
            vTaskResume(xTasks[i]); 
        }
        remover_paciente_atendido(paciente);                              // Remove o paciente na fila de struct Paciente
    }
    vTaskDelete(NULL);                                                    // Deleta a si mesma
}

// Função que adiciona um paciente novo na fila do hospital (na struct de Paciente)
void adicionar_paciente_aleatorio(void *pvParameters) {
    // Vetor com tempos variados para o tempo de chegada de um paciente novo
    int tempos[] = {1000, 1000, 1000, 1000, 1000, 2000, 2000, 2000, 2000, 2000, 3000, 3000, 3000, 3000, 3000};
    // Vetor com nomes para pacientes que serão colocados na fila
    char nomes[MAX_PACIENTES][MAX_NOME] = {
        "Ana", "Joao", "Maria", "Carlos", "Fernanda", "Gabriel", "Lucas", "Amanda", "Roberta", "Rafael", 
        "Sofia", "Eduardo", "Ricardo", "Juliana", "Renato", "Patricia", "Paulo", "Claudia", "Pedro", "Felipe",
        "Larissa", "Beatriz", "Marcelo", "Paula", "Andre", "Carla", "Thiago", "Lucia", "Marcos", "Larissa",
        "Cristina", "Rodrigo", "Tatiane", "Luana", "Eliane", "Tiago", "Vanessa", "Priscila", "Douglas", "Katia",
        "Cesar", "Celia", "Alice", "Isabela", "Sergio", "Alessandra", "Vera", "Francisco", "Raquel", "Vitor"
    };
    // Tipos de prioridades que podem ser assumidas - Quando menos quantidade de um número, menor a probabilidade de ele sair
    int numeros[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    // Contador para pegar nomes no vetor de nomes na ordem descrita dentro do vetor
    int paciente_idx = 0; 

    while (1) {
        TickType_t inicio = xTaskGetTickCount();                          // Tempo inicial da iteração
        srand(time(NULL));                                                // Inicializa a semente do rand()
        int num_index = rand() % 11;                                      // Escolhe o indice do vetor de 11 números (todos os 11 sendo 1, 2 ou 3)
        int prioridade = numeros[num_index];                              // Usa o indice escolhido para definir aleatoriamente a prioridade do paciente
        Paciente paciente;                                                // Cria nova struct Paciente
        strcpy(paciente.nome, nomes[paciente_idx]);                       // Preenche o nome do paciente
        paciente.prioridade = prioridade;                                 // Preenche a prioridade do paciente
        paciente.tempo_chegada = xTaskGetTickCount();                     // Registra o tempo de chegada
        paciente_idx = paciente_idx + 1;                                  // Acrescenta no contador para pegar próximo nome

        adicionar_paciente(paciente);                                     // Adiciona o paciente na fila de struct
        atualizar_csv(paciente);                                          // Atualiza o arquivo CSV com a mudança feita na fila de struct 

        // printf("Novo paciente chegou: %s, Prioridade: %d\n", paciente.nome, paciente.prioridade);

        // Adiciona o paciente à fila de prioridade correta com base na prioridade
        if (paciente.prioridade == PRIORIDADE_1) {
            xQueueSend(fila_prioridade_1, &paciente, 0);
        } else if (paciente.prioridade == PRIORIDADE_2) {
            xQueueSend(fila_prioridade_2, &paciente, 0);
        } else {
            xQueueSend(fila_prioridade_3, &paciente, 0);
        }

        srand(time(NULL));                                            // Inicializa a semente do rand()
        int index = rand() % 15;                                      // Escolhe o indice do vetor de 10 números
        int TIME_CREATE = tempos[index];                              // Usa o indice escolhido para definir aleatoriamente o tempo de espera até adicionar outro paciente

        // Calcula o tempo gasto na iteração atual
        TickType_t fim = xTaskGetTickCount();
        double tempo_gasto = (double)(fim - inicio) * portTICK_PERIOD_MS / 1000.0; // Converte para segundos

        // Atualiza o WCRT, se necessário
        if (tempo_gasto > wcrt_adicionar_paciente) {
            wcrt_adicionar_paciente = tempo_gasto;
            printf("Novo WCRT da task adicionar_paciente_aleatorio: %f segundos\n", wcrt_adicionar_paciente);
        }

        // Aguarda antes de adicionar outro paciente
        vTaskDelay(pdMS_TO_TICKS(TIME_CREATE));
    }
}

// Função para mandar os eventos para o cvs
void envia_evento(char msg[MAX_NOME]){
    char mensagem[MAX_NOME] = "Evento: ";
    Paciente paciente;                                                    // Cria nova struct Paciente
    if (!strcmp(msg, "vasco")){
        strcat(mensagem, "O Vasco acaba de ser CAMPEÃO!!!");
    }
    else if(!strcmp(msg, "chuvas")){
        strcat(mensagem, "Começou a chover, mas nada muito forte!");
    }
    else if(!strcmp(msg, "reunião")){
        strcat(mensagem, "Está tendo reunião da administração!");
    }
    else if(!strcmp(msg, "estadios")){
        strcat(mensagem, "O Vasco está jogando no estádio da Cidade!");
    }
    else if(!strcmp(msg, "manutencoes")){
        strcat(mensagem, "Manutenção em andamento na ala de cirurgia!");
    }
    else if(!strcmp(msg, "visitantes")){
        strcat(mensagem, "Visitas administrativas em andamento!");
    }
    else if(!strcmp(msg, "palestrantes")){
        strcat(mensagem, "Palestra sobre Nefrologia em andamento!");
    }
    else if(!strcmp(msg, "entregadoresE")){
        strcat(mensagem, "Parceiros entregando mercadoria hospitalar!");
    }
    else if(!strcmp(msg, "banheiroEntupi")){
        strcat(mensagem, "O banheiro do andar foi interditado!");
    }
    else if(!strcmp(msg, "TrocadePorteiros")){
        strcat(mensagem, "Troca de plantão dos porteiros!");
    }
    else if(!strcmp(msg, "lampadaqueimada")){
        strcat(mensagem, "Lâmpada queimada no corredor!");
    }
    else if(!strcmp(msg, "-")){
        strcat(mensagem, "--");
    }
    strcpy(paciente.nome, mensagem);                                  // Preenche o nome do paciente
    paciente.prioridade = 100;                                        // Preenche a prioridade do paciente
    adicionar_paciente(paciente);                                     // Adiciona o paciente na fila de struct Paciente
    atualizar_csv(paciente);                                          // Atualiza o arquivo CSV com a mudança feita na fila de struct
    vTaskDelay(pdMS_TO_TICKS(TIME_INTERRUP));                         // Aguarda antes de liberar TaskAdd
    remover_paciente(paciente);                                       // Remove o paciente na fila de struct Paciente
}

// Função que cria eventos aleatórios
void cria_evento_aleatorio(void *pvParameters){
    // Vetor com nomes dos possíveis eventos a acontecer
    char eventos[][20] = {
        "terremoto", "vasco", "energia", "chuvas", "triagens", "painel", "reunião", "estadios",
        "manutencoes", "visitantes", "palestrantes", "entregadoresE", "banheiroEntupi", "TrocadePorteiros", "lampadaqueimada", 
        "-"
    };

    while(1){
        TickType_t inicio = xTaskGetTickCount();                      // Tempo inicial da iteração
        srand(time(NULL));                                            // Inicializa a semente do rand()
        int index = rand() % 16;                                      // Escolhe o indice do vetor de 32 palavras
        char eve[MAX_NOME];
        strcpy(eve, eventos[index]);                                  // Usa o indice escolhido para definir aleatoriamente o evento que vai acontecer

        if (!strcmp(eve, "terremoto") || !strcmp(eve, "energia") || !strcmp(eve, "triagens") || !strcmp(eve, "painel")){
            TaskHandle_t xHandle = NULL;
            xTaskCreate(evento_interrupcao, "Interrupcao", 1000, eve, 3, &xHandle);
        }
        else{
            envia_evento(eve);
        }
        
        // Calcula o tempo gasto na iteração atual
        TickType_t fim = xTaskGetTickCount();
        double tempo_gasto = (double)(fim - inicio) * portTICK_PERIOD_MS / 1000.0; // Converte para segundos

        // Atualiza o WCRT, se necessário
        if (tempo_gasto > wcrt_interrupcoes) {
            wcrt_interrupcoes = tempo_gasto;
            printf("Novo WCRT da task eventos de interrupções: %f segundos\n", wcrt_interrupcoes);
        }

        // Aguarda antes de criar outro evento aleatório
        vTaskDelay(pdMS_TO_TICKS(TIME_INTERRUP));
    }
}

// Função para avaliar os valores escolhidos
void avalia(){
    if(TIME_ATEND < 0){
        printf("\n\nTempo de Atendimento negativo, por favor corrija para um valor não negativo!\n\n");
        exit(0);
    }
    if(TIME_INTERRUP < 0){
        printf("\n\nTempo de Interrupção negativo, por favor corrija para um valor não negativo!\n\n");
        exit(0);
    }
    if(NUM_MEDICOS < 0){
        printf("\n\nNúmero de Médicos de Plantão negativo, por favor corrija para um valor não negativo!\n\n");
        exit(0);
    }
}

// Função para pegar teclagem e fazer interrompimento
void tecla_interrompe(void *pvParameters){
    char tecla;
    Paciente paciente;                                                    // Cria nova struct Paciente
    char msg[] = "Evento: ";

    while(1){
        TickType_t inicio = xTaskGetTickCount();                      // Tempo inicial da iteração
        // printf("Pressione qualquer tecla para continuar...\n");
        if (kbhit()) {                                                // Verifica se uma tecla foi pressionada
            tecla = getch();
        }                                                             // Captura a tecla pressionada
        
        // printf("\nVocê pressionou: %c\n", tecla);

        TaskHandle_t xHandle = NULL;
        if (tecla == '1'){
            strcat(msg, "Interrompe Atendimento!");
            strcpy(paciente.nome, msg);                                       // Preenche o nome do paciente
            paciente.prioridade = 0;                                          // Preenche a prioridade do paciente
            adicionar_paciente_atendido(paciente);                            // Adiciona o paciente na fila de struct Paciente Atendido 
            atualizar_csv(paciente);                                          // Atualiza o arquivo CSV com a mudança feita na fila de struct
            for (int i = 0; i < NUM_MEDICOS; i++){                            // Suspende as tarefas de atendimento
                vTaskSuspend(xTasks[i]); 
            }
            vTaskDelay(pdMS_TO_TICKS(TIME_INTERRUP-1000));                         // Aguarda antes de liberar TaskAdd
            for (int i = 0; i < NUM_MEDICOS; i++){                            // Retoma as tarefas de atendimento
                vTaskResume(xTasks[i]); 
            }
            remover_paciente_atendido(paciente);                              // Remove o paciente na fila de struct Paciente
        }
        else if (tecla == '2'){
            strcat(msg, "Interrompe Chegada à Fila!");
            strcpy(paciente.nome, msg);                                       // Preenche o nome do paciente
            paciente.prioridade = 0;                                          // Preenche a prioridade do paciente
            adicionar_paciente(paciente);                                     // Adiciona o paciente na fila de struct Paciente
            atualizar_csv(paciente);                                          // Atualiza o arquivo CSV com a mudança feita na fila de struct
            vTaskSuspend(xTaskAdd);                                           // Suspende a TaskAdd
            vTaskDelay(pdMS_TO_TICKS(TIME_INTERRUP-1000));                         // Aguarda antes de liberar TaskAdd
            vTaskResume(xTaskAdd);                                            // Retoma a TaskAdd
            remover_paciente(paciente);                                       // Remove o paciente na fila de struct Paciente
        }
        else if (tecla == '3'){                                
            strcat(msg, "Interrompe Chegada à Fila e Atendimento!");
            strcpy(paciente.nome, msg);                                       // Preenche o nome do paciente
            paciente.prioridade = 0;                                          // Preenche a prioridade do paciente
            adicionar_paciente(paciente);                                     // Adiciona o paciente na fila de struct Paciente
            adicionar_paciente_atendido(paciente);                            // Adiciona o paciente na fila de struct Paciente Atendido 
            atualizar_csv(paciente);                                          // Atualiza o arquivo CSV com a mudança feita na fila de struct
            vTaskSuspend(xTaskAdd);                                           // Suspende a TaskAdd
            for (int i = 0; i < NUM_MEDICOS; i++){                            // Suspende as tarefas de atendimento
                vTaskSuspend(xTasks[i]); 
            }
            vTaskDelay(pdMS_TO_TICKS(TIME_INTERRUP-1000));                         // Aguarda antes de liberar TaskAdd
            vTaskResume(xTaskAdd);                                            // Retoma a TaskAdd
            for (int i = 0; i < NUM_MEDICOS; i++){                            // Retoma as tarefas de atendimento
                vTaskResume(xTasks[i]); 
            }
            remover_paciente_atendido(paciente);                              // Remove o paciente na fila de struct Paciente
            remover_paciente(paciente);                                       // Remove o paciente na fila de struct Paciente
        }

        strcpy(msg, "Evento: ");
        tecla = 'x';

        // Calcula o tempo gasto na iteração atual
        TickType_t fim = xTaskGetTickCount();
        double tempo_gasto = (double)(fim - inicio) * portTICK_PERIOD_MS / 1000.0; // Converte para segundos

        // Atualiza o WCRT, se necessário
        if (tempo_gasto > wcrt_interrupcoes_manual) {
            wcrt_interrupcoes_manual = tempo_gasto;
            printf("Novo WCRT da task eventos de interrupções: %f segundos\n", wcrt_interrupcoes_manual);
        }

        // Aguarda antes de criar outro evento aleatório
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Função principal - MAIN
int main(void) {
    // Avalia variáveis escolidas antes de começar
    avalia();
    // Inicializa três filas de prioridade do tipo FIFO para cada tipo de prioriade (1, 2 e 3)
    fila_prioridade_1 = xQueueCreate(50, sizeof(Paciente)); // Fila FIFO com 50 espaços para prioridade 1
    fila_prioridade_2 = xQueueCreate(50, sizeof(Paciente)); // Fila FIFO com 50 espaços para prioridade 2
    fila_prioridade_3 = xQueueCreate(50, sizeof(Paciente)); // Fila FIFO com 50 espaços para prioridade 3

    // Cria os mutex antes de iniciar as tasks
    xMutexPaciente = xSemaphoreCreateMutex();
    xMutexPacienteAtendido = xSemaphoreCreateMutex();
    xMutexAtualizaCSV = xSemaphoreCreateMutex();
    xMutexPrioridade1 = xSemaphoreCreateMutex();
    xMutexPrioridade2 = xSemaphoreCreateMutex();
    XMutexPrioridade3 = xSemaphoreCreateMutex();

    // Inicializa arquivo CSV que será preenchido para o código python ler e gerar a tabela e gráfico
    // Aqui podem ser inicializadas pessoas na fila antes de começar o código, ou pode ser iniciada uma fila sem pessoas
    inicializar_csv();

    // Aguarda ENTER para começar o código de fato
    printf("Pressione Enter para começar\n");
    while (getchar() != '\n'); // Limpa buffer

    // Cria as tarefas que serão processadas em tempo real
    for (int i = 1; i < NUM_MEDICOS + 1; i++) {
        int *numero = pvPortMalloc(sizeof(int));  // Aloca um novo int na heap
        if (numero == NULL) {
            printf("Erro ao alocar memória\n");
            continue;
        }
    
        *numero = i;  // Armazena o valor correto no ponteiro
        char num[5];
        sprintf(num, "%d", i);
        char titulo[20] = "Atendimentos ";
        strcat(titulo, num);
    
        xTaskCreate(atendimentos, titulo, 1000, (void *)numero, 2, &xTasks[i-1]);
    }
    xTaskCreate(adicionar_paciente_aleatorio, "Adicionar Paciente Aleatorio", 1000, NULL, 1, &xTaskAdd);
    //xTaskCreate(cria_evento_aleatorio, "Cria um evento aleatório para interrupção", 5000, NULL, 2, &xTaskInterrup);  // Faz interrupção automática
    xTaskCreate(tecla_interrompe, "Cria interrupção por tecla pressionada", 1000, NULL, 4, &xTaskInterrup2);            // Faz interrupção manual (teclas 1, 2 e 3)

    // Inicia o FreeRTOS e assim inicia o processamento das tasks
    vTaskStartScheduler();
    return 0;
}