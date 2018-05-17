#include <Windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#include <process.h>

void readTChars(TCHAR *p, int maxchars) {
	int len;
	_fgetts(p, maxchars, stdin);
	len = _tcslen(p);
	if (p[len - 1] == TEXT('\n'))
		p[len - 1] = TEXT('\0');
}

void pressEnter() {
	TCHAR somekeys[25];
	_tprintf(TEXT("\nPress enter > "));
	readTChars(somekeys, 25);
}

#define OPNAME_SZ 30
#define MSGTEXT_SZ 75

typedef struct _MSG {
	unsigned msgnum;
	TCHAR szOP[OPNAME_SZ];
	TCHAR szMessage[MSGTEXT_SZ];
} Shared_MSG;

TCHAR szName[] = TEXT("fmMsgSpace");

#define MSGBUFSIZE sizeof(Shared_MSG)

#define MUTEX_NAME TEXT("RWMUTEX")

typedef struct _ControlData {
	HANDLE hMapFile;
	Shared_MSG * shared;
	int ThreadDeveContinuar;
	TCHAR MyName[OPNAME_SZ];
	HANDLE hRWMutex;
} ContrData;

void writeMensagem(ContrData * pcdata, TCHAR * msgtext) {
	WaitForSingleObject(pcdata->hRWMutex, INFINITE);
	pcdata->shared->msgnum++;
	_tcscpy(pcdata->shared->szOP, pcdata->MyName);
	_tcscpy(pcdata->shared->szMessage, msgtext);
	ReleaseMutex(pcdata->hRWMutex);
}

void readMensagem(ContrData * pcdata, Shared_MSG * msg) {
	WaitForSingleObject(pcdata->hRWMutex, INFINITE);
	CopyMemory(msg, pcdata->shared, sizeof(Shared_MSG));
	ReleaseMutex(pcdata->hRWMutex);
}

unsigned peekMensagem(ContrData * pcdata) {
	unsigned msgnum;
	WaitForSingleObject(pcdata->hRWMutex, INFINITE);
	msgnum = pcdata->shared->msgnum;
	ReleaseMutex(pcdata->hRWMutex);
	return msgnum;
}

BOOL iniMemAndSync(ContrData* cdata) {
	cdata->hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, MSGBUFSIZE, szName);
	if (cdata->hMapFile == NULL) {
		_tprintf(TEXT("Problema na memoria partilhada(%d).\n"), GetLastError());
		return FALSE;
	}
	cdata->hRWMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
	if (cdata->hRWMutex == NULL) {
		_tprintf(TEXT("O mutex deu problemas (%d).\n"), GetLastError());
		return FALSE;
	}
	return TRUE;
}

unsigned int __stdcall listenerThread(void *p) {
	ContrData * pcd = (ContrData *)p;
	unsigned int current = peekMensagem(pcd);
	Shared_MSG rcv;
	while (pcd->ThreadDeveContinuar) {
		Sleep(500); // péssima ideia -> usar sinc
		if (peekMensagem(pcd) > current) {
			readMensagem(pcd, &rcv);
			current = rcv.msgnum;
			_tprintf(TEXT("[%d]: %s: %s\n"), current, rcv.szOP, rcv.szMessage);
			if (_tcscmp(rcv.szMessage, TEXT("FUGIR")) == 0)
				pcd->ThreadDeveContinuar = 0;
		}
	}
	return 0;
}

int _tmain() {
	ContrData cdata;
	DWORD tid;
	HANDLE thnd;

	TCHAR myText[MSGTEXT_SZ];
	Shared_MSG currentMSG;
	_tprintf(TEXT("\nCliente de Msg v.2a\n"));
	_tprintf(TEXT("\nNome -> "));
	readTChars(cdata.MyName, OPNAME_SZ);

	cdata.hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, szName);

	if (cdata.hMapFile == NULL) {
		_tprintf(TEXT("\nPrimeiro Cliente - Criar Memoria e Mutex."));
		if (!iniMemAndSync(&cdata)) {
			_tprintf(TEXT("Impossível prossegir (erro: %d)."), GetLastError());
			exit(1);
		}
		_tprintf(TEXT("\nMemoria mapeada e mutex criados"));
	}
	else {
		cdata.hRWMutex = OpenMutex(SYNCHRONIZE, TRUE, MUTEX_NAME);
		if (cdata.hRWMutex == NULL) {
			_tprintf(TEXT("O mutex deu azar (erro: %d). Sair.\n"), GetLastError());
			return FALSE;
		}
	}

	_tprintf(TEXT("Vou Criar a view da memoria partilhada"));

	cdata.shared = (Shared_MSG *)MapViewOfFile(cdata.hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, MSGBUFSIZE);

	if (cdata.shared == NULL) {
		_tprintf(TEXT("Erro na view da mem partilhada (erro: %d). \n"), GetLastError());
		CloseHandle(cdata.hMapFile);
		return 1;
	}

	_tprintf(TEXT("vou lançar thread para ouvir o que se passa.\n"));
	cdata.ThreadDeveContinuar = 1;
	_beginthreadex(0, 0, listenerThread, &cdata, 0, &tid);
	thnd = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
	_tprintf(TEXT("Tudo ok. Vou ver a mensagem actual.\n"));
	readMensagem(&cdata, &currentMSG);
	_tprintf(TEXT("(%d): %s\n"), currentMSG.msgnum, currentMSG.szMessage);

	_tprintf(TEXT("Escreve mensagens. exit para sair, FUGIR para sairem todos\n"));
	while (1) {
		readTChars(myText, MSGTEXT_SZ);
		if (_tcscmp(myText, TEXT("exit")) == 0)
			break;
		if (cdata.ThreadDeveContinuar == 0) {
			_tprintf(TEXT("Entretanto alguém mandou fugir. Sair.\n"));
			break;
		}
		writeMensagem(&cdata, myText);
	}

	_tprintf(TEXT("Cliente vai fechar.\n"));
	cdata.ThreadDeveContinuar = 0;
	WaitForSingleObject(thnd, INFINITE);

	_tprintf(TEXT("thread ouvinte encerrada.\n"));
	UnmapViewOfFile(cdata.shared);
	CloseHandle(cdata.hMapFile);
	_tprintf(TEXT("Ficheiro desmapeado e recursos libertados.\n"));
	return 0;
}