// 1. apt install lirc
// 2. append these to /boot/config.txt
//    dtoverlay=lirc-rpi:gpio_in_pin=2,gpio_out_pin=3
//    dtoverlay=
// 3. reboot

// How To Record IR Code:
// > mode2 -d /dev/lirc0 | sed -ue '1d' | tee aircon_on.txt
// > cat aircon_on.txt | awk '{if(NR % 30) ORS=" "; else ORS="\n"; print $2}' | cat -; echo
// Paste printed text to /etc/lirc/lircd.conf and keep less than 80 chars per line.

// make: gcc ./piServer.c -o piServer -lpthread -lwiringPi

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <wiringPi.h>
#include <sys/types.h>
#include <sys/time.h>

#define GPIO_1    23
#define GPIO_2    22
#define GPIO_3    24
#define GPIO_4     5
#define GPIO_L    17
#define GPIO_R     4
#define GPIO_BKL  27

#define RED         "\x1B[31m"
#define GREEN       "\x1B[92m"
#define YELLOW      "\x1B[93m"
#define BLUE        "\x1B[34m"
#define MAG         "\x1B[35m"
#define CYAN        "\x1B[96m"
#define WHITE       "\x1B[37m"
#define CRESET      "\x1B[0m"
#define CDIALOGBG   "\x1B[104m"

#define MAX_SCREEN_X 40
#define MAX_SCREEN_Y 30

enum PrintScreenType { PRINT_NORMAL = 0, PRINT_CENTER, PRINT_MID_L, PRINT_MID_R };
enum MenuOnScreen { MENU_ITEM_0 = 17, MENU_ITEM_1 = 20, MENU_ITEM_2 = 23 };
int MenuList[3] = { MENU_ITEM_0, MENU_ITEM_1, MENU_ITEM_2 };

void* ThreadLive(void* args);
void* ThreadInput(void* args);
void PrintMain();
void PrintArrow();
int SwitchService();
int RunOnceService();
int GetServiceStatus();
int ExecuteCMD(char* inputLine, char* outputLine, int outputLength);
void PrintScreen(int x, int y, int type, char* color, char* inputText);
void Trim(char* inputBuffer, int bufferSize);

static int exitFlag = 0;
static int bklFlag = 1;
static int pushFlag = 0;

static int arrowPos = 0;
static int serviceStatus[3] = { FALSE, FALSE, FALSE };

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char* argv[])
{
	struct timeval delay;
	pthread_t threadLive, threadInput;

	PrintMain();

	if (-1 == wiringPiSetupGpio())
	{
		system("clear");
		printf("Failed to init GPIO.\n");
		return 1;
	}

	pinMode(GPIO_1, INPUT);
        pinMode(GPIO_2, INPUT);
        pinMode(GPIO_3, INPUT);
        pinMode(GPIO_4, INPUT);
        pinMode(GPIO_L, INPUT);
        pinMode(GPIO_R, INPUT);
        pinMode(GPIO_BKL, OUTPUT);

	pullUpDnControl (GPIO_1, PUD_UP);
	pullUpDnControl (GPIO_2, PUD_UP);
	pullUpDnControl (GPIO_3, PUD_UP);
	pullUpDnControl (GPIO_4, PUD_UP);
	pullUpDnControl (GPIO_L, PUD_UP);
	pullUpDnControl (GPIO_R, PUD_UP);

	digitalWrite(GPIO_BKL, 1);

	pthread_mutex_init (&mutex, NULL);

	if (pthread_create(&threadLive, NULL, ThreadLive, NULL) )
	{
		system("clear");
    		printf("Error creating threadLive.");
    		return 1;
	}

        if (pthread_create(&threadInput, NULL, ThreadInput, NULL) )
        {
                system("clear");
                printf("Error creating threadInput.");
                return 1;
        }

	while (0 == exitFlag)
	{
		delay.tv_sec = 0;
		delay.tv_usec = 100 * 1000;
		select(0, NULL,NULL, NULL, &delay);
	}

	pthread_join(threadLive, NULL);
	pthread_join(threadInput, NULL);

	pthread_mutex_destroy(&mutex);

	system("clear");
	printf("Bye\n");
	fflush(stdout);

	return 0;
}

