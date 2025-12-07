/*******************************************************************************
 * COMPLETE SENSOR STREAM SYSTEM - RA6M5
 * UART: SCI5 (P501 TX, P502 RX)
 * I2C:  IIC0 (P400 SDA, P401 SCL)
 * ICP10101 + ZMOD4510
 ******************************************************************************/

#include "hal_data.h"
#include <stdint.h>

/* ============================================================================
 * UART SCI5 (P501 TX / P502 RX)
 * ==========================================================================*/
void setup_sci5_uart(void)
{
    R_SCI5->SCR_b.TE = 0;
    R_SCI5->SCR_b.RE = 0;

    R_MSTP->MSTPCRB_b.MSTPB26 = 1;   // Stop module

    /* Unlock PFS */
    R_PMISC->PWPR_b.B0WI = 0;
    R_PMISC->PWPR_b.PFSWE = 1;

    // TX = P501
    R_PORT5->PDR_b.PDR1 = 1;
    R_PFS->PORT[5].PIN[1].PmnPFS_b.PMR  = 0;
    R_PFS->PORT[5].PIN[1].PmnPFS_b.PSEL = 0b00101;
    R_PFS->PORT[5].PIN[1].PmnPFS_b.PMR  = 1;

    // RX = P502
    R_PORT5->PDR_b.PDR2 = 0;
    R_PFS->PORT[5].PIN[2].PmnPFS_b.PMR  = 0;
    R_PFS->PORT[5].PIN[2].PmnPFS_b.PSEL = 0b00101;
    R_PFS->PORT[5].PIN[2].PmnPFS_b.PMR  = 1;

    /* Lock PFS */
    R_PMISC->PWPR_b.PFSWE = 0;
    R_PMISC->PWPR_b.B0WI  = 1;

    /* Enable module */
    R_MSTP->MSTPCRB_b.MSTPB26 = 0;

    /* UART config */
    R_SCI5->SMR = 0;
    R_SCI5->SMR_b.CKS  = 1;   // PCLKA/4
    R_SCI5->SMR_b.STOP = 0;   // 1 stop
    R_SCI5->SMR_b.PE   = 0;   // No parity
    R_SCI5->SMR_b.CHR  = 0;   // 8-bit

    R_SCI5->SCMR = 0xF2;      // CHR1=1, SMIF=0

    R_SCI5->SEMR = 0;
    R_SCI5->SEMR_b.ABCS = 0;
    R_SCI5->SEMR_b.BGDM = 0;

    /* Baud 9600 @ PCLKA=100MHz, CKS=1 */
    R_SCI5->BRR = 81;

    R_SCI5->SCR_b.TE = 1;
    R_SCI5->SCR_b.RE = 1;
}

void uart_send_char(uint8_t c)
{
    while (!R_SCI5->SSR_b.TDRE);
    R_SCI5->TDR = c;
}

void uart_send_string(const char *s)
{
    while (*s) uart_send_char(*s++);
}

void uart_print_u32(uint32_t v)
{
    if (v == 0) { uart_send_char('0'); return; }
    char b[12]; int i=0;
    while (v) { b[i++] = '0' + (v%10); v/=10; }
    while (i--) uart_send_char(b[i]);
}

void uart_print_hex(uint8_t v)
{
    const char *h="0123456789ABCDEF";
    uart_send_char(h[(v>>4)&0xF]);
    uart_send_char(h[v&0xF]);
}

/* ============================================================================
 * I2C0 BUS RECOVERY + DRIVER
 * ==========================================================================*/
#define TIMEOUT_FAST  50000
#define TIMEOUT_READ  2000000

void i2c_force_bus_clear(void)
{
    /* Unlock PFS */
    R_PMISC->PWPR_b.B0WI = 0;
    R_PMISC->PWPR_b.PFSWE = 1;

    /* ----------------------------------------------------
     * Set SCL0=P400 and SDA0=P401 to GPIO output HIGH
     * ---------------------------------------------------- */
    // SCL0 = P400
    R_PFS->PORT[4].PIN[0].PmnPFS = 0;              // Set to GPIO
    R_PFS->PORT[4].PIN[0].PmnPFS_b.PDR  = 1;       // Output
    R_PFS->PORT[4].PIN[0].PmnPFS_b.PODR = 1;       // HIGH

    // SDA0 = P401
    R_PFS->PORT[4].PIN[1].PmnPFS = 0;
    R_PFS->PORT[4].PIN[1].PmnPFS_b.PDR  = 1;
    R_PFS->PORT[4].PIN[1].PmnPFS_b.PODR = 1;

    /* Lock PFS */
    R_PMISC->PWPR_b.PFSWE = 0;
    R_PMISC->PWPR_b.B0WI  = 1;

    /* ----------------------------------------------------
     * Pulse SCL (P400) 9 times to free SDA
     * ---------------------------------------------------- */
    for (int i = 0; i < 9; i++)
    {
        R_PORT4->PODR_b.PODR0 = 0;        // SCL LOW
        R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MILLISECONDS);

        R_PORT4->PODR_b.PODR0 = 1;        // SCL HIGH
        R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MILLISECONDS);
    }

    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
}


