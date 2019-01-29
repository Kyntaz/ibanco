#include "contas.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>

#define atrasar() sleep(ATRASO)

int contasSaldos[NUM_CONTAS];
int saiFlag = 0;
pthread_mutex_t mutex_contas[NUM_CONTAS];

void fecha_transf(int id1, int id2);
void abre_transf(int id1, int id2);

int contaExiste(int idConta) {
  return (idConta > 0 && idConta <= NUM_CONTAS);
}

void inicializarContas() {
  int i;
  for (i=0; i<NUM_CONTAS; i++)
    contasSaldos[i] = 0;
  inicializaMutexContas();
}

int debitar(int idConta, int valor) {
  atrasar();
  if (!contaExiste(idConta))
    return -1;
  if (pthread_mutex_lock(&mutex_contas[idConta-1]) != 0)
    fprintf(stderr, "Erro a manipular mutex.\n");
  if (contasSaldos[idConta - 1] < valor) {
    if (pthread_mutex_unlock(&mutex_contas[idConta-1]) != 0)
      fprintf(stderr, "Erro a manipular mutex.\n");
      return -1;
  }
  atrasar();
  contasSaldos[idConta - 1] -= valor;
  if (pthread_mutex_unlock(&mutex_contas[idConta-1]) != 0)
    fprintf(stderr, "Erro a manipular mutex.\n");
  return 0;
}

int creditar(int idConta, int valor) {
  atrasar();
  if (!contaExiste(idConta))
    return -1;
  if (pthread_mutex_lock(&mutex_contas[idConta-1]) != 0)
    fprintf(stderr, "Erro a manipular mutex.\n");
  contasSaldos[idConta - 1] += valor;
  if (pthread_mutex_unlock(&mutex_contas[idConta-1]) != 0)
    fprintf(stderr, "Erro a manipular mutex.\n");
  return 0;
}

int lerSaldo(int idConta) {
  int saldo;
  atrasar();
  if (!contaExiste(idConta))
    return -1;
  if (pthread_mutex_lock(&mutex_contas[idConta-1]) != 0)
    fprintf(stderr, "Erro a manipular mutex.\n");
  saldo = contasSaldos[idConta - 1];
  if (pthread_mutex_unlock(&mutex_contas[idConta-1]) != 0)
    fprintf(stderr, "Erro a manipular mutex.\n");
  return saldo;
}

int transferir(int idConta1, int idConta2, int valor) {
  atrasar();
  if (!contaExiste(idConta1) || !contaExiste(idConta2)) return -1;
  if (idConta1 == idConta2) return -1;
  if (valor < 0) return -1;

  fecha_transf(idConta1-1, idConta2-1);
  if (contasSaldos[idConta1 - 1] < valor) {
    abre_transf(idConta1-1, idConta2-1);
    return -1;
  }
  contasSaldos[idConta1 - 1] -= valor;
  contasSaldos[idConta2 - 1] += valor;
  abre_transf(idConta1-1, idConta2-1);
  return 0;
}

void simular(int numAnos) {
  int i;
  for (i = 0; i <= numAnos; i++) {
    int j;
    printf("SIMULACAO: Ano %d\n=================\n", i);
    for (j = 1; contaExiste(j); j++) {
      int varSaldo;
      int saldoInicial = lerSaldo(j);

      printf("Conta %d, Saldo %d\n", j, saldoInicial);
      varSaldo = saldoInicial * TAXAJURO - CUSTOMANUTENCAO;
      varSaldo = (-varSaldo >= saldoInicial) ? saldoInicial : varSaldo;
      if (varSaldo > 0) creditar(j, varSaldo);
      else if (varSaldo < 0) debitar(j, -varSaldo);
    }
    printf("\n");

    if (saiFlag) {
      printf("Simulacao terminada por signal\n");
      exit(EXIT_SUCCESS);
    }
  }
}

void paraSimular() { saiFlag = 1; }

void inicializaMutexContas() {
  int i;
  for(i = 0; i < NUM_CONTAS; i++)
    if (pthread_mutex_init(&mutex_contas[i],NULL) != 0) {
      fprintf(stderr, "Erro a criar mutexes das contas.");
    }
}

void fecha_transf(int id1, int id2) {
  if (pthread_mutex_lock(&mutex_contas[id1<id2 ? id1 : id2]) != 0)
    fprintf(stderr, "Erro a manipular mutex.");
  if (pthread_mutex_lock(&mutex_contas[id1<id2 ? id2 : id1]) != 0)
    fprintf(stderr, "Erro a manipular mutex.");
}

void abre_transf(int id1, int id2) {
  if (pthread_mutex_unlock(&mutex_contas[id1<id2 ? id2 : id1]) != 0)
    fprintf(stderr, "Erro a manipular mutex.");
  if (pthread_mutex_unlock(&mutex_contas[id1<id2 ? id1 : id2]) != 0)
    fprintf(stderr, "Erro a manipular mutex.");
}
