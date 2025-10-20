#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

LOG_MODULE_REGISTER(sht41_app);

#define SHT4X_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(sensirion_sht4x)
#define UART_NODE DT_NODELABEL(uart1)  // Use UART1 for RS485

#define RS485_DIR_PIN 4

static const struct device *sht4x = DEVICE_DT_GET(SHT4X_NODE);
static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);
static const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

static void sensor_value_to_double(struct sensor_value *val, double *dval)
{
    *dval = (double)val->val1 + (double)val->val2 / 1000000;
}

static void rs485_set_tx_mode(bool enable_tx)
{
    if (enable_tx) {
        // Set direction pin HIGH for transmit mode
        gpio_pin_set(gpio_dev, RS485_DIR_PIN, 1);
    } else {
        // Set direction pin LOW for receive mode  
        gpio_pin_set(gpio_dev, RS485_DIR_PIN, 0);
    }
}

static int send_rs485_data(const char *data, int len)
{
    // Enable TX mode
    rs485_set_tx_mode(true);
    
    // Small delay to ensure direction change is processed
    k_sleep(K_MSEC(1));
    
    // Send data
    for (int i = 0; i < len; i++) {
        uart_poll_out(uart_dev, data[i]);
    }
    
    // Wait for transmission to complete (calculate based on baud rate)
    // At 9600 baud: ~1ms per character + safety margin
    k_sleep(K_MSEC((len * 1000 / 9600) + 5));
    
    // Switch back to RX mode
    rs485_set_tx_mode(false);
    
    return 0;
}

static int process_sample(const struct device *dev)
{
    struct sensor_value temp, humidity;
    double temp_d, humidity_d;
    int ret;

    ret = sensor_sample_fetch(dev);
    if (ret < 0) {
        LOG_ERR("Failed to fetch sample: %d", ret);
        return ret;
    }

    ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
    if (ret < 0) {
        LOG_ERR("Failed to get temperature: %d", ret);
        return ret;
    }

    ret = sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &humidity);
    if (ret < 0) {
        LOG_ERR("Failed to get humidity: %d", ret);
        return ret;
    }

    sensor_value_to_double(&temp, &temp_d);
    sensor_value_to_double(&humidity, &humidity_d);

    // Print the temperature and humidity to the console
    printk("Temperature: %.2f °C\n", temp_d);
    printk("Humidity: %.2f %%RH\n", humidity_d);

    // Prepare JSON message for RS485 transmission
    char json_msg[128];
    int len = snprintf(json_msg, sizeof(json_msg), "{\"temp\":%.2f,\"humidity\":%.2f,\"timestamp\":%llu}\r\n", temp_d, humidity_d, k_uptime_get());

    // Send to PC via RS485
    if (len > 0) {
        send_rs485_data(json_msg, len);
        LOG_INF("Sent %d bytes via RS485", len);
    }

    return 0;
}

int main(void)
{
    printk("SHT41 Sensor Demo on ESP32 with RS485\n");

    // Configure GPIO for RS485 direction control
    if (!device_is_ready(gpio_dev)) {
        LOG_ERR("GPIO device not ready");
        return -1;
    }
    
    int ret = gpio_pin_configure(gpio_dev, RS485_DIR_PIN, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure RS485 direction pin: %d", ret);
        return ret;
    }
    
    // Initialize to receive mode
    rs485_set_tx_mode(false);

    // Check if UART device is ready
    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready");
        return -1;
    }

    if (!device_is_ready(sht4x)) {
        LOG_ERR("SHT4X device not ready");
        return -1;
    }

    LOG_INF("SHT4X sensor ready");
    LOG_INF("RS485 communication initialized");

    while (1) {
        int ret = process_sample(sht4x);
        if (ret < 0) {
            LOG_ERR("Error processing sample: %d", ret);
        }
        
        k_sleep(K_SECONDS(2));
    }

    return 0;
}
