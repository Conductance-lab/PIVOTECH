#ifndef READ_HARDWARE_CORE1_HPP
#define READ_HARDWARE_CORE1_HPP

#include <stdint.h>



// Test helper to (re)configure I2C pins and frequency for Core1 tests.
// Default 'print' parameter is provided here (declarations should carry defaults).
void test_configure_i2c_pins(int32_t frequency, bool print = 0);

// Start/stop helper functions for Core1 SSD1306 test
void start_core1_ssd1306_test(bool is_i2c_mode);
void stop_core1_ssd1306_test(void);

// MPU6050 & generic controls exported from hardware_core1.cpp
void start_core1_mpu6050_test(bool is_i2c_mode);
void stop_core1_test(void);
int32_t get_screen_refresh_rate(void);

// MPU6050 data structure (moved to header so callers can inspect members)
struct MPU6050_Data
{
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
    int16_t temp;
    float accel_x_g, accel_y_g, accel_z_g;
    float gyro_x_dps, gyro_y_dps, gyro_z_dps;
    float temperature_c;
};

struct MPU6050_Data *get_mpu6050_data(void);

// Indicate when a device-level error occurred during Core1 operations
extern bool device_error_occurred;

// Signals used to pause/resume Core1 background tasks
extern bool i2c_run_in_core1;


// Communication mode and test runner state
extern bool test_communication_mode;
extern bool i2cspi_test_running;

// Frequencies configured for tests
extern int32_t i2c_freq;
extern int32_t spi_freq;

// Test helper functions
void test_configure_communication_i2c_init(void);
void core1_test_internal(void);



#endif // READ_HARDWARE_CORE1_HPP