//==================================RTOS Libraries Includes==========================//
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "wifi_manager/wifi_manager.h"

#include "logging/logging.h"
#include "driver/twai.h"
#include "RTC_Time_Sync/rtc_time_sync.h"
#include "telemetry_config.h"
#include "connectivity/connectivity.h"
#include "udp_sender/udp_sender.h"
#include "mqtt_sender/mqtt_sender.h"

#define LED_GPIO 2 // GPIO pin for the LED
#define Queue_Size 10

/*
 * ================================================================
 * 							SDIO Config Variables
 * ================================================================
 *
 * */

// Debug Varibales
/* SDIO_FileConfig SDIO_test;
SDIO_FileConfig SDIO_txt;
SDIO_TxBuffer buffer;  */
SDIO_FileConfig LOG_CSV;
SDIO_TxBuffer SDIO_buffer;

/*
 * ================================================================
 * 							CAN Config Variables
 * ================================================================
 *
 * */
// 1. Configure TWAI with pins from your ESP32 pinout
twai_general_config_t g_config = {
    .mode = TWAI_MODE_NORMAL,
    .tx_io = GPIO_NUM_21, // CAN TX on GPIO21
    .rx_io = GPIO_NUM_22, // CAN RX on GPIO22
    .clkout_io = TWAI_IO_UNUSED,
    .bus_off_io = TWAI_IO_UNUSED,
    .tx_queue_len = 5,
    .rx_queue_len = 5,
    .alerts_enabled = TWAI_ALERT_ALL,
    .clkout_divider = 0};
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();

twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

/*
 * ================================================================
 * 							RTOS Config Variables
 * ================================================================
 *
 * */
// Define Queue Handler
QueueHandle_t CAN_SDIO_queue_Handler;
QueueHandle_t telemetry_queue;

// Define Tasks Handler to hold task ID
TaskHandle_t CAN_Receive_TaskHandler;
TaskHandle_t SDIO_Log_TaskHandler;

// Declare Tasks Entery point
void CAN_Receive_Task_init(void *pvParameters);
void SDIO_Log_Task_init(void *pvParameters);

