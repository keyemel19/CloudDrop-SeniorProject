#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include <sys/time.h>
#include <time.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "main.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"

// #include "aws_cert.h"
#include "hivemq_cert.h"

// #define WIFI_SSID "Edolas2.4"
// #define WIFI_PASS "Tusbas2016"

#define BUTTON_GPIO GPIO_NUM_9  // Change if needed
#define MQTT_TOPIC "esp32/timestamp"
#define HIVEMQ_BROKER_URI "mqtts://0e945be5150741f1b887fc056f514976.s1.eu.hivemq.cloud:8883"
// #define AWS_BROKER_URI "mqtts://your-aws-endpoint.amazonaws.com"
#define MOTION_TIMER_DEBOUNCE_IN_US 1500000  // Start 1.5-sec.

static const char *TAG = "MQTT_BUTTON";
esp_mqtt_client_handle_t client;
QueueHandle_t button_queue;
static esp_timer_handle_t motion_timer;


#include "freertos/semphr.h"
#define LED_RED   32  
#define LED_GREEN 33  
#define LED_BLUE  GPIO_NUM_7
#define MOTION_IN  GPIO_NUM_4

SemaphoreHandle_t led_semaphore;  // Semaphore for LED control
SemaphoreHandle_t motion_semaphore;  // Semaphore for motion_ISR




// Task to control LED based on semaphore
//void rgb_task(void *arg) {
    //while (1) {
        //if (xSemaphoreTake(led_semaphore, portMAX_DELAY)) {
            //ESP_LOGI(TAG, "RED ON");
//
            //gpio_set_level(LED_RED, 1);  // Turn LED ON
            //vTaskDelay(pdMS_TO_TICKS(500));  // Wait 500ms
            //gpio_set_level(LED_RED, 0);  // Turn LED ON
//
            //ESP_LOGI(TAG, "GREEN ON");
            //gpio_set_level(LED_GREEN, 1);  // Turn LED ON
            //vTaskDelay(pdMS_TO_TICKS(500));  // Wait 500ms
            //gpio_set_level(LED_GREEN, 0);  // Turn LED ON
//
            //ESP_LOGI(TAG, "BLUE ON");
            //gpio_set_level(LED_BLUE, 1);  // Turn LED ON
            //vTaskDelay(pdMS_TO_TICKS(500));  // Wait 500ms
            //gpio_set_level(LED_BLUE, 0);  // Turn LED ON
//
            //vTaskDelay(pdMS_TO_TICKS(500));  // Wait 500ms
        //}
    //}
//}

// Function to get timestamp (both human-readable & UNIX time)
void get_timestamp(char *buffer, size_t size) {
    struct timeval tv;
    struct tm timeinfo;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &timeinfo);
    snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d (Unix: %lld)", 
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,(long long) tv.tv_sec);
}

void led_task(void *arg) {
    while (1) {
        if (xSemaphoreTake(led_semaphore, portMAX_DELAY)) {
            for (int i = 0; i < 3; i++) {  // 10 blinks (2 Hz = 500ms on/off)
                gpio_set_level(LED_BLUE, 1);  // LED ON
                vTaskDelay(pdMS_TO_TICKS(200));  // 250ms
                gpio_set_level(LED_BLUE, 0);  // LED OFF
                vTaskDelay(pdMS_TO_TICKS(200));  // 250ms
            }
            ESP_LOGI(TAG, "LED_BLUE Stopped. Waiting for next motion...");
        }
    }
}


void motion_task(void *arg) {
    char timestamp[64];
    while (1) {
        if (xSemaphoreTake(motion_semaphore, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Motion Detected! Blinking LED_BLUE for 5 seconds...");

            get_timestamp(timestamp, sizeof(timestamp));
            // Print to Debug Console
            ESP_LOGI(TAG, "Motion Sensed! Timestamp: %s", timestamp);
            // Publish to MQTT
            char payload[128];
            snprintf(payload, sizeof(payload), "{\"motion_sensed\": \"%s\"}", timestamp);
            esp_mqtt_client_publish(client, MQTT_TOPIC, payload, 0, 1, 0);

            xSemaphoreGive(led_semaphore);
            // vTaskDelay(pdMS_TO_TICKS(250));  // 250ms
        }
    }
}


// Initialize LED
void led_init() {
    ESP_LOGI(TAG, "Starting led_init");
   // gpio_reset_pin(LED_RED);
    gpio_reset_pin(LED_BLUE);
    //gpio_reset_pin(LED_GREEN);
    // gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);
    // gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_BLUE, GPIO_MODE_OUTPUT);
    // gpio_set_level(LED_RED, 0);
    // gpio_set_level(LED_GREEN, 0);
    gpio_set_level(LED_BLUE, 0);
}






// Button ISR handler
static void IRAM_ATTR button_isr_handler(void *arg) {
    int pin = (int)arg;
    xQueueSendFromISR(button_queue, &pin, NULL);
}

// Motion sensor
// Sets the button to be higher priority for now
static void IRAM_ATTR motion_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xHigherPriorityTaskWoken) { portYIELD_FROM_ISR(); }
    gpio_intr_disable(MOTION_IN);  // Disable interrupts temporarily
    esp_timer_start_once(motion_timer, MOTION_TIMER_DEBOUNCE_IN_US);  // Start 200ms timer (200000µs)
}

