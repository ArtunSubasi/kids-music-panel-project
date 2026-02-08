#include "rfid_scanner.h"
#include "board_pins.h"
#include "esp_log.h"
#include "driver/rc522_spi.h"

static const char *TAG = "RFID_SCANNER";

esp_err_t rfid_scanner_init(rfid_scanner_t *scanner)
{
    if (!scanner) {
        ESP_LOGE(TAG, "scanner pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    rc522_spi_config_t driver_config = {
        .host_id = BOARD_RFID_SPI_HOST,
        .bus_config = &(spi_bus_config_t){
            .miso_io_num = BOARD_RFID_SPI_MISO,
            .mosi_io_num = BOARD_RFID_SPI_MOSI,
            .sclk_io_num = BOARD_RFID_SPI_CLK,
        },
        .dev_config = {
            .spics_io_num = BOARD_RFID_PIN_CS,
        },
        .rst_io_num = BOARD_RFID_PIN_RST,
    };

    esp_err_t ret = rc522_spi_create(&driver_config, &scanner->driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RC522 SPI driver creation failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = rc522_driver_install(scanner->driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RC522 driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    rc522_config_t scanner_config = {
        .driver = scanner->driver,
    };

    ret = rc522_create(&scanner_config, &scanner->scanner);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RC522 scanner creation failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "RFID Scanner initialized successfully");
    return ESP_OK;
}

void rfid_scanner_start(rfid_scanner_t *scanner, void (*event_handler)(void *, const char *, int32_t, void *))
{
    if (!scanner || !scanner->scanner) {
        ESP_LOGW(TAG, "Scanner not initialized");
        return;
    }

    if (!event_handler) {
        ESP_LOGW(TAG, "Event handler is NULL");
        return;
    }

    rc522_register_events(scanner->scanner, RC522_EVENT_PICC_STATE_CHANGED,
                         event_handler, NULL);
    rc522_start(scanner->scanner);
    ESP_LOGI(TAG, "RFID Scanner started");
}
