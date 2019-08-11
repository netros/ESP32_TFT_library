/* TFT demo

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <time.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "tftspi.h"
#include "tft.h"
#include "spiffs_vfs.h"


#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "freertos/event_groups.h"
#include "esp_attr.h"
#include <sys/time.h>
#include <unistd.h>
#include "lwip/err.h"
#include "lwip/apps/sntp.h"
#include "esp_log.h"
#include "nvs_flash.h"

// HK

#include "esp_types.h"
#include "freertos/queue.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (3.4179) // sample test interval for the first timer
#define TIMER_INTERVAL1_SEC   (1.00)   // sample test interval for the second timer
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload

//HK END



// ==========================================================
// Define which spi bus to use TFT_VSPI_HOST or TFT_HSPI_HOST
#define SPI_BUS TFT_HSPI_HOST
// ==========================================================


static void angle_face_HK();
static int _demo_pass = 0;
static uint8_t doprint = 1;
static struct tm* tm_info;
static char tmp_buff[64];
static time_t time_now, time_last = 0;
static const char *file_fonts[3] = {"/spiffs/fonts/DotMatrix_M.fon", "/spiffs/fonts/Ubuntu.fon", "/spiffs/fonts/Grotesk24x48.fon"};

//HK
TaskHandle_t Task1;
TaskHandle_t Task2;

#define GDEMO_TIME 1000
#define GDEMO_INFO_TIME 5000

//==================================================================================
#ifdef CONFIG_EXAMPLE_USE_WIFI

static const char tag[] = "[TFT Demo]";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = 0x00000001;

//------------------------------------------------------------
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

//-------------------------------
static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_LOGI(tag, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

//-------------------------------
static void initialize_sntp(void)
{
    ESP_LOGI(tag, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

//--------------------------
static int obtain_time(void)
{
	int res = 1;
    initialise_wifi();
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    initialize_sntp();

    // wait for time to be set
    int retry = 0;
    const int retry_count = 20;

    time(&time_now);
	tm_info = localtime(&time_now);

    while(tm_info->tm_year < (2016 - 1900) && ++retry < retry_count) {
        //ESP_LOGI(tag, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		sprintf(tmp_buff, "Wait %0d/%d", retry, retry_count);
    	TFT_print(tmp_buff, CENTER, LASTY);
		vTaskDelay(500 / portTICK_RATE_MS);
        time(&time_now);
    	tm_info = localtime(&time_now);
    }
    if (tm_info->tm_year < (2016 - 1900)) {
    	ESP_LOGI(tag, "System time NOT set.");
    	res = 0;
    }
    else {
    	ESP_LOGI(tag, "System time is set.");
    }

    ESP_ERROR_CHECK( esp_wifi_stop() );
    return res;
}

#endif  //CONFIG_EXAMPLE_USE_WIFI
//==================================================================================


//----------------------
static void _checkTime()
{
	time(&time_now);
	if (time_now > time_last) {
		color_t last_fg, last_bg;
		time_last = time_now;
		tm_info = localtime(&time_now);
		sprintf(tmp_buff, "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

		TFT_saveClipWin();
		TFT_resetclipwin();

		Font curr_font = cfont;
		last_bg = _bg;
		last_fg = _fg;
		_fg = TFT_YELLOW;
		_bg = (color_t){ 64, 64, 64 };
		TFT_setFont(DEFAULT_FONT, NULL);

		TFT_fillRect(1, _height-TFT_getfontheight()-8, _width-3, TFT_getfontheight()+6, _bg);
		TFT_print(tmp_buff, CENTER, _height-TFT_getfontheight()-5);

		cfont = curr_font;
		_fg = last_fg;
		_bg = last_bg;

		TFT_restoreClipWin();
	}
}

/*
//----------------------
static int _checkTouch()
{
	int tx, ty;
	if (TFT_read_touch(&tx, &ty, 0)) {
		while (TFT_read_touch(&tx, &ty, 1)) {
			vTaskDelay(20 / portTICK_RATE_MS);
		}
		return 1;
	}
	return 0;
}
*/

//---------------------
static int Wait(int ms)
{
	uint8_t tm = 1;
	if (ms < 0) {
		tm = 0;
		ms *= -1;
	}
	if (ms <= 50) {
		vTaskDelay(ms / portTICK_RATE_MS);
		//if (_checkTouch()) return 0;
	}
	else {
		for (int n=0; n<ms; n += 50) {
			vTaskDelay(50 / portTICK_RATE_MS);
			if (tm) _checkTime();
			//if (_checkTouch()) return 0;
		}
	}
	return 1;
}

