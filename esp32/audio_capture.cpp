#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/i2s_std.h"
#include "esp_log.h"

static const char *TAG = "AUDIO_CAPTURE";

/*
 * Audio configuration
 */
#define SAMPLE_RATE     16000
#define SAMPLE_BITS     16
#define CHANNEL_COUNT   1

/*
 * Ping-pong DMA buffers.
 *
 * DMA continuously fills one buffer while the application
 * processes the other buffer.
 */
#define BUFFER_SAMPLES  512

static int16_t dma_buffer_0[BUFFER_SAMPLES];
static int16_t dma_buffer_1[BUFFER_SAMPLES];

/*
 * Queue used to transfer completed audio buffers
 * from the audio capture task to the processing task.
 */
typedef struct
{
    int16_t *buffer;
    size_t samples;
} audio_buffer_t;

static QueueHandle_t audio_queue;

static i2s_chan_handle_t rx_handle;


/*
 * Configure the ESP32-S3 I2S peripheral.
 */
static void configure_i2s()
{
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    ESP_ERROR_CHECK(
        i2s_new_channel(&chan_cfg, NULL, &rx_handle)
    );

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),

        .slot_cfg =
            I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                I2S_DATA_BIT_WIDTH_16BIT,
                I2S_SLOT_MODE_MONO
            ),

        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,

            /*
             * Replace these GPIO numbers with the
             * GPIO wiring used by your ESP32-S3 board.
             */
            .bclk = 4,
            .ws   = 5,
            .dout = I2S_GPIO_UNUSED,
            .din  = 6,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false
            }
        }
    };

    ESP_ERROR_CHECK(
        i2s_channel_init_std_mode(rx_handle, &std_cfg)
    );

    ESP_ERROR_CHECK(
        i2s_channel_enable(rx_handle)
    );

    ESP_LOGI(TAG, "I2S microphone initialized");
}


/*
 * Audio capture task.
 *
 * The task continuously reads samples from the I2S DMA.
 * Network communication is NOT performed here.
 */
static void audio_capture_task(void *arg)
{
    bool use_buffer_0 = true;

    while (true)
    {
        int16_t *active_buffer =
            use_buffer_0 ? dma_buffer_0 : dma_buffer_1;

        size_t bytes_read = 0;

        esp_err_t result = i2s_channel_read(
            rx_handle,
            active_buffer,
            BUFFER_SAMPLES * sizeof(int16_t),
            &bytes_read,
            portMAX_DELAY
        );

        if (result == ESP_OK)
        {
            audio_buffer_t completed_buffer;

            completed_buffer.buffer = active_buffer;
            completed_buffer.samples =
                bytes_read / sizeof(int16_t);

            /*
             * Send completed buffer to processing task.
             *
             * The network thread is completely independent.
             */
            xQueueSend(
                audio_queue,
                &completed_buffer,
                portMAX_DELAY
            );

            /*
             * Switch between buffer 0 and buffer 1.
             */
            use_buffer_0 = !use_buffer_0;
        }
    }
}


/*
 * Audio processing task.
 *
 * In the real system this could perform:
 * - preprocessing
 * - voice activity detection
 * - compression
 * - buffering
 * - transmission preparation
 */
static void audio_processing_task(void *arg)
{
    audio_buffer_t audio;

    while (true)
    {
        if (xQueueReceive(
                audio_queue,
                &audio,
                portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(
                TAG,
                "Received audio buffer: %d samples",
                (int)audio.samples
            );

            /*
             * Do NOT perform blocking network operations here.
             *
             * A separate network task should consume
             * processed audio.
             */
        }
    }
}


/*
 * Application entry point.
 */
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting audio capture system");

    audio_queue = xQueueCreate(
        4,
        sizeof(audio_buffer_t)
    );

    if (audio_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create audio queue");
        return;
    }

    configure_i2s();

    xTaskCreate(
        audio_capture_task,
        "audio_capture",
        4096,
        NULL,
        5,
        NULL
    );

    xTaskCreate(
        audio_processing_task,
        "audio_processing",
        4096,
        NULL,
        4,
        NULL
    );

    ESP_LOGI(TAG, "Audio capture tasks started");
}