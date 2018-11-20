/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cJSON.h"

/* Can run 'make menuconfig' to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
void jsonCreateTask(void *pvParameters)
{
    printf("This is a task thread!\n");
    while(1)
    {
        vTaskDelay(5000/portTICK_PERIOD_MS);
        cJSON *root, *fmt;
        char *rendered;

        root = cJSON_CreateObject();

        //cJSON_AddItemToObject(root, "name", cJSON_CreateString("example"));
        //cJSON_AddItemToObject(root , "format", fmt = cJSON_CreateObject());
        cJSON_AddStringToObject(root, "type", "rect");
        cJSON_AddNumberToObject(root, "width", 1920);
        cJSON_AddNumberToObject(root, "height", 1080);
        cJSON_AddFalseToObject(root, "interface");
        cJSON_AddNumberToObject(root, "Frame Rate", 24);

        if(cJSON_HasObjectItem(root, "height"))
        {
            cJSON *height;
            height = cJSON_GetObjectItemCaseSensitive(root, "height");
            int convertido = atoi(cJSON_Print(height));
            printf("Height = %d\n\n", convertido);
        }

        else
        {
            printf("Objeto não possui item name.\n\n");
        }

        rendered = cJSON_Print(root);
        printf("json Created: %s \n", rendered);
        cJSON_Delete(root);
    }

    printf("End of the Task\n");
    vTaskDelete(NULL);

}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
    xTaskCreatePinnedToCore(&jsonCreateTask, "jsonCreateTask", 2048, NULL, 5, NULL, 0);
}