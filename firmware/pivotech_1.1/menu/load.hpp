#ifndef MENU_LOAD_HPP
#define MENU_LOAD_HPP

#include "CONFIG_FLO.hpp"
#include "menu/flash.hpp"
#include "menu/SGateSetting.hpp"

bool loadrequset(bool source_1UART_0USB);

extern bool if_load_state;
extern bool if_receive_massage;
extern char* current_configdata;

#endif // MENU_LOAD_HPP
