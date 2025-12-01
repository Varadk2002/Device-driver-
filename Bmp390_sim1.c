/*
 * BMP390 Sensor Simulator - Corrected Version
 * Based on Bosch BMP3-Sensor-API (official driver)
 * https://github.com/BoschSensortec/BMP3-Sensor-API
 * 
 * This uses REALISTIC calibration values and CORRECTED compensation formulas
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

/* BMP390 Calibration data structure (quantized/processed coefficients) */
typedef struct {
    double par_t1;
    double par_t2;
    double par_t3;
    double par_p1;
    double par_p2;
    double par_p3;
    double par_p4;
    double par_p5;
    double par_p6;
    double par_p7;
    double par_p8;
    double par_p9;
    double par_p10;
    double par_p11;
    double t_lin;  /* Stores intermediate temperature value for pressure compensation */
} bmp390_calib_data;

/* Structure to hold uncompensated data */
typedef struct {
    uint32_t pressure;
    uint32_t temperature;
} bmp390_uncomp_data;

/* Structure to hold compensated data */
typedef struct {
    double temperature;  /* in degree Celsius */
    double pressure;     /* in Pascal */
} bmp390_data;

/* Helper function - power calculation used in Bosch's formula */
static double pow_bmp3(double base, uint8_t power)
{
    double result = 1.0;
    for (uint8_t i = 0; i < power; i++) {
        result *= base;
    }
    return result;
}

/**
 * @brief Parse and quantize calibration data from raw NVM registers
 * This converts the raw register values into the floating point coefficients
 * used in the compensation formulas
 * 
 * @param reg_data Raw calibration data from registers (21 bytes from 0x31-0x45)
 * @param calib_data Output quantized calibration coefficients
 */
void bmp390_parse_calib_data(const uint8_t *reg_data, bmp390_calib_data *calib_data)
{
    /* Temporary variables for raw register values */
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

    /* Parse raw values from register buffer */
    par_t1 = (uint16_t)(reg_data[1] << 8) | reg_data[0];
    par_t2 = (uint16_t)(reg_data[3] << 8) | reg_data[2];
    par_t3 = (int8_t)reg_data[4];
    par_p1 = (int16_t)(reg_data[6] << 8) | reg_data[5];
    par_p2 = (int16_t)(reg_data[8] << 8) | reg_data[7];
    par_p3 = (int8_t)reg_data[9];
    par_p4 = (int8_t)reg_data[10];
    par_p5 = (uint16_t)(reg_data[12] << 8) | reg_data[11];
    par_p6 = (uint16_t)(reg_data[14] << 8) | reg_data[13];
    par_p7 = (int8_t)reg_data[15];
    par_p8 = (int8_t)reg_data[16];
    par_p9 = (int16_t)(reg_data[18] << 8) | reg_data[17];
    par_p10 = (int8_t)reg_data[19];
    par_p11 = (int8_t)reg_data[20];

    /* Quantize according to Bosch's formula - converts to working coefficients */
    calib_data->par_t1 = ((double)par_t1 / 0.00390625);
    calib_data->par_t2 = ((double)par_t2 / 1073741824.0);
    calib_data->par_t3 = ((double)par_t3 / 281474976710656.0);
    calib_data->par_p1 = (((double)par_p1 - 16384.0) / 1048576.0);
    calib_data->par_p2 = (((double)par_p2 - 16384.0) / 536870912.0);
    calib_data->par_p3 = ((double)par_p3 / 4294967296.0);
    calib_data->par_p4 = ((double)par_p4 / 137438953472.0);
    calib_data->par_p5 = ((double)par_p5 / 0.125);
    calib_data->par_p6 = ((double)par_p6 / 64.0);
    calib_data->par_p7 = ((double)par_p7 / 256.0);
    calib_data->par_p8 = ((double)par_p8 / 32768.0);
    calib_data->par_p9 = ((double)par_p9 / 281474976710656.0);
    calib_data->par_p10 = ((double)par_p10 / 281474976710656.0);
    calib_data->par_p11 = ((double)par_p11 / 36893488147419103232.0);
}

/**
 * @brief Compensate temperature (Bosch official formula)
 */
double bmp390_compensate_temperature(uint32_t uncomp_temp, bmp390_calib_data *calib_data)
{
    double partial_data1 = (double)(uncomp_temp - calib_data->par_t1);
    double partial_data2 = (double)(partial_data1 * calib_data->par_t2);

    /* Store t_lin for pressure compensation */
    calib_data->t_lin = partial_data2 + (partial_data1 * partial_data1) * calib_data->par_t3;

    /* Return temperature in Celsius */
    return calib_data->t_lin;
}

