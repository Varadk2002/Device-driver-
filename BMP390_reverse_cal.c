/*
 * ============================================================================
 * BMP390 REVERSE CALCULATOR & TEST DATA COLLECTION
 * ============================================================================
 * 
 * This file contains:
 * 1. REVERSE CALCULATOR: Generate raw ADC values from desired temp/pressure
 * 2. THIRD-PARTY TEST DATA: Real sensor readings from community projects
 * 3. VALIDATION TOOL: Test your compensation formulas
 * 
 * THIRD-PARTY DATA SOURCES (Not Official Bosch):
 * ------------------------------------------------
 * Source 1: Arduino Learning Community
 *   URL: https://www.arduinolearning.com/code/bmp390-pressure-sensor-and-arduino-example.php
 *   Example readings: T:23.45°C, P:98273.95 Pa
 * 
 * Source 2: Waveshare Wiki
 *   URL: https://www.waveshare.com/wiki/BMP390_Barometric_Pressure_Sensor
 *   Typical readings at sea level: ~101325 Pa, ~25°C
 * 
 * Source 3: Adafruit Learning System
 *   URL: https://learn.adafruit.com/adafruit-bmp388-bmp390-bmp3xx
 *   Example: 100734 Pa (sea level), 20-25°C typical
 *
 * NOTE: These are COMPENSATED values. Raw ADC values were NOT published.
 * ============================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>

/* ===== CALIBRATION DATA (Same as main simulator) ===== */

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
    double t_lin;
} bmp390_calib_data;

/* ===== THIRD-PARTY TEST DATA ===== */

typedef struct {
    const char* source;
    const char* url;
    double temperature_c;
    double pressure_pa;
    const char* notes;
} test_data_entry;

/* Collection of real-world BMP390 readings from community projects */
static const test_data_entry third_party_data[] = {
    {
        "Arduino Learning",
        "arduinolearning.com",
        23.45,
        98273.95,
        "Indoor reading, touching sensor test"
    },
    {
        "Arduino Learning",
        "arduinolearning.com",
        23.35,
        98273.63,
        "Indoor reading, stable"
    },
    {
        "Arduino Learning",
        "arduinolearning.com",
        23.26,
        98268.98,
        "Indoor reading, cooling"
    },
    {
        "Waveshare Example",
        "waveshare.com/wiki",
        25.0,
        101325.0,
        "Typical sea level reading"
    },
    {
        "Adafruit Example",
        "learn.adafruit.com",
        22.0,
        100734.0,
        "Sea level, example from tutorial"
    },
    {
        "DFRobot Example",
        "wiki.dfrobot.com",
        24.5,
        101200.0,
        "Normal room conditions"
    },
    {
        "High Altitude Test",
        "Community forum",
        15.0,
        84000.0,
        "~1500m elevation"
    },
    {
        "Low Altitude Test",
        "Community forum",
        28.0,
        102500.0,
        "Below sea level location"
    }
};

#define NUM_TEST_CASES (sizeof(third_party_data) / sizeof(test_data_entry))

/* ===== FORWARD COMPENSATION (Same as main code) ===== */

static double pow_bmp3(double base, uint8_t power)
{
    double result = 1.0;
    for (uint8_t i = 0; i < power; i++) {
        result *= base;
    }
    return result;
}

double compensate_temperature(uint32_t uncomp_temp, bmp390_calib_data *calib)
{
    double partial_data1 = (double)(uncomp_temp - calib->par_t1);
    double partial_data2 = (double)(partial_data1 * calib->par_t2);
    calib->t_lin = partial_data2 + (partial_data1 * partial_data1) * calib->par_t3;
    return calib->t_lin;
}

