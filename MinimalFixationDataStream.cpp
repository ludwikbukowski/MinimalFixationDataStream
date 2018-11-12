/*
 * This is an example that demonstrates how to connect to the EyeX Engine and subscribe to the fixation data stream.
 *
 * Copyright 2013-2014 Tobii Technology AB. All rights reserved.
 */


#include <conio.h>
#include <assert.h>
#include "eyex/EyeX.h"

#include <string>
#include <istream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>//winsock

#include <windows.h>//threads
#include <tchar.h>//threads
#include <strsafe.h>//threads

#pragma comment(lib, "wininet.lib")//winsock
#pragma comment(lib,"Ws2_32.lib")//winsock

#include <regex>
#include "mylib.cpp"
#include "md5\md5.h"


#pragma comment (lib, "Tobii.EyeX.Client.lib")

// ID of the global interactor that provides our data stream; must be unique within the application.
static const TX_STRING InteractorId = "Rainbow Dash";

// global variables
static TX_HANDLE g_hGlobalInteractorSnapshot = TX_EMPTY_HANDLE;





#define MAX_THREADS 3
static DWORD   threadID;
static HANDLE  hThread;
static SOCKET clientfd;
static LPVOID lpParam;
DWORD WINAPI SendOnUserInput(LPVOID lpParam);

struct websocketT
{
	SOCKET* listenfd;
	SOCKET* connectfd;
	struct sockaddr_in server;
	struct sockaddr *clientInfo;//later filled in by call to accept()
	struct sockaddr_in *clientIPv4info;// represent a pointer to a IP
	std::string port;
	std::string ipAddress;
	std::string path;
	std::string origin;
	std::string location;
};







long long longParse(std::string str)
{
	long long result = 0;
	for (int i = 0; i < str.length(); i++)
	{
		unsigned digit = str[str.length() - 1 - i] - '0';
		result += digit * pow((double)10, i);
	}
	return result;
}

//assumes this machine is Little Endian
char* createMD5Buffer(int result1, int result2, char challenge[8])
{
	char* raw_answer = (char*)calloc(MD5_SIZE, sizeof(char));
	raw_answer[0] = ((unsigned char*)&result1)[3];
	raw_answer[1] = ((unsigned char*)&result1)[2];
	raw_answer[2] = ((unsigned char*)&result1)[1];
	raw_answer[3] = ((unsigned char*)&result1)[0];
	raw_answer[4] = ((unsigned char*)&result2)[3];
	raw_answer[5] = ((unsigned char*)&result2)[2];
	raw_answer[6] = ((unsigned char*)&result2)[1];
	raw_answer[7] = ((unsigned char*)&result2)[0];
	for (int i = 0; i < 8; i++)
	{
		raw_answer[8 + i] = challenge[i];
	}
	//display: (debugging)
	/*printf("raw answser bytes: ");
	for (int i = 0; i < MD5_SIZE; i++)
	printf(" %d", (unsigned char)raw_answer[i]);
	printf("\n\n");*/
	return raw_answer;
}



char* createHashstd(std::string key1, std::string key2, char* challenge)
{
	int spaces1 = 0;
	int spaces2 = 0;
	std::string digits1, digits2;
	int result1, result2;
	for (int i = 0; i < key1.length(); i++)
	{
		if (key1[i] == 0x20)
			spaces1++;
	}
	for (int i = 0; i < key2.length(); i++)
	{
		if (key2[i] == 0x20)
			spaces2++;
	}

	for (int i = 0; i < key1.length(); i++)
	{
		if (isdigit(key1[i]))
		{
			digits1 += key1[i];
		}
	}
	for (int i = 0; i < key2.length(); i++)
	{
		if (isdigit(key2[i]))
		{
			digits2 += key2[i];
		}
	}
	result1 = longParse(digits1) / spaces1;
	result2 = longParse(digits2) / spaces2;

	char* raw_answer = createMD5Buffer(result1, result2, challenge);

	/* calculate the sig */
	char * sig = (char*)calloc(MD5_SIZE, sizeof(char));

	md5_buffer(raw_answer, MD5_SIZE, sig);	//sig is the MD5 hash

											//debug
											/*for (int i = 0; i < MD5_SIZE; i++)
											{
											printf("%d %d\n", raw_answer[i], (unsigned char)sig[i]);
											}*/
											/* convert from the sig to a string rep */
											//char* str = (char*)calloc(64, sizeof(char));    
											//md5_sig_to_string(sig, str, sizeof(str));

	return sig;
}