void* ThreadInput(void* args)
{
	struct timeval delay;
	bklFlag = 1;
	pushFlag = 0;

	while (0 == exitFlag)
	{
                pthread_mutex_lock(&mutex);

                if (0 == digitalRead(GPIO_L))
                {
			if (0 == pushFlag)
                        {
                        	pushFlag = 1;

				if (0 < arrowPos) arrowPos--;
				PrintArrow();
				//PrintScreen(0, 0, PRINT_NORMAL, WHITE, "U");
				//PrintScreen(0, 0, PRINT_NORMAL, WHITE, "");
			}
			//usleep(500);
                }
                else if (0 == digitalRead(GPIO_R))
                {
                        if (0 == pushFlag)
                        {
                                pushFlag = 1;

				if (2 > arrowPos) arrowPos++;
        	                PrintArrow();
				//PrintScreen(0, 0, PRINT_NORMAL, WHITE, "D");
				//PrintScreen(0, 0, PRINT_NORMAL, WHITE, "");
			}
			//usleep(500);
                }
                else if (0 == digitalRead(GPIO_1))
                {
			if (0 == pushFlag)
			{
				pushFlag = 1;

				SwitchService();
	 	                //PrintScreen(0, 0, PRINT_NORMAL, WHITE, "E");
	                        //PrintScreen(0, 0, PRINT_NORMAL, WHITE, "");
			}
                }
                else if (0 == digitalRead(GPIO_2))
                {
                        if (0 == pushFlag)
                        {
                                pushFlag = 1;

				RunOnceService();
                                //PrintScreen(0, 0, PRINT_NORMAL, WHITE, "B");
                                //PrintScreen(0, 0, PRINT_NORMAL, WHITE, "");
                        }
                }
                else if (0 == digitalRead(GPIO_3))
                {
                        if (0 == pushFlag)
			{
				pushFlag = 1;

				if (1 == bklFlag)
				{
					bklFlag = 0;
				}
				else
				{
                                	bklFlag = 1;
				}

                        	digitalWrite(GPIO_BKL, bklFlag);
       			}
		}
		else if (0 == digitalRead(GPIO_4))
                {
			pushFlag++;

			if (300 < pushFlag)
			{
				PrintScreen(0, 0, PRINT_NORMAL, WHITE, "Shutdown");
				usleep(500);

				system("shutdown now");
			}
                }
		else
		{
			if (10 > pushFlag)
			{
				pushFlag = 0;
			}
			else
			{
                                PrintScreen(0, 0, PRINT_NORMAL, WHITE, "Reboot");
                                usleep(500);

				system("reboot");
			}
		}

		fflush(stdout);
                pthread_mutex_unlock(&mutex);

                delay.tv_sec = 0;
                delay.tv_usec = 10 * 1000;
                select(0, NULL,NULL, NULL, &delay);
	}
}