//-------------------------------------------------------------------
static unsigned int rand_interval(unsigned int min, unsigned int max)
{
    int r;
    const unsigned int range = 1 + max - min;
    const unsigned int buckets = RAND_MAX / range;
    const unsigned int limit = buckets * range;

    /* Create equal size buckets all in a row, then fire randomly towards
     * the buckets until you land in one of them. All buckets are equally
     * likely. If you land off the end of the line of buckets, try again. */
    do
    {
        r = rand();
    } while (r >= limit);

    return min + (r / buckets);
}

// Generate random color
//-----------------------------
static color_t random_color() {

	color_t color;
	color.r  = (uint8_t)rand_interval(8,252);
	color.g  = (uint8_t)rand_interval(8,252);
	color.b  = (uint8_t)rand_interval(8,252);
	return color;
}

//---------------------
static void _dispTime()
{
	Font curr_font = cfont;
    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);

    time(&time_now);
	time_last = time_now;
	tm_info = localtime(&time_now);
	sprintf(tmp_buff, "Seoul Time: %02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
	// printf("HK:::::: %02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
	TFT_print(tmp_buff, CENTER, _height-TFT_getfontheight()-5);

    cfont = curr_font;
}

//---------------------------------
static void disp_header(char *info)
{
	TFT_fillScreen(TFT_BLACK);
	TFT_resetclipwin();

	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };

    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);
	TFT_fillRect(0, 0, _width-1, TFT_getfontheight()+8, _bg);
	TFT_drawRect(0, 0, _width-1, TFT_getfontheight()+8, TFT_CYAN);

	TFT_fillRect(0, _height-TFT_getfontheight()-9, _width-1, TFT_getfontheight()+8, _bg);
	TFT_drawRect(0, _height-TFT_getfontheight()-9, _width-1, TFT_getfontheight()+8, TFT_CYAN);

	TFT_print(info, CENTER, 4);
	 _dispTime();

	_bg = TFT_BLACK;
	TFT_setclipwin(0,TFT_getfontheight()+9, _width-1, _height-TFT_getfontheight()-10);
}

//---------------------------------------------
static void update_header(char *hdr, char *ftr)
{
	color_t last_fg, last_bg;

	TFT_saveClipWin();
	TFT_resetclipwin();

	Font curr_font = cfont;
	last_bg = _bg;
	last_fg = _fg;
	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };
    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);

	if (hdr) {
		TFT_fillRect(1, 1, _width-3, TFT_getfontheight()+6, _bg);
		TFT_print(hdr, CENTER, 4);
	}

	if (ftr) {
		TFT_fillRect(1, _height-TFT_getfontheight()-8, _width-3, TFT_getfontheight()+6, _bg);
		if (strlen(ftr) == 0) _dispTime();
		else TFT_print(ftr, CENTER, _height-TFT_getfontheight()-5);
	}

	cfont = curr_font;
	_fg = last_fg;
	_bg = last_bg;

	TFT_restoreClipWin();
}


//------------------------
// Image demo
//-------------------------
static void disp_images() {
    uint32_t tstart;

	disp_header("JPEG IMAGES");

	if (spiffs_is_mounted) {
		// ** Show scaled (1/8, 1/4, 1/2 size) JPG images
		TFT_jpg_image(CENTER, CENTER, 3, SPIFFS_BASE_PATH"/images/test1.jpg", NULL, 0);
		Wait(500);

		TFT_jpg_image(CENTER, CENTER, 2, SPIFFS_BASE_PATH"/images/test2.jpg", NULL, 0);
		Wait(500);

		TFT_jpg_image(CENTER, CENTER, 1, SPIFFS_BASE_PATH"/images/test4.jpg", NULL, 0);
		Wait(500);

		// ** Show full size JPG image
		tstart = clock();
		TFT_jpg_image(CENTER, CENTER, 0, SPIFFS_BASE_PATH"/images/test3.jpg", NULL, 0);
		tstart = clock() - tstart;
		if (doprint) printf("       JPG Decode time: %u ms\r\n", tstart);
		sprintf(tmp_buff, "Decode time: %u ms", tstart);
		update_header(NULL, tmp_buff);
		Wait(-GDEMO_INFO_TIME);

		// ** Show BMP image
		update_header("BMP IMAGE", "");
		for (int scale=5; scale >= 0; scale--) {
			tstart = clock();
			TFT_bmp_image(CENTER, CENTER, scale, SPIFFS_BASE_PATH"/images/tiger.bmp", NULL, 0);
			tstart = clock() - tstart;
			if (doprint) printf("    BMP time, scale: %d: %u ms\r\n", scale, tstart);
			sprintf(tmp_buff, "Decode time: %u ms", tstart);
			update_header(NULL, tmp_buff);
			Wait(-500);
		}
		Wait(-GDEMO_INFO_TIME);
	}
	else if (doprint) printf("  No file system found.\r\n");
}

static void update_face()
{
//HKHK

	int x, y, n;


		y = 12;
		
        time(&time_now);
		tm_info = localtime(&time_now);

		_fg = TFT_ORANGE;
		sprintf(tmp_buff, "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

		TFT_setFont(2, NULL);
		
        TFT_print(tmp_buff, CENTER, y);

        angle_face_HK();
	
}

//---------------------
static void font_demo()
{
	int x, y, n;
	uint32_t end_time;

	disp_header("CLOCK DEMO");

	//int ms = 0;
	int last_sec = 0;
	//HK uint32_t ctime = clock();
	end_time = clock() + GDEMO_TIME*2;
	n = 0;
	////HK while ((clock() < end_time) && (Wait(0))) {
	//HK ADD
    		ESP_LOGI(tag, "HKHK width: %d height: %d", _width, _height);
	while (1) {
		y = 12;
		//HK ms = clock() - ctime;
		time(&time_now);
		tm_info = localtime(&time_now);
		if (tm_info->tm_sec != last_sec) {
			last_sec = tm_info->tm_sec;
		//HK ms = 0;
		//HK ctime = clock();
		}

		_fg = TFT_ORANGE;
		//HK sprintf(tmp_buff, "%02d:%02d:%03d", tm_info->tm_min, tm_info->tm_sec, ms);
		sprintf(tmp_buff, "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

		//HK TFT_setFont(FONT_7SEG, NULL);
		TFT_setFont(2, NULL);

	    if ((_width < 240) || (_height < 240)) 
			set_7seg_font_atrib(24, 1, 1, TFT_DARKGREY);
		else 
			set_7seg_font_atrib(12, 2, 1, TFT_GREEN);
		//TFT_clearStringRect(12, y, tmp_buff);
		TFT_print(tmp_buff, CENTER, y);
		//HK n++;
	

	}
	sprintf(tmp_buff, "%d STRINGS", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

}

//---------------------
static void rect_demo()
{
	int x, y, w, h, n;

	disp_header("RECTANGLE DEMO");

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(4, dispWin.x2-4);
		y = rand_interval(4, dispWin.y2-2);
		w = rand_interval(2, dispWin.x2-x);
		h = rand_interval(2, dispWin.y2-y);
		TFT_drawRect(x,y,w,h,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d RECTANGLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("FILLED RECTANGLE", "");
	TFT_fillWindow(TFT_BLACK);
	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(4, dispWin.x2-4);
		y = rand_interval(4, dispWin.y2-2);
		w = rand_interval(2, dispWin.x2-x);
		h = rand_interval(2, dispWin.y2-y);
		TFT_fillRect(x,y,w,h,random_color());
		TFT_drawRect(x,y,w,h,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d RECTANGLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//----------------------
static void pixel_demo()
{
	int x, y, n;

	disp_header("DRAW PIXEL DEMO");

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(0, dispWin.x2);
		y = rand_interval(0, dispWin.y2);
		TFT_drawPixel(x,y,random_color(),1);
		n++;
	}
	sprintf(tmp_buff, "%d PIXELS", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//---------------------
static void line_demo()
{
	int x1, y1, x2, y2, n;

	disp_header("LINE DEMO");

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x1 = rand_interval(0, dispWin.x2);
		y1 = rand_interval(0, dispWin.y2);
		x2 = rand_interval(0, dispWin.x2);
		y2 = rand_interval(0, dispWin.y2);
		TFT_drawLine(x1,y1,x2,y2,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d LINES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//----------------------
static void aline_demo()
{
	int x, y, len, angle, n;

	disp_header("LINE BY ANGLE DEMO");

	x = (dispWin.x2 - dispWin.x1) / 2;
	y = (dispWin.y2 - dispWin.y1) / 2;
	if (x < y) len = x - 8;
	else len = y -8;

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		for (angle=0; angle < 360; angle++) {
			TFT_drawLineByAngle(x,y, 0, len, angle, random_color());
			n++;
		}
	}

	TFT_fillWindow(TFT_BLACK);
	end_time = clock() + GDEMO_TIME;
	while ((clock() < end_time) && (Wait(0))) {
		for (angle=0; angle < 360; angle++) {
			TFT_drawLineByAngle(x, y, len/4, len/4,angle, random_color());
			n++;
		}
		for (angle=0; angle < 360; angle++) {
			TFT_drawLineByAngle(x, y, len*3/4, len/4,angle, random_color());
			n++;
		}
	}
	sprintf(tmp_buff, "%d LINES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

static void angle_face_HK()
{
	int x, y, len, hour, min, sec;

	x = (dispWin.x2 - dispWin.x1) / 2;
	y = (dispWin.y2 - dispWin.y1) / 2;
	if (x < y) len = x - 8;
	else len = y -8;

    hour = (tm_info->tm_hour);
    min = (tm_info->tm_min);
    sec = (tm_info->tm_sec);


    
    
    TFT_drawLineByAngle(x,y, 0, len/4, (hour-1)*15, TFT_BLACK);
    TFT_drawLineByAngle(x,y, 0, len/4, hour*15, TFT_CYAN);
    
    TFT_drawLineByAngle(x,y, 0, len/2, (min-1)*6, TFT_BLACK);
    TFT_drawLineByAngle(x,y, 0, len/2, min*6, TFT_YELLOW);
    
    TFT_drawLineByAngle(x,y, 0, len, (sec-1)*6, TFT_BLACK);
    TFT_drawLineByAngle(x,y, 0, len, sec*6, TFT_RED);

}

//--------------------
static void arc_demo()
{
	uint16_t x, y, r, th, n, i;
	float start, end;
	color_t color, fillcolor;

	disp_header("ARC DEMO");

	x = (dispWin.x2 - dispWin.x1) / 2;
	y = (dispWin.y2 - dispWin.y1) / 2;

	th = 6;
	uint32_t end_time = clock() + GDEMO_TIME;
	i = 0;
	while ((clock() < end_time) && (Wait(0))) {
		if (x < y) r = x - 4;
		else r = y - 4;
		start = 0;
		end = 20;
		n = 1;
		while (r > 10) {
			color = random_color();
			TFT_drawArc(x, y, r, th, start, end, color, color);
			r -= (th+2);
			n++;
			start += 30;
			end = start + (n*20);
			i++;
		}
	}
	sprintf(tmp_buff, "%d ARCS", i);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("OUTLINED ARC", "");
	TFT_fillWindow(TFT_BLACK);
	th = 8;
	end_time = clock() + GDEMO_TIME;
	i = 0;
	while ((clock() < end_time) && (Wait(0))) {
		if (x < y) r = x - 4;
		else r = y - 4;
		start = 0;
		end = 350;
		n = 1;
		while (r > 10) {
			color = random_color();
			fillcolor = random_color();
			TFT_drawArc(x, y, r, th, start, end, color, fillcolor);
			r -= (th+2);
			n++;
			start += 20;
			end -= n*10;
			i++;
		}
	}
	sprintf(tmp_buff, "%d ARCS", i);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//-----------------------
static void circle_demo()
{
	int x, y, r, n;

	disp_header("CIRCLE DEMO");

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(8, dispWin.x2-8);
		y = rand_interval(8, dispWin.y2-8);
		if (x < y) r = rand_interval(2, x/2);
		else r = rand_interval(2, y/2);
		TFT_drawCircle(x,y,r,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d CIRCLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("FILLED CIRCLE", "");
	TFT_fillWindow(TFT_BLACK);
	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(8, dispWin.x2-8);
		y = rand_interval(8, dispWin.y2-8);
		if (x < y) r = rand_interval(2, x/2);
		else r = rand_interval(2, y/2);
		TFT_fillCircle(x,y,r,random_color());
		TFT_drawCircle(x,y,r,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d CIRCLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//------------------------
static void ellipse_demo()
{
	int x, y, rx, ry, n;

	disp_header("ELLIPSE DEMO");

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(8, dispWin.x2-8);
		y = rand_interval(8, dispWin.y2-8);
		if (x < y) rx = rand_interval(2, x/4);
		else rx = rand_interval(2, y/4);
		if (x < y) ry = rand_interval(2, x/4);
		else ry = rand_interval(2, y/4);
		TFT_drawEllipse(x,y,rx,ry,random_color(),15);
		n++;
	}
	sprintf(tmp_buff, "%d ELLIPSES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("FILLED ELLIPSE", "");
	TFT_fillWindow(TFT_BLACK);
	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(8, dispWin.x2-8);
		y = rand_interval(8, dispWin.y2-8);
		if (x < y) rx = rand_interval(2, x/4);
		else rx = rand_interval(2, y/4);
		if (x < y) ry = rand_interval(2, x/4);
		else ry = rand_interval(2, y/4);
		TFT_fillEllipse(x,y,rx,ry,random_color(),15);
		TFT_drawEllipse(x,y,rx,ry,random_color(),15);
		n++;
	}
	sprintf(tmp_buff, "%d ELLIPSES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("ELLIPSE SEGMENTS", "");
	TFT_fillWindow(TFT_BLACK);
	end_time = clock() + GDEMO_TIME;
	n = 0;
	int k = 1;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(8, dispWin.x2-8);
		y = rand_interval(8, dispWin.y2-8);
		if (x < y) rx = rand_interval(2, x/4);
		else rx = rand_interval(2, y/4);
		if (x < y) ry = rand_interval(2, x/4);
		else ry = rand_interval(2, y/4);
		TFT_fillEllipse(x,y,rx,ry,random_color(), (1<<k));
		TFT_drawEllipse(x,y,rx,ry,random_color(), (1<<k));
		k = (k+1) & 3;
		n++;
	}
	sprintf(tmp_buff, "%d SEGMENTS", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//-------------------------
static void triangle_demo()
{
	int x1, y1, x2, y2, x3, y3, n;

	disp_header("TRIANGLE DEMO");

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x1 = rand_interval(4, dispWin.x2-4);
		y1 = rand_interval(4, dispWin.y2-2);
		x2 = rand_interval(4, dispWin.x2-4);
		y2 = rand_interval(4, dispWin.y2-2);
		x3 = rand_interval(4, dispWin.x2-4);
		y3 = rand_interval(4, dispWin.y2-2);
		TFT_drawTriangle(x1,y1,x2,y2,x3,y3,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d TRIANGLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("FILLED TRIANGLE", "");
	TFT_fillWindow(TFT_BLACK);
	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x1 = rand_interval(4, dispWin.x2-4);
		y1 = rand_interval(4, dispWin.y2-2);
		x2 = rand_interval(4, dispWin.x2-4);
		y2 = rand_interval(4, dispWin.y2-2);
		x3 = rand_interval(4, dispWin.x2-4);
		y3 = rand_interval(4, dispWin.y2-2);
		TFT_fillTriangle(x1,y1,x2,y2,x3,y3,random_color());
		TFT_drawTriangle(x1,y1,x2,y2,x3,y3,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d TRIANGLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//---------------------
static void poly_demo()
{
	uint16_t x, y, rot, oldrot;
	int i, n, r;
	uint8_t sides[6] = {3, 4, 5, 6, 8, 10};
	color_t color[6] = {TFT_WHITE, TFT_CYAN, TFT_RED,       TFT_BLUE,     TFT_YELLOW,     TFT_ORANGE};
	color_t fill[6]  = {TFT_BLUE,  TFT_NAVY,   TFT_DARKGREEN, TFT_DARKGREY, TFT_LIGHTGREY, TFT_OLIVE};

	disp_header("POLYGON DEMO");

	x = (dispWin.x2 - dispWin.x1) / 2;
	y = (dispWin.y2 - dispWin.y1) / 2;

	rot = 0;
	oldrot = 0;
	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		if (x < y) r = x - 4;
		else r = y - 4;
		for (i=5; i>=0; i--) {
			TFT_drawPolygon(x, y, sides[i], r, TFT_BLACK, TFT_BLACK, oldrot, 1);
			TFT_drawPolygon(x, y, sides[i], r, color[i], color[i], rot, 1);
			r -= 16;
            if (r <= 0) break;
			n += 2;
		}
		Wait(100);
		oldrot = rot;
		rot = (rot + 15) % 360;
	}
	sprintf(tmp_buff, "%d POLYGONS", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("FILLED POLYGON", "");
	rot = 0;
	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		if (x < y) r = x - 4;
		else r = y - 4;
		TFT_fillWindow(TFT_BLACK);
		for (i=5; i>=0; i--) {
			TFT_drawPolygon(x, y, sides[i], r, color[i], fill[i], rot, 2);
			r -= 16;
            if (r <= 0) break;
			n += 2;
		}
		Wait(500);
		rot = (rot + 15) % 360;
	}
	sprintf(tmp_buff, "%d POLYGONS", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//----------------------
static void touch_demo()
{
#if USE_TOUCH
	int tx, ty, ltx, lty, doexit = 0;

	disp_header("TOUCH DEMO");
	TFT_setFont(DEFAULT_FONT, NULL);
	_fg = TFT_YELLOW;
	TFT_print("Touch to draw", CENTER, 40);
	TFT_print("Touch footer to clear", CENTER, 60);

	ltx = -9999;
	lty = -999;
	while (1) {
		if (TFT_read_touch(&tx, &ty, 0)) {
			// Touched
			if (((tx >= dispWin.x1) && (tx <= dispWin.x2)) &&
				((ty >= dispWin.y1) && (ty <= dispWin.y2))) {
				if ((doexit > 2) || ((abs(tx-ltx) < 5) && (abs(ty-lty) < 5))) {
					if (((abs(tx-ltx) > 0) || (abs(ty-lty) > 0))) {
						TFT_fillCircle(tx-dispWin.x1, ty-dispWin.y1, 4,random_color());
						sprintf(tmp_buff, "%d,%d", tx, ty);
						update_header(NULL, tmp_buff);
					}
					ltx = tx;
					lty = ty;
				}
				doexit = 0;
			}
			else if (ty > (dispWin.y2+5)) TFT_fillWindow(TFT_BLACK);
			else {
				doexit++;
				if (doexit == 2) update_header(NULL, "---");
				if (doexit > 50) return;
				vTaskDelay(100 / portTICK_RATE_MS);
			}
		}
		else {
			doexit++;
			if (doexit == 2) update_header(NULL, "---");
			if (doexit > 50) return;
			vTaskDelay(100 / portTICK_RATE_MS);
		}
	}
#endif
}

void tft_demo_task(void *pvParameters) {


    printf("Task1 running on core %d", xPortGetCoreID());
    for (;;)    {

		font_demo();

    }


}

//===============
void tft_demo_init() {

	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	TFT_resetclipwin();

	image_debug = 0;

    char dtype[16];
    
    switch (tft_disp_type) {
        case DISP_TYPE_ILI9341:
            sprintf(dtype, "ILI9341");
            break;
        case DISP_TYPE_ILI9488:
            sprintf(dtype, "ILI9488");
            break;
        case DISP_TYPE_ST7789V:
            sprintf(dtype, "ST7789V");
            break;
        case DISP_TYPE_ST7735:
            sprintf(dtype, "ST7735");
            break;
        case DISP_TYPE_ST7735R:
            sprintf(dtype, "ST7735R");
            break;
        case DISP_TYPE_ST7735B:
            sprintf(dtype, "ST7735B");
            break;
        default:
            sprintf(dtype, "Unknown");
    }
    
    //HK uint8_t disp_rot = PORTRAIT;
    uint8_t disp_rot = LANDSCAPE;
	_demo_pass = 0;
	gray_scale = 0;
	doprint = 1;

	TFT_setRotation(disp_rot);
	disp_header("ESP32 TFT DEMO");
	TFT_setFont(COMIC24_FONT, NULL);
	int tempy = TFT_getfontheight() + 4;
	_fg = TFT_ORANGE;
	TFT_print("ESP32", CENTER, (dispWin.y2-dispWin.y1)/2 - tempy);
	TFT_setFont(UBUNTU16_FONT, NULL);
	_fg = TFT_CYAN;
	TFT_print("TFT Demo", CENTER, LASTY+tempy);
	tempy = TFT_getfontheight() + 4;
	TFT_setFont(DEFAULT_FONT, NULL);
	_fg = TFT_GREEN;
	sprintf(tmp_buff, "Read speed: %5.2f MHz", (float)max_rdclock/1000000.0);
	TFT_print(tmp_buff, CENTER, LASTY+tempy);

	Wait(4000);

		_bg = TFT_BLACK;
		TFT_setRotation(LANDSCAPE);
		disp_header("Welcome to ESP32");


}

typedef struct {
    int type;  // the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;

xQueueHandle timer_queue;


static void inline print_timer_counter(uint64_t counter_value)
{
    printf("Counter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32),
                                    (uint32_t) (counter_value));
    printf("Time   : %.8f s\n", (double) counter_value / TIMER_SCALE);
}


void IRAM_ATTR timer_group0_isr(void *para)
{
    int timer_idx = (int) para;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t intr_status = TIMERG0.int_st_timers.val;
    TIMERG0.hw_timer[timer_idx].update = 1;
    uint64_t timer_counter_value =
        ((uint64_t) TIMERG0.hw_timer[timer_idx].cnt_high) << 32
        | TIMERG0.hw_timer[timer_idx].cnt_low;

    /* Prepare basic event data
       that will be then sent back to the main program task */
    timer_event_t evt;
    evt.timer_group = 0;
    evt.timer_idx = timer_idx;
    evt.timer_counter_value = timer_counter_value;

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        evt.type = TEST_WITHOUT_RELOAD;
        TIMERG0.int_clr_timers.t0 = 1;
        timer_counter_value += (uint64_t) (TIMER_INTERVAL0_SEC * TIMER_SCALE);
        TIMERG0.hw_timer[timer_idx].alarm_high = (uint32_t) (timer_counter_value >> 32);
        TIMERG0.hw_timer[timer_idx].alarm_low = (uint32_t) timer_counter_value;
    } else if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_1) {
        evt.type = TEST_WITH_RELOAD;
        TIMERG0.int_clr_timers.t1 = 1;
    } else {
        evt.type = -1; // not supported even type
    }

    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

    /* Now just send the event data back to the main program task */
    xQueueSendFromISR(timer_queue, &evt, NULL);
}


static void example_tg0_timer_init(int timer_idx, bool auto_reload, double timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = auto_reload;
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr,
        (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}

static void timer_example_evt_task(void *arg)
{
    while (1) {
        timer_event_t evt;
        xQueueReceive(timer_queue, &evt, portMAX_DELAY);

        /* Print information that the timer reported an event */
/* HK
        if (evt.type == TEST_WITHOUT_RELOAD) {
             printf("\n    Example timer without reload\n");
        } else if (evt.type == TEST_WITH_RELOAD) {
             printf("\n    Example timer with auto reload\n");
        } else {
             printf("\n    UNKNOWN EVENT TYPE\n");
        }
         printf("Group[%d], timer[%d] alarm event: %d\n", evt.timer_group, evt.timer_idx, evt.type);

         printf("------- EVENT TIME --------\n");
         print_timer_counter(evt.timer_counter_value);

         printf("-------- TASK TIME --------\n");
        uint64_t task_counter_value;
         timer_get_counter_value(evt.timer_group, evt.timer_idx, &task_counter_value);
         print_timer_counter(task_counter_value);
HK */

        update_header(NULL,"");
        update_face();
    }
}


//=============
void app_main()
{

    esp_err_t ret;

    // === SET GLOBAL VARIABLES ==========================

    // ===================================================
    // ==== Set display type                         =====
    tft_disp_type = DEFAULT_DISP_TYPE;
	//tft_disp_type = DISP_TYPE_ILI9341;
	//tft_disp_type = DISP_TYPE_ILI9488;
	//tft_disp_type = DISP_TYPE_ST7735B;
    // ===================================================

	// ===================================================
	// === Set display resolution if NOT using default ===
	// === DEFAULT_TFT_DISPLAY_WIDTH &                 ===
    // === DEFAULT_TFT_DISPLAY_HEIGHT                  ===
	_width = DEFAULT_TFT_DISPLAY_WIDTH;  // smaller dimension
	_height = DEFAULT_TFT_DISPLAY_HEIGHT; // larger dimension
	//_width = 128;  // smaller dimension
	//_height = 160; // larger dimension
	// ===================================================

	// ===================================================
	// ==== Set maximum spi clock for display read    ====
	//      operations, function 'find_rd_speed()'    ====
	//      can be used after display initialization  ====
	max_rdclock = 8000000;
	// ===================================================

    // ====================================================================
    // === Pins MUST be initialized before SPI interface initialization ===
    // ====================================================================
    TFT_PinsInit();

    // ====  CONFIGURE SPI DEVICES(s)  ====================================================================================

    spi_lobo_device_handle_t spi;
	
    spi_lobo_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,				// set SPI MISO pin
        .mosi_io_num=PIN_NUM_MOSI,				// set SPI MOSI pin
        .sclk_io_num=PIN_NUM_CLK,				// set SPI CLK pin
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
		.max_transfer_sz = 6*1024,
    };
    spi_lobo_device_interface_config_t devcfg={
        .clock_speed_hz=8000000,                // Initial clock out at 8 MHz
        .mode=0,                                // SPI mode 0
        .spics_io_num=-1,                       // we will use external CS pin
		.spics_ext_io_num=PIN_NUM_CS,           // external CS pin
		.flags=LB_SPI_DEVICE_HALFDUPLEX,        // ALWAYS SET  to HALF DUPLEX MODE!! for display spi
    };



    vTaskDelay(500 / portTICK_RATE_MS);
	printf("\r\n==============================\r\n");
    printf("TFT display DEMO, LoBo 11/2017\r\n");
	printf("==============================\r\n");
    printf("Pins used: miso=%d, mosi=%d, sck=%d, cs=%d\r\n", PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);

	// ==================================================================
	// ==== Initialize the SPI bus and attach the LCD to the SPI bus ====

	ret=spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
    assert(ret==ESP_OK);
	printf("SPI: display device added to spi bus (%d)\r\n", SPI_BUS);
	disp_spi = spi;

	// ==== Test select/deselect ====
	ret = spi_lobo_device_select(spi, 1);
    assert(ret==ESP_OK);
	ret = spi_lobo_device_deselect(spi);
    assert(ret==ESP_OK);

	printf("SPI: attached display device, speed=%u\r\n", spi_lobo_get_speed(spi));
	printf("SPI: bus uses native pins: %s\r\n", spi_lobo_uses_native_pins(spi) ? "true" : "false");


	// ================================
	// ==== Initialize the Display ====

	printf("SPI: display init...\r\n");
	TFT_display_init();
    printf("OK\r\n");
    #if USE_TOUCH == TOUCH_TYPE_STMPE610
	stmpe610_Init();
	vTaskDelay(10 / portTICK_RATE_MS);
    uint32_t tver = stmpe610_getID();
    printf("STMPE touch initialized, ver: %04x - %02x\r\n", tver >> 8, tver & 0xFF);
    #endif
	
	// ---- Detect maximum read speed ----
	max_rdclock = find_rd_speed();
	printf("SPI: Max rd speed = %u\r\n", max_rdclock);

    // ==== Set SPI clock used for display operations ====
	spi_lobo_set_speed(spi, DEFAULT_SPI_CLOCK);
	printf("SPI: Changed speed to %u\r\n", spi_lobo_get_speed(spi));

    printf("\r\n---------------------\r\n");
	printf("Graphics demo started\r\n");
	printf("---------------------\r\n");

	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	gray_scale = 0;
    TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
	TFT_setRotation(PORTRAIT);
	TFT_setFont(DEFAULT_FONT, NULL);
	TFT_resetclipwin();

#ifdef CONFIG_EXAMPLE_USE_WIFI

    ESP_ERROR_CHECK( nvs_flash_init() );

    // ===== Set time zone ======
	//HK setenv("TZ", "CET-1CEST", 0);
	setenv("TZ", "KST-9", 1);
	tzset();
	// ==========================

	disp_header("GET NTP TIME");

    	time(&time_now);
	tm_info = localtime(&time_now);

	// Is time set? If not, tm_year will be (1970 - 1900).
    if (tm_info->tm_year < (2016 - 1900)) {
        ESP_LOGI(tag, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        _fg = TFT_CYAN;
    	TFT_print("Time is not set yet", CENTER, CENTER);
    	TFT_print("Connecting to WiFi", CENTER, LASTY+TFT_getfontheight()+2);
    	TFT_print("Getting time over NTP", CENTER, LASTY+TFT_getfontheight()+2);
    	_fg = TFT_YELLOW;
    	TFT_print("Wait", CENTER, LASTY+TFT_getfontheight()+2);
        if (obtain_time()) {
        	_fg = TFT_GREEN;
        	TFT_print("System time is set.", CENTER, LASTY);
        }
        else {
        	_fg = TFT_RED;
        	TFT_print("ERROR.", CENTER, LASTY);
        }
        time(&time_now);
    	update_header(NULL, "");
    	Wait(-2000);
    }
#endif

	disp_header("File system INIT");
    _fg = TFT_CYAN;
	TFT_print("Initializing SPIFFS...", CENTER, CENTER);
    // ==== Initialize the file system ====
    printf("\r\n\n");
	vfs_spiffs_register();
    if (!spiffs_is_mounted) {
    	_fg = TFT_RED;
    	TFT_print("SPIFFS not mounted !", CENTER, LASTY+TFT_getfontheight()+2);
    }
    else {
    	_fg = TFT_GREEN;
    	TFT_print("SPIFFS Mounted.", CENTER, LASTY+TFT_getfontheight()+2);
    }
	Wait(-2000);

	//=========
    // Run demo
    //=========
    
    tft_demo_init();

/*
     xTaskCreatePinnedToCore(tft_demo_task, // task function
                            "tft demo", // name of task
                            2048, // stack size of task
                            NULL, // parameter of the task 
                            5,  // priority
                            &Task1, // Task Handle to kepp track of created task
                            0); // pin task to core 0
    
*/
    timer_queue = xQueueCreate(10, sizeof(timer_event_t)); 
    example_tg0_timer_init(TIMER_1, TEST_WITH_RELOAD, TIMER_INTERVAL1_SEC);
    
    xTaskCreatePinnedToCore(timer_example_evt_task, // task function
                            "timer demo", // name of task
                            2048, // stack size of task
                            NULL, // parameter of the task 
                            6,  // priority
                            &Task2, // Task Handle to kepp track of created task
                            1); // pin task to core 0
}
