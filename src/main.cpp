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
#define UART_NODE DT_NODELABEL(uart1)
#define RS485_DIR_PIN 4

static const struct device *sht4x = DEVICE_DT_GET(SHT4X_NODE);
static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);
static const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

// converts sensor value to double
static void sensor_value_to_double(struct sensor_value *val, double *dval)
{
    *dval = (double)val->val1 + (double)val->val2 / 1000000;
}

static void rs485_send(const char *data)
{
    // set DE/RE high to enable transmit mode
    gpio_pin_set(gpio_dev, RS485_DIR_PIN, 1);

    uart_poll_out(uart_dev, '\n');
    for (int i = 0; data[i] != '\0'; i++) {
        uart_poll_out(uart_dev, data[i]);
    }

    // small delay to ensure data is sent before disabling TX
    k_msleep(10);

    // set DE/RE low to go back to receive mode
    gpio_pin_set(gpio_dev, RS485_DIR_PIN, 0);
}

// performs the sensor reading sequence
static int process_sample(const struct device *dev)
{
    // declare variables for temperature and humidity
    struct sensor_value temp, humidity;
    double temp_d, humidity_d;
    int ret;

    // fetch the sample from the sensor
    ret = sensor_sample_fetch(dev);
    if (ret < 0) {
        LOG_ERR("Failed to fetch sample: %d", ret);
        return ret;
    }

    // get the temperature from the sensor
    ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
    if (ret < 0) {
        LOG_ERR("Failed to get temperature: %d", ret);
        return ret;
    }

    // get the humidity from the sensor
    ret = sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &humidity);
    if (ret < 0) {
        LOG_ERR("Failed to get humidity: %d", ret);
        return ret;
    }

    // convert the sensor values to doubles
    sensor_value_to_double(&temp, &temp_d);
    sensor_value_to_double(&humidity, &humidity_d);

    // print the temperature and humidity
    printk("Temperature: %.2f °C\n", temp_d);
    printk("Humidity: %.2f %%RH\n", humidity_d);

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "T=%.2fC H=%.2f%%\r\n", temp_d, humidity_d);
    rs485_send(buffer);

    // return success
    return 0;
}

// main function
int main(void)
{
    // check if the sensor is ready
    if (!device_is_ready(sht4x)) {
        LOG_ERR("SHT4X device not ready");
        return -1;
    }

    LOG_INF("SHT4X sensor ready");

    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready");
        return -1;
    }

    if (!device_is_ready(gpio_dev)) {
        LOG_ERR("GPIO device not ready");
        return -1;
    }

    gpio_pin_configure(gpio_dev, RS485_DIR_PIN, GPIO_OUTPUT_INACTIVE);

    LOG_INF("RS485 communication initialized");

    // loop forever
    while (1) {
        // process the sample
        int ret = process_sample(sht4x);
        // if there is an error, print an error message
        if (ret < 0) {
            LOG_ERR("Error processing sample: %d", ret);
        }
        
        k_sleep(K_SECONDS(2));
    }

    // return success
    return 0;
}