void* ThreadLive(void* args)
{
	struct timeval delay;
	int ret = 0;
	int liveCount = 0;
        char buffer[1024] = { 0 };
    	time_t now;
    	struct tm *timenow;
    	char strtemp[255];

	while (0 == exitFlag)
	{
		pthread_mutex_lock(&mutex);

		PrintScreen(0, 6, PRINT_CENTER, RED, "                  N / A                 ");
        	PrintScreen(0, 9, PRINT_MID_L, CYAN, "                    ");
        	PrintScreen(0, 9, PRINT_MID_R, CYAN, "                    ");

        	ret = ExecuteCMD("hostname -I 2>&1", buffer, sizeof(buffer));
        	if (0 == ret)
		{
			if (0 == system("ping -q -c 1 1.2.4.8 1>/dev/null 2>&1"))
			{
				PrintScreen(0, 6, PRINT_CENTER, CYAN, buffer);
			}
			else
			{
				PrintScreen(0, 6, PRINT_CENTER, RED, buffer);
			}

			//cat /proc/net/wireless | grep 'wlan0' | awk '{print $4}' | tr -d .
		}
		else
		{
			PrintScreen(0, 6, PRINT_CENTER, CYAN, "ERROR");
		}

	        ret = ExecuteCMD("uptime | head -1 | awk -F, '{print $1}' | awk '{print $3}'", buffer, sizeof(buffer));
                if (0 == ret)
                {
                        PrintScreen(0, 9, PRINT_MID_L, CYAN, buffer);
                }
                else
                {
                        PrintScreen(0, 9, PRINT_MID_L, CYAN, "ERROR");
                }

	        ret = ExecuteCMD("uptime | head -1 | awk -F': ' '{print $2}'", buffer, sizeof(buffer));
                if (0 == ret)
                {
                        PrintScreen(0, 9, PRINT_MID_R, CYAN, buffer);
                }
                else
                {
                        PrintScreen(0, 9, PRINT_MID_R, CYAN, "ERROR");
                }

		ret = ExecuteCMD("df -h --output=source,avail,pcent | grep '/dev/root' | awk '{print $2\"(\"$3\")\"}'", buffer, sizeof(buffer));
		if (0 == ret)
                {
                        PrintScreen(0, 12, PRINT_MID_L, CYAN, buffer);
                }
                else
                {
                        PrintScreen(0, 12, PRINT_MID_L, CYAN, "ERROR");
                }

		ret = ExecuteCMD("cat /sys/class/thermal/thermal_zone0/temp | awk '{printf (\"%.1f °C\", $1/1000)}'", buffer, sizeof(buffer));
                if (0 == ret)
                {
                        PrintScreen(0, 12, PRINT_MID_R, CYAN, buffer);
                }
                else
                {
                        PrintScreen(0, 12, PRINT_MID_R, CYAN, "ERROR");
                }

		GetServiceStatus();

		time(&now);
    		timenow = localtime(&now);
		memset((void *)buffer, (int)'\0', sizeof(buffer));
		strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M", timenow);
    		PrintScreen(0, MAX_SCREEN_Y, PRINT_CENTER, WHITE, buffer);

        	PrintScreen(0, 0, PRINT_NORMAL, WHITE, "");

        	fflush(stdout);
		pthread_mutex_unlock(&mutex);

              	delay.tv_sec = 5;
               	delay.tv_usec = 1000;
               	select(0, NULL,NULL, NULL, &delay);
	}
}

void PrintMain()
{
        system("clear");

 	PrintScreen(0, 2, PRINT_CENTER, WHITE, "======================================");
        PrintScreen(0, 2, PRINT_CENTER, CYAN, " DuanWY-Pi ");

        PrintScreen(0, 5, PRINT_CENTER, WHITE, "- Network Information -");
        PrintScreen(0, 6, PRINT_CENTER, CYAN, "N/A");

        PrintScreen(0, 8, PRINT_MID_L, WHITE, "- Up Time -");
        PrintScreen(0, 8, PRINT_MID_R, WHITE, "- CPU Load -");
        PrintScreen(0, 9, PRINT_MID_L, CYAN, "N/A");
        PrintScreen(0, 9, PRINT_MID_R, CYAN, "N/A");

        PrintScreen(0, 11, PRINT_MID_L, WHITE, "- Free(Used%) -");
        PrintScreen(0, 11, PRINT_MID_R, WHITE, "- CPU Temp -");
        PrintScreen(0, 12, PRINT_MID_L, CYAN, "N/A");
        PrintScreen(0, 12, PRINT_MID_R, CYAN, "N/A");

	//PrintScreen(0, 13, PRINT_CENTER, WHITE, "                                      ");
	//PrintScreen(0, 14, PRINT_CENTER, WHITE, "                                      ");
        PrintScreen(0, 15, PRINT_CENTER, WHITE, "======================================");

        PrintScreen(10, MENU_ITEM_0, PRINT_NORMAL, WHITE, "[  ] Aircon Forced-On");
	PrintScreen(10, MENU_ITEM_1, PRINT_NORMAL, WHITE, "[  ] TV Auto On");
	PrintScreen(10, MENU_ITEM_2, PRINT_NORMAL, WHITE, "[  ] Cloud Schedule Sync");

	PrintScreen(0, 27, PRINT_CENTER, WHITE, "======================================");

	PrintArrow();

        PrintScreen(0, 0, PRINT_NORMAL, WHITE, "");

	fflush(stdout);
}