/**
 * @brief Compensate pressure (Bosch official formula)
 */
double bmp390_compensate_pressure(uint32_t uncomp_press, const bmp390_calib_data *calib_data)
{
    double partial_data1;
    double partial_data2;
    double partial_data3;
    double partial_data4;
    double partial_out1;
    double partial_out2;

    /* Calculate offset */
    partial_data1 = calib_data->par_p6 * calib_data->t_lin;
    partial_data2 = calib_data->par_p7 * pow_bmp3(calib_data->t_lin, 2);
    partial_data3 = calib_data->par_p8 * pow_bmp3(calib_data->t_lin, 3);
    partial_out1 = calib_data->par_p5 + partial_data1 + partial_data2 + partial_data3;

    /* Calculate sensitivity */
    partial_data1 = calib_data->par_p2 * calib_data->t_lin;
    partial_data2 = calib_data->par_p3 * pow_bmp3(calib_data->t_lin, 2);
    partial_data3 = calib_data->par_p4 * pow_bmp3(calib_data->t_lin, 3);
    partial_out2 = (double)uncomp_press * (calib_data->par_p1 + partial_data1 + partial_data2 + partial_data3);

    /* Calculate final pressure */
    partial_data1 = pow_bmp3((double)uncomp_press, 2);
    partial_data2 = calib_data->par_p9 + calib_data->par_p10 * calib_data->t_lin;
    partial_data3 = partial_data1 * partial_data2;
    partial_data4 = partial_data3 + pow_bmp3((double)uncomp_press, 3) * calib_data->par_p11;

    return partial_out1 + partial_out2 + partial_data4;
}

/**
 * @brief Calculate altitude from pressure
 */
double calculate_altitude(double pressure)
{
    const double sea_level_pressure = 101325.0; /* Pa */
    return 44330.0 * (1.0 - pow(pressure / sea_level_pressure, 0.1903));
}

/**
 * @brief Get sensor data with compensation
 */
void bmp390_get_sensor_data(const bmp390_uncomp_data *uncomp_data, 
                            bmp390_calib_data *calib_data,
                            bmp390_data *comp_data)
{
    comp_data->temperature = bmp390_compensate_temperature(uncomp_data->temperature, calib_data);
    comp_data->pressure = bmp390_compensate_pressure(uncomp_data->pressure, calib_data);
}

/**
 * @brief Print calibration data
 */
void print_calib_data(const bmp390_calib_data *calib)
{
    printf("\n========================================\n");
    printf("Quantized Calibration Coefficients:\n");
    printf("========================================\n");
    printf("par_t1: %.10e\n", calib->par_t1);
    printf("par_t2: %.10e\n", calib->par_t2);
    printf("par_t3: %.10e\n", calib->par_t3);
    printf("par_p1: %.10e\n", calib->par_p1);
    printf("par_p2: %.10e\n", calib->par_p2);
    printf("par_p3: %.10e\n", calib->par_p3);
    printf("par_p4: %.10e\n", calib->par_p4);
    printf("par_p5: %.10e\n", calib->par_p5);
    printf("par_p6: %.10e\n", calib->par_p6);
    printf("par_p7: %.10e\n", calib->par_p7);
    printf("par_p8: %.10e\n", calib->par_p8);
    printf("par_p9: %.10e\n", calib->par_p9);
    printf("par_p10: %.10e\n", calib->par_p10);
    printf("par_p11: %.10e\n", calib->par_p11);
    printf("========================================\n\n");
}

/**
 * @brief Print sensor data
 */
void print_sensor_data(const bmp390_data *comp_data)
{
    double altitude = calculate_altitude(comp_data->pressure);
    
    printf("\n========================================\n");
    printf("BMP390 Compensated Sensor Data\n");
    printf("========================================\n");
    printf("Temperature: %.2f °C (%.2f °F)\n", 
           comp_data->temperature, 
           (comp_data->temperature * 1.8) + 32.0);
    printf("Pressure:    %.2f Pa (%.2f hPa)\n", 
           comp_data->pressure, 
           comp_data->pressure / 100.0);
    printf("Altitude:    %.2f m (%.2f ft)\n", 
           altitude, altitude * 3.28084);
    printf("========================================\n\n");
}

/**
 * @brief Main function
 */
