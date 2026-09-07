#ifndef MENU_SEND_HPP
#define MENU_SEND_HPP

#include "menu/menu.hpp"

void update_backstring_SendOpention_OnlySelected(EasyUIPage_t *page);
void update_backstring_SendOpention_all(EasyUIPage_t *page, bool if_have_title = 0);

// Function exposed by menu/send.cpp
void SendMessage(EasyUIItem_t *item, EasyUIPage_t *page);

extern char *backString;
extern bool core1_need_wait;

#endif // MENU_SEND_HPP