void PrintDialog()
{
	PrintScreen(0, 12, PRINT_CENTER, CDIALOGBG, "                              ");
	PrintScreen(0, 13, PRINT_CENTER, CDIALOGBG, "                              ");
	PrintScreen(0, 14, PRINT_CENTER, CDIALOGBG, "           Working...         ");
	PrintScreen(0, 15, PRINT_CENTER, CDIALOGBG, "                              ");
	PrintScreen(0, 16, PRINT_CENTER, CDIALOGBG, "                              ");

	PrintScreen(0, 0, PRINT_NORMAL, WHITE, "");

	fflush(stdout);
}

void PrintArrow()
{
	int eachPos = 0;

	for (eachPos = 0; eachPos < sizeof(MenuList); eachPos++)
	{
		if (arrowPos == eachPos)
		{
			PrintScreen(7, MenuList[eachPos], PRINT_NORMAL, YELLOW, "->");
		}
		else
		{
			PrintScreen(7, MenuList[eachPos], PRINT_NORMAL, YELLOW, "  ");
		}
	}

	PrintScreen(0, 0, PRINT_NORMAL, WHITE, "");
	fflush(stdout);
}

int SwitchService()
{
	int ret = 0;
	char buffer[1024] = { 0 };

	PrintDialog();

	if (MENU_ITEM_0 == MenuList[arrowPos])
        {
		serviceStatus[0] = (TRUE == serviceStatus[0]) ? FALSE : TRUE;
	}
        else if (MENU_ITEM_1 == MenuList[arrowPos])
        {
		serviceStatus[1] = (TRUE == serviceStatus[1]) ? FALSE : TRUE;
        }
        else if (MENU_ITEM_2 == MenuList[arrowPos])
        {
		serviceStatus[2] = (TRUE == serviceStatus[2]) ? FALSE : TRUE;
        }

	ExecuteCMD("cat \"# piServer Auto Script\" >/tmp/crontab.cron", buffer, sizeof(buffer));

	if (TRUE == serviceStatus[0])
	{
		ExecuteCMD("cat /home/pi/piServerProfile/aircon.cron >>/tmp/crontab.cron", buffer, sizeof(buffer));
	}
        if (TRUE == serviceStatus[1])
        {
                ExecuteCMD("cat /home/pi/piServerProfile/tv.cron >>/tmp/crontab.cron", buffer, sizeof(buffer));
        }
        if (TRUE == serviceStatus[2])
        {
                ExecuteCMD("cat /home/pi/piServerProfile/sync.cron >>/tmp/crontab.cron", buffer, sizeof(buffer));
        }

	ExecuteCMD("crontab /tmp/crontab.cron", buffer, sizeof(buffer));

	PrintMain();

	return ret;
}

int RunOnceService()
{
        int ret = 0;
        char buffer[1024] = { 0 };

        PrintDialog();

        if (MENU_ITEM_0 == MenuList[arrowPos])
        {
                ExecuteCMD("/home/pi/piServerProfile/aircon.sh", buffer, sizeof(buffer));
        }
        else if (MENU_ITEM_1 == MenuList[arrowPos])
        {
                ExecuteCMD("/home/pi/piServerProfile/tv.sh", buffer, sizeof(buffer));
        }
        else if (MENU_ITEM_2 == MenuList[arrowPos])
        {
                ExecuteCMD("/home/pi/piServerProfile/sync.sh", buffer, sizeof(buffer));
        }
        else
        {

        }

        PrintMain();

        return ret;
}

