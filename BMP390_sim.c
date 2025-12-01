/*
 * BMP390 Sensor Simulator
 * Based on Bosch BMP390 Datasheet compensation algorithms
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>

/* BMP390 Calibration data structure */
typedef struct {
    uint16_t par_t1;
    uint16_t par_t2;
    int8_t par_t3;
    int16_t par_p1;
    int16_t par_p2;
    int8_t par_p3;
    int8_t par_p4;
    uint16_t par_p5;
    uint16_t par_p6;
    int8_t par_p7;
    int8_t par_p8;
    int16_t par_p9;
    int8_t par_p10;
    int8_t par_p11;
} bmp390_calib_data;

/* Structure to hold uncompensated data */
typedef struct {
    uint32_t pressure;
    uint32_t temperature;
} bmp390_uncomp_data;

/* Structure to hold compensated data */
typedef struct {
    double temperature;  // in degree Celsius
    double pressure;     // in Pascal
} bmp390_data;

/* Global variable to store t_lin for pressure compensation */
static double t_lin = 0.0;

/**
 * @brief Compensate raw temperature data
 * 
 * @param uncomp_temp Uncompensated temperature data (20-bit ADC value)
 * @param calib_data Pointer to calibration data structure
 * @return Compensated temperature in degree Celsius
 */
double bmp390_compensate_temperature(uint32_t uncomp_temp, const bmp390_calib_data *calib_data)
{
    double partial_data1;
    double partial_data2;
    double partial_data3;
    double partial_data4;
    double partial_data5;
    double partial_data6;
    double comp_temp;

    partial_data1 = (double)(uncomp_temp - (256.0 * calib_data->par_t1));
    partial_data2 = (double)(calib_data->par_t2 * partial_data1);
    partial_data3 = (double)(partial_data1 * partial_data1);
    partial_data4 = (double)(partial_data3 * calib_data->par_t3);
    partial_data5 = (double)((partial_data2 * 262144.0) + partial_data4);
    partial_data6 = (double)(partial_data5 / 4294967296.0);

    /* Store t_lin for pressure compensation */
    t_lin = partial_data6;
    
    /* Calculate compensated temperature */
    comp_temp = (double)(partial_data6 / 100.0);

    return comp_temp;
}

/**
 * @brief Compensate raw pressure data
 * 
 * @param uncomp_press Uncompensated pressure data (20-bit ADC value)
 * @param calib_data Pointer to calibration data structure
 * @return Compensated pressure in Pascal
 */
double bmp390_compensate_pressure(uint32_t uncomp_press, const bmp390_calib_data *calib_data)
{
    double partial_data1;
    double partial_data2;
    double partial_data3;
    double partial_data4;
    double partial_data5;
    double partial_data6;
    double offset;
    double sensitivity;
    double comp_press;

    /* Calculate offset */
    partial_data1 = t_lin * t_lin;
    partial_data2 = partial_data1 / 64.0;
    partial_data3 = (partial_data2 * t_lin) / 256.0;
    partial_data4 = (calib_data->par_p8 * partial_data3);
    partial_data5 = (calib_data->par_p7 * partial_data1) * 16.0;
    partial_data6 = (calib_data->par_p6 * t_lin) * 4194304.0;
    offset = (calib_data->par_p5 * 140737488355328.0) + partial_data4 + partial_data5 + partial_data6;

    /* Calculate sensitivity */
    partial_data2 = (calib_data->par_p4 * partial_data3);
    partial_data4 = (calib_data->par_p3 * partial_data1) * 4.0;
    partial_data5 = (calib_data->par_p2 - 16384.0) * t_lin * 2097152.0;
    sensitivity = ((calib_data->par_p1 - 16384.0) * 70368744177664.0) + partial_data2 + partial_data4 + partial_data5;

    /* Calculate compensated pressure */
    partial_data1 = (sensitivity / 16777216.0) * uncomp_press;
    partial_data2 = calib_data->par_p10 * t_lin;
    partial_data3 = partial_data2 + (65536.0 * calib_data->par_p9);
    partial_data4 = (partial_data3 * uncomp_press) / 8192.0;
    partial_data5 = (partial_data4 / 512.0);
    partial_data6 = (double)uncomp_press * (double)uncomp_press;
    partial_data2 = (calib_data->par_p11 * partial_data6) / 65536.0;
    partial_data3 = (partial_data2 * uncomp_press) / 128.0;
    partial_data4 = (offset / 4.0) + partial_data1 + partial_data5 + partial_data3;
    comp_press = (partial_data4 / 25600.0);

    return comp_press;
}

