#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <setjmp.h>
#include <3ds.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#define STACKSIZE (4 * 1024)
#define WAIT_TIMEOUT 1000000000ULL

#define WIDTH 400
#define HEIGHT 240
#define SCREEN_SIZE WIDTH * HEIGHT * 2
#define BUF_SIZE SCREEN_SIZE * 2
#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

static u32 *SOC_buffer = NULL;
s32 sock = -1, csock = -1;
int ret;
u8 *buf = NULL;
u8 *currentframe = NULL;
u32	clientlen;
int sendframe = 1;
struct sockaddr_in client;
struct sockaddr_in server;
static int hits=0;
static jmp_buf exitJmp;

inline void clearScreen(void) {
	u8 *frame = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
	memset(frame, 0, 320 * 240 * 3);
}

void hang(char *message) {
	clearScreen();
	printf("%s", message);
	printf("Press start to exit");

	while (aptMainLoop()) {
		hidScanInput();

		u32 kHeld = hidKeysHeld();
		if (kHeld & KEY_START) longjmp(exitJmp, 1);
	}
}

void cleanup() {
	camExit();
	gfxExit();
	acExit();
}
bool running = true;
void threadMain(void *arg)
{
	while (running)
	{
		csock = accept (sock, (struct sockaddr *) &client, &clientlen);

		if (csock<0) {
			if(errno != EAGAIN) {
				failExit("accept: %d %s\n", errno, strerror(errno));
			}
		} else {
			fcntl(csock, F_SETFL, fcntl(csock, F_GETFL, 0) & ~O_NONBLOCK);
			printf("Connecting port %d from %s\n", client.sin_port, inet_ntoa(client.sin_addr));
			while(running){
				if(sendframe == 1){
					sendframe = 0;
					int returnval = send(csock, buf, BUF_SIZE,0);
					sendframe = 1;
					if(returnval == -1)
						break;
				}
			}
			
			
		}

		//svcSleepThread(sleepDuration);
	}
}
void writePictureToFramebufferRGB565(void *fb, void *img, u16 x, u16 y, u16 width, u16 height) {
	u8 *fb_8 = (u8*) fb;
	u16 *img_16 = (u16*) img;
	int i, j, draw_x, draw_y;
	for(j = 0; j < height; j++) {
		for(i = 0; i < width; i++) {
			draw_y = y + height - j;
			draw_x = x + i;
			u32 v = (draw_y + draw_x * height) * 3;
			u16 data = img_16[j * width + i];
			uint8_t b = ((data >> 11) & 0x1F) << 3;
			uint8_t g = ((data >> 5) & 0x3F) << 2;
			uint8_t r = (data & 0x1F) << 3;
			fb_8[v] = r;
			fb_8[v+1] = g;
			fb_8[v+2] = b;
		}
	}
}
void socShutdown() {
	printf("waiting for socExit...\n");
	socExit();
}