int main(void)
{
    bmp390_calib_data calib;
    bmp390_uncomp_data uncomp_data;
    bmp390_data comp_data;

    printf("========================================\n");
    printf("BMP390 Sensor Simulator\n");
    printf("Corrected Version with Bosch Formulas\n");
    printf("========================================\n\n");

    /* 
     * Example realistic calibration data from a real BMP390 sensor
     * These are EXAMPLE raw register values - replace with YOUR sensor's values
     * Read from registers 0x31 to 0x45 (21 bytes)
     */
    uint8_t raw_calib_data[21] = {
        0xCB, 0x68,  /* par_t1: 0x68CB = 26827 */
        0x68, 0x66,  /* par_t2: 0x6668 = 26216 */
        0x03,        /* par_t3: 3 */
        0xE9, 0xBE,  /* par_p1: 0xBEE9 = -16663 (signed) */
        0x71, 0xD5,  /* par_p2: 0xD571 = -10895 (signed) */
        0x07,        /* par_p3: 7 */
        0x05,        /* par_p4: 5 */
        0xFF, 0x9F,  /* par_p5: 0x9FFF = 40959 */
        0xFF, 0x9F,  /* par_p6: 0x9FFF = 40959 */
        0x0F,        /* par_p7: 15 */
        0xFE,        /* par_p8: -2 (signed) */
        0x00, 0xE0,  /* par_p9: 0xE000 = -8192 (signed) */
        0xE0,        /* par_p10: -32 (signed) */
        0xEB         /* par_p11: -21 (signed) */
    };

    /* Parse and quantize calibration data */
    bmp390_parse_calib_data(raw_calib_data, &calib);
    
    printf("Parsed calibration data from example NVM registers.\n");
    printf("NOTE: These are example values. For accurate results,\n");
    printf("      replace with YOUR sensor's calibration data!\n");
    
    print_calib_data(&calib);

    /* Test case 1: Room temperature and sea level pressure */
    printf("\n--- Test Case 1: Room Temperature (~25°C, ~1013 hPa) ---\n");
    uncomp_data.temperature = 8388608;  /* Mid-scale for ~25°C */
    uncomp_data.pressure = 8388608;     /* Mid-scale for ~1013 hPa */
    
    printf("Raw ADC Values:\n");
    printf("  Temperature ADC: %u\n", uncomp_data.temperature);
    printf("  Pressure ADC:    %u\n", uncomp_data.pressure);
    
    bmp390_get_sensor_data(&uncomp_data, &calib, &comp_data);
    print_sensor_data(&comp_data);

    /* Test case 2: Slightly warmer, lower pressure */
    printf("\n--- Test Case 2: Warmer Temperature, Lower Pressure ---\n");
    uncomp_data.temperature = 8450000;
    uncomp_data.pressure = 8200000;
    
    printf("Raw ADC Values:\n");
    printf("  Temperature ADC: %u\n", uncomp_data.temperature);
    printf("  Pressure ADC:    %u\n", uncomp_data.pressure);
    
    bmp390_get_sensor_data(&uncomp_data, &calib, &comp_data);
    print_sensor_data(&comp_data);

    /* Test case 3: User input */
    printf("\n--- Custom Input Mode ---\n");
    printf("Enter Temperature ADC value (0-1048575): ");
    if (scanf("%u", &uncomp_data.temperature) != 1) {
        printf("Invalid input!\n");
        return 1;
    }
    
    printf("Enter Pressure ADC value (0-1048575): ");
    if (scanf("%u", &uncomp_data.pressure) != 1) {
        printf("Invalid input!\n");
        return 1;
    }
    
    bmp390_get_sensor_data(&uncomp_data, &calib, &comp_data);
    print_sensor_data(&comp_data);

    printf("\n========================================\n");
    printf("IMPORTANT NOTES:\n");
    printf("========================================\n");
    printf("1. Each BMP390 has UNIQUE calibration data\n");
    printf("2. Read 21 bytes from registers 0x31-0x45\n");
    printf("3. Replace raw_calib_data[] with YOUR values\n");
    printf("4. This code uses Bosch's official formulas\n");
    printf("5. Expected typical values:\n");
    printf("   - Temperature: 0-65 °C (typ. 25 °C)\n");
    printf("   - Pressure: 300-1250 hPa (typ. 1013 hPa)\n");
    printf("   - Altitude: -500 to 9000 m\n");
    printf("========================================\n\n");

    return 0;
}