/**
 * @brief Calculate altitude from pressure
 * 
 * @param pressure Pressure in Pascal
 * @return Altitude in meters
 */
double calculate_altitude(double pressure)
{
    double altitude;
    const double sea_level_pressure = 101325.0; // Pa
    
    altitude = 44330.0 * (1.0 - pow(pressure / sea_level_pressure, 0.1903));
    
    return altitude;
}

/**
 * @brief Get sensor data with compensation
 * 
 * @param uncomp_data Pointer to uncompensated data structure
 * @param calib_data Pointer to calibration data structure
 * @param comp_data Pointer to store compensated data
 */
void bmp390_get_sensor_data(const bmp390_uncomp_data *uncomp_data, 
                            const bmp390_calib_data *calib_data,
                            bmp390_data *comp_data)
{
    /* Compensate temperature data */
    comp_data->temperature = bmp390_compensate_temperature(uncomp_data->temperature, calib_data);
    
    /* Compensate pressure data */
    comp_data->pressure = bmp390_compensate_pressure(uncomp_data->pressure, calib_data);
}

/**
 * @brief Print sensor data
 */
void print_sensor_data(const bmp390_data *comp_data)
{
    double altitude;
    
    printf("\n========================================\n");
    printf("BMP390 Compensated Sensor Data\n");
    printf("========================================\n");
    
    printf("Temperature: %.2f °C (%.2f °F)\n", 
           comp_data->temperature, 
           (comp_data->temperature * 1.8) + 32.0);
    
    printf("Pressure:    %.2f Pa (%.2f hPa)\n", 
           comp_data->pressure, 
           comp_data->pressure / 100.0);
    
    altitude = calculate_altitude(comp_data->pressure);
    printf("Altitude:    %.2f m (%.2f ft)\n", 
           altitude, 
           altitude * 3.28084);
    
    printf("========================================\n\n");
}

/**
 * @brief Main function - BMP390 Simulator
 */
int main(void)
{
    /* Example calibration data - replace with actual values from your sensor */
    bmp390_calib_data calib = {
        .par_t1 = 26811,
        .par_t2 = 26184,
        .par_t3 = 3,
        .par_p1 = -16647,
        .par_p2 = -10879,
        .par_p3 = 7,
        .par_p4 = 5,
        .par_p5 = 0xFFFFFFFF & (-94),  // Handle unsigned
        .par_p6 = 0xFFFFFFFF & (-7),   // Handle unsigned
        .par_p7 = 15,
        .par_p8 = -2,
        .par_p9 = -8192,
        .par_p10 = -32,
        .par_p11 = -21
    };

    /* Uncompensated sensor data (raw ADC values) */
    bmp390_uncomp_data uncomp_data = {
        .temperature = 8388608,  // 20-bit ADC value
        .pressure = 8388608      // 20-bit ADC value
    };

    /* Compensated sensor data */
    bmp390_data comp_data;

    printf("BMP390 Sensor Simulator\n");
    printf("=======================\n\n");
    
    printf("Raw ADC Values:\n");
    printf("  Temperature ADC: %u\n", uncomp_data.temperature);
    printf("  Pressure ADC:    %u\n", uncomp_data.pressure);

    /* Get compensated sensor data */
    bmp390_get_sensor_data(&uncomp_data, &calib, &comp_data);

    /* Print results */
    print_sensor_data(&comp_data);

    /* Example with different raw values */
    printf("\n--- Example 2 ---\n");
    uncomp_data.temperature = 8500000;
    uncomp_data.pressure = 8200000;
    
    printf("Raw ADC Values:\n");
    printf("  Temperature ADC: %u\n", uncomp_data.temperature);
    printf("  Pressure ADC:    %u\n", uncomp_data.pressure);
    
    bmp390_get_sensor_data(&uncomp_data, &calib, &comp_data);
    print_sensor_data(&comp_data);

    /* Example 3 - User input */
    printf("\n--- Custom Input ---\n");
    printf("Enter Temperature ADC value (0-1048575): ");
    scanf("%u", &uncomp_data.temperature);
    
    printf("Enter Pressure ADC value (0-1048575): ");
    scanf("%u", &uncomp_data.pressure);
    
    bmp390_get_sensor_data(&uncomp_data, &calib, &comp_data);
    print_sensor_data(&comp_data);

    return 0;
}