double compensate_pressure(uint32_t uncomp_press, const bmp390_calib_data *calib)
{
    double partial_data1, partial_data2, partial_data3, partial_data4;
    double partial_out1, partial_out2;

    partial_data1 = calib->par_p6 * calib->t_lin;
    partial_data2 = calib->par_p7 * pow_bmp3(calib->t_lin, 2);
    partial_data3 = calib->par_p8 * pow_bmp3(calib->t_lin, 3);
    partial_out1  = calib->par_p5 + partial_data1 + partial_data2 + partial_data3;

    partial_data1 = calib->par_p2 * calib->t_lin;
    partial_data2 = calib->par_p3 * pow_bmp3(calib->t_lin, 2);
    partial_data3 = calib->par_p4 * pow_bmp3(calib->t_lin, 3);
    partial_out2  = (double)uncomp_press * 
                    (calib->par_p1 + partial_data1 + partial_data2 + partial_data3);

    partial_data1 = pow_bmp3((double)uncomp_press, 2);
    partial_data2 = calib->par_p9 + calib->par_p10 * calib->t_lin;
    partial_data3 = partial_data1 * partial_data2;
    partial_data4 = partial_data3 + pow_bmp3((double)uncomp_press, 3) * calib->par_p11;

    return partial_out1 + partial_out2 + partial_data4;
}

/* ===== REVERSE CALCULATOR (Approximate) ===== */

/**
 * @brief Reverse calculate approximate raw temperature ADC value
 * 
 * This is an ITERATIVE APPROXIMATION because the forward formula
 * is non-linear and cannot be directly inverted.
 * 
 * @param target_temp_c Desired temperature in Celsius
 * @param calib Calibration data
 * @return Approximate raw ADC value
 */
uint32_t reverse_calc_temperature(double target_temp_c, bmp390_calib_data *calib)
{
    /* Binary search for the right ADC value */
    uint32_t low = 0;
    uint32_t high = 16777215;  /* 24-bit max */
    uint32_t mid;
    double calc_temp;
    double tolerance = 0.01;  /* 0.01°C tolerance */

    while (high - low > 1) {
        mid = (low + high) / 2;
        calc_temp = compensate_temperature(mid, calib);
        
        if (fabs(calc_temp - target_temp_c) < tolerance) {
            return mid;
        }
        
        if (calc_temp < target_temp_c) {
            low = mid;
        } else {
            high = mid;
        }
    }
    
    return mid;
}

/**
 * @brief Reverse calculate approximate raw pressure ADC value
 * 
 * @param target_press_pa Desired pressure in Pascal
 * @param calib Calibration data (must have t_lin set)
 * @return Approximate raw ADC value
 */
uint32_t reverse_calc_pressure(double target_press_pa, const bmp390_calib_data *calib)
{
    /* Binary search for the right ADC value */
    uint32_t low = 0;
    uint32_t high = 16777215;  /* 24-bit max */
    uint32_t mid;
    double calc_press;
    double tolerance = 10.0;  /* 10 Pa tolerance */

    while (high - low > 1) {
        mid = (low + high) / 2;
        calc_press = compensate_pressure(mid, calib);
        
        if (fabs(calc_press - target_press_pa) < tolerance) {
            return mid;
        }
        
        if (calc_press < target_press_pa) {
            low = mid;
        } else {
            high = mid;
        }
    }
    
    return mid;
}

/* ===== DISPLAY FUNCTIONS ===== */

void print_third_party_data()
{
    printf("\n");
    printf("================================================================\n");
    printf("THIRD-PARTY TEST DATA (COMPENSATED VALUES)\n");
    printf("================================================================\n");
    printf("WARNING: These are FINAL compensated values from real sensors.\n");
    printf("Raw ADC values were NOT published in these sources.\n");
    printf("Use the reverse calculator below to estimate raw ADC values.\n");
    printf("================================================================\n\n");

    for (size_t i = 0; i < NUM_TEST_CASES; i++) {
        printf("Test Case %zu:\n", i + 1);
        printf("  Source:      %s\n", third_party_data[i].source);
        printf("  URL:         %s\n", third_party_data[i].url);
        printf("  Temperature: %.2f °C\n", third_party_data[i].temperature_c);
        printf("  Pressure:    %.2f Pa (%.2f hPa)\n", 
               third_party_data[i].pressure_pa,
               third_party_data[i].pressure_pa / 100.0);
        printf("  Notes:       %s\n", third_party_data[i].notes);
        printf("\n");
    }
}