int i2c0_write(uint8_t slave, uint8_t *data, uint32_t len, uint32_t timeout)
{
    uint32_t t;

    // Wait for bus to be free (instead of returning error immediately)
    t = timeout;
    while (R_IIC0->ICCR2_b.BBSY) {
        if (--t == 0) {
            // Force STOP to try to clear bus
            R_IIC0->ICCR2_b.SP = 1;
            R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);

            // Check again
            if (R_IIC0->ICCR2_b.BBSY) {
                return -100;  // Still busy, give up
            }
            break;
        }
    }

    R_IIC0->ICCR2_b.ST = 1;

    t=timeout;
    while (!R_IIC0->ICSR2_b.TDRE)
        if (--t == 0) return -1;

    R_IIC0->ICDRT = (slave<<1) | 0;

    t=timeout;
    while (!R_IIC0->ICSR2_b.TDRE)
    {
        if (R_IIC0->ICSR2_b.NACKF){
            R_IIC0->ICSR2_b.NACKF=0;
            R_IIC0->ICCR2_b.SP=1;
            R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
            return -2;
        }
        if (--t==0) return -3;
    }

    for(uint32_t i=0;i<len;i++){
        R_IIC0->ICDRT=data[i];
        t=timeout;
        while(!R_IIC0->ICSR2_b.TDRE){
            if(R_IIC0->ICSR2_b.NACKF){
                R_IIC0->ICSR2_b.NACKF=0;
                R_IIC0->ICCR2_b.SP=1;
                R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
                return -4;
            }
            if(--t==0) return -5;
        }
    }

    R_IIC0->ICCR2_b.SP=1;
    R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MILLISECONDS);
    return 0;
}

int i2c0_read(uint8_t slave, uint8_t *data, uint32_t len, uint32_t timeout)
{
    uint32_t t;

    // Wait for bus to be free
    t = timeout;
    while (R_IIC0->ICCR2_b.BBSY) {
        if (--t == 0) {
            // Force STOP to try to clear bus
            R_IIC0->ICCR2_b.SP = 1;
            R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);

            // Check again
            if (R_IIC0->ICCR2_b.BBSY) {
                return -100;  // Still busy, give up
            }
            break;
        }
    }

    R_IIC0->ICCR2_b.ST = 1;

    t=timeout;
    while(!R_IIC0->ICSR2_b.TDRE)
        if(--t==0) return -1;

    R_IIC0->ICDRT = (slave<<1) | 1;

    t=timeout;
    while(!R_IIC0->ICSR2_b.RDRF)
    {
        if(R_IIC0->ICSR2_b.NACKF){
            R_IIC0->ICSR2_b.NACKF=0;
            R_IIC0->ICCR2_b.SP=1;
            R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
            return -2;
        }
        if(--t==0) return -3;
    }

    R_IIC0->ICMR3_b.ACKBT = 0;

    for(uint32_t i=0;i<len;i++){
        if(i==(len-1)) R_IIC0->ICMR3_b.ACKBT=1;

        t=timeout;
        while(!R_IIC0->ICSR2_b.RDRF)
            if(--t==0) return -4;

        data[i] = R_IIC0->ICDRR;
    }

    R_IIC0->ICCR2_b.SP = 1;
    R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MILLISECONDS);
    return 0;
}

/* ============================================================================
 * FULL HARDWARE SETUP
 * ==========================================================================*/
