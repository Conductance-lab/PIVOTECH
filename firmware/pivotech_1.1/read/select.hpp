#ifndef READ_SELECT_HPP
#define READ_SELECT_HPP

// void Selector_Unenable(void);
// void Selector_Read(void);
void Selector_TA_TX_TB_RX(void);
void Selector_SPI(void);
void Selector_TA_RX_TB_TX(void);
void Selector_Read_TATB_ADC(void);
void Selector_Read_readSYS_ADC(void);
void Selector_Read_XY_ADC(void);
void Selector_AI_Free(void); // AI 测试模式默认前端态

#endif // READ_SELECT_HPP
