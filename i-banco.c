/*
// Projeto SO - exercicio 1, version 1
// Sistemas Operativos, DEI/IST/ULisboa 2016-17
*/
/*
// Pedro Quintas, 83546;
// Catarina Brás, 83439;
*/

#include "commandlinereader.h"
#include "contas.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

#define COMANDO_DEBITAR "debitar"
#define COMANDO_CREDITAR "creditar"
#define COMANDO_LER_SALDO "lerSaldo"
#define COMANDO_SIMULAR "simular"
#define COMANDO_TRANSFERIR "transferir"
#define COMANDO_SAIR "sair"

#define MAXARGS 4
#define BUFFER_SIZE 100
#define MAXPROCESSES 20
#define NUM_TRABALHADORAS 3
#define CMD_BUFFER_DIM  (NUM_TRABALHADORAS * 2)

typedef enum op_enum {OP_ERRO, OP_LER, OP_CREDITAR, OP_DEBITAR, OP_SAIR, OP_TRANSFERIR, OP_SIMULAR} op;

typedef struct {
    op operacao;
    int idConta1;
    int idConta2;
    int valor;
} comando_t;

typedef void (*sighandler_t)(int);

pthread_t tid[NUM_TRABALHADORAS];
comando_t cmd_buffer[CMD_BUFFER_DIM];
int buff_write_idx = 0, buff_read_idx = 0, trabalhadoras_espera = 0, simular_acabado = 0;
sem_t sem_read, sem_write;
pthread_mutex_t mutex_read, mutex_write, mutex_simular;
pthread_mutex_t mutex_contas[NUM_CONTAS];
pthread_cond_t cond_trabalhadoras, cond_main;

/* Declaracao de funcoes */
void *trabalho(void *arg);
comando_t criaComando(op operacao, int idConta1, int idConta2, int valor);
void insereComando(comando_t cmd);

