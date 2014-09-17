#include <stdio.h>
#include <string.h>
#include <time.h>
#include "rc4.h"
#include "network.h"
#include "logmsg.h"
#include "service.h"

#define USAGE "USAGE: tcpredirect [-k [<left-key>]:[<right-key>]] \n" \
	"\t[-b [left-buffer-size]:[right-buffer-size]]\n" \
	"\t[-t [left-timeout]:[right-timeout]]\n" \
	"\t[<left-addr>]:<left-port>:[<right-addr>]:<right-port>\n"

static service_t *s = NULL;

#ifdef _WIN32
#include <windows.h>

BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
    switch(CEvent)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
		if (s) {
			fdevent_stop(s->ev);
		}
		break;
    }
    return TRUE;
}

static void setup(int argc, const char *argv[])
{
	int error;
	WSADATA wsa;

	error = WSAStartup (0x0202, &wsa);
	if(error) {
		die("ERROR Starting up windows socket\n");
	}
	if (wsa.wVersion != 0x0202) {
		WSACleanup();
		die("ERROR Socket version mismatch\n");
	}
	if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler,TRUE)==FALSE) {
		DWORD dwError = GetLastError();
		WSACleanup();
		die("ERROR Set console handler, code %d\n", dwError);
    }
}

static void cleanup()
{
	WSACleanup();
}

#elif __linux
#include <signal.h>

static void sig_handler(int signo)
{
	if (s) {
		fdevent_stop(s->ev);
	}
}

static void setup(int argc, const char *argv[])
{
	if (signal(SIGTERM, sig_handler)==SIG_ERR || 
		signal(SIGINT, sig_handler)==SIG_ERR) {
		die("ERROR Set signal handler\n");
	}
}

static void cleanup()
{
}

#endif

int main(int argc, const char *argv[])
{
	setup(argc, argv);

	s = service_init();
	if (service_conf_parse(s->conf, argc, argv) != 0) {
		logmsg(USAGE);
	} else if (net_create_server(s->conf->leftaddr, s->conf->leftport, &s->fdsrv)!=0) {
		logmsg("ERROR Create server socket\n");
	} else {
		fdevent_register(s->ev, s->fdsrv, NULL);
		fdevent_set(s->ev, s->fdsrv, FDEVENT_READ|FDEVENT_EXCEPT);
		fdevent_loop(s->ev);
	}

	service_destroy(s);
	s = NULL;
	cleanup();
	return 0;
}