int GetServiceStatus()
{
	int ret = 0;
        char buffer[1024] = { 0 };

	ret = ExecuteCMD("crontab -l | grep piServerProfile/aircon.sh", buffer, sizeof(buffer));
        if (0 == ret && strlen(buffer) > 0)
        {
                PrintScreen(11, MENU_ITEM_0, PRINT_NORMAL, GREEN, "ON");
		serviceStatus[0] = TRUE;
        }
        else
        {
		PrintScreen(11, MENU_ITEM_0, PRINT_NORMAL, WHITE, "  ");
                serviceStatus[0] = FALSE;
        }
	ret = 0;
	memset((void *)buffer, (int)'\0', sizeof(buffer));

        ret = ExecuteCMD("crontab -l | grep piServerProfile/tv.sh", buffer, sizeof(buffer));
        if (0 == ret && strlen(buffer) > 0)
        {
                PrintScreen(11, MENU_ITEM_1, PRINT_NORMAL, GREEN, "ON");
                serviceStatus[1] = TRUE;
        }
        else
        {
                PrintScreen(11, MENU_ITEM_1, PRINT_NORMAL, WHITE, "  ");
                serviceStatus[1] = FALSE;
        }
        ret = 0;
        memset((void *)buffer, (int)'\0', sizeof(buffer));

        ret = ExecuteCMD("crontab -l | grep piServerProfile/sync.sh", buffer, sizeof(buffer));
        if (0 == ret && strlen(buffer) > 0)
        {
                PrintScreen(11, MENU_ITEM_2, PRINT_NORMAL, GREEN, "ON");
                serviceStatus[2] = TRUE;
        }
        else
        {
                PrintScreen(11, MENU_ITEM_2, PRINT_NORMAL, WHITE, "  ");
                serviceStatus[2] = FALSE;
        }

	return ret;
}

int ExecuteCMD(char* inputLine, char* outputLine, int outputLength)
{
	FILE *fp = NULL;

	if (NULL == inputLine || NULL == outputLine || 0 == outputLength) return 1;

	memset((void *)outputLine, (int)'\0', outputLength);

	fp = popen(inputLine, "r");
	if (NULL == fp)
	{
		return 1;
	}

	while (NULL != fgets(outputLine, outputLength, fp)) { usleep(10); }

	Trim(outputLine, outputLength);

	pclose(fp);
	fp = NULL;

	return 0;
}


void PrintScreen(int x, int y, int type, char* color, char* inputText)
{
	if (NULL == color || NULL == inputText) return;

	if (PRINT_CENTER == type)
	{
		x = (MAX_SCREEN_X - strlen(inputText)) / 2;
	}
	else if (PRINT_MID_L == type)
	{
		x = (MAX_SCREEN_X / 2 - strlen(inputText)) / 2;
	}
        else if (PRINT_MID_R == type)
        {
                x = MAX_SCREEN_X / 2 + (MAX_SCREEN_X / 2 - strlen(inputText)) / 2;
        }

	if (x < 0) x = 0;

	printf("\033[%d;%dH", y+1, x+1);
	printf("%s%s%s", color, inputText, CRESET);
}

void Trim(char* inputBuffer, int bufferSize)
{
	int pos= 0;
	char termChar = '\n';
	char spaceChar = ' ';
	char emptyChar = '\0';

	for (pos = bufferSize - 1; pos >= 0; pos--)
	{
                if (spaceChar == (char)inputBuffer[pos])
                {
                        inputBuffer[pos] = emptyChar;
		}
		else if (termChar == (char)inputBuffer[pos])
		{
			inputBuffer[pos] = emptyChar;
		}
		else if (emptyChar == (char)inputBuffer[pos])
		{
			continue;
		}
		else
		{
			break;
		}
	}
}
