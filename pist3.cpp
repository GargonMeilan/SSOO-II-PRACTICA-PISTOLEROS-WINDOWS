//JON GARCIA GONZALEZ 

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "pist3.h"


#define ERR 100
#define minPistoleros 2
#define maxPistoleros 26
//Struct mem compartida
typedef struct memC {
	int nPist;
	int contador;

	int nMuertos;
	int nVivos;
	int flagsVivos[26];
} memtype;



//Cargar DLL
HINSTANCE fDLL = LoadLibrary(TEXT("pist3.dll"));



//Seccion critica
CRITICAL_SECTION sc;

//Evento
HANDLE pistEvent;
HANDLE apuntarEvent;
HANDLE dispararEvent;
HANDLE morirEvent;
HANDLE esperarHijos;

int(*PIST_inicio)(int, int, int);
int(*PIST_nuevoPistolero)(char);
char(*PIST_vIctima)(void);
int(*PIST_disparar)(char);
void(*PIST_morirme)(void);
int(*PIST_fin)(void);
void(*pon_error)(char*);


//FICHERODEPURACION
FILE* guion = NULL;
char cadena[1000];/*
guion = fopen("C:\\Users\\Jon\\Desktop\\Guion.txt", "a");
sprintf(cadena, "\nHay %d psitoleros a %d velocidad semilla %d\n", nPistoleros, velocidad, semilla);
fputs(cadena, guion);
fclose(guion);*/

//FuncionPistoleros
DWORD WINAPI funcionPistoleros(LPVOID parametro);


int main(int argc, char* argv[])
{
	
    int nPistoleros;
    int velocidad;
    int semilla = 0;
	int i;
	
	
	//Paso de argumentos
	if (argc > 4 || argc < 3) {
		fprintf(stderr, "\n Parametros incorrecto\n");
		exit(ERROR);
	}
	else {
		if (argc == 4) {
			nPistoleros = atoi(argv[1]);
			velocidad = atoi(argv[2]);
			semilla = atoi(argv[3]);
		}
		else {
			nPistoleros = atoi(argv[1]);
			velocidad = atoi(argv[2]);
			semilla = 0;
		}
		if (nPistoleros<minPistoleros || nPistoleros>maxPistoleros) {
			fprintf(stderr, "\n Numero de pistoleros incorrecto (2-26)\n");
			exit(ERROR);
		}
		if (velocidad < 0) {
			fprintf(stderr, "\n La velocidad tiene que ser 0 o mayor\n");
			exit(ERROR);
		}
	}

	//Seccion critica
	InitializeCriticalSection(&sc);
	
	//evento
	pistEvent = CreateEvent(NULL, TRUE, FALSE, L"pistEvent");
	if (pistEvent == NULL) { perror("Error Evento"); exit(ERR); }

	apuntarEvent = CreateEvent(NULL, TRUE, FALSE, L"apuntarEvent");
	if (apuntarEvent == NULL) { perror("Error Evento"); exit(ERR); }

	dispararEvent = CreateEvent(NULL, TRUE, FALSE, L"dispararEvent");
	if (dispararEvent == NULL) { perror("Error Evento"); exit(ERR); }

	morirEvent = CreateEvent(NULL, TRUE, FALSE, L"morirEvent");
	if (morirEvent == NULL) { perror("Error Evento"); exit(ERR); }

	esperarHijos = CreateEvent(NULL, TRUE, FALSE, L"esperarHijos");
	if (esperarHijos == NULL) { perror("Error Evento"); exit(ERR); }



	

	
	//LIBRERIA DLL
	if (fDLL == NULL) { perror("ERROR EN CREAR LIBRERIA"); exit(ERR); }
	PIST_inicio = (int(*)(int,int,int))GetProcAddress(fDLL, "PIST_inicio");
	PIST_nuevoPistolero = (int(*)(char))GetProcAddress(fDLL, "PIST_nuevoPistolero");
	PIST_vIctima = (char(*)(void))GetProcAddress(fDLL, "PIST_vIctima");
	PIST_disparar = (int(*)(char))GetProcAddress(fDLL, "PIST_disparar");
	PIST_morirme = (void(*)(void))GetProcAddress(fDLL, "PIST_morirme");
	PIST_fin = (int(*)(void))GetProcAddress(fDLL, "PIST_fin");

	pon_error = (void(*)(char*))GetProcAddress(fDLL, "pon_error");

	//Memoria Compartida
	HANDLE mem;
	LPVOID refMem;
	if (NULL == (mem = CreateFileMapping((HANDLE)-1, NULL, PAGE_READWRITE, 0, sizeof(memtype),L"memP"))) {
		perror("Error memoria");
		exit(ERR);
	}
	if (NULL == (refMem = (LPVOID)MapViewOfFile(mem, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(memtype)))) {
		perror("Error al mapear memoria");
		CloseHandle(mem);
		exit(ERR);
	}
	memtype n;
	n.contador = 0;	
	n.nPist = nPistoleros;
	n.nVivos = nPistoleros;
	n.nMuertos = 0;
	for (i = 0; i < nPistoleros; i++) {
		n.flagsVivos[i] = 1;
	}
	for (i = nPistoleros; i < 26; i++) {
		n.flagsVivos[i] = 0;
	}
	CopyMemory(refMem, &n, sizeof(memtype));
	
	PIST_inicio(nPistoleros, velocidad, semilla);

    
	

	HANDLE pistoleros[maxPistoleros];//Manejador para cada hilo
	
	//CREAR UN HILO POR CADA PISTOLERO
	for (i = 0; i < nPistoleros; i++) {
		
		pistoleros[i] = CreateThread(NULL, 0, funcionPistoleros, LPVOID(i), 0, NULL);
		if (pistoleros[i] == NULL) { perror("ERROR AL CREAR EL HILO"); exit(ERR); }
		
	}
	

	WaitForSingleObject(esperarHijos, INFINITE);



	PIST_fin();


	//Cerrar manejadores de hijos
	for (i = 0; i < nPistoleros; i++) {
		CloseHandle(pistoleros[i]);
	}
	//Cerrar eventos
	CloseHandle(pistEvent);
	CloseHandle(apuntarEvent);
	CloseHandle(morirEvent);
	CloseHandle(dispararEvent);
	CloseHandle(esperarHijos);
	//Cerrar memoria
	UnmapViewOfFile(refMem);	
	CloseHandle(mem);
	//Cerrar libreria
	FreeLibrary(fDLL);
    return 0;
}




