#ifndef _UI_HPP_
#define _UI_HPP_

#include "menu/ssd1306.hpp"
#include "Fonts/fonts.h"
#include "menu/Curve.hpp" // animation easing helpers

// UI drawing primitives exported from UI.cpp
void UIDrawSgate(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t Xlength, uint8_t Ylength, bool on, int ignorerunning = 0);
void UIDisplayStr_font12(int16_t x, int16_t y, char *pString, size_t keep_chars = 64, bool Invert = 0);
void ApplyBlurEffect(uint8_t *buf, float intensity);

// Modify this to fit your EasyKeyInit
#define KEY_UP KEYUP
#define KEY_DOWN KEYDOWN
#define KEY_FORWARD KEYRIGHT
#define KEY_BACKWARD KEYLEFT
#define KEY_CONFIRM KEYA

// extern EasyKey_t keyUp, keyDown;           // Used to control value up and down
// extern EasyKey_t keyForward, keyBackward;  // Used to control indicator movement
// extern EasyKey_t keyConfirm;               // Used to change page or call function

// Operation response
// extern uint8_t opnEnter, opnExit, opnUp, opnDown;

// High level helpers provided by UI.cpp
void UISgateinformation(void);
void UIUpdateInformation(void);
void UIUpdateSponser(void);
bool UILoadConfigUSB(void);
void testpage(void);

// Top-level UI update and navigation helpers
void UI_Update(void);
void UI_START(void);
extern bool function_to_menu;

// Reset operation flags (defined in UI.cpp)
void ResetOpn(void);

// #define ChangeVal_Width 80

#define SCREEN_WIDTH SSD1306_WIDTH
#define SCREEN_HEIGHT SSD1306_HEIGHT
// #define FONT_WIDTH              7       ///自适应参数 不进行定义
// #define FONT_HEIGHT             12        //
// #define ITEM_HEIGHT             20    /自适应参数 不进行定义
#define CHECK_BOX_OFFSET 2 // checkbox中间的空隙
#define SCROLL_BAR_WIDTH 2
#define ITEM_LINES ((uint8_t)(SCREEN_HEIGHT / ITEM_HEIGHT))
#define MAX_LAYER 8

typedef double paramType;

typedef enum
{
    ITEM_PAGE_DESCRIPTION,
    ITEM_JUMP_PAGE,
    ITEM_SWITCH,
    ITEM_CHANGE_VALUE,
    ITEM_RADIO_BUTTON,
    ITEM_CHECKBOX,
    ITEM_MESSAGE,
    ITEM_EVENT,
    ITEM_SEND,
    ITEM_INFOMATION,
    ITEM_COUNT // 自动等于枚举成员总数
} EasyUIItem_e;

typedef enum
{
    PAGE_LIST,
    PAGE_ICON,
    PAGE_CUSTOM
} EasyUIPage_e;

typedef enum
{
    UINT_e,
    INT_e,
    FLOAT_e,
    UFLOAT_e

} ValueType_e;

typedef enum
{
    NoSend,                             //  (send)  flag  value
    Send_Title,                         //  send
    Send_Value,                         //          flag  value
    Send_Title_Value,                   //          flag  value
    RealtimeSend_Value,                 //                value
    RealtimeSend_Title_Value,           //                value 
    SendOption_Title,                   //                      flaglist(flag=1)
    SendOption_Value,                   //                      flaglist    valuelist
    SendOption_Title_Value,             //                      flaglist    valuelist
    SendtoSettings,
    Send_SaveSettings,
} SendMode_e;

typedef struct EasyUI_item
{
    struct EasyUI_item *next;

    EasyUIItem_e funcType;
    uint8_t SendMode;
    uint16_t id;
    int16_t lineId;
    float posForCal;
    double step;
    int16_t position;
    char *title;

    uint8_t *icon;          // PAGE_ICON
    char *msg;              // ITEM_MESSAGE
    bool *flag;             // ITEM_CHECKBOX and ITEM_RADIO_BUTTON and ITEM_SWITCH
    bool flagDefault;       // Factory default setting
    paramType *param;       // ITEM_CHANGE_VALUE and ITEM_PROGRESS_BAR
    paramType paramDefault; // Factory default setting
    paramType paramBackup;  // ITEM_CHANGE_VALUE and ITEM_PROGRESS_BAR
    uint16_t pageId;         // ITEM_JUMP_PAGE
    uint8_t ValueType;
    void (*Event)(struct EasyUI_item *item); // ITEM_CHANGE_VALUE and ITEM_PROGRESS_BAR
} EasyUIItem_t;

typedef struct EasyUI_page
{
    struct EasyUI_page *next;

    EasyUIPage_e funcType;
    EasyUIItem_t *itemHead, *itemTail;
    uint16_t id;

    void (*Event)(struct EasyUI_page *page);
} EasyUIPage_t;

extern char *EasyUIVersion;
extern bool functionIsRunning, listLoop;
extern EasyUIPage_t *pageHead, *pageTail;

// Layer control used by UI animations
extern uint8_t layer;
extern uint8_t pageIndex[MAX_LAYER];
extern uint8_t itemIndex[MAX_LAYER];

// Animation globals defined in UI.cpp
extern int seedvalue;
extern int16_t lengthStart, heightStart, yStart, scroll_yStart, itemsYPosStart, xStart;

void UIAddItem(EasyUIPage_t *page, EasyUIItem_t *item, char *_title, EasyUIItem_e func, ...);
void UIAddPage(EasyUIPage_t *page, EasyUIPage_e func, ...);
// void EasyUITransitionAnim();
// void EasyUIBackgroundBlur();
// void EasyUIKeyActionMonitor();

// void EasyUIDrawMsgBox(char *msg);
// float EasyUIGetBatteryVoltage();

// void EasyUIEventChangeUint(EasyUIItem_t *item);
// void EasyUIEventChangeInt(EasyUIItem_t *item);
// void EasyUIEventChangeFloat(EasyUIItem_t *item);
// void EasyUIEventSaveSettings(EasyUIItem_t *item);
// void EasyUIEventResetSettings(EasyUIItem_t *item);

// void EasyUIInit(uint8_t mode);
// void EasyUI(uint8_t timer);
void MenuConfigInit();
void MenuReset();

#endif
