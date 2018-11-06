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

#define BLINK_GPIO CONFIG_BLINK_GPIO

/* Can run 'make menuconfig' to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE

#define LEDC_HS_CH0_GPIO       (19)
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0

#define LEDC_HS_CH1_GPIO       (18)
#define LEDC_HS_CH1_CHANNEL    LEDC_CHANNEL_1

#define LEDC_HS_CH2_GPIO       (5)
#define LEDC_HS_CH2_CHANNEL    LEDC_CHANNEL_2

#define LEDC_HS_CH3_GPIO       (17)
#define LEDC_HS_CH3_CHANNEL    LEDC_CHANNEL_3

#define LEDC_HS_CH4_GPIO       (16)
#define LEDC_HS_CH4_CHANNEL    LEDC_CHANNEL_4

#define LEDC_HS_CH5_GPIO       (4)
#define LEDC_HS_CH5_CHANNEL    LEDC_CHANNEL_5

#define LEDC_TEST_CH_NUM       (6)

#define DEFAULT_VREF    	   1300        //1100Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES  		   64          //Multisampling

#define adcResolution          ADC_WIDTH_BIT_11

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
        .channel    = LEDC_HS_CH0_CHANNEL,
        .duty       = 64,
        .gpio_num   = LEDC_HS_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .timer_sel  = LEDC_HS_TIMER
    },

};


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