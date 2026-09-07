#ifndef READ_MPU6050_DEMO_HPP
#define READ_MPU6050_DEMO_HPP

#ifdef __cplusplus
extern "C" {
#endif

void mpu6050_demo_main(void);
int mpu6050_demo_get_status_code(void);
int mpu6050_demo_get_i2c_sel(void);
int mpu6050_demo_get_pair_sel(void);
void mpu6050_demo_set_pair_sel_value(int pair_sel);

#ifdef __cplusplus
}
#endif

#endif // READ_MPU6050_DEMO_HPP
