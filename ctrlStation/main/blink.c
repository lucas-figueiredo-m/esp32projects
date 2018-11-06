/* Blink Example

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


/*
    DEFINES E MAPEAMENTOS
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE

#define PWM_CH0_GPIO           (19)
#define PWM_CH0                LEDC_CHANNEL_0

#define PWM_CH1_GPIO           (18)
#define PWM_CH1                LEDC_CHANNEL_1

#define PWM_CH2_GPIO           (5)
#define PWM_CH2                LEDC_CHANNEL_2

#define PWM_CH3_GPIO           (17)
#define PWM_CH3                LEDC_CHANNEL_3

#define PWM_CH4_GPIO           (16)
#define PWM_CH4                LEDC_CHANNEL_4

#define PWM_CH5_GPIO           (4)
#define PWM_CH5                LEDC_CHANNEL_5

#define LEDC_TEST_CH_NUM       (6)

#define DEFAULT_VREF    	   1300        //1100Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES  		   64          //Multisampling

#define adcResolution          ADC_WIDTH_BIT_11


/*
    DECLARAÇÃO DE VARIÁVEIS GLOBAIS
*/
static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_atten_t atten = ADC_ATTEN_DB_11;//at = 0
static const adc_unit_t unit = ADC_UNIT_1;

/*
	Resolução do PWM:  12 bits (4096)
	Frequência Máxima: 19kHz

	Resolução do PWM:  11 bits (2048)
	Frequência Máxima: 38kHz

*/

/*
    CONFIGURAÇÃO DO TIMER E DO PWM
*/
QueueHandle_t xQueueReadings;

ledc_timer_config_t ledc_timer = 
{
    .duty_resolution = LEDC_TIMER_11_BIT,  // resolution of PWM duty
    .freq_hz = 38000,                      // frequency of PWM signal
    .speed_mode = LEDC_HS_MODE,            // timer mode
    .timer_num = LEDC_HS_TIMER             // timer index
};
    // Set configuration of timer0 for high speed channels

ledc_channel_config_t ledc_channel[LEDC_TEST_CH_NUM] = 
{
    {
        .channel    = PWM_CH0,
        .duty       = 0,
        .gpio_num   = PWM_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .timer_sel  = LEDC_HS_TIMER
    },

    {
        .channel    = PWM_CH1,
        .duty       = 0,
        .gpio_num   = PWM_CH1_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .timer_sel  = LEDC_HS_TIMER
    },

    {
        .channel    = PWM_CH2,
        .duty       = 0,
        .gpio_num   = PWM_CH2_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .timer_sel  = LEDC_HS_TIMER
    },

    {
        .channel    = PWM_CH3,
        .duty       = 0,
        .gpio_num   = PWM_CH3_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .timer_sel  = LEDC_HS_TIMER
    },

    {
        .channel    = PWM_CH4,
        .duty       = 0,
        .gpio_num   = PWM_CH4_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .timer_sel  = LEDC_HS_TIMER
    },

    {
        .channel    = PWM_CH5,
        .duty       = 0,
        .gpio_num   = PWM_CH5_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .timer_sel  = LEDC_HS_TIMER
    },
};
/*
    FIM DA CONFIGURAÇÃO DO TIMER E DO PWM
*/

/*
    CONFIGURAÇÃO DO HTTPREST
*/
httpd_uri_t valuesPut = {
    .uri       = "/valuesPut",
    .method    = HTTP_PUT,
    .handler   = valuesPutHandler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

httpd_uri_t paramsGet = {
    .uri       = "/paramsGet",
    .method    = HTTP_GET,
    .handler   = paramsGetHandler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};



/*
    FIM DA CONFIGURAÇÃO DO HTTPREST
*/

void adcReadTask(void *pvParameter)
{
    //check_efuse();

    //Configure ADC
    if (unit == ADC_UNIT_1) 
    {
        adc1_config_width(adcResolution);
        adc1_config_channel_atten(channel, atten);
    } 

    else 
    {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, adcResolution, DEFAULT_VREF, adc_chars);
    //print_char_val_type(val_type);

    //Continuously sample ADC1
    while (1) 
    {
        uint32_t adc_reading = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) 
        {
            if (unit == ADC_UNIT_1) 
            {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
            } 

            else 
            {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, adcResolution, &raw);
                adc_reading += raw;
            }
        }
        adc_reading /= NO_OF_SAMPLES;
        if(xQueueReadings !=0)
        {
            if(xQueueSendToBack(xQueueReadings, &adc_reading, 50/portTICK_PERIOD_MS))
            {
                printf("\nValue posted\n");
            }

            else
            {
                printf("\nFailed to post value\n");
            }
        }
        //Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
        vTaskDelay(50/portTICK_PERIOD_MS);
    }
}

void pwmControlTask(void *pvParameter)
{
    //int ch;
    uint32_t adcRead = 0;

    ledc_timer_config(&ledc_timer);
    ledc_channel_config(&ledc_channel[0]);

    while (1) 
    {
        if(xQueueReceive(xQueueReadings, &adcRead, 50/portTICK_PERIOD_MS))
        {
            printf("\nReading Successfull. New PWM: %d", adcRead);
            ledc_set_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel, adcRead);
            ledc_update_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel);
        }
    }
}

void app_main(void)
{
    xQueueReadings = xQueueCreate(10, sizeof(int));
    xTaskCreate(&adcReadTask, "adcReadTask", 5000, NULL, 5, NULL);
    xTaskCreate(&pwmControlTask, "pwmControlTask", 5000, NULL, 5, NULL);
}

// Guru Meditation Error: Core  1 panic'ed (LoadProhibited). Exception was unhandled.