//returns false if the handshake request fails to parse
//out: challenge
bool parseHandshake(websocketT *ws, char* request, std::string * key1, std::string * key2, std::string* resource,
	char** challenge)//out
{
	char* key1pattern = "(Sec-WebSocket-Key:)[[:s:]](.+\\r\\n)";
	//* key2pattern = "(Sec-WebSocket-Key2:)[[:s:]](.+\\r\\n)";
	char* resourcePattern = "(GET)[[:s:]](/[[:alnum:]]+)";
	std::match_results<const char*> key1M, key2M, getresrcM;



	std::string wsrequest(request);

	//get challenge
	for (int i = 0; i < 8; i++)
		(*challenge)[i] = wsrequest[wsrequest.length() - 8 + i];

	std::tr1::regex rx1(key1pattern);
	//tr1::regex rx2(key2pattern);
	std::tr1::regex rx3(resourcePattern);

	//match Sec-WebSocket-Key1 
	std::tr1::regex_search(wsrequest.c_str(), key1M, rx1);
	*key1 = key1M[2];

	//match Sec-WebSocket-Key1 		
	//tr1::regex_search(wsrequest.c_str(), key2M, rx2);	
	//*key2 = key2M[2];

	//match GET (resource)
	std::tr1::regex_search(wsrequest.c_str(), getresrcM, rx3);
	*resource = getresrcM[2];


	if (*key1 == "")
		return false;
	else
		return true;
}

//returns handshake response
void createHandshakeResponse(websocketT *ws, char* challenge, std::string key1,
	char** response)//out
{
	//	char* answer = createHash(key1, key2, challenge);//length == MD5_SIZE



	char output[29] = {};
	WebSocketHandshake::generate(key1.c_str(), output);
	std::cout << output << std::endl;


	std::string handshake = "HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
		"Upgrade: WebSocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Origin: " + ws->origin + "\r\n" +
		"Sec-WebSocket-Location: " + ws->location + "\r\n" +
		"Sec-WebSocket-Accept: " + output + "\r\n\r\n";


	const char* handshake_bytes = handshake.c_str();
	int responseLength = handshake.length() + MD5_SIZE;
	*response = (char*)calloc(responseLength, sizeof(char));
	for (int i = 0; i < handshake.length(); i++)
	{
		(*response)[i] = handshake_bytes[i];
	}
	// for (int i = 0; i < MD5_SIZE; i++)
	// {
	// 	(*response)[handshake.length() + i] = answer[i];
	// }
	/*for (int i = handshake.length(); i < *length; i++)
	printf("%d\n", (unsigned char)(*response)[i]);*/
	(*response)[responseLength] = '\0';//null-terminate (for debug or strlen)
}


//returns TRUE if web socket client handshake succeeds
bool negotiateHandshake(websocketT *ws, char* request)
{
	//get client handshake	
	printf("Received opening message from client:\n\n%s\n", request);
	char* challenge = (char*)calloc(8, sizeof(char));
	char *response = NULL;
	std::string key1, key2, resource;
	if (!parseHandshake(ws, request, &key1, &key2, &resource, &challenge))
		return false;
	printf("Continue to handshake\n");
	createHandshakeResponse(ws, challenge, key1, &response);
	int sent = send(*ws->connectfd, response, strlen(response), 0);
	return sent > 0;
}