void motion_timer_callback(void *arg) {
    xSemaphoreGive(motion_semaphore);  // Unblock motion task
    gpio_intr_enable(MOTION_IN);  // Re-enable interrupt
}

void motion_sense_init() {
    ESP_LOGI(TAG, "Initializing motion sensor...");
    gpio_reset_pin(MOTION_IN);
    gpio_set_direction(MOTION_IN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(MOTION_IN, GPIO_PULLDOWN_ONLY);  // Ensure default LOW
    gpio_set_intr_type(MOTION_IN, GPIO_INTR_POSEDGE);  // Trigger on rising edge
    gpio_install_isr_service(0);
    gpio_isr_handler_add(MOTION_IN, motion_isr_handler, NULL);

    // Create a 200ms software timer
    const esp_timer_create_args_t motion_timer_args = {
        .callback = &motion_timer_callback,
        .name = "motion_timer"
    };
    esp_timer_create(&motion_timer_args, &motion_timer);
}


// MQTT Event Handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    (void)client;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected");
            break;
        default:
            break;
    }
}


#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"

// Event group to signal when connected
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW("WiFi", "Disconnected! Retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WiFi", "Connected! IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}






// Task to monitor button presses and trigger LED
void button_task(void *arg) {
    int pin;
    char timestamp[64];

    while (1) {
        if (xQueueReceive(button_queue, &pin, portMAX_DELAY)) {
            // Get the current time

            get_timestamp(timestamp, sizeof(timestamp));
            // Print to Debug Console
            ESP_LOGI(TAG, "Button Pressed! Timestamp: %s", timestamp);
            // Publish to MQTT
            char payload[128];
            snprintf(payload, sizeof(payload), "{\"button_pushed\": \"%s\"}", timestamp);
            esp_mqtt_client_publish(client, MQTT_TOPIC, payload, 0, 1, 0);

            // Signal LED task to turn on the LED
            xSemaphoreGive(led_semaphore);
        }
    }
}




void time_sync() {
    ESP_LOGI("NTP", "Starting SNTP...");

     // Set timezone to MDT
    setenv("TZ", "MST7MDT", 1);  // MST7MDT for Mountain Standard Time (7 hours behind UTC) and Daylight Savings Time
    tzset();  // Apply the timezone change


    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");  // Use default NTP server
    esp_sntp_init();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo;
    int retry = 0;
    const int retry_count = 10;

    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < retry_count) {
        ESP_LOGI("NTP", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));  // Wait 2 seconds
        retry++;
    }

    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI("NTP", "Time synchronized: %s", asctime(&timeinfo));
}


void wifi_init() {
    wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
            .sta = {
                .ssid = "Edolas2.4",
                .password = "tusbas2016",
            },
        };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI("WiFi", "Connecting to WiFi...");

    // Wait until connected
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}




// Initialize MQTT with username/password authentication
void mqtt_app_start() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = HIVEMQ_BROKER_URI,
        .broker.verification.certificate = (const char *) hivemq_cert_pem,
        .credentials.username = "SNRproject2025",  // Replace with actual username
        .credentials.authentication.password = "SNRproject2025"   // Replace with actual password
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}





// Initialize button
void button_init() {
    gpio_reset_pin(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(BUTTON_GPIO, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, (void *)BUTTON_GPIO);
    button_queue = xQueueCreate(10, sizeof(int));
}

// Main function
void app_main(void) {
    ESP_LOGI(TAG, "Starting ESP32 MQTT Button");

    led_semaphore = xSemaphoreCreateBinary();  // Or xSemaphoreCreateBinary() if you need a binary semaphore
    if (led_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create led semaphore!");
        // Handle error
        }
    motion_semaphore = xSemaphoreCreateBinary();  // Or xSemaphoreCreateBinary() if you need a binary semaphore
        if (motion_semaphore == NULL) {
            ESP_LOGE(TAG, "Failed to create motion semaphore!");
            // Handle error
        }
    

    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init();
    time_sync();
    led_init();
    motion_sense_init();
    button_init();
    mqtt_app_start();
    
    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
    xTaskCreate(motion_task, "motion_task", 4096, NULL, 5, NULL);
    xTaskCreate(led_task, "led_task", 4096, NULL, 5, NULL);


}