int main() {
	acInit();
	gfxInitDefault();
	consoleInit(GFX_BOTTOM, NULL);

	gfxSetDoubleBuffering(GFX_TOP, true);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);

	if(setjmp(exitJmp)) {
		cleanup();
		return 0;
	}

	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);

	if(SOC_buffer == NULL) {
		failExit("memalign: failed to allocate\n");
	}

	if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
    	failExit("socInit: 0x%08X\n", (unsigned int)ret);
	}
	atexit(socShutdown);

	clientlen = sizeof(client);

	sock = socket (AF_INET, SOCK_STREAM, IPPROTO_IP);

	if (sock < 0) {
		failExit("socket: %d %s\n", errno, strerror(errno));
	}

	memset (&server, 0, sizeof (server));
	memset (&client, 0, sizeof (client));

	server.sin_family = AF_INET;
	server.sin_port = htons (80);
	server.sin_addr.s_addr = gethostid();

	printf("Listening on %s\n",inet_ntoa(server.sin_addr));
		
	if ( (ret = bind (sock, (struct sockaddr *) &server, sizeof (server))) ) {
		close(sock);
		failExit("bind: %d %s\n", errno, strerror(errno));
	}

	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

	if ( (ret = listen( sock, 5)) ) {
		failExit("listen: %d %s\n", errno, strerror(errno));
	}
	s32 prio = 0;
	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	Thread t;
	threadCreate(threadMain, 0, STACKSIZE, prio-1, -2, false);
	u32 kDown;

	printf("Initializing camera\n");

	printf("camInit: 0x%08X\n", (unsigned int) camInit());

	printf("CAMU_SetSize: 0x%08X\n", (unsigned int) CAMU_SetSize(SELECT_OUT1_OUT2, SIZE_CTR_TOP_LCD, CONTEXT_A));
	printf("CAMU_SetOutputFormat: 0x%08X\n", (unsigned int) CAMU_SetOutputFormat(SELECT_OUT1_OUT2, OUTPUT_RGB_565, CONTEXT_A));

	printf("CAMU_SetFrameRate: 0x%08X\n", (unsigned int) CAMU_SetFrameRate(SELECT_OUT1_OUT2, FRAME_RATE_30));

	printf("CAMU_SetNoiseFilter: 0x%08X\n", (unsigned int) CAMU_SetNoiseFilter(SELECT_OUT1_OUT2, true));
	printf("CAMU_SetAutoExposure: 0x%08X\n", (unsigned int) CAMU_SetAutoExposure(SELECT_OUT1_OUT2, true));
	printf("CAMU_SetAutoWhiteBalance: 0x%08X\n", (unsigned int) CAMU_SetAutoWhiteBalance(SELECT_OUT1_OUT2, true));
	printf("CAMU_SetTrimming: 0x%08X\n", (unsigned int) CAMU_SetTrimming(PORT_CAM1, false));
	printf("CAMU_SetTrimming: 0x%08X\n", (unsigned int) CAMU_SetTrimming(PORT_CAM2, false));

	buf = malloc(BUF_SIZE);
	currentframe = malloc(BUF_SIZE);
	
	if(!buf) {
		hang("Failed to allocate memory!");
	}

	u32 bufSize;
	printf("CAMU_GetMaxBytes: 0x%08X\n", (unsigned int) CAMU_GetMaxBytes(&bufSize, WIDTH, HEIGHT));
	printf("CAMU_SetTransferBytes: 0x%08X\n", (unsigned int) CAMU_SetTransferBytes(PORT_BOTH, bufSize, WIDTH, HEIGHT));

	printf("CAMU_Activate: 0x%08X\n", (unsigned int) CAMU_Activate(SELECT_OUT1_OUT2));

	Handle camReceiveEvent[4] = {0};
	bool captureInterrupted = false;
	s32 index = 0;

	printf("CAMU_GetBufferErrorInterruptEvent: 0x%08X\n", (unsigned int) CAMU_GetBufferErrorInterruptEvent(&camReceiveEvent[0], PORT_CAM1));
	printf("CAMU_GetBufferErrorInterruptEvent: 0x%08X\n", (unsigned int) CAMU_GetBufferErrorInterruptEvent(&camReceiveEvent[1], PORT_CAM2));

	printf("CAMU_ClearBuffer: 0x%08X\n", (unsigned int) CAMU_ClearBuffer(PORT_BOTH));
	printf("CAMU_SynchronizeVsyncTiming: 0x%08X\n", (unsigned int) CAMU_SynchronizeVsyncTiming(SELECT_OUT1, SELECT_OUT2));

	printf("CAMU_StartCapture: 0x%08X\n", (unsigned int) CAMU_StartCapture(PORT_BOTH));
	printf("CAMU_PlayShutterSound: 0x%08X\n", (unsigned int) CAMU_PlayShutterSound(SHUTTER_SOUND_TYPE_MOVIE));

	gfxFlushBuffers();
	gspWaitForVBlank();
	gfxSwapBuffers();
	printf("Press Start to exit to Homebrew Launcher\n");

	while (aptMainLoop()) {

		if (!captureInterrupted) {
			hidScanInput();
			kDown = hidKeysDown();
			if (kDown & KEY_START) {
				break;
			}
		}

		if (camReceiveEvent[2] == 0) {
			CAMU_SetReceiving(&camReceiveEvent[2], buf, PORT_CAM1, SCREEN_SIZE, (s16)bufSize);
		}
		if (camReceiveEvent[3] == 0) {
			CAMU_SetReceiving(&camReceiveEvent[3], buf + SCREEN_SIZE, PORT_CAM2, SCREEN_SIZE, (s16)bufSize);
		}
		if(sendframe == 1){
			memcpy(currentframe, buf, BUF_SIZE);
		}
		if (captureInterrupted) {
			CAMU_StartCapture(PORT_BOTH);
			captureInterrupted = false;
		}

		svcWaitSynchronizationN(&index, camReceiveEvent, 4, false, WAIT_TIMEOUT);
		switch (index) {
		case 0:
			svcCloseHandle(camReceiveEvent[2]);
			camReceiveEvent[2] = 0;

			captureInterrupted = true;
			continue; 
			break;
		case 1:
			svcCloseHandle(camReceiveEvent[3]);
			camReceiveEvent[3] = 0;

			captureInterrupted = true;
			continue;
			break;
		case 2:
			svcCloseHandle(camReceiveEvent[2]);
			camReceiveEvent[2] = 0;
			break;
		case 3:
			svcCloseHandle(camReceiveEvent[3]);
			camReceiveEvent[3] = 0;
			break;
		default:
			break;
		}
			
		writePictureToFramebufferRGB565(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), buf, 0, 0, WIDTH, HEIGHT);

		gfxFlushBuffers();
		gspWaitForVBlank();
		gfxSwapBuffers();
	}
	running = false;
	printf("CAMU_StopCapture: 0x%08X\n", (unsigned int) CAMU_StopCapture(PORT_BOTH));
	for (int i = 0; i < 4; i++)	{
		if (camReceiveEvent[i] != 0) {
			svcCloseHandle(camReceiveEvent[i]);
		}
	}

	printf("CAMU_Activate: 0x%08X\n", (unsigned int) CAMU_Activate(SELECT_NONE));

	free(buf);
	cleanup();
	return 0;
}
void failExit(const char *fmt, ...) {
	if(sock>0) close(sock);
	if(csock>0) close(csock);

	printf(CONSOLE_RESET);
	printf("\nPress B to exit\n");

	while (aptMainLoop()) {
		gspWaitForVBlank();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_B) exit(0);
	}
}