//accepts a pointer (SOCKET*) instead of returning SOCKET to make initialization errors clearer.
//
void open_listenfd(SOCKET* listenfd, char* ipAddress, char* port)
{
	struct sockaddr_in server = { 0 }; //struct. must be zero-d out. later cast to (SOCKADDR*)						
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(ipAddress);//INADDR_ANY; // inet_addr("127.0.0.1");
	server.sin_port = htons((u_short)atoi(port));//dynamic port range: 49152 - 65535 ;
	*listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (bind(*listenfd, (SOCKADDR*)&server, sizeof(server)) == SOCKET_ERROR) {
		perror("failed to bind\n");
	}
	
		
	if (listen(*listenfd, SOMAXCONN) == SOCKET_ERROR) {
		perror("Error listening on socket.\n");
		return;
	}
	printf("Server is listening on socket!... %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
}

void websocketT_init(websocketT* ws, char* ipaddress, char* port, char* origin = "null", char* path = "/echo")
{
	ws->origin = origin;
	ws->port = port;
	ws->path = path;
	ws->ipAddress = ipaddress;
	ws->location = "ws://" + ws->ipAddress + ":" + ws->port + ws->path;
	ws->listenfd = (SOCKET*)malloc(sizeof(SOCKET));
	ws->connectfd = (SOCKET*)malloc(sizeof(SOCKET));
	ws->clientIPv4info = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	ws->clientInfo = (struct sockaddr*)malloc(sizeof(struct sockaddr));
	open_listenfd(ws->listenfd, ipaddress, port);
}

//blocks until a client connects
void AcceptOne(websocketT *ws)
{
	// Accept 1 client connection.
	*(ws->connectfd) = accept(*ws->listenfd, ws->clientInfo, NULL);
	ws->clientIPv4info = (struct sockaddr_in*)ws->clientInfo;
	printf("Client connected. Address %s:%d\n", inet_ntoa(ws->clientIPv4info->sin_addr), ntohs(ws->clientIPv4info->sin_port));
}



char* frame(char* text, int* length)
{
	*length = strlen(text) + 3;
	char* frame = (char*)calloc(*length, sizeof(char));
	frame[0] = 0x81;
	for (int i = 0; i < strlen(text); i++)
	{
		frame[1 + i] = text[i];
	}
	frame[*length + 2] = 0xFF;//rest is null-terminated (calloc)
	return frame;
}


char * makeFrame(char* msg, int msg_length)
{
	char * buffer = (char*)calloc(BUFSIZ, sizeof(char));
	int pos = 0;
	int size = msg_length;
	buffer[pos++] = (unsigned char)0x81; // text frame

	if (size <= 125) {
		buffer[pos++] = size;
	}
	else if (size <= 65535) {
		buffer[pos++] = 126; //16 bit length follows

		buffer[pos++] = (size >> 8) & 0xFF; // leftmost first
		buffer[pos++] = size & 0xFF;
	}
	else { // >2^16-1 (65535)
		buffer[pos++] = 127; //64 bit length follows

							 // write 8 bytes length (significant first)

							 // since msg_length is int it can be no longer than 4 bytes = 2^32-1
							 // padd zeroes for the first 4 bytes
		for (int i = 3; i >= 0; i--) {
			buffer[pos++] = 0;
		}
		// write the actual 32bit msg_length in the next 4 bytes
		for (int i = 3; i >= 0; i--) {
			buffer[pos++] = ((size >> 8 * i) & 0xFF);
		}
	}
	memcpy((void*)(buffer + pos), msg, size);
	return buffer;
}



char* unframe(char* payload, int received)
{
	char end = 0xFF;
	char* text = (char*)calloc(received, sizeof(char));
	for (int i = 1; payload[i] != end && i < received; i++)
	{
		text[i - 1] = payload[i];
	}
	return text;
}

DWORD WINAPI SendOnUserInput(LPVOID lpParam)
{
	SOCKET fd = (SOCKET)lpParam;// *((SOCKET*)lpParam);
	int sent;
	//char* sendbuf = "test msg";
	//char * sendbuf2 = "test msg";
	printf("\n***Enter text to send to client at any time***\n");
	do
	{
		char* sendbuf = (char*)calloc(BUFSIZ, sizeof(char));
		std::cin.getline(sendbuf, BUFSIZ);
		//TODO: process data frame (0x00, 0xFF)...
		int length = strlen(sendbuf);
		int size = 0;
		//char* data = frame(sendbuf, &length);
		char * data;
		data = makeFrame(sendbuf, length);
		sent = send(fd, data, length + 2, 0);
		printf("\nSent %d bytes.\n", sent);
	} while (sent > 0);
	return 0;
}











/*
 * Initializes g_hGlobalInteractorSnapshot with an interactor that has the Fixation Data behavior.
 */
BOOL InitializeGlobalInteractorSnapshot(TX_CONTEXTHANDLE hContext)
{
	TX_HANDLE hInteractor = TX_EMPTY_HANDLE;
	TX_FIXATIONDATAPARAMS params = { TX_FIXATIONDATAMODE_SENSITIVE };
	BOOL success;

	success = txCreateGlobalInteractorSnapshot(
		hContext,
		InteractorId,
		&g_hGlobalInteractorSnapshot,
		&hInteractor) == TX_RESULT_OK;
	success &= txCreateFixationDataBehavior(hInteractor, &params) == TX_RESULT_OK;

	txReleaseObject(&hInteractor);

	return success;
}

/*
 * Callback function invoked when a snapshot has been committed.
 */
void TX_CALLCONVENTION OnSnapshotCommitted(TX_CONSTHANDLE hAsyncData, TX_USERPARAM param)
{
	// check the result code using an assertion.
	// this will catch validation errors and runtime errors in debug builds. in release builds it won't do anything.

	TX_RESULT result = TX_RESULT_UNKNOWN;
	txGetAsyncDataResultCode(hAsyncData, &result);
	assert(result == TX_RESULT_OK || result == TX_RESULT_CANCELLED);
}

/*
 * Callback function invoked when the status of the connection to the EyeX Engine has changed.
 */
void TX_CALLCONVENTION OnEngineConnectionStateChanged(TX_CONNECTIONSTATE connectionState, TX_USERPARAM userParam)
{
	switch (connectionState) {
	case TX_CONNECTIONSTATE_CONNECTED: {
			BOOL success;
			printf("The connection state is now CONNECTED (We are connected to the EyeX Engine)\n");
			// commit the snapshot with the global interactor as soon as the connection to the engine is established.
			// (it cannot be done earlier because committing means "send to the engine".)
			success = txCommitSnapshotAsync(g_hGlobalInteractorSnapshot, OnSnapshotCommitted, NULL) == TX_RESULT_OK;
			if (!success) {
				printf("Failed to initialize the data stream.\n");
			}
			else
			{
				printf("Waiting for fixation data to start streaming...\n");
			}
		}
		break;

	case TX_CONNECTIONSTATE_DISCONNECTED:
		printf("The connection state is now DISCONNECTED (We are disconnected from the EyeX Engine)\n");
		break;

	case TX_CONNECTIONSTATE_TRYINGTOCONNECT:
		printf("The connection state is now TRYINGTOCONNECT (We are trying to connect to the EyeX Engine)\n");
		break;

	case TX_CONNECTIONSTATE_SERVERVERSIONTOOLOW:
		printf("The connection state is now SERVER_VERSION_TOO_LOW: this application requires a more recent version of the EyeX Engine to run.\n");
		break;

	case TX_CONNECTIONSTATE_SERVERVERSIONTOOHIGH:
		printf("The connection state is now SERVER_VERSION_TOO_HIGH: this application requires an older version of the EyeX Engine to run.\n");
		break;
	}
}

/*
 * Handles an event from the fixation data stream.
 */
void OnFixationDataEvent(TX_HANDLE hFixationDataBehavior, LPVOID lpParam)
{
	TX_FIXATIONDATAEVENTPARAMS eventParams;
	TX_EYEPOSITIONDATAEVENTPARAMS params2;
	TX_FIXATIONDATAEVENTTYPE eventType;
	char* eventDescription;
	SOCKET fd = (SOCKET)lpParam;// *((SOCKET*)lpParam);
	int sent = 0;
	if (txGetFixationDataEventParams(hFixationDataBehavior, &eventParams) == TX_RESULT_OK) {
		eventType = eventParams.EventType;

		eventDescription = (eventType == TX_FIXATIONDATAEVENTTYPE_DATA) ? "Data"
			: ((eventType == TX_FIXATIONDATAEVENTTYPE_END) ? "End"
				: "Begin");

		printf("Fixation %s: (%.1f, %.1f) timestamp %.0f ms\n", eventDescription, eventParams.X, eventParams.Y, eventParams.Timestamp);
		if (TX_RESULT_OK != txGetEyePositionDataEventParams(hFixationDataBehavior, &params2)) {

			return;
		}
		bool left = params2.HasLeftEyePosition;
		bool right = params2.HasRightEyePosition;
		char * sendbufX = (char*)calloc(BUFSIZ, sizeof(char));
		if (right == true && left == true) {
			char * sendbufY = (char*)calloc(BUFSIZ, sizeof(char));
			sprintf(sendbufX, "%f ", eventParams.X);
			sprintf(sendbufY, "%f", eventParams.Y);
			strcat(sendbufX, sendbufY);
		}
		else if (left == true  || right == true){
			sprintf(sendbufX, "blink");
		}
			//char* sendbuf = arrayX;
			//strcpy(sendbuf, "test");
			//TODO: process data frame (0x00, 0xFF)...
			int length = strlen(sendbufX);
			int size = 0;
			//char* data = frame(sendbuf, &length);
			char * data;
			data = makeFrame(sendbufX, length);
			sent = send(fd, data, length + 2, 0);
			printf("\nSent %d bytes.\n", sent);
		
	
	} else {
		printf("Failed to interpret fixation data event packet.\n");
	}
}

/*
 * Callback function invoked when an event has been received from the EyeX Engine.
 */
void TX_CALLCONVENTION HandleEvent(TX_CONSTHANDLE hAsyncData, TX_USERPARAM userParam)
{
	TX_HANDLE hEvent = TX_EMPTY_HANDLE;
	TX_HANDLE hBehavior = TX_EMPTY_HANDLE;

	txGetAsyncDataContent(hAsyncData, &hEvent);

	// NOTE. Uncomment the following line of code to view the event object. The same function can be used with any interaction object.
	//OutputDebugStringA(txDebugObject(hEvent));

	if (txGetEventBehavior(hEvent, &hBehavior, TX_BEHAVIORTYPE_FIXATIONDATA) == TX_RESULT_OK) {
		OnFixationDataEvent(hBehavior, lpParam);
		txReleaseObject(&hBehavior);
	}

	// NOTE since this is a very simple application with a single interactor and a single data stream, 
	// our event handling code can be very simple too. A more complex application would typically have to 
	// check for multiple behaviors and route events based on interactor IDs.
	
	txReleaseObject(&hEvent);
}

/*
 * Application entry point.
 */
int main(int argc, char* argv[])
{
	TX_CONTEXTHANDLE hContext = TX_EMPTY_HANDLE;
	TX_TICKET hConnectionStateChangedTicket = TX_INVALID_TICKET;
	TX_TICKET hEventHandlerTicket = TX_INVALID_TICKET;
	BOOL success;

	// initialize and enable the context that is our link to the EyeX Engine.
	success = txInitializeEyeX(TX_EYEXCOMPONENTOVERRIDEFLAG_NONE, NULL, NULL, NULL, NULL) == TX_RESULT_OK;
	success &= txCreateContext(&hContext, TX_FALSE) == TX_RESULT_OK;
	success &= InitializeGlobalInteractorSnapshot(hContext);
	success &= txRegisterConnectionStateChangedHandler(hContext, &hConnectionStateChangedTicket, OnEngineConnectionStateChanged, NULL) == TX_RESULT_OK;
	success &= txRegisterEventHandler(hContext, &hEventHandlerTicket, HandleEvent, NULL) == TX_RESULT_OK;
	success &= txEnableConnection(hContext) == TX_RESULT_OK;

	// let the events flow until a key is pressed.
	if (success) {
		printf("Initialization was successful.\n");
	} else {
		printf("Initialization failed.\n");
	}


	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR)// Initialize Winsock
		printf("Error at WSAStartup()\n");

	struct websocketT ws = { 0 };
	websocketT_init(&ws, "127.0.0.1",
		"37950",//"443", //443 works with "ws://" currently
		"null", "");
	AcceptOne(&ws);//blocks until 1 client connects.

				   //for each client:
				   //must receive opening message from client (or else server cannot send, and client cannot receive.
	char* request0 = (char*)calloc(BUFSIZ, sizeof(char));
	int received = recv(*ws.connectfd, request0, BUFSIZ, 0);
	//block until opening message received	
	if (negotiateHandshake(&ws, request0))
	{
		lpParam = (LPVOID)*ws.connectfd;
//		hThread = CreateThread(
	//		NULL,                   // default security attributes
		//	0,                      // use default stack size  
			//SendOnUserInput, // thread function name
	//		(LPVOID)*ws.connectfd, // argument to thread function 
		//	0,// use default creation flags 
			//&threadID);   // returns the thread identifier 
		while(1){}

	}
	else {
		printf("SOME ERROR\n");

	}






	printf("Press any key to exit...\n");
	_getch();
	printf("Exiting.\n");

	// disable and delete the context.
	txDisableConnection(hContext);
	txReleaseObject(&g_hGlobalInteractorSnapshot);
	success = txShutdownContext(hContext, TX_CLEANUPTIMEOUT_DEFAULT, TX_FALSE) == TX_RESULT_OK;
	success &= txReleaseContext(&hContext) == TX_RESULT_OK;
	success &= txUninitializeEyeX() == TX_RESULT_OK;
	if (!success) {
		printf("EyeX could not be shut down cleanly. Did you remember to release all handles?\n");
	}

	return 0;
}
