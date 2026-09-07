#ifndef READ_ADCREAD_HPP
#define READ_ADCREAD_HPP



void ADCReadInit(void);
int ReadADCChannel(int channel);

// ADC helper result
typedef struct {
    uint16_t ta;
    uint16_t tb;
} ADCReadResult;

ADCReadResult adc_read_refresh(bool if_wait = true);
float read_adc_voltage(uint16_t adc_value);


// Pin open state for TA/TB detection
enum testpin_openstate
{
    PINSTATE_OPEN,
    PINSTATE_HIGH,
    PINSTATE_LOW
};

struct testpin_state
{
    testpin_openstate ta_pinstate;
    testpin_openstate tb_pinstate;
};

// Return current test pin state
testpin_state checkpin();



#endif // READ_ADCREAD_HPP