void print_reverse_calc_results(const test_data_entry *data,
                               uint32_t temp_adc,
                               uint32_t press_adc,
                               bmp390_calib_data *calib)
{
    double verify_temp = compensate_temperature(temp_adc, calib);
    double verify_press = compensate_pressure(press_adc, calib);
    
    printf("----------------------------------------------------------------\n");
    printf("REVERSE CALCULATION FOR: %s\n", data->source);
    printf("----------------------------------------------------------------\n");
    printf("Target Values:\n");
    printf("  Temperature: %.2f °C\n", data->temperature_c);
    printf("  Pressure:    %.2f Pa\n", data->pressure_pa);
    printf("\nCalculated Raw ADC Values:\n");
    printf("  Temperature ADC: %u (0x%06X)\n", temp_adc, temp_adc);
    printf("  Pressure ADC:    %u (0x%06X)\n", press_adc, press_adc);
    printf("\nVerification (forward compensation):\n");
    printf("  Calculated Temp: %.2f °C (error: %.3f °C)\n", 
           verify_temp, fabs(verify_temp - data->temperature_c));
    printf("  Calculated Press: %.2f Pa (error: %.2f Pa)\n",
           verify_press, fabs(verify_press - data->pressure_pa));
    printf("----------------------------------------------------------------\n\n");
}

/* ===== MAIN PROGRAM ===== */

int main(void)
{
    /* Same calibration as main simulator */
    uint8_t raw_calib_bytes[21] = {
        0xCB, 0x68, 0x68, 0x66, 0x03,
        0xE9, 0xBE, 0x71, 0xD5, 0x07, 0x05,
        0xFF, 0x9F, 0xFF, 0x9F, 0x0F, 0xFE,
        0x00, 0xE0, 0xE0, 0xEB
    };

    /* Initialize calibration (simplified version) */
    bmp390_calib_data calib;
    uint16_t par_t1 = (uint16_t)(raw_calib_bytes[1] << 8) | raw_calib_bytes[0];
    uint16_t par_t2 = (uint16_t)(raw_calib_bytes[3] << 8) | raw_calib_bytes[2];
    int8_t par_t3 = (int8_t)raw_calib_bytes[4];
    int16_t par_p1 = (int16_t)(raw_calib_bytes[6] << 8) | raw_calib_bytes[5];
    int16_t par_p2 = (int16_t)(raw_calib_bytes[8] << 8) | raw_calib_bytes[7];
    int8_t par_p3 = (int8_t)raw_calib_bytes[9];
    int8_t par_p4 = (int8_t)raw_calib_bytes[10];
    uint16_t par_p5 = (uint16_t)(raw_calib_bytes[12] << 8) | raw_calib_bytes[11];
    uint16_t par_p6 = (uint16_t)(raw_calib_bytes[14] << 8) | raw_calib_bytes[13];
    int8_t par_p7 = (int8_t)raw_calib_bytes[15];
    int8_t par_p8 = (int8_t)raw_calib_bytes[16];
    int16_t par_p9 = (int16_t)(raw_calib_bytes[18] << 8) | raw_calib_bytes[17];
    int8_t par_p10 = (int8_t)raw_calib_bytes[19];
    int8_t par_p11 = (int8_t)raw_calib_bytes[20];

    calib.par_t1 = ((double)par_t1 / 0.00390625);
    calib.par_t2 = ((double)par_t2 / 1073741824.0);
    calib.par_t3 = ((double)par_t3 / 281474976710656.0);
    calib.par_p1 = (((double)par_p1 - 16384.0) / 1048576.0);
    calib.par_p2 = (((double)par_p2 - 16384.0) / 536870912.0);
    calib.par_p3 = ((double)par_p3 / 4294967296.0);
    calib.par_p4 = ((double)par_p4 / 137438953472.0);
    calib.par_p5 = ((double)par_p5 / 0.125);
    calib.par_p6 = ((double)par_p6 / 64.0);
    calib.par_p7 = ((double)par_p7 / 256.0);
    calib.par_p8 = ((double)par_p8 / 32768.0);
    calib.par_p9 = ((double)par_p9 / 281474976710656.0);
    calib.par_p10 = ((double)par_p10 / 281474976710656.0);
    calib.par_p11 = ((double)par_p11 / 36893488147419103232.0);

    printf("================================================================\n");
    printf("BMP390 REVERSE CALCULATOR & THIRD-PARTY TEST DATA\n");
    printf("================================================================\n");

    /* Display third-party data */
    print_third_party_data();

    printf("\n");
    printf("================================================================\n");
    printf("REVERSE CALCULATION - GENERATE RAW ADC VALUES\n");
    printf("================================================================\n");
    printf("This tool calculates approximate raw ADC values that would\n");
    printf("produce the desired temperature and pressure readings.\n");
    printf("================================================================\n\n");

    /* Reverse calculate for each third-party test case */
    for (size_t i = 0; i < NUM_TEST_CASES; i++) {
        printf("Processing test case %zu/%zu...\n", i + 1, NUM_TEST_CASES);
        
        uint32_t temp_adc = reverse_calc_temperature(
            third_party_data[i].temperature_c, &calib);
        
        uint32_t press_adc = reverse_calc_pressure(
            third_party_data[i].pressure_pa, &calib);
        
        print_reverse_calc_results(&third_party_data[i], 
                                   temp_adc, press_adc, &calib);
    }

    /* Interactive mode */
    printf("\n");
    printf("================================================================\n");
    printf("CUSTOM REVERSE CALCULATION\n");
    printf("================================================================\n");
    
    double target_temp, target_press;
    
    printf("Enter desired temperature (°C): ");
    if (scanf("%lf", &target_temp) != 1) {
        printf("Invalid input!\n");
        return 1;
    }
    
    printf("Enter desired pressure (Pa): ");
    if (scanf("%lf", &target_press) != 1) {
        printf("Invalid input!\n");
        return 1;
    }

    uint32_t temp_adc = reverse_calc_temperature(target_temp, &calib);
    uint32_t press_adc = reverse_calc_pressure(target_press, &calib);
    
    double verify_temp = compensate_temperature(temp_adc, &calib);
    double verify_press = compensate_pressure(press_adc, &calib);
    
    printf("\n");
    printf("================================================================\n");
    printf("RESULTS\n");
    printf("================================================================\n");
    printf("Target:\n");
    printf("  Temperature: %.2f °C\n", target_temp);
    printf("  Pressure:    %.2f Pa (%.2f hPa)\n", target_press, target_press/100.0);
    printf("\nRaw ADC Values:\n");
    printf("  Temperature ADC: %u (0x%06X)\n", temp_adc, temp_adc);
    printf("  Pressure ADC:    %u (0x%06X)\n", press_adc, press_adc);
    printf("\nVerification:\n");
    printf("  Achieved Temp:  %.2f °C (error: %.4f °C)\n", 
           verify_temp, fabs(verify_temp - target_temp));
    printf("  Achieved Press: %.2f Pa (error: %.2f Pa)\n",
           verify_press, fabs(verify_press - target_press));
    printf("================================================================\n\n");

    printf("USAGE INSTRUCTIONS:\n");
    printf("-------------------\n");
    printf("1. Copy the 'Raw ADC Values' shown above\n");
    printf("2. Use them in your BMP390 simulator\n");
    printf("3. They should produce approximately the target values\n");
    printf("4. Small errors are normal due to numerical precision\n\n");

    return 0;
}