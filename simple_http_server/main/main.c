/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/ledc.h"

#include "sdkconfig.h"

#include "esp_adc_cal.h"
#include "esp_err.h"
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "cJSON.h"

#include <http_server.h>

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 * The examples use simple WiFi configuration that you can set via
 * 'make menuconfig'.
 * If you'd rather not, just change the below entries to strings
 * with the config you want -
 * ie. #define EXAMPLE_WIFI_SSID "mywifissid"
*/

#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE

#define LEDC_HS_CH0_GPIO       (19)
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0

#define LEDC_HS_CH1_GPIO       (18)
#define LEDC_HS_CH1_CHANNEL    LEDC_CHANNEL_1

#define LEDC_HS_CH2_GPIO       (5)
#define LEDC_HS_CH2_CHANNEL    LEDC_CHANNEL_2

#define LEDC_TEST_CH_NUM       (3)

#define A1CHANNEL (1 << 0)
#define B1CHANNEL (1 << 1)
#define C1CHANNEL (1 << 2)
#define OVERCURRENT (1 << 3)

#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD

#define DEFAULT_VREF    1300
#define NO_OF_SAMPLES   64
#define adcResolution          ADC_WIDTH_BIT_11

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t adcA1_channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_channel_t adcB1_channel = ADC_CHANNEL_7;     //GPIO35
static const adc_channel_t adcC1_channel = ADC_CHANNEL_4;     //GPIO32
static const adc_atten_t atten = ADC_ATTEN_DB_11;//at = 0
static const adc_unit_t unit = ADC_UNIT_1;

static const char *TAG  ="APP";
static const char *TAG2 ="TASK";


char *internetProtocol;
cJSON *postJSON, *getJSON;
char *channel              = "A1";
char *type                 = "cc";
int frequency              = 0;
uint32_t amplitude         = 1024;
uint32_t i_max             = 1024;
int phase                  = 0;
uint32_t offset            = 0;
int rise_time              = 0;

uint32_t current           = 0;
uint32_t voltage           = 0;

uint32_t adc_A1 = 0;
uint32_t adc_B1 = 0;
uint32_t adc_C1 = 0;


QueueHandle_t xQueuePWM, xQueueChannel;
EventGroupHandle_t channel_opt;


ledc_timer_config_t ledc_timer = {
    .duty_resolution = LEDC_TIMER_11_BIT,  // resolution of PWM duty
    .freq_hz = 38000,                      // amplitude of PWM signal
    .speed_mode = LEDC_HS_MODE,            // timer mode
    .timer_num = LEDC_HS_TIMER             // timer index
};
    // Set configuration of timer0 for high speed channels

ledc_channel_config_t ledc_channel[LEDC_TEST_CH_NUM] = {
    {
        .channel    = LEDC_HS_CH0_CHANNEL,
        .duty       = 1024,
        .gpio_num   = LEDC_HS_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .timer_sel  = LEDC_HS_TIMER
    },

    {
        .channel    = LEDC_HS_CH1_CHANNEL,
        .duty       = 1024,
        .gpio_num   = LEDC_HS_CH1_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .timer_sel  = LEDC_HS_TIMER
    },

    {
        .channel    = LEDC_HS_CH2_CHANNEL,
        .duty       = 1024,
        .gpio_num   = LEDC_HS_CH2_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .timer_sel  = LEDC_HS_TIMER
    },

}; // MAX DUTY = 4095

/*-------------------------------------------------------------*/
/*--------------------------- TASKS ---------------------------*/
/*-------------------------------------------------------------*/
void channelControlTask(void *pvParameter) {

    EventBits_t waitingChannel, receivedChannel;
    waitingChannel = (EventBits_t) pvParameter;
    int chSet = 0;

    while(1) {

        receivedChannel = xEventGroupWaitBits(channel_opt, A1CHANNEL | B1CHANNEL | C1CHANNEL, pdTRUE, pdFALSE, portMAX_DELAY);

        if(receivedChannel == A1CHANNEL) chSet = 0;

        if(receivedChannel == B1CHANNEL) chSet = 1;

        if(receivedChannel == C1CHANNEL) chSet = 2;

        if(xQueueChannel != 0 && xQueuePWM != 0) {

            if(xQueueSendToBack(xQueuePWM, &amplitude, portMAX_DELAY) & xQueueSendToBack(xQueueChannel, &chSet, portMAX_DELAY)) {
                ESP_LOGI(TAG2, "New PMW valued %d set into channel '%s'", amplitude, channel );

            }
        }
    }
}

void adcReadTaskA1(void *pvParameter) {

    adc1_config_width(adcResolution);
    adc1_config_channel_atten(adcA1_channel, atten);

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, adcResolution, DEFAULT_VREF, adc_chars);

    //Continuously sample ADC1
    while (1) {
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++)
            adc_A1 += adc1_get_raw((adc_channel_t)adcA1_channel);

        adc_A1 /= NO_OF_SAMPLES;

        vTaskDelay(50/portTICK_PERIOD_MS);
    }
}

