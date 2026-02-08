#include "display.h"
#include "board_pins.h"
#include "esp_log.h"
#include "driver/spi_master.h"

static const char *TAG = "DISPLAY";

esp_err_t display_init(display_t *display)
{
    if (!display) {
        ESP_LOGE(TAG, "display pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* Initialize SPI2 bus for OLED */
    spi_bus_config_t buscfg = {
        .mosi_io_num = BOARD_OLED_SPI_MOSI,
        .sclk_io_num = BOARD_OLED_SPI_CLK,
        .miso_io_num = BOARD_OLED_SPI_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    esp_err_t ret = spi_bus_initialize(BOARD_OLED_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        /* ESP_ERR_INVALID_STATE means bus already initialized (ok for us) */
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ssd1306_config_t cfg = {
        .bus = SSD1306_SPI,
        .width = 128,
        .height = 64,
        .iface.spi = {
            .host = BOARD_OLED_SPI_HOST,
            .cs_gpio = BOARD_OLED_PIN_CS,
            .dc_gpio = BOARD_OLED_PIN_DC,
            .rst_gpio = BOARD_OLED_PIN_RST,
            .clk_hz = BOARD_OLED_SPI_FREQ_HZ,
        },
    };

    ret = ssd1306_new_spi(&cfg, &display->handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Display initialized successfully");
    return ESP_OK;
}

void display_show(display_t *display, const char *line1, const char *line2)
{
    if (!display || !display->handle) {
        ESP_LOGW(TAG, "Display not initialized");
        return;
    }

    if (!line1 || !line2) {
        ESP_LOGW(TAG, "Invalid text pointers");
        return;
    }

    ssd1306_clear(display->handle);
    ssd1306_draw_text(display->handle, 0, 0, "RFID SCANNER", true);
    ssd1306_draw_text(display->handle, 0, 20, (char *)line1, true);
    ssd1306_draw_text(display->handle, 0, 40, (char *)line2, true);
    ssd1306_display(display->handle);
}

void display_clear(display_t *display)
{
    if (!display || !display->handle) {
        ESP_LOGW(TAG, "Display not initialized");
        return;
    }

    ssd1306_clear(display->handle);
    ssd1306_display(display->handle);
}
