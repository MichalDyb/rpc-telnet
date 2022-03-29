#include "telnet.h"
#include <stdio.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pty.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

// Na początkou należy uruchomić rpcbind
// KOMPILACJA Z FLAGĄ -lutil -lpthread 
#define MAX_HOST 20 // Deklaracja i incjalizacja stałej przechowujacej ilość maksymalną liczbe klientów
#define TIMEOUT_SECONDS 5 * 60// Deklaracja i incjalizacja stałej przechowujacej maskymalny czas bezczynności klienta

struct telnet_client // Definicja struktury przechowujacej dane klienta
{
	int master, slave; // Deskryptory PTY używane do wysłania polecenia do shella
	bool active; // Flaga sprawdzająca czy struktura jest wykorzystywana przez jakiegoś klienta
	int shellFdRW[2]; // Deskryptory pipe używane do odczytu danych z shella
	pid_t shellPid; // Identyfikator procesu shella
	time_t lastTimeUsed; // Zmienna przechowująca w sekundach czas ostatniego żądania klienta
};
typedef struct telnet_client telnet_client; // Definicja typu

void* drop_unused_connections(void* pclient) // Definicja funkcji wątku zwalniająca zbyt długo nie używane struktury klientów
{
	telnet_client *clients = pclient;	
	time_t current_time = time(NULL);
	for(int i = 0; i < MAX_HOST; i++)
	{
		if(clients[i].active)
			if(current_time - clients[i].lastTimeUsed >= TIMEOUT_SECONDS)
			{
				clients[i].active = false;
				kill(clients[i].shellPid, SIGKILL); // Wysyłamy sygnał SIGKILL do procesu potomnego (shella)
				close(clients[i].shellFdRW[0]);
				printf("|Server|: Zakończono połączenie z klientem (id klienta = %d) z powodu braku aktywności.\n", i + 1);
			}
	}
	return NULL;
}

MyData * execute_1_svc(MyData *argp, struct svc_req *rqstp)
{
	static MyData result; 
	static telnet_client clients[MAX_HOST];

	pthread_t thread_id;
	pthread_create(&thread_id, NULL, drop_unused_connections, clients); // Wywołanie wątku sprawdzającego, czy przekroczono TIMEOUT

	// Inicjalizacja struktury zawierającej argumenty przekazywane do procedury zdalnej
	result.message.message_val = "";
	result.message.message_len = 0;
	result.client_id = -1; // Id mniejsze od 0 oznacza, że nie przyznano jeszcze id klienta

	if(argp->state == 0) // Odebrano prośbę od klienta o próbie nawiązania połączenia z serwerem
	{
		result.state = 0; // Ustawienie stanu połączenia na jaki oczekuje teraz klient
		for(int i = 0; i < MAX_HOST; i++) // Sprawdzenie czy jest wolne miejsce w tablicy klientów
		{	
			if(!clients[i].active)
			{
				clients[i].active = true;
				clients[i].lastTimeUsed = time(NULL);
				result.client_id = i;
				if(clients[i].master == 0 || clients[i].slave == 0)
					openpty(&clients[i].master, &clients[i].slave, NULL, NULL, NULL); // Utworzenie PTY
				pipe(clients[i].shellFdRW);
				if ((clients[i].shellPid = fork()) == 0) // Utworzenie procesu potomnego dla shella
				{
					dup2(clients[i].slave, 0); // Podłączenie strony slave PTY do standardowego wejścia procesu potomnego
					dup2(clients[i].shellFdRW[1], 1); // Podłączenie strony zapisu pipe do standardowego wyjścia procesu potomnego
					dup2(clients[i].shellFdRW[1], 2); // Podłączenie strony zapisu pipe do standardowego wyjścia błędów procesu potomnego
					close(clients[i].shellFdRW[0]); // Zamknięcie nieużywanej strony odczytu pipe w procesie potomnym
					execl("/bin/sh", "-i", (const char*)NULL); // Zastąpienie obrazu aktualnego programu w przestrzeni adresowej procesu obrazem programu shella. Wykonanie programu /bin/sh z parametrm -i (powłoka interaktywna) oraz standardowym środowiskiem z zmiennej **environ
				}
				close(clients[i].shellFdRW[1]); // Zamknięcie nieużywanej strony zapisu pipe w procesie głównym
				printf("|Server|: Nawiązano połączenie z nowym klientem (id klienta = %d).\n", i + 1);
				break;
			}
		}
	}
	else if(argp->state == 1) // Odebranie komendy od klienta do wykonania w powłoce
	{
		result.state = 1; // Ustawienie stanu połączenia na jaki oczekuje teraz klient
		if(argp->client_id > -1 && argp->client_id < MAX_HOST) // Sprawdzenie czy id klienta znajduje się w zakresie
		{
			if(!clients[argp->client_id].active) // Sprawdzenie czy dane połączenie jest nie aktywne
			{
				result.state = 3; // Ustawienie stanu połączenia, aby poinformować klienta o błędzie
			}
			else // Jeśli połączenie jest aktywne serwer wykonuje komende od klienta 
			{ 
				/* Odebranie wiadomości od klienta */
				int commandLen = argp->message.message_len;
				char command[commandLen];
				bzero(command, commandLen); // wyczyszczenie obszaru pamięci
				for(int i = 0; i < commandLen + 1; i++)
					command[i] = argp->message.message_val[i];
				command[commandLen] = '\0';

				if(strcmp(command, "exit") == 0)
				{
					kill(clients[argp->client_id].shellPid, SIGKILL); // Wysyłamy sygnał SIGKILL do procesu potomnego (shella)
					close(clients[argp->client_id].shellFdRW[0]);

					pipe(clients[argp->client_id].shellFdRW);
					if ((clients[argp->client_id].shellPid = fork()) == 0) // Utworzenie procesu potomnego dla shella
					{
						dup2(clients[argp->client_id].slave, 0); // Podłączenie strony slave PTY do standardowego wejścia procesu potomnego
						dup2(clients[argp->client_id].shellFdRW[1], 1); // Podłączenie strony zapisu pipe do standardowego wyjścia procesu potomnego
						dup2(clients[argp->client_id].shellFdRW[1], 2); // Podłączenie strony zapisu pipe do standardowego wyjścia błędów procesu potomnego
						close(clients[argp->client_id].shellFdRW[0]); // Zamknięcie nieużywanej strony odczytu pipe w procesie potomnym
						execl("/bin/sh", "-i", (const char*)NULL); // Zastąpienie obrazu aktualnego programu w przestrzeni adresowej procesu obrazem programu shella. Wykonanie programu /bin/sh z parametrm -i (powłoka interaktywna) oraz standardowym środowiskiem z zmiennej **environ
					}
					close(clients[argp->client_id].shellFdRW[1]); // Zamknięcie nieużywanej strony zapisu pipe w procesie głównym
					printf("|Server-%d|: exit\n", argp->client_id + 1);
				}
				else
				{
					char msg[4095]; // Zmienna przechowująca wiadomośc odczytaną z shella
					fd_set rfds; // Struktura opisująca zestaw deskryptorów
					struct timeval tv; // Struktura opisująca czas użyta do ustawienia opóźnienia dla select
					tv.tv_sec = 0; // 0s
					tv.tv_usec = 0; // 0ms

					bzero(msg, 4095); // Wyzerowanie zmiennych
					printf("|Server-%d|: %s\n", argp->client_id + 1, command);

					write(clients[argp->client_id].master, command, commandLen); // Wysyłamy polecenie do shella
					write(clients[argp->client_id].master, "\r\n", 2); // Symulacja Enter

					FD_ZERO(&rfds); // Wyzerowanie struktury zestawu deskryptorów
					FD_SET(clients[argp->client_id].shellFdRW[0], &rfds); // Dodanie deskryptora do zestawu deskryptorów

					sleep(5);
					if(select(clients[argp->client_id].shellFdRW[0] + 1, &rfds, NULL, NULL, &tv)) // Monitorowanie deskryptora pod kątem możliwości wykonania operacji I/O
					{
						result.message.message_len = read(clients[argp->client_id].shellFdRW[0], msg, 4095 - 1); // Odczytanie wyniku działania polecenia
						msg[result.message.message_len] = '\0'; // Dodanie znaku końca ciągu znaków
						result.message.message_val = msg;
					}
				}
			}
		}
		else
			result.state = 3; // Ustawienie stanu połączenia, aby poinformować klienta o błędzie
	}
	else if(argp->state == 2) // Odebrano informację od klienta o zakończeniu połączenia z serwerem
	{
		result.state = 2; // Ustawienie stanu połączenia na jaki oczekuje teraz klient
		if(argp->client_id >= 0 && argp->client_id < MAX_HOST) // Sprawdzenie czy id klienta znajduje się w zakresie
			if(clients[argp->client_id].active) // Sprawdzenie czy dane połączenie jest aktywne
			{
				clients[argp->client_id].active = false; // Ustawienie danej struktury klienta na wolną
				kill(clients[argp->client_id].shellPid, SIGKILL); // Wysyłamy sygnał SIGKILL do procesu potomnego (shella)
				close(clients[argp->client_id].shellFdRW[0]);
				printf("|Server|: Zakończono połączenie z klientem (id klienta = %d).\n", argp->client_id + 1);
			}
	}
	else // Jeśli klient podan stanu połączenia który nie istnieje
	{
		result.state = 3; // Ustawienie stanu połączenia, aby poinformować klienta o błędzie
		printf("|Server|: Otrzymano nieprawidłowe żądanie od klienta, które zostanie zignorowane (id klienta = %d).\n", argp->client_id + 1);
	}

	return &result; // Zwrócenie struktury klientowi
}

#ifndef SIG_PF
#define SIG_PF void(*)(int)
#endif

static void my_program_1(struct svc_req *rqstp, register SVCXPRT *transp)
{
	union {
		MyData execute_1_arg;
	} argument;
	char *result;
	xdrproc_t _xdr_argument, _xdr_result;
	char *(*local)(char *, struct svc_req *);

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply (transp, (xdrproc_t) xdr_void, (char *)NULL);
		return;

	case EXECUTE:
		_xdr_argument = (xdrproc_t) xdr_MyData;
		_xdr_result = (xdrproc_t) xdr_MyData;
		local = (char *(*)(char *, struct svc_req *)) execute_1_svc;
		break;

	default:
		svcerr_noproc (transp);
		return;
	}
	memset ((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
		svcerr_decode (transp);
		return;
	}
	result = (*local)((char *)&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, (xdrproc_t) _xdr_result, result)) {
		svcerr_systemerr (transp);
	}
	if (!svc_freeargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
		printf("|Error|: Wystąpił błąd, nie udało się zwolnić argumentów.\n");
		exit(1);
	}
	return;
}

int main (int argc, char **argv)
{
	register SVCXPRT *transp;

	pmap_unset (MY_PROGRAM, MY_VERSION);

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		printf("|Error|: Wystąpił błąd, nie można utworzyć usługi tcp.\n");
		exit(1);
	}
	if (!svc_register(transp, MY_PROGRAM, MY_VERSION, my_program_1, IPPROTO_TCP)) {
		printf("|Error|: Wystąpił błąd, nie udało się zarejestrować (MY_PROGRAM, MY_VERSION, tcp).\n");
		exit(1);
	}

	svc_run();
	printf("|Error|: Wystąpił błąd, serwer przestał funkcjonować. (Powrót z funkcji svc_run()).\n");
	exit(1);
}