void adcReadTaskB1(void *pvParameter) {

    adc1_config_width(adcResolution);
    adc1_config_channel_atten(adcB1_channel, atten);

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, adcResolution, DEFAULT_VREF, adc_chars);

    //Continuously sample ADC1
    while (1) {
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++)
            adc_B1 += adc1_get_raw((adc_channel_t)adcB1_channel);

        adc_B1 /= NO_OF_SAMPLES;
        
        vTaskDelay(50/portTICK_PERIOD_MS);
    }
}

void adcReadTaskC1(void *pvParameter) {

    adc1_config_width(adcResolution);
    adc1_config_channel_atten(adcC1_channel, atten);

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, adcResolution, DEFAULT_VREF, adc_chars);

    //Continuously sample ADC1
    while (1) {
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++)
            adc_C1 += adc1_get_raw((adc_channel_t)adcC1_channel);

        adc_C1 /= NO_OF_SAMPLES;
        
        vTaskDelay(50/portTICK_PERIOD_MS);
    }
}

void pwmControlTask(void *pvParameter) {
    //int ch;
    uint32_t json_duty = 0;
    int json_channel = 0;

    EventBits_t waitingChannel;
    waitingChannel = (EventBits_t)pvParameter;

    ledc_timer_config(&ledc_timer);

    for (int i = 0; i < LEDC_TEST_CH_NUM; ++i) {
        ledc_channel_config(&ledc_channel[i]);
    }
    

    while (1) {
        if(xQueueReceive(xQueuePWM, &json_duty, portMAX_DELAY) & xQueueReceive(xQueueChannel, &json_channel, portMAX_DELAY)) {
            printf("\nReading Successfull. New PWM: %d", json_duty);
            ledc_set_duty(ledc_channel[json_channel].speed_mode, ledc_channel[json_channel].channel, json_duty);
            ledc_update_duty(ledc_channel[json_channel].speed_mode, ledc_channel[json_channel].channel);
        }
    }
}

/*-------------------------------------------------------------*/
/*--------------------------- TASKS ---------------------------*/
/*-------------------------------------------------------------*/

/*-------------------------------------------------------------*/
/*------------------------- FUNCTIONS -------------------------*/
/*-------------------------------------------------------------*/

char *createGetJSON(uint32_t v, uint32_t i) {

    char *rendered;

    getJSON = cJSON_CreateObject();

    cJSON_AddNumberToObject(getJSON, "voltage", v);
    cJSON_AddNumberToObject(getJSON, "current", i);

    rendered = cJSON_Print(getJSON);

    return rendered;
}

void createPostJSON() {
	postJSON = cJSON_CreateObject();

	cJSON_AddStringToObject(postJSON, "channel", channel);
    cJSON_AddStringToObject(postJSON, "type", type);
    cJSON_AddNumberToObject(postJSON, "frequency", frequency);
    cJSON_AddNumberToObject(postJSON, "amplitude", amplitude);
    cJSON_AddNumberToObject(postJSON, "i_max", i_max);
    cJSON_AddNumberToObject(postJSON, "phase", phase);
    cJSON_AddNumberToObject(postJSON, "offset", offset);
    cJSON_AddNumberToObject(postJSON, "rise_time", rise_time);
}

void removeChar(char *str, char garbage) {

    char *src, *dst;
    for (src = dst = str; *src != '\0'; src++) {
        *dst = *src;
        if (*dst != garbage) dst++;
    }
    *dst = '\0';
}


void updateJSON(char *incoming) {

    EventBits_t canal;

    cJSON *var          = cJSON_Parse(incoming);
    cJSON *newChannel   = NULL;
    cJSON *newType      = NULL;
    

    newChannel  = cJSON_GetObjectItemCaseSensitive(var, "channel");
    newType     = cJSON_GetObjectItemCaseSensitive(var, "type");

    frequency   = cJSON_GetObjectItemCaseSensitive(var, "frequency")->valueint;
    amplitude   = cJSON_GetObjectItemCaseSensitive(var, "amplitude")->valueint;
    i_max       = cJSON_GetObjectItemCaseSensitive(var, "i_max")->valueint;
    phase       = cJSON_GetObjectItemCaseSensitive(var, "phase")->valueint;
    offset      = cJSON_GetObjectItemCaseSensitive(var, "offset")->valueint;
    rise_time   = cJSON_GetObjectItemCaseSensitive(var, "rise_time")->valueint;


    if(cJSON_IsString(newChannel)) {
        channel = cJSON_Print(newChannel);
        removeChar(channel,'/');
        removeChar(channel,'"');
        printf("channel: %s\n",channel);  // TEM QUE REMOVER AS ASPAS
    }

    if(strcmp(channel,"A1") == 0) {
        canal = xEventGroupSetBits(channel_opt, A1CHANNEL);
        ESP_LOGI(TAG, "Configurando canal A1 ...");
    }

    else if(strcmp(channel,"B1") == 0) {
        canal = xEventGroupSetBits(channel_opt, B1CHANNEL);
        ESP_LOGI(TAG, "Configurando canal B1 ...");
    }

    else if(strcmp(channel,"C1") == 0) {
        canal = xEventGroupSetBits(channel_opt, C1CHANNEL);
        ESP_LOGI(TAG, "Configurando canal C1 ...");
    }
}

