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

#define DEFAULT_VREF    1300        //1100Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_atten_t atten = ADC_ATTEN_DB_11;//at = 0
static const adc_unit_t unit = ADC_UNIT_1;

QueueHandle_t xQueueReadings;

/*
void blink_task(void *pvParameter)
{
     Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.
    
    gpio_pad_select_gpio(BLINK_GPIO);
     Set the GPIO as a push/pull output 
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    while(1) {
         Blink off (output low) 
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
         Blink on (output high) 
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}*/

void adcReadTask(void *pvParameter)
{
    //check_efuse();

    //Configure ADC
    if (unit == ADC_UNIT_1) 
    {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(channel, atten);
    } 

    else 
    {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    //esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
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
                adc2_get_raw((adc2_channel_t)channel, ADC_WIDTH_BIT_12, &raw);
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
                printf("\nFeiled to post value\n");
            }
        }
        //Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void pwmControlTask(void *pvParameter)
{
    //int ch;
    int adcRead;

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT,  // resolution of PWM duty
        .freq_hz = 20000,                      // frequency of PWM signal
        .speed_mode = LEDC_HS_MODE,            // timer mode
        .timer_num = LEDC_HS_TIMER             // timer index
    };
    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);



    ledc_channel_config_t ledc_channel[LEDC_TEST_CH_NUM] = {
        {
            .channel    = LEDC_HS_CH0_CHANNEL,
            .duty       = 64,
            .gpio_num   = LEDC_HS_CH0_GPIO,
            .speed_mode = LEDC_HS_MODE,
            .timer_sel  = LEDC_HS_TIMER
        },

       /*{
            .channel    = LEDC_HS_CH1_CHANNEL,
            .duty       = 128,
            .gpio_num   = LEDC_HS_CH1_GPIO,
            .speed_mode = LEDC_HS_MODE,
            .timer_sel  = LEDC_HS_TIMER
        },

        {
            .channel    = LEDC_HS_CH2_CHANNEL,
            .duty       = 256,
            .gpio_num   = LEDC_HS_CH2_GPIO,
            .speed_mode = LEDC_HS_MODE,
            .timer_sel  = LEDC_HS_TIMER
        },

        {
            .channel    = LEDC_HS_CH3_CHANNEL,
            .duty       = 512,
            .gpio_num   = LEDC_HS_CH3_GPIO,
            .speed_mode = LEDC_HS_MODE,
            .timer_sel  = LEDC_HS_TIMER
        },

        {
            .channel    = LEDC_HS_CH4_CHANNEL,
            .duty       = 1024,
            .gpio_num   = LEDC_HS_CH4_GPIO,
            .speed_mode = LEDC_HS_MODE,
            .timer_sel  = LEDC_HS_TIMER
        },

        {
            .channel    = LEDC_HS_CH5_CHANNEL,
            .duty       = 2048,
            .gpio_num   = LEDC_HS_CH5_GPIO,
            .speed_mode = LEDC_HS_MODE,
            .timer_sel  = LEDC_HS_TIMER
        },*/

    };
/*
    for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++) 
    {
        ledc_channel_config(&ledc_channel[ch]);
    }*/

    // Initialize fade service.
    //ledc_fade_func_install(0);
    ledc_channel_config(&ledc_channel[0]);

    while (1) 
    {
        if(xQueueReceive(xQueueReadings, &adcRead, 50/portTICK_PERIOD_MS))
        {
            printf("\nReading Successfull\n");
            ledc_set_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel, adcRead);
            ledc_update_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel);
        }
/*
        printf("1. LEDC fade up to duty = %d\n", LEDC_TEST_DUTY);
        for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++) {
            ledc_set_fade_with_time(ledc_channel[ch].speed_mode,
                    ledc_channel[ch].channel, LEDC_TEST_DUTY, LEDC_TEST_FADE_TIME);
            ledc_fade_start(ledc_channel[ch].speed_mode,
                    ledc_channel[ch].channel, LEDC_FADE_NO_WAIT);
        }
        vTaskDelay(LEDC_TEST_FADE_TIME / portTICK_PERIOD_MS);*/
    }

}


void app_main(void)
{
    xQueueReadings = xQueueCreate(10, sizeof(int));
    xTaskCreate(&adcReadTask, "adcReadTask", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
    xTaskCreate(&pwmControlTask, "pwmControlTask", 10000, NULL, 5, NULL);
}