void setup_i2c0(void)
{
    /* Enable module */
    R_MSTP->MSTPCRB_b.MSTPB9 = 0;

    /* Unlock PFS */
    R_PMISC->PWPR_b.B0WI = 0;
    R_PMISC->PWPR_b.PFSWE = 1;

    /* Configure P400 as SDA0 (IIC0) */
    R_PFS->PORT[4].PIN[0].PmnPFS_b.PMR   = 0;       // GPIO mode first
    R_PFS->PORT[4].PIN[0].PmnPFS_b.PSEL  = 7;       // IIC0 function
    R_PFS->PORT[4].PIN[0].PmnPFS_b.NCODR = 1;       // Open-drain
    R_PFS->PORT[4].PIN[0].PmnPFS_b.PCR   = 1;       // Pull-up enable
    R_PFS->PORT[4].PIN[0].PmnPFS_b.PMR   = 1;       // Peripheral mode

    /* Configure P401 as SCL0 (IIC0) */
    R_PFS->PORT[4].PIN[1].PmnPFS_b.PMR   = 0;       // GPIO mode first
    R_PFS->PORT[4].PIN[1].PmnPFS_b.PSEL  = 7;       // IIC0 function
    R_PFS->PORT[4].PIN[1].PmnPFS_b.NCODR = 1;       // Open-drain
    R_PFS->PORT[4].PIN[1].PmnPFS_b.PCR   = 1;       // Pull-up enable
    R_PFS->PORT[4].PIN[1].PmnPFS_b.PMR   = 1;       // Peripheral mode

    /* Lock PFS */
    R_PMISC->PWPR_b.PFSWE = 0;
    R_PMISC->PWPR_b.B0WI  = 1;

    /* I2C0 init */
    R_IIC0->ICCR1_b.ICE     = 0;
    R_IIC0->ICCR1_b.IICRST  = 1;
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
    R_IIC0->ICCR1_b.IICRST  = 0;

    R_IIC0->ICBRH = 24;  // 400 kHz
    R_IIC0->ICBRL = 24;

    R_IIC0->ICMR1 = 0;
    R_IIC0->ICMR2 = 0;
    R_IIC0->ICMR3 = 0;

    R_IIC0->ICCR2 = 0;     // Clear all control bits
    R_IIC0->ICSR2 = 0;     // Clear all status flags

    R_IIC0->ICCR1_b.ICE = 1;

    // Force STOP condition to clear any stuck bus state
    R_IIC0->ICCR2_b.SP = 1;
    R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
}
void i2c0_scan(void)
{
    uart_send_string("I2C Bus Scan...\r\n");

    uint8_t dummy = 0x00;
    int found_count = 0;

    for (uint8_t addr = 1; addr < 0x7F; addr++)
    {
        int r = i2c0_write(addr, &dummy, 1, 20000);

        if (r == 0)
        {
            uart_send_string("Found: 0x");
            uart_print_hex(addr);
            uart_send_string("\r\n");
            found_count++;
        }
        else if (addr == 0x33 || addr == 0x63)  // Debug for expected addresses
        {
            uart_send_string("Addr 0x");
            uart_print_hex(addr);
            uart_send_string(" error: ");
            uart_print_u32((uint32_t)(-r));
            uart_send_string("\r\n");
        }
    }

    uart_send_string("Scan complete. Found: ");
    uart_print_u32(found_count);
    uart_send_string(" devices\r\n");
}

/* ============================================================================
 * HS3001 TEMPERATURE & HUMIDITY SENSOR
 * ==========================================================================*/
#define HS3001_ADDR 0x44

typedef struct {
    bool initialized;
} hs3001_sensor_t;

static hs3001_sensor_t hs_sensor = {0};

// Initialize sensor
bool hs3001_init(void)
{
    uart_send_string("  HS3001 sensor at 0x44\r\n");
    hs_sensor.initialized = true;
    return true;
}

// Read temperature and humidity
bool hs3001_read(float *temp_c, float *humidity_percent)
{
    if (!hs_sensor.initialized) {
        return false;
    }

    // Trigger measurement
    uart_send_string("  Triggering...");
    uint8_t trigger_cmd = 0x00;
    if (i2c0_write(HS3001_ADDR, &trigger_cmd, 1, TIMEOUT_FAST) != 0) {
        uart_send_string("failed\r\n");
        return false;
    }
    uart_send_string("OK\r\n");

    // Wait for measurement to complete (50ms should be enough for 14-bit)
    R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);

    // Read 4 bytes: [Humidity MSB][Humidity LSB][Temperature MSB][Temperature LSB]
    uint8_t data[4];
    uart_send_string("  Reading data...");
    if (i2c0_read(HS3001_ADDR, data, 4, TIMEOUT_READ) != 0) {
        uart_send_string("failed\r\n");
        // Force bus recovery on read failure
        i2c_force_bus_clear();
        R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);
        return false;
    }
    uart_send_string("OK\r\n");

    // Show raw bytes
    uart_send_string("  Raw: ");
    for(int i=0; i<4; i++) {
        uart_print_hex(data[i]);
        uart_send_char(' ');
    }
    uart_send_string("\r\n");

    // Check status bits [15:14] in first byte
    // 00 = valid data
    // 01 = stale data (data already fetched since last measurement)
    // 10 = command mode (not in normal mode)
    // 11 = reserved
    uint8_t status = (data[0] >> 6) & 0x03;
    uart_send_string("  Status: ");
    uart_print_hex(status);
    
    if (status == 0x02) {
        uart_send_string(" (command mode - sensor not ready)\r\n");
        return false;
    } else if (status == 0x01) {
        uart_send_string(" (stale data - using previous measurement)\r\n");
        // Continue to parse - stale data is still valid, just not fresh
    } else if (status == 0x00) {
        uart_send_string(" (valid data)\r\n");
    } else {
        uart_send_string(" (reserved/unknown)\r\n");
    }

    // Parse humidity (first 2 bytes, 14-bit)
    // Bits [13:0] of first 2 bytes (status bits in [15:14])
    uint16_t hum_raw = ((uint16_t)(data[0] & 0x3F) << 8) | data[1];
    *humidity_percent = ((float)hum_raw / 16383.0f) * 100.0f;

    // Parse temperature (last 2 bytes, 14-bit)
    // HS3001 format: Byte2=[T5 T4 T3 T2 T1 T0 x x], Byte3=[T13 T12 T11 T10 T9 T8 T7 T6]
    // Need to swap: data[3] is MSB, data[2] is LSB
    uint16_t temp_raw = ((uint16_t)data[3] << 6) | ((data[2] >> 2) & 0x3F);

    *temp_c = (((float)temp_raw / 16383.0f) * 165.0f) - 40.0f;

    // Clamp temperature to reasonable range
    if (*temp_c < -40.0f) *temp_c = -40.0f;
    if (*temp_c > 125.0f) *temp_c = 125.0f;

    uart_send_string("  Hum_raw=");
    uart_print_u32(hum_raw);
    uart_send_string(", Temp_raw=");
    uart_print_u32(temp_raw);
    uart_send_string(" (0x");
    uart_print_hex(data[2]);
    uart_print_hex(data[3]);
    uart_send_string(")\r\n");

    return true;
}

/* ============================================================================
 * MAIN
 * ==========================================================================*/
void hal_entry(void)
{
    setup_sci5_uart();
    uart_send_string("\r\n--- SCI5 UART READY ---\r\n");

    // Clear bus FIRST (before enabling I2C peripheral)
    uart_send_string("Clearing I2C bus...\r\n");
    i2c_force_bus_clear();
    R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);

    // Then setup I2C peripheral
    setup_i2c0();
    R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);

    // Scan I2C bus to verify sensor presence
    uart_send_string("Scanning I2C bus...\r\n");
    i2c0_scan();
    uart_send_string("\r\n");

    /* ============================================================================
     * HS3001 INITIALIZATION
     * ==========================================================================*/
    uart_send_string("=== HS3001 Initialization ===\r\n");

    if (hs3001_init())
    {
        uart_send_string("HS3001 initialized successfully!\r\n");
    }
    else
    {
        uart_send_string("HS3001 initialization FAILED!\r\n");
    }

    uart_send_string("\r\n=== Starting Data Stream ===\r\n");

    /* ============================================================================
     * MAIN LOOP - Read and process sensor data
     * ==========================================================================*/
    while(1)
    {
        float temperature_c, humidity_percent;

        if (hs3001_read(&temperature_c, &humidity_percent))
        {
            /* ========== Output JSON ========== */
            uart_send_string("{\"temperature_c\":");
            int temp_int = (int)temperature_c;
            int temp_frac = (int)((temperature_c - temp_int) * 100);
            if (temp_frac < 0) temp_frac = -temp_frac;
            uart_print_u32(temp_int);
            uart_send_char('.');
            if (temp_frac < 10) uart_send_char('0');
            uart_print_u32(temp_frac);

            uart_send_string(",\"humidity_percent\":");
            int hum_int = (int)humidity_percent;
            int hum_frac = (int)((humidity_percent - hum_int) * 100);
            if (hum_frac < 0) hum_frac = -hum_frac;
            uart_print_u32(hum_int);
            uart_send_char('.');
            if (hum_frac < 10) uart_send_char('0');
            uart_print_u32(hum_frac);

            uart_send_string("}\r\n");
        }
        else
        {
            uart_send_string("{\"error\":\"read_failed\"}\r\n");
        }

        // Wait 2 seconds before next reading
        R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
    }
}