DWORD WINAPI funcionPistoleros(LPVOID parametro) {

	EnterCriticalSection(&sc);//Creacion pistolero
	HANDLE memoria = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, L"memP");
	LPVOID m = (LPVOID)MapViewOfFile(memoria, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(int));
	memtype* memP = NULL;
	char victima;
	
	int idVictima;

	
	int id = (int)parametro;
	char name = id+65;
	PIST_nuevoPistolero(name);	
	memP = (memtype*)m;	
	
	
	
	LeaveCriticalSection(&sc);

	EnterCriticalSection(&sc);//ESPERAR A QUE TODOS HAYAN SIDO CREADOS
	memP->contador = memP->contador + 1;
	
	if (memP->contador == memP->nVivos) {
		memP->contador = 0;
		SetEvent(pistEvent);
	}
	LeaveCriticalSection(&sc);
	WaitForSingleObject(pistEvent, INFINITE);
	


	while (memP->nVivos > 0) {

		victima = PIST_vIctima();
				
		EnterCriticalSection(&sc);//ESPERAR A QUE TODOS HAYAN APUNTADO-
		memP->contador = memP->contador + 1;
		
		if (memP->contador == memP->nVivos) {
			memP->contador = 0;
			ResetEvent(morirEvent);
			SetEvent(apuntarEvent);
		}
		LeaveCriticalSection(&sc);
		WaitForSingleObject(apuntarEvent, INFINITE);
		
	

		

		PIST_disparar(victima); 
		idVictima = victima - 65;
		memP->flagsVivos[idVictima]=0;

		EnterCriticalSection(&sc);//ESPERAR A QUE TODOS HAYAN DISPARADO-
		memP->contador = memP->contador + 1;
		
		if (memP->contador == memP->nVivos) {
			memP->contador = 0;
			ResetEvent(apuntarEvent);
			SetEvent(dispararEvent);
		}
		LeaveCriticalSection(&sc);
		WaitForSingleObject(dispararEvent, INFINITE);

		//COMPROBAR SI HAN MUERTO
		if(memP->flagsVivos[id]==0){
			PIST_morirme();
			EnterCriticalSection(&sc);
			memP->nMuertos = memP->nMuertos + 1;
			memP->contador = memP->contador + 1;
			if (memP->contador == memP->nVivos) {
				memP->contador = 0;
				memP->nVivos = memP->nVivos - memP->nMuertos;
				memP->nMuertos = 0;
				ResetEvent(dispararEvent);
				SetEvent(morirEvent);
			}

			if (memP->nVivos == 0) {
				SetEvent(esperarHijos);
			}
			
			
			LeaveCriticalSection(&sc);

			UnmapViewOfFile(m);
			CloseHandle(memoria);
			ExitThread(0);
		}

		EnterCriticalSection(&sc);//ESPERAR A QUE  HAYAN MUERTO-
		memP->contador = memP->contador + 1;
		
		if (memP->contador == memP->nVivos) {
			memP->contador = 0;
			memP->nVivos = memP->nVivos - memP->nMuertos;
			ResetEvent(dispararEvent);
			SetEvent(morirEvent);
		}
		LeaveCriticalSection(&sc);
		WaitForSingleObject(morirEvent, INFINITE);

		

		if (memP->nVivos == 1) {
			UnmapViewOfFile(m);
			CloseHandle(memoria);
			SetEvent(esperarHijos);
			ExitThread(0);
		}
		
	}
	
		ExitThread(0);


}
