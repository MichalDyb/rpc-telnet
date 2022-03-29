#include <stdio.h>
#include <string.h>
#include "telnet.h"
#include <memory.h>

static struct timeval TIMEOUT = { 25, 0 }; // Deklaracja i inicjalizacja zmiennej przechowującej timeout oczekiwania na wiadomość zwrotną z wywołanej procedury zdalnej
MyData * execute_1(MyData *argp, CLIENT *clnt) // Deklarcja i defnicja funkcji wywołującej procedurę zdalną
{
	static MyData clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call (clnt, EXECUTE,
		(xdrproc_t) xdr_MyData, (caddr_t) argp,
		(xdrproc_t) xdr_MyData, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

void main(int argc, char* argv[])
{
    // Deklaracja i inicjalizacja zmiennych
    char *host; // Zmienna przechowująca adres hosta
	char msg[4095]; // Zmienna przechowująca wiadomośc odczytaną z klawiatury	
    CLIENT *clnt = NULL; // Deklaracja uchwytu klienta
	MyData  inPut; // Struktura przechowująca argumenty przekazywane do procedury zdalnej
	MyData  *outPut; // Struktura przechowująca wyniki otrzymane dzięki wywołaniu procedury zdalnej

	// Inicjalizacja struktury zawierającej argumenty przekazywane do procedury zdalnej
	inPut.message.message_val = "";
	inPut.message.message_len = 0;
	inPut.state = 0;
	inPut.client_id = -1; // Id mniejsze od 0 oznacza, że nie przyznano jeszcze id klienta

	if (argc < 2) // Sprawdzenie czy podano odpowiednią ilość argumentów wejściowych podczas uruchomienia programu
	{ 
		printf ("|Error|: Błąd utworzenia uchwytu dla klienta, nie podano adresu ip hosta.\n");
		exit(1);
	}
	host = argv[1]; // Pobranie adres hosta z argumentów wejściowych programu

	clnt = clnt_create (host, MY_PROGRAM, MY_VERSION, "tcp"); // Utworzenie uchwytu klienta
	if (clnt == NULL) 
	{
		printf ("|Error|: Błąd utworzenia uchwytu dla klienta, brak połączenia lub nieprawidłowy adres hosta.\n");
		exit(1);
	}

	outPut = execute_1(&inPut, clnt); // Wywołanie procedury zdalnej w celu otrzymania id klienta
	if (outPut == (MyData *) NULL) // Sprawdzenie czy otrzymano wiadomość zwrotną od wywołanej procedury zdalnej
	{ 
		printf ("|Error|: Brak odpowiedzi od serwera, połączenie zostało zerwane.\n");
		if(clnt != NULL)
			clnt_destroy(clnt); // Usunięcie uchwytu klienta
		exit(1); // Nieudane wyjście z programu
	}

	if(outPut->client_id < 0)
	{
		printf ("|Client|: Serwer odmówił połącznia z powodu przekroczonia liczby hostów. Spróbuj później.\n");
		if(clnt != NULL)
			clnt_destroy(clnt); // Usunięcie uchwytu klienta
		exit(0); // Nieudane wyjście z programu
	}
	else 
	{
		inPut.state = 1;
		inPut.client_id = outPut->client_id;
		// Wiadomość powitalna
		printf("|Client|: Nawiązano połączenie z serwerem (id klienta = %d).\n", outPut->client_id + 1);
		printf("|Client|: Podaj komendę i potwierdź klawiszem enter. Pusty ciąg znaków kończy działanie programu.\n"); 
	}

    while (1)
    {
		printf("|Client|: ");
		bzero(msg, sizeof(msg)); // Wyczyszczenie obszaru pamięci tablicy msg
		memset(msg, '\0', sizeof(msg));
		fgets(msg, sizeof msg, stdin); // Oczekiwanie na polecenie
		if(strlen(msg) == 1 && msg[0] == '\n') // Dla pustego ciągu znaków, kończy działanie programu
			break; // Udane wyjście z programu
		msg[strlen(msg) - 1] = '\0';

		inPut.message.message_val= msg;
		inPut.message.message_len = strlen(msg);
		
		outPut = execute_1(&inPut, clnt); // Wywołanie procedury zdalnej
		if (outPut == (MyData *) NULL) // Sprawdzenie czy otrzymano wiadomość zwrotną od wywołanej procedury zdalnej
		{ 
			printf ("|Client|: Brak odpowiedzi od serwera, połączenie zostało zerwane.\n");
			if(clnt != NULL)
				clnt_destroy(clnt); // Usunięcie uchwytu klienta
			exit(1); // Nieudane wyjście z programu
		}

		if(outPut->state == 3)
		{
			printf ("|Client|: Serwer zgłosił błąd, połączenie zostało zerwane.\n");
			if(clnt != NULL)
				clnt_destroy(clnt); // Usunięcie uchwytu klienta
			exit(1); // Nieudane wyjście z programu
		}

		if(outPut->message.message_len > 0)
			printf("%s", outPut->message.message_val); // Wyświetlenie wiadomości otrzymanej poprzez wywołanie metody zdalnej
	} 
	
	inPut.state = 2;
	outPut = execute_1(&inPut, clnt); // Wywołanie procedury zdalnej w celu poinformowania serwera o zakończeniu połączenia
	if (outPut == (MyData *) NULL) // Sprawdzenie czy otrzymano wiadomość zwrotną od wywołanej procedury zdalnej
	{ 
		printf ("|Error|: Brak odpowiedzi od serwera, połączenie zostało zerwane.\n");
		if(clnt != NULL)
			clnt_destroy(clnt); // Usunięcie uchwytu klienta
		exit(1); // Nieudane wyjście z programu
	}

	if(clnt != NULL) { 
		clnt_destroy(clnt); // Usunięcie uchwytu klienta
		printf("|Client|: Zakończono połączenie z serwerem.\n");
	}
	printf("|Client|: Działanie aplikacji zostało zakończone.\n");
    exit(0); // Udane wyjście z programu
}