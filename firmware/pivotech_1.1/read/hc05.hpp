#ifndef READ_HC05_HPP
#define READ_HC05_HPP

#ifdef __cplusplus
extern "C" {
#endif

void hc05_at_mode(void);
int hc05_get_mode_code(void);
int hc05_get_modal_code(void);
int hc05_get_name_suffix_value(void);
const char *hc05_get_name_text_value(void);
const char *hc05_get_pass_selection_value(void);
int hc05_get_has_atmode_dev_value(void);
int hc05_get_last_cfg_ok_value(void);
int hc05_get_ms_s_verified_value(void);
int hc05_get_ms_m_verified_value(void);
int hc05_get_s_mode_result_ok_value(void);
int hc05_get_s_mode_result_fail_value(void);
int hc05_get_ms_s_result_ok_value(void);
int hc05_get_ms_s_result_fail_value(void);
int hc05_get_ms_m_result_ok_value(void);
int hc05_get_ms_m_result_fail_value(void);
void hc05_set_protocol_config(int mode, const char *name, const char *password, int action);

#ifdef __cplusplus
}
#endif

#endif // READ_HC05_HPP
