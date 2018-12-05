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
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/ledc.h"
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

#define LEDC_TEST_CH_NUM       (6)

#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD

static const char *TAG  ="APP";
static const char *TAG2 ="TASK";
char *internetProtocol;
cJSON *root;
char *channel              = "A1";
char *type                 = "cc";
int frequency              = 0;
uint32_t amplitude         = 0;
uint32_t i_max             = 0;
int phase                  = 0;
uint32_t offset            = 0;
int rise_time              = 0;




QueueHandle_t xQueueReadings;

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
        .duty       = 0,
        .gpio_num   = LEDC_HS_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .timer_sel  = LEDC_HS_TIMER
    },

}; // MAX DUTY = 4095

void pwmControlTask(void *pvParameter) {
    //int ch;
    uint32_t json_duty = 0;

    ledc_timer_config(&ledc_timer);
    ledc_channel_config(&ledc_channel[0]);

    while (1) {
        if(xQueueReceive(xQueueReadings, &json_duty, portMAX_DELAY)) {
            printf("\nReading Successfull. New PWM: %d", json_duty);
            ledc_set_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel, json_duty);
            ledc_update_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel);
        }
    }
}

char *jsonToString() {
	char *rendered;

	root = cJSON_CreateObject();

	//cJSON_AddItemToObject(root, "name", cJSON_CreateString("example"));
	//cJSON_AddItemToObject(root , "format", fmt = cJSON_CreateObject());
	cJSON_AddStringToObject(root, "channel", channel);
    cJSON_AddStringToObject(root, "type", type);
    cJSON_AddNumberToObject(root, "frequency", frequency);
    cJSON_AddNumberToObject(root, "amplitude", amplitude);
    cJSON_AddNumberToObject(root, "i_max", i_max);
    cJSON_AddNumberToObject(root, "phase", phase);
    cJSON_AddNumberToObject(root, "offset", offset);
    cJSON_AddNumberToObject(root, "rise_time", rise_time);
    
	rendered = cJSON_Print(root);

	return rendered;
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

    cJSON *var          = cJSON_Parse(incoming);
    cJSON *newChannel   = NULL;
    cJSON *newType      = NULL;
    

    newChannel  = cJSON_GetObjectItemCaseSensitive(var, "channel");
    newType     = cJSON_GetObjectItemCaseSensitive(var, "type");

    frequency = cJSON_GetObjectItemCaseSensitive(var, "frequency")->valueint;
    amplitude = cJSON_GetObjectItemCaseSensitive(var, "amplitude")->valueint;
    i_max     = cJSON_GetObjectItemCaseSensitive(var, "i_max")->valueint;
    phase     = cJSON_GetObjectItemCaseSensitive(var, "phase")->valueint;
    offset    = cJSON_GetObjectItemCaseSensitive(var, "offset")->valueint;
    rise_time = cJSON_GetObjectItemCaseSensitive(var, "rise_time")->valueint;


    if(cJSON_IsString(newChannel)) {
        channel = cJSON_Print(newChannel);
        removeChar(channel,'/');
        removeChar(channel,'"');
        printf("channel: %s\n",channel);  // TEM QUE REMOVER AS ASPAS
    }

    //amplitude = atoi(cJSON_Print(newAmplitude));
    if(xQueueReadings !=0)
    {
        if(xQueueSendToBack(xQueueReadings, &amplitude, portMAX_DELAY))
        {
            printf("\nValue posted\n");
        }

        else
        {
            printf("\nFailed to post value\n");
        }
    }
}


/* An HTTP GET handler */
esp_err_t hello_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    httpd_resp_set_type(req, HTTPD_TYPE_JSON);

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
            }
        }
        free(buf);
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = jsonToString();//(const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, strlen(resp_str));

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

/* An HTTP POST handler */
esp_err_t echo_post_handler(httpd_req_t *req)
{
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
        printf("\n%s\n", internetProtocol);
        ESP_LOGI(TAG2, "====================================");

    }

    // End response
    char *pbuf = &buf;
    updateJSON(pbuf);
    httpd_resp_send(req, pbuf, strlen(pbuf));
    return ESP_OK;
}

httpd_uri_t echo = {
    .uri       = "/echo",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
    .user_ctx  = NULL
};


httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &echo);
        //httpd_register_uri_handler(server, &ctrl);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
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

static void initialise_wifi(void *arg)
{
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

void app_main()
{
    static httpd_handle_t server = NULL;
    //ESP_ERROR_CHECK(nvs_flash_init());
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
    initialise_wifi(&server);
    xQueueReadings = xQueueCreate(10, sizeof(int));
    xTaskCreate(&pwmControlTask, "pwmControlTask", 5000, NULL, 5, NULL);
}