void app_main()
{
    //==========================================WIFI Implementation (DONE)===========================================
    // ESP_ERROR_CHECK(wifi_init("Mi A2", "min@fathy2004"));
    // ESP_ERROR_CHECK(wifi_init("Belal's A34", "password"));
    ESP_ERROR_CHECK(wifi_init("Fathy WIFI", "Min@F@thy.2004$$"));

    /* Wait until connected */
    xEventGroupWaitBits(wifi_event_group(), WIFI_CONNECTED_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);

    ESP_LOGI("WIFI", "Connected to WiFi!");
    //==========================================RTC_Time_Sync Implementation (DONE)===========================================
    Time_Sync_obtain_time();
    //==========================================SDIO Implementation (DONE)===========================================

    static const char *TAG = "SDIO";
    esp_err_t ret;
    ret = SDIO_SD_Init();

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    char name_buffer[12] = "CAN_0.CSV";
    LOG_CSV.name = name_buffer;
    LOG_CSV.type = CSV;

    snprintf(LOG_CSV.path, sizeof(LOG_CSV.path), "%s/%s", MOUNT_POINT, LOG_CSV.name);

    // // Check if the files exists and Modification Time more than 2 days
    // //if it exists and last modified was more than 2 days
    // //      Increment the name of the file and check again
    // // if it exists and last modified in less than 2 days           |
    // //      Don't change name and add to the already existing file  |   This logic is implemented
    // // if it doesn't exist                                          |   SDIO_SD_Create_Write_File()
    // //      Create file                                             |

    struct stat st;
    uint8_t Session_Num = 0;
    while((stat(LOG_CSV.path, &st) == 0) && (compare_file_time_days(LOG_CSV.path) > MAX_DAYS_MODIFIED))
    {
        //it exists and last modified was more than 2 days
        Session_Num++;
        //Update Name and path
        snprintf(name_buffer, sizeof(name_buffer), "LOG_%u.CSV", Session_Num);
        snprintf(LOG_CSV.path, sizeof(LOG_CSV.path), "%s/%s", MOUNT_POINT, LOG_CSV.name);
    }

    //@debug SDIO
    /*
        SDIO_txt.name = "Test2.TXT";
        SDIO_txt.type = TXT;
        buffer.string = "Hello World line 1\r\nHello World line 2\r\nHello World line 3\r";
        if (SDIO_SD_Create_Write_File(&SDIO_txt, &buffer) == ESP_OK)
        {
            ESP_LOGI(TAG, "%s Written Successfully!", SDIO_txt.name);
        }

        LOG_CSV.name = "LOG_1.CSV";
        LOG_CSV.type = CSV;
        SDIO_buffer.string = "LOG1";
        SDIO_buffer.timestamp = "2023-10-01 12:00:00";
        SDIO_buffer.adc.SUS_1 = 15;
        SDIO_buffer.adc.SUS_2 = 20;
        SDIO_buffer.adc.SUS_3 = 25;
        SDIO_buffer.adc.SUS_4 = 30;
        SDIO_buffer.adc.PRESSURE_1 = 10;
        SDIO_buffer.adc.PRESSURE_2 = 15;
        SDIO_buffer.prox_encoder.RPM_front_left = 1000;
        SDIO_buffer.prox_encoder.RPM_front_right = 1100;
        SDIO_buffer.prox_encoder.RPM_rear_left = 1200;
        SDIO_buffer.prox_encoder.RPM_rear_right = 1300;
        SDIO_buffer.prox_encoder.ENCODER_angle = 45;
        SDIO_buffer.imu_accel.x = 100;
        SDIO_buffer.imu_accel.y = 200;
        SDIO_buffer.imu_accel.z = 300;

        if (SDIO_SD_Create_Write_File(&LOG_CSV, &SDIO_buffer) == ESP_OK)
        {
            ESP_LOGI(TAG, "%s Written Successfully!", LOG_CSV.name);
        }

        // SDIO_SD_Read_Data(&SDIO_txt);
        SDIO_SD_Read_Data(&LOG_CSV);

        //Append data to the existing files
        buffer.string = "Hello World line 4\r\nHello World line 5\r\n";
        if (SDIO_SD_Add_Data(&SDIO_txt, &buffer) == ESP_OK)
        {
            ESP_LOGI(TAG, "TEST.TXT Appended Successfully!");
        }

        //Assign Zero to all elements of SDIO_buffer
        EMPTY_SDIO_BUFFER(SDIO_buffer);

        if (SDIO_SD_Add_Data(&LOG_CSV, &SDIO_buffer) == ESP_OK)
        {
            ESP_LOGI(TAG, "%s Appended Successfully!", LOG_CSV.name);
        }

        if (SDIO_SD_Close_file() == ESP_OK)
        {
            ESP_LOGI(TAG, "File Closed Successfully!");
        }

        // Read the files again to verify the appended data
        SDIO_SD_Read_Data(&SDIO_txt);
        SDIO_SD_Read_Data(&LOG_CSV);

        twai_message_t rx_msg;
        SDIO_SD_LOG_CAN_Message(&rx_msg);

        // All done, unmount partition and disable SDMMC peripheral
        if(SDIO_SD_DeInit() == ESP_OK)
        ESP_LOGI(TAG, "Card unmounted successfully"); */

    //==========================================CAN Implementation (DONE)===========================================

    // Can Initialization
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK)
    {
        ESP_LOGE("CAN", "successfully installed TWAI driver");
    }

    if (twai_start() == ESP_OK)
    {
        ESP_LOGE("CAN", "successfully started TWAI driver");
    }

    //==========================================RTOS Implementation (Semaphore can be added)===========================================

    //=======================Create Queue====================//

    telemetry_queue = xQueueCreate(Queue_Size, sizeof(twai_message_t));
    CAN_SDIO_queue_Handler = xQueueCreate(Queue_Size, sizeof(twai_message_t));

    if (telemetry_queue == NULL) // If there is no queue created
    {
        ESP_LOGE("RTOS", "Unable to Create Structure Queue\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (CAN_SDIO_queue_Handler == NULL) // If there is no queue created
    {
        ESP_LOGE("RTOS", "Unable to Create Structure Queue");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    //=============Define Tasks=================//
    xTaskCreate((TaskFunction_t)SDIO_Log_Task_init, "SDIO_Log_Task", 4096, NULL, (UBaseType_t)4, &SDIO_Log_TaskHandler);
    xTaskCreate((TaskFunction_t)CAN_Receive_Task_init, "CAN_Receive_Task", 4096, NULL, (UBaseType_t)3, &CAN_Receive_TaskHandler);
    /* #if USE_MQTT
        xTaskCreate(mqtt_sender_task, "mqtt_sender", 4096, telemetry_queue, 4, NULL);
    #else
        xTaskCreate(udp_sender_task, "udp_sender", 4096, telemetry_queue, 4, NULL);
    #endif
        xTaskCreate(connectivity_monitor_task, "conn_monitor", 4096, NULL, 4, NULL); */

    while (1)
    {

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void CAN_Receive_Task_init(void *pvParameters) // DONE
{
    const char *TAG = "CAN_Receive_Task";
    twai_message_t rx_msg;
    esp_err_t ret;
    uint32_t alerts = 0;
    twai_status_info_t s;
    ESP_LOGI("CAN_Receive_Task", "CAN IS WORKING");
    while (1)
    {
        if (twai_receive(&rx_msg, pdMS_TO_TICKS(1000)) == ESP_OK)
        {

            // Format the message into the string buffer
            xQueueSend(telemetry_queue, &rx_msg, (TickType_t)10);

            if (xQueueSend(CAN_SDIO_queue_Handler, &rx_msg, (TickType_t)10) != pdPASS)
            {
                if (SDIO_Log_TaskHandler != NULL)
                {
                    xTaskNotifyGive(SDIO_Log_TaskHandler); // Notify SDIO task
                }
            }
        }
        else
        {
            ESP_LOGI(TAG, "No message received within the timeout period");
            ret = twai_read_alerts(&alerts, 0);
            if (ret == ESP_OK)
            {
                ESP_LOGI(TAG, "TWAI alert: %08ld", alerts);
            }
            twai_get_status_info(&s);
            ESP_LOGI(TAG, "RX errors: %ld, bus errors: %ld, RX queue full: %ld",
                     s.rx_error_counter, s.bus_error_count, s.rx_missed_count);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void SDIO_Log_Task_init(void *pvParameters) // WORKS! Needs testing
{                                           

    const char *TAG = "SDIO_Log_Task";
    ESP_LOGI(TAG, "SDO_LOG IS WORKING");

    // Assign Zero to all elements of SDIO_buffer and Log initial Line
    EMPTY_SDIO_BUFFER(SDIO_buffer);

    if (SDIO_SD_Create_Write_File(&LOG_CSV, &SDIO_buffer) == ESP_OK)
        ESP_LOGI(TAG, "%s Written Successfully!", LOG_CSV.name);

    // if (SDIO_SD_Close_file() == ESP_OK)
    //     ESP_LOGI(TAG, "File Closed Successfully!");

    twai_message_t buffer;
    const uint8_t NUM_IDS = COMM_CAN_ID_COUNT;
    uint8_t id_received[NUM_IDS];
    TickType_t period = pdMS_TO_TICKS(50);

    while (1)
    {
        // Wait for notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // 1. Clear buffer and flags
        EMPTY_SDIO_BUFFER(SDIO_buffer);
        memset(&buffer, 0, sizeof(twai_message_t));

        TickType_t start = xTaskGetTickCount();
        TickType_t now = start;
        uint8_t received_count = 0;

        while ((now - start) < period && received_count < NUM_IDS)
        {
            TickType_t remaining = period - (now - start);
            if (xQueueReceive(CAN_SDIO_queue_Handler, &buffer, remaining))
            {
                //@debug SDIO
                // To print the Message Received
                /*  printf("ID = 0x%03lX ", buffer.identifier);
                printf("Extended? %s ", buffer.extd ? "Yes" : "No");
                printf("RTR? %s ", buffer.rtr ? "Yes" : "No");
                printf("DLC = %d\n", buffer.data_length_code);
                for (int i = 0; i < buffer.data_length_code; i++)
                {
                    printf("byte[%d] = 0x%02X ", i, buffer.data[i]);
                }
                printf("\n");
                */

                 switch (buffer.identifier) // Check the ID of the message
                {
                case COMM_CAN_ID_IMU_ANGLE:
                    if (id_received[COMM_CAN_ID_IMU_ANGLE - COMM_CAN_ID_FISRT] == 0)
                    {
                        SDIO_buffer.imu_ang = *((COMM_message_IMU_t *)buffer.data);
                        id_received[COMM_CAN_ID_IMU_ANGLE - COMM_CAN_ID_FISRT] = 1;
                        received_count++;
                    }
                    break;

                case COMM_CAN_ID_IMU_ACCEL:
                    if (id_received[COMM_CAN_ID_IMU_ACCEL - COMM_CAN_ID_FISRT] == 0)
                    {
                        SDIO_buffer.imu_accel = *((COMM_message_IMU_t *)buffer.data);
                        id_received[COMM_CAN_ID_IMU_ACCEL - COMM_CAN_ID_FISRT] = 1;
                        received_count++;
                    }
                    break;

                case COMM_CAN_ID_ADC:
                    if (id_received[COMM_CAN_ID_ADC - COMM_CAN_ID_FISRT] == 0)
                    {
                        SDIO_buffer.adc = *((COMM_message_ADC_t *)buffer.data);
                        id_received[COMM_CAN_ID_ADC - COMM_CAN_ID_FISRT] = 1;
                        received_count++;
                    }
                    break;

                case COMM_CAN_ID_PROX_ENCODER:
                    if (id_received[COMM_CAN_ID_PROX_ENCODER - COMM_CAN_ID_FISRT] == 0)
                    {
                        SDIO_buffer.prox_encoder = *((COMM_message_PROX_encoder_t *)buffer.data);
                        id_received[COMM_CAN_ID_PROX_ENCODER - COMM_CAN_ID_FISRT] = 1;
                        received_count++;
                    }
                    break;

                case COMM_CAN_ID_GPS_LATLONG:
                    if (id_received[COMM_CAN_ID_GPS_LATLONG - COMM_CAN_ID_FISRT] == 0)
                    {
                        SDIO_buffer.gps = *((COMM_message_GPS_t *)buffer.data);
                        id_received[COMM_CAN_ID_GPS_LATLONG - COMM_CAN_ID_FISRT] = 1;
                        received_count++;
                    }
                    break;

                case COMM_CAN_ID_TEMP:
                    if (id_received[COMM_CAN_ID_TEMP - COMM_CAN_ID_FISRT] == 0)
                    {
                        SDIO_buffer.temp = *((COMM_message_Temp_t *)buffer.data);
                        id_received[COMM_CAN_ID_TEMP - COMM_CAN_ID_FISRT] = 1;
                        received_count++;
                    }
                    break;

                default:
                    break;
                }
            }
            now = xTaskGetTickCount();
        }

        // Log the Buffer on SD card
        //@debug SDIO
        //SDIO_SD_log_can_message_to_csv(&buffer);

        if (SDIO_SD_Add_Data(&LOG_CSV, &SDIO_buffer) != ESP_OK)
            ESP_LOGI(TAG, "ERROR! : %s is not Created // Appedended", LOG_CSV.name);
        else
            ESP_LOGI(TAG, "Logged CAN message to %s", LOG_CSV.name);

        
    }
}