/*-------------------------------------------------------------*/
/*------------------------- FUNCTIONS -------------------------*/
/*-------------------------------------------------------------*/


/*-------------------------------------------------------------*/
/*----------------------- HTTP HANDLERS -----------------------*/
/*-------------------------------------------------------------*/


/* An HTTP GET handler */
/* Get header value string length and allocate memory for length + 1, 
 * extra byte for null termination: buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1; */
/* Copy null terminated value string into buffer and test it: httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) */
/* Send response with custom headers and body set as the
 * string passed in user context: httpd_resp_send(req, resp_str, strlen(resp_str));*/
/* After sending the HTTP response the old HTTP request
 * headers are lost. Check if HTTP request headers can be read now: httpd_req_get_hdr_value_len(req, "Host") == 0*/
esp_err_t getInfoA1_get_handler(httpd_req_t *req) {
    char*  buf;
    size_t buf_len;
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;

    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    uint32_t voltage = ledc_get_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel);

    const char* resp_str = createGetJSON(voltage, adc_A1);
    httpd_resp_send(req, resp_str, strlen(resp_str));

    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

httpd_uri_t getInfoA1 = {
    .uri       = "/getInfoA1",
    .method    = HTTP_GET,
    .handler   = getInfoA1_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

esp_err_t getInfoB1_get_handler(httpd_req_t *req) {
    char*  buf;
    size_t buf_len;
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    uint32_t voltage = ledc_get_duty(ledc_channel[1].speed_mode, ledc_channel[1].channel);

    const char* resp_str = createGetJSON(voltage, adc_B1);
    httpd_resp_send(req, resp_str, strlen(resp_str));

    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

httpd_uri_t getInfoB1 = {
    .uri       = "/getInfoB1",
    .method    = HTTP_GET,
    .handler   = getInfoB1_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

esp_err_t getInfoC1_get_handler(httpd_req_t *req) {
    char*  buf;
    size_t buf_len;
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    uint32_t voltage = ledc_get_duty(ledc_channel[2].speed_mode, ledc_channel[2].channel);

    const char* resp_str = createGetJSON(voltage, adc_C1);
    httpd_resp_send(req, resp_str, strlen(resp_str));

    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

httpd_uri_t getInfoC1 = {
    .uri       = "/getInfoC1",
    .method    = HTTP_GET,
    .handler   = getInfoC1_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

/* An HTTP POST handler */
esp_err_t setParameters_post_handler(httpd_req_t *req) {
    char buf[400];
    int ret, remaining = req->content_len;
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) < 0) {
            return ESP_FAIL;
        }

        

        /* Send back the same data */
        
        remaining -= ret;

        

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
        printf("\n%s\n", internetProtocol);

    }

    // End response
    char *pbuf = &buf;
    updateJSON(pbuf);
    httpd_resp_send(req, pbuf, strlen(pbuf));
    return ESP_OK;
}

httpd_uri_t setParameters = {
    .uri       = "/setParameters",
    .method    = HTTP_POST,
    .handler   = setParameters_post_handler,
    .user_ctx  = NULL
};

httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &getInfoA1);
        httpd_register_uri_handler(server, &getInfoB1);
        httpd_register_uri_handler(server, &getInfoC1);
        httpd_register_uri_handler(server, &setParameters);
        //httpd_register_uri_handler(server, &ctrl);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server) {
    // Stop the httpd server
    httpd_stop(server);
}

/*-------------------------------------------------------------*/
/*----------------------- HTTP HANDLERS -----------------------*/
/*-------------------------------------------------------------*/

static esp_err_t event_handler(void *ctx, system_event_t *event) {
    httpd_handle_t *server = (httpd_handle_t *) ctx;

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
        ESP_LOGI(TAG, "Got IP: '%s'",
                ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        internetProtocol = ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip);

        /* Start the web server */
        if (*server == NULL) {
            *server = start_webserver();
        }
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
        ESP_ERROR_CHECK(esp_wifi_connect());

        /* Stop the web server */
        if (*server) {
            stop_webserver(*server);
            *server = NULL;
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void *arg) {
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, arg));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main() {
    static httpd_handle_t server = NULL;
    //ESP_ERROR_CHECK(nvs_flash_init());
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    createPostJSON();
    channel_opt = xEventGroupCreate();

    ESP_ERROR_CHECK(ret);
    initialise_wifi(&server);
    xQueuePWM     = xQueueCreate(10, sizeof(int));
    xQueueChannel = xQueueCreate(10, sizeof(int));
    xTaskCreate(&pwmControlTask, "pwmControlTask", 5000, NULL, 5, NULL);
    xTaskCreate(&channelControlTask, "channelControlTask", 5000, NULL, 6, NULL);
    xTaskCreate(&adcReadTaskA1, "adcReadTaskA1", 5000, NULL, 7, NULL);
    xTaskCreate(&adcReadTaskB1, "adcReadTaskB1", 5000, NULL, 7, NULL);
    xTaskCreate(&adcReadTaskC1, "adcReadTaskC1", 5000, NULL, 7, NULL);
}