int main (int argc, char** argv) {

    int nfilhos = 0, i;
    char *args[MAXARGS + 1];
    char buffer[BUFFER_SIZE];
    sighandler_t prevSig;

    if (sem_init(&sem_read, 0, 0) == -1)
        perror("Erro na inicialização do semáforo");

    if (sem_init(&sem_write, 0, CMD_BUFFER_DIM) == -1)
        perror("Erro na inicialização do semáforo");

    if (pthread_mutex_init(&mutex_read, NULL) != 0)
        fprintf(stderr, "Erro na inicialização do mutex");

    if (pthread_mutex_init(&mutex_write, NULL) != 0)
        fprintf(stderr, "Erro na inicialização do mutex");

    if (pthread_mutex_init(&mutex_simular, NULL) != 0)
        fprintf(stderr, "Erro na inicialização do mutex");

    if (pthread_cond_init(&cond_trabalhadoras, NULL) != 0)
        fprintf(stderr, "Erro na inicialização da condicao");

    if (pthread_cond_init(&cond_main, NULL) != 0)
    	fprintf(stderr, "Erro na inicialização da condicao");

    prevSig = signal(SIGUSR1, paraSimular);
    if (prevSig == SIG_ERR) perror("Erro na chamada a signal");

    inicializarContas();

    printf("Bem-vinda/o ao i-banco\n\n");

    /* Criacao das tarefas */
    for(i = 0; i < NUM_TRABALHADORAS; i++)
        if (pthread_create(&tid[i], 0, trabalho, NULL) != 0)
            fprintf(stderr, "Erro a criar tarefas.");

    while (1) {
        int numargs;

        numargs = readLineArguments(args, MAXARGS+1, buffer, BUFFER_SIZE);

        /* EOF (end of file) do stdin ou comando "sair" */
        if (numargs < 0 ||
            (numargs > 0 && (strcmp(args[0], COMANDO_SAIR) == 0))) {

            if (numargs == 2 && (strcmp(args[1], "agora") == 0)) {
                int killRet;
                killRet = kill(0, SIGUSR1);
                if (killRet == 1 || killRet == 64) fprintf(stderr, "Erro na chamada de sistema kill.\n");
            }

            printf("i-banco vai terminar.\n--\n");

            while (nfilhos-- > 0) {
                int pid, exitStatus;
                pid = wait(&exitStatus);

             /* Tratamento de erro do wait */
            if (pid == -1) {
                    fprintf(stderr, "Erro na chamada de sistema wait.\n");
                    exit(EXIT_FAILURE);
                }
                printf("FILHO TERMINADO (PID=%d, terminou %s)\n", pid,
                    WIFEXITED(exitStatus) ? "normalmente" : "abruptamente");
            }

            for(i = 0; i < NUM_TRABALHADORAS; i++) {
                comando_t cmd = criaComando(OP_SAIR, 0, 0, 0);
                insereComando(cmd);
            }

            for(i = 0; i < NUM_TRABALHADORAS; i++)
                if (pthread_join(tid[i], NULL) != 0) {
                    fprintf(stderr, "Erro na chamada de sistema pthread_join");
                    exit(EXIT_FAILURE);
                }


            printf("--\ni-banco terminou.\n");

            exit(EXIT_SUCCESS);
        }

        else if (numargs == 0)
            /* Nenhum argumento; ignora e volta a pedir */
            continue;

        /* Debitar */
        else if (strcmp(args[0], COMANDO_DEBITAR) == 0) {
            int idConta, valor;
            comando_t cmd;

            if (numargs < 3) {
                printf("%s: Sintaxe inválida, tente de novo.\n", COMANDO_DEBITAR);
                continue;
            }


            idConta = atoi(args[1]);
            valor = atoi(args[2]);

            cmd = criaComando(OP_DEBITAR, idConta, 0, valor);
            insereComando(cmd);
    }

    /* Creditar */
    else if (strcmp(args[0], COMANDO_CREDITAR) == 0) {
        int idConta, valor;
        comando_t cmd;
        if (numargs < 3) {
            printf("%s: Sintaxe inválida, tente de novo.\n", COMANDO_CREDITAR);
            continue;
        }

        idConta = atoi(args[1]);
        valor = atoi(args[2]);

        cmd = criaComando(OP_CREDITAR, idConta, 0, valor);
        insereComando(cmd);
    }

    /* Ler Saldo */
    else if (strcmp(args[0], COMANDO_LER_SALDO) == 0) {
        int idConta;
        comando_t cmd;
        if (numargs < 2) {
            printf("%s: Sintaxe inválida, tente de novo.\n", COMANDO_LER_SALDO);
            continue;
        }
        idConta = atoi(args[1]);

        cmd = criaComando(OP_LER, idConta, 0, 0);
        insereComando(cmd);
    }

    /* Simular */
    else if (strcmp(args[0], COMANDO_SIMULAR) == 0) {
        int pid;

        if (numargs != 2) {
            printf("%s: Sintaxe inválida, tente de novo.\n", COMANDO_SIMULAR);
            continue;
        }

        if (atoi(args[1]) < 0) {
            printf("%s: Número inválido de anos.\n", COMANDO_SIMULAR);
            continue;
        }

        for (i = 0; i < NUM_TRABALHADORAS; i++) {
        	comando_t cmd = criaComando(OP_SIMULAR, 0, 0, 0);
        	insereComando(cmd);
        }

        if (pthread_mutex_lock(&mutex_simular) != 0)
        	fprintf(stderr, "Erro na manipulação do mutex");
        simular_acabado = 0;

		while (trabalhadoras_espera < NUM_TRABALHADORAS)
			if(pthread_cond_wait(&cond_trabalhadoras, &mutex_simular) != 0)
				fprintf(stderr, "Erro a manipular condição.");

        if (nfilhos < MAXPROCESSES) pid = fork();
        else printf("%s: Número máximo de processos atingido.\n", COMANDO_SIMULAR);
        if (pid == -1) {
            /* Tratamento do caso de erro no fork */
            fprintf(stderr,"Erro na chamada de sistema fork.\n");
            if (pthread_mutex_unlock(&mutex_simular) != 0)
        		fprintf(stderr, "Erro na manipulação do mutex");
            continue;
        }
        nfilhos++;
        if (pid == 0) {
            /* Processo filho */
            simular(atoi(args[1]));
            exit(EXIT_SUCCESS);
        }
        printf("\n");

        simular_acabado = 1;
        trabalhadoras_espera = 0;
        if(pthread_cond_broadcast(&cond_main) != 0)
        	fprintf(stderr, "Erro a manipular condição.");

        if (pthread_mutex_unlock(&mutex_simular) != 0)
        	fprintf(stderr, "Erro na manipulação do mutex");
    }

	/* Transferir */
	else if (strcmp(args[0], COMANDO_TRANSFERIR) == 0) {
		comando_t cmd;
		int idContaOr, idContaDes, valor;

		if (numargs != 4) {
            printf("%s: Sintaxe inválida, tente de novo.\n\n", COMANDO_TRANSFERIR);
            continue;
        }

		idContaOr = atoi(args[1]);
		idContaDes = atoi(args[2]);
		valor = atoi(args[3]);
		cmd = criaComando(OP_TRANSFERIR, idContaOr, idContaDes, valor);
		insereComando(cmd);
	}

    else {
        printf("Comando desconhecido. Tente de novo.\n");
    }
  }
}

