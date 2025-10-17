#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sht41_app);

#define SHT4X_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(sensirion_sht4x)

static const struct device *sht4x = DEVICE_DT_GET(SHT4X_NODE);

static void sensor_value_to_double(struct sensor_value *val, double *dval)
{
    *dval = (double)val->val1 + (double)val->val2 / 1000000;
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

    printk("Temperature: %.2f °C\n", temp_d);
    printk("Humidity: %.2f %%RH\n", humidity_d);

    return 0;
}

int main(void)
{
    printk("SHT41 Sensor Demo on ESP32\n");

    if (!device_is_ready(sht4x)) {
        LOG_ERR("SHT4X device not ready");
        return -1;
    }

    LOG_INF("SHT4X sensor ready");

    while (1) {
        int ret = process_sample(sht4x);
        if (ret < 0) {
            LOG_ERR("Error processing sample: %d", ret);
        }
        
        k_sleep(K_SECONDS(2));
    }

    return 0;
}
