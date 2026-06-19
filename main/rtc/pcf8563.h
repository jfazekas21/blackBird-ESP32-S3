#ifndef MAIN_PCF8563_H_
#define MAIN_PCF8563_H_

#include <time.h>
#include <stdbool.h>
#include "driver/i2c.h"
#include "actor.h"
#include "i2cdev.h"


#if defined (B543) ||(B542) || defined(B394)
#define SDA_GPIO_PIN                    (GPIO_NUM_4)
#define SCL_GPIO_PIN                    (GPIO_NUM_5)
#else
#define SDA_GPIO_PIN                    (GPIO_NUM_19)
#define SCL_GPIO_PIN                    (GPIO_NUM_20)
#endif



#define PCF8563_ADDR                    (0x51) //!< I2C address (0xA2 >> 1)
#define I2C_NUM                         (0)

#define PCF8563_ADDR_STATUS1            (0x00)
#define PCF8563_ADDR_STATUS2            (0x01)
#define PCF8563_ADDR_TIME_MS            (0x00)
#define PCF8563_ADDR_TIME               (0x01)
#define PCF8563_ADDR_ALARM_1            (0x08)
#define PCF8563_ADDR_ALARM_2            (0x08)
#define PCF8563_ADDR_ALARM_EN           (0x10)
#define PCF8563_ADDR_TIMESTAMP_1        (0x11)
#define PCF8563_ADDR_TIMESTAMP_2        (0x17)
#define PCF8563_ADDR_TIMESTAMP_3        (0x1D)
#define PCF8563_ADDR_CONTROL_OFFSET     (0x24)
#define PCF8563_ADDR_CONTROL            (0x25)
#define PCF8563_ADDR_CONTROL_FUNCTION   (0x28)


uint8_t bcd2dec(uint8_t val);
uint8_t dec2bcd(uint8_t val);
esp_err_t pcf8563_init_desc(i2c_dev_t *dev, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio);
esp_err_t pcf8563_reset(i2c_dev_t *dev);
esp_err_t pcf8563_set_time(i2c_dev_t *dev, struct tm *time);
esp_err_t pcf8563_get_time(i2c_dev_t *dev, struct tm *time);
esp_err_t pcf8563_enable_100th_second_mode(i2c_dev_t *dev);
esp_err_t pcf8563_set_time_ms(i2c_dev_t *dev, struct tm *time, uint8_t time_ms);
esp_err_t pcf8563_get_time_ms(i2c_dev_t *dev, struct tm *time, uint8_t *time_ms);
void pcf8563_set_device_handle(i2c_dev_t *dev);
i2c_dev_t *pcf8563_get_device_handle(void);

#endif /* MAIN_PCF8563_H_ */