void *trabalho(void *arg) {
    while(1) {
      int saldo;
        comando_t cmd;

        if (sem_wait(&sem_read) != 0)
          perror("Erro a esperar pelo semáforo.");
        if (pthread_mutex_lock(&mutex_read) != 0)
          fprintf(stderr, "Erro a manipular mutex.\n");
        cmd = cmd_buffer[buff_read_idx];
        buff_read_idx = (buff_read_idx + 1) % CMD_BUFFER_DIM;
        if (pthread_mutex_unlock(&mutex_read) != 0)
          fprintf(stderr, "Erro a manipular mutex.\n");
        if (sem_post(&sem_write) != 0)
          perror("Erro a assinalar o semáforo.");

        switch(cmd.operacao) {
            case OP_LER:
                if ((saldo = lerSaldo (cmd.idConta1)) < 0)
                    printf("%s(%d): Erro.\n\n", COMANDO_LER_SALDO, cmd.idConta1);
                else
                    printf("%s(%d): O saldo da conta é %d.\n\n", COMANDO_LER_SALDO, cmd.idConta1, saldo);
                break;

            case OP_CREDITAR:
                if (creditar(cmd.idConta1, cmd.valor) < 0)
                    printf("%s(%d, %d): Erro\n\n", COMANDO_CREDITAR, cmd.idConta1, cmd.valor);
                else
                    printf("%s(%d, %d): OK\n\n", COMANDO_CREDITAR, cmd.idConta1, cmd.valor);
                break;

            case OP_DEBITAR:
                if (debitar (cmd.idConta1, cmd.valor) < 0)
                    printf("%s(%d, %d): Erro\n\n", COMANDO_DEBITAR, cmd.idConta1, cmd.valor);
                else
                    printf("%s(%d, %d): OK\n\n", COMANDO_DEBITAR, cmd.idConta1, cmd.valor);
                break;

			case OP_TRANSFERIR:
				if (transferir(cmd.idConta1, cmd.idConta2, cmd.valor) < 0)
					printf("Erro ao transferir valor da conta %d para a conta %d.\n\n", cmd.idConta1, cmd.idConta2);
				else
					printf("%s(%d, %d, %d): OK\n\n", COMANDO_TRANSFERIR, cmd.idConta1, cmd.idConta2, cmd.valor);
				break;

			case OP_SIMULAR:
				if (pthread_mutex_lock(&mutex_simular) != 0)
					fprintf(stderr, "Erro a manipular mutex.\n");

				trabalhadoras_espera++;

				if(pthread_cond_signal(&cond_trabalhadoras) != 0)
					fprintf(stderr, "Erro a manipular condição.");

				while (simular_acabado == 0)
					if (pthread_cond_wait(&cond_main, &mutex_simular) != 0)
						fprintf(stderr, "Erro a manipular condição.");

				if (pthread_mutex_unlock(&mutex_simular) != 0)
					fprintf(stderr, "Erro a manipular mutex.\n");
				break;

            case OP_SAIR:
                return NULL;

            case OP_ERRO:
                fprintf(stderr, "Erro na leitura do buffer.\n");
                break;

            default:
                fprintf(stderr, "Erro na leitura do buffer.\n");
                break;

        }
    }
}

comando_t criaComando(op operacao, int idConta1, int idConta2, int valor) {
    comando_t novocomando;
    novocomando.operacao = operacao;
    novocomando.idConta1 = idConta1;
    novocomando.idConta2 = idConta2;
    novocomando.valor = valor;
    return novocomando;
}

void insereComando(comando_t cmd) {
    if (sem_wait(&sem_write) != 0)
      perror("Erro a esperar pelo semáforo.");
    if (pthread_mutex_lock(&mutex_write) != 0)
      fprintf(stderr, "Erro a manipular mutex.\n");
    cmd_buffer[buff_write_idx] = cmd;
    buff_write_idx = (buff_write_idx + 1) % CMD_BUFFER_DIM;
    if (pthread_mutex_unlock(&mutex_write) != 0)
      fprintf(stderr, "Erro a manipular mutex.\n");
    if (sem_post(&sem_read) != 0)
      perror("Erro a assinalar o semáforo.");
}
