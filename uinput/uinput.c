/* 2013
 * Maciej Szeptuch (Neverous) <neverous@neverous.info>
 *
 * Uinput access functions.
 *
 * TODO:
 *  - Mouse wheel (REL_WHEEL are not forwarded now??, what am i missing)
 *  - Mouse relative movement(probably need separate devices since xorg support only abs or rel per device not both)
 *  - Gamepad support?
 */

#include <linux/uinput.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "uinput.h"
#include "keys.h"

static int keyTranslation[65535] = {0};
static void build_key_translation_table ();

int32_t uInitializeMouse(uint16_t width, uint16_t height)
{
    int32_t device = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(device < 0)
    {
        perror("Cannot open");
        return -1;
    }

    if(ioctl(device, UI_SET_EVBIT, EV_SYN) < 0) {perror("IOCTL error"); close(device); return -1;}
    if(ioctl(device, UI_SET_EVBIT, EV_KEY) < 0) {perror("IOCTL error"); close(device); return -1;}
    if(ioctl(device, UI_SET_EVBIT, EV_ABS) < 0) {perror("IOCTL error"); close(device); return -1;}
    if(ioctl(device, UI_SET_EVBIT, EV_REL) < 0) {perror("IOCTL error"); close(device); return -1;}

    // MOUSE SUPPORT
    if(ioctl(device, UI_SET_KEYBIT, BTN_MOUSE) < 0) {perror("IOCTL error"); close(device); return -1;}
    if(ioctl(device, UI_SET_RELBIT, REL_WHEEL) < 0) {perror("IOCTL error"); close(device); return -1;}
    if(ioctl(device, UI_SET_RELBIT, REL_HWHEEL) < 0) {perror("IOCTL error"); close(device); return -1;}

    for(uint32_t b = 0, buttons = sizeof(BTN_MAP) / sizeof(BTN_MAP[0]); b < buttons; ++ b)
        if(ioctl(device, UI_SET_KEYBIT, BTN_MAP[b]) < 0) {perror("IOCTL error"); close(device); return -1;}

    if(ioctl(device, UI_SET_ABSBIT, ABS_X) < 0) {perror("IOCTL error"); close(device); return -1;}
    if(ioctl(device, UI_SET_ABSBIT, ABS_Y) < 0) {perror("IOCTL error"); close(device); return -1;}
    if(ioctl(device, UI_SET_RELBIT, REL_X) < 0) {perror("IOCTL error"); close(device); return -1;}
    if(ioctl(device, UI_SET_RELBIT, REL_Y) < 0) {perror("IOCTL error"); close(device); return -1;}

    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "synergyMouse");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x1;
    uidev.id.product = 0x1;
    uidev.id.version = 1;

    uidev.absmax[ABS_X] = width;
    uidev.absmax[ABS_Y] = height;

    if(write(device, &uidev, sizeof(uidev)) < 0) {perror("Write error"); close(device); return -1;}
    if(ioctl(device, UI_DEV_CREATE) < 0) {perror("IOCTL error"); close(device); return -1;}

    sleep(2);
    return device;
}

int32_t uInitializeKeyboard(void)
{
    int32_t device = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(device < 0)
    {
        perror("Cannot open");
        return -1;
    }

    if(ioctl(device, UI_SET_EVBIT, EV_SYN) < 0) {perror("IOCTL error"); close(device); return -1;}
    if(ioctl(device, UI_SET_EVBIT, EV_KEY) < 0) {perror("IOCTL error"); close(device); return -1;}
    if(ioctl(device, UI_SET_EVBIT, EV_MSC) < 0) {perror("IOCTL error"); close(device); return -1;}

    // KEYBOARD SUPPORT
    for(uint32_t k = 0; k < 256; ++ k)
        if(ioctl(device, UI_SET_KEYBIT, KEY_RESERVED + k) < 0) {perror("IOCTL error"); close(device); return -1;}

    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "synergyKeyboard");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x1;
    uidev.id.product = 0x2;
    uidev.id.version = 1;

    if(write(device, &uidev, sizeof(uidev)) < 0) {perror("Write error"); close(device); return -1;}
    if(ioctl(device, UI_DEV_CREATE) < 0) {perror("IOCTL error"); close(device); return -1;}

    sleep(2);
    return device;
}

void uClose(int32_t device)
{
    if(device < 0)
        return;

    ioctl(device, UI_DEV_DESTROY);
    close(device);
}

void uSync(int32_t device)
{
    if(device < 0)
        return;

    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    if(write(device, &ev, sizeof(ev)) < 0)
        perror("Write error");
}

void uMouseMotion(int32_t device, const uint16_t x, const uint16_t y)
{
    if(device < 0)
        return;

    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_ABS;
    ev.code = ABS_X;
    ev.value = x;
    if(write(device, &ev, sizeof(ev)) < 0)
        perror("Write error");

    ev.code = ABS_Y;
    ev.value = y;
    if(write(device, &ev, sizeof(ev)) < 0)
        perror("Write error");

    uSync(device);
}

void uMouseRelativeMotion(int32_t device, const int16_t dx, const int16_t dy)
{
    if(device < 0)
        return;

    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_REL;
    if(dx)
    {
        ev.code = REL_X;
        ev.value = dx;
        if(write(device, &ev, sizeof(ev)) < 0)
            perror("Write error");

        uSync(device);
    }
    
    if(dy)
    {
        ev.code = REL_Y;
        ev.value = dy;
        if(write(device, &ev, sizeof(ev)) < 0)
            perror("Write error");

        uSync(device);
    }
}

void uMouseWheel(int32_t device, const int16_t dx, const int16_t dy)
{
    if(device < 0)
        return;

    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_REL;
    if(dx)
    {
        ev.code = REL_HWHEEL;
        ev.value = dx > 0 ? 1 : -1;
        if(write(device, &ev, sizeof(ev)) < 0)
            perror("Write error");

        uSync(device);
    }

    if(dy)
    {
        ev.code = REL_WHEEL;
        ev.value = dy > 0 ? 1 : -1;
        if(write(device, &ev, sizeof(ev)) < 0)
            perror("Write error");
        //fprintf(stderr, "write type = %d, code = %d, value = %d\n", ev.type, ev.code, ev.value);

        uSync(device);
    }
}

void uMouseButton(int32_t device, const uint8_t button, const uint8_t state)
{
    if(device < 0)
        return;

    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_KEY;
    ev.code = BTN_MAP[button];
    ev.value = state;
    if(write(device, &ev, sizeof(ev)) < 0)
        perror("Write error");

    uSync(device);
}

void uKey(int32_t device, const uint16_t key, const uint8_t state)
{
    if(device < 0)
        return;

    if (keyTranslation[0] == 0)
        build_key_translation_table();

    if (keyTranslation[key] == -1)
    {
        fprintf(stderr, "key %d translation error\n", key);
        return;
    }

    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_KEY;
    ev.code = keyTranslation[key];
    ev.value = state;
    if(write(device, &ev, sizeof(ev)) < 0)
        perror("Write error");

    uSync(device);
}

// src: https://github.com/symless/synergy-core/blob/master/src/lib/synergy/key_types.h {
typedef uint16_t KeyID;

// no key
static const KeyID		kKeyNone		= 0x0000;

// TTY functions
static const KeyID		kKeyBackSpace	= 0xEF08;	/* back space, back char */
static const KeyID		kKeyTab			= 0xEF09;
static const KeyID		kKeyLinefeed	= 0xEF0A;	/* Linefeed, LF */
static const KeyID		kKeyClear		= 0xEF0B;
static const KeyID		kKeyReturn		= 0xEF0D;	/* Return, enter */
static const KeyID		kKeyPause		= 0xEF13;	/* Pause, hold */
static const KeyID		kKeyScrollLock	= 0xEF14;
static const KeyID		kKeySysReq		= 0xEF15;
static const KeyID		kKeyEscape		= 0xEF1B;
static const KeyID		kKeyHenkan		= 0xEF23;	/* Start/Stop Conversion */
static const KeyID		kKeyKana		= 0xEF26;	/* Kana */
static const KeyID		kKeyHiraganaKatakana = 0xEF27;	/* Hiragana/Katakana toggle */
static const KeyID		kKeyZenkaku		= 0xEF2A;	/* Zenkaku/Hankaku */
static const KeyID		kKeyKanzi		= 0xEF2A;	/* Kanzi */
static const KeyID		kKeyHangul		= 0xEF31;	/* Hangul */
static const KeyID		kKeyHanja		= 0xEF34;	/* Hanja */
static const KeyID		kKeyDelete		= 0xEFFF;	/* Delete, rubout */

// cursor control
static const KeyID		kKeyHome		= 0xEF50;
static const KeyID		kKeyLeft		= 0xEF51;	/* Move left, left arrow */
static const KeyID		kKeyUp			= 0xEF52;	/* Move up, up arrow */
static const KeyID		kKeyRight		= 0xEF53;	/* Move right, right arrow */
static const KeyID		kKeyDown		= 0xEF54;	/* Move down, down arrow */
static const KeyID		kKeyPageUp		= 0xEF55;
static const KeyID		kKeyPageDown	= 0xEF56;
static const KeyID		kKeyEnd			= 0xEF57;	/* EOL */
static const KeyID		kKeyBegin		= 0xEF58;	/* BOL */

// misc functions
static const KeyID		kKeySelect		= 0xEF60;	/* Select, mark */
static const KeyID		kKeyPrint		= 0xEF61;
static const KeyID		kKeyExecute		= 0xEF62;	/* Execute, run, do */
static const KeyID		kKeyInsert		= 0xEF63;	/* Insert, insert here */
static const KeyID		kKeyUndo		= 0xEF65;	/* Undo, oops */
static const KeyID		kKeyRedo		= 0xEF66;	/* redo, again */
static const KeyID		kKeyMenu		= 0xEF67;
static const KeyID		kKeyFind		= 0xEF68;	/* Find, search */
static const KeyID		kKeyCancel		= 0xEF69;	/* Cancel, stop, abort, exit */
static const KeyID		kKeyHelp		= 0xEF6A;	/* Help */
static const KeyID		kKeyBreak		= 0xEF6B;
static const KeyID		kKeyAltGr	 	= 0xEF7E;	/* Character set switch */
static const KeyID		kKeyNumLock		= 0xEF7F;

// keypad
static const KeyID		kKeyKP_Space	= 0xEF80;	/* space */
static const KeyID		kKeyKP_Tab		= 0xEF89;
static const KeyID		kKeyKP_Enter	= 0xEF8D;	/* enter */
static const KeyID		kKeyKP_F1		= 0xEF91;	/* PF1, KP_A, ... */
static const KeyID		kKeyKP_F2		= 0xEF92;
static const KeyID		kKeyKP_F3		= 0xEF93;
static const KeyID		kKeyKP_F4		= 0xEF94;
static const KeyID		kKeyKP_Home		= 0xEF95;
static const KeyID		kKeyKP_Left		= 0xEF96;
static const KeyID		kKeyKP_Up		= 0xEF97;
static const KeyID		kKeyKP_Right	= 0xEF98;
static const KeyID		kKeyKP_Down		= 0xEF99;
static const KeyID		kKeyKP_PageUp	= 0xEF9A;
static const KeyID		kKeyKP_PageDown	= 0xEF9B;
static const KeyID		kKeyKP_End		= 0xEF9C;
static const KeyID		kKeyKP_Begin	= 0xEF9D;
static const KeyID		kKeyKP_Insert	= 0xEF9E;
static const KeyID		kKeyKP_Delete	= 0xEF9F;
static const KeyID		kKeyKP_Equal	= 0xEFBD;	/* equals */
static const KeyID		kKeyKP_Multiply	= 0xEFAA;
static const KeyID		kKeyKP_Add		= 0xEFAB;
static const KeyID		kKeyKP_Separator= 0xEFAC;	/* separator, often comma */
static const KeyID		kKeyKP_Subtract	= 0xEFAD;
static const KeyID		kKeyKP_Decimal	= 0xEFAE;
static const KeyID		kKeyKP_Divide	= 0xEFAF;
static const KeyID		kKeyKP_0		= 0xEFB0;
static const KeyID		kKeyKP_1		= 0xEFB1;
static const KeyID		kKeyKP_2		= 0xEFB2;
static const KeyID		kKeyKP_3		= 0xEFB3;
static const KeyID		kKeyKP_4		= 0xEFB4;
static const KeyID		kKeyKP_5		= 0xEFB5;
static const KeyID		kKeyKP_6		= 0xEFB6;
static const KeyID		kKeyKP_7		= 0xEFB7;
static const KeyID		kKeyKP_8		= 0xEFB8;
static const KeyID		kKeyKP_9		= 0xEFB9;

// function keys
static const KeyID		kKeyF1			= 0xEFBE;
static const KeyID		kKeyF2			= 0xEFBF;
static const KeyID		kKeyF3			= 0xEFC0;
static const KeyID		kKeyF4			= 0xEFC1;
static const KeyID		kKeyF5			= 0xEFC2;
static const KeyID		kKeyF6			= 0xEFC3;
static const KeyID		kKeyF7			= 0xEFC4;
static const KeyID		kKeyF8			= 0xEFC5;
static const KeyID		kKeyF9			= 0xEFC6;
static const KeyID		kKeyF10			= 0xEFC7;
static const KeyID		kKeyF11			= 0xEFC8;
static const KeyID		kKeyF12			= 0xEFC9;
static const KeyID		kKeyF13			= 0xEFCA;
static const KeyID		kKeyF14			= 0xEFCB;
static const KeyID		kKeyF15			= 0xEFCC;
static const KeyID		kKeyF16			= 0xEFCD;
static const KeyID		kKeyF17			= 0xEFCE;
static const KeyID		kKeyF18			= 0xEFCF;
static const KeyID		kKeyF19			= 0xEFD0;
static const KeyID		kKeyF20			= 0xEFD1;
static const KeyID		kKeyF21			= 0xEFD2;
static const KeyID		kKeyF22			= 0xEFD3;
static const KeyID		kKeyF23			= 0xEFD4;
static const KeyID		kKeyF24			= 0xEFD5;
static const KeyID		kKeyF25			= 0xEFD6;
static const KeyID		kKeyF26			= 0xEFD7;
static const KeyID		kKeyF27			= 0xEFD8;
static const KeyID		kKeyF28			= 0xEFD9;
static const KeyID		kKeyF29			= 0xEFDA;
static const KeyID		kKeyF30			= 0xEFDB;
static const KeyID		kKeyF31			= 0xEFDC;
static const KeyID		kKeyF32			= 0xEFDD;
static const KeyID		kKeyF33			= 0xEFDE;
static const KeyID		kKeyF34			= 0xEFDF;
static const KeyID		kKeyF35			= 0xEFE0;

// modifiers
static const KeyID		kKeyShift_L		= 0xEFE1;	/* Left shift */
static const KeyID		kKeyShift_R		= 0xEFE2;	/* Right shift */
static const KeyID		kKeyControl_L	= 0xEFE3;	/* Left control */
static const KeyID		kKeyControl_R	= 0xEFE4;	/* Right control */
static const KeyID		kKeyCapsLock	= 0xEFE5;	/* Caps lock */
static const KeyID		kKeyShiftLock	= 0xEFE6;	/* Shift lock */
static const KeyID		kKeyMeta_L		= 0xEFE7;	/* Left meta */
static const KeyID		kKeyMeta_R		= 0xEFE8;	/* Right meta */
static const KeyID		kKeyAlt_L		= 0xEFE9;	/* Left alt */
static const KeyID		kKeyAlt_R		= 0xEFEA;	/* Right alt */
static const KeyID		kKeySuper_L		= 0xEFEB;	/* Left super */
static const KeyID		kKeySuper_R		= 0xEFEC;	/* Right super */
static const KeyID		kKeyHyper_L		= 0xEFED;	/* Left hyper */
static const KeyID		kKeyHyper_R		= 0xEFEE;	/* Right hyper */

// multi-key character composition
static const KeyID		kKeyCompose			= 0xEF20;
static const KeyID		kKeyDeadGrave		= 0x0300;
static const KeyID		kKeyDeadAcute		= 0x0301;
static const KeyID		kKeyDeadCircumflex	= 0x0302;
static const KeyID		kKeyDeadTilde		= 0x0303;
static const KeyID		kKeyDeadMacron		= 0x0304;
static const KeyID		kKeyDeadBreve		= 0x0306;
static const KeyID		kKeyDeadAbovedot	= 0x0307;
static const KeyID		kKeyDeadDiaeresis	= 0x0308;
static const KeyID		kKeyDeadAbovering	= 0x030a;
static const KeyID		kKeyDeadDoubleacute	= 0x030b;
static const KeyID		kKeyDeadCaron		= 0x030c;
static const KeyID		kKeyDeadCedilla		= 0x0327;
static const KeyID		kKeyDeadOgonek		= 0x0328;

// more function and modifier keys
static const KeyID		kKeyLeftTab			= 0xEE20;

// update modifiers
static const KeyID		kKeySetModifiers	= 0xEE06;
static const KeyID		kKeyClearModifiers	= 0xEE07;

// group change
static const KeyID		kKeyNextGroup		= 0xEE08;
static const KeyID		kKeyPrevGroup		= 0xEE0A;

// extended keys
static const KeyID		kKeyEject			= 0xE001;
static const KeyID		kKeySleep			= 0xE05F;
static const KeyID		kKeyWWWBack			= 0xE0A6;
static const KeyID		kKeyWWWForward		= 0xE0A7;
static const KeyID		kKeyWWWRefresh		= 0xE0A8;
static const KeyID		kKeyWWWStop			= 0xE0A9;
static const KeyID		kKeyWWWSearch		= 0xE0AA;
static const KeyID		kKeyWWWFavorites	= 0xE0AB;
static const KeyID		kKeyWWWHome			= 0xE0AC;
static const KeyID		kKeyAudioMute		= 0xE0AD;
static const KeyID		kKeyAudioDown		= 0xE0AE;
static const KeyID		kKeyAudioUp			= 0xE0AF;
static const KeyID		kKeyAudioNext		= 0xE0B0;
static const KeyID		kKeyAudioPrev		= 0xE0B1;
static const KeyID		kKeyAudioStop		= 0xE0B2;
static const KeyID		kKeyAudioPlay		= 0xE0B3;
static const KeyID		kKeyAppMail			= 0xE0B4;
static const KeyID		kKeyAppMedia		= 0xE0B5;
static const KeyID		kKeyAppUser1		= 0xE0B6;
static const KeyID		kKeyAppUser2		= 0xE0B7;
static const KeyID		kKeyBrightnessDown	= 0xE0B8;
static const KeyID		kKeyBrightnessUp	= 0xE0B9;
static const KeyID		kKeyMissionControl	= 0xE0C0;
static const KeyID		kKeyLaunchpad		= 0xE0C1;
// }

// src: https://github.com/symless/synergy-android-7/blob/master/app/src/main/jni/synergy-jni.c {
static void build_key_translation_table () {
    int i = 0;
    for (i = 0; i < 65535; i++) {
        keyTranslation [i] = -1;
    }

    keyTranslation ['a'] = KEY_A;
    keyTranslation ['b'] = KEY_B;
    keyTranslation ['c'] = KEY_C;
    keyTranslation ['d'] = KEY_D;
    keyTranslation ['e'] = KEY_E;
    keyTranslation ['f'] = KEY_F;
    keyTranslation ['g'] = KEY_G;
    keyTranslation ['h'] = KEY_H;
    keyTranslation ['i'] = KEY_I;
    keyTranslation ['j'] = KEY_J;
    keyTranslation ['k'] = KEY_K;
    keyTranslation ['l'] = KEY_L;
    keyTranslation ['m'] = KEY_M;
    keyTranslation ['n'] = KEY_N;
    keyTranslation ['o'] = KEY_O;
    keyTranslation ['p'] = KEY_P;
    keyTranslation ['q'] = KEY_Q;
    keyTranslation ['r'] = KEY_R;
    keyTranslation ['s'] = KEY_S;
    keyTranslation ['t'] = KEY_T;
    keyTranslation ['u'] = KEY_U;
    keyTranslation ['v'] = KEY_V;
    keyTranslation ['w'] = KEY_W;
    keyTranslation ['x'] = KEY_X;
    keyTranslation ['y'] = KEY_Y;
    keyTranslation ['z'] = KEY_Z;

    keyTranslation ['A'] = KEY_A;
    keyTranslation ['B'] = KEY_B;
    keyTranslation ['C'] = KEY_C;
    keyTranslation ['D'] = KEY_D;
    keyTranslation ['E'] = KEY_E;
    keyTranslation ['F'] = KEY_F;
    keyTranslation ['G'] = KEY_G;
    keyTranslation ['H'] = KEY_H;
    keyTranslation ['I'] = KEY_I;
    keyTranslation ['J'] = KEY_J;
    keyTranslation ['K'] = KEY_K;
    keyTranslation ['L'] = KEY_L;
    keyTranslation ['M'] = KEY_M;
    keyTranslation ['N'] = KEY_N;
    keyTranslation ['O'] = KEY_O;
    keyTranslation ['P'] = KEY_P;
    keyTranslation ['Q'] = KEY_Q;
    keyTranslation ['R'] = KEY_R;
    keyTranslation ['S'] = KEY_S;
    keyTranslation ['T'] = KEY_T;
    keyTranslation ['U'] = KEY_U;
    keyTranslation ['V'] = KEY_V;
    keyTranslation ['W'] = KEY_W;
    keyTranslation ['X'] = KEY_X;
    keyTranslation ['Y'] = KEY_Y;
    keyTranslation ['Z'] = KEY_Z;

    keyTranslation ['0'] = KEY_0;
    keyTranslation ['1'] = KEY_1;
    keyTranslation ['2'] = KEY_2;
    keyTranslation ['3'] = KEY_3;
    keyTranslation ['4'] = KEY_4;
    keyTranslation ['5'] = KEY_5;
    keyTranslation ['6'] = KEY_6;
    keyTranslation ['7'] = KEY_7;
    keyTranslation ['8'] = KEY_8;
    keyTranslation ['9'] = KEY_9;

    /**
     * this is definitely not the correct way to handle
     *  the shift key... this should really be done with
     *  the mask...
     */

	keyTranslation ['!'] = KEY_1;
	keyTranslation ['@'] = KEY_2;
	keyTranslation ['#'] = KEY_3;
	keyTranslation ['$'] = KEY_4;
	keyTranslation ['%'] = KEY_5;
	keyTranslation ['^'] = KEY_6;
	keyTranslation ['&'] = KEY_7;
	keyTranslation ['*'] = KEY_8;
	keyTranslation ['('] = KEY_9;
	keyTranslation [')'] = KEY_0;
	keyTranslation ['-'] = KEY_MINUS;
	keyTranslation ['_'] = KEY_MINUS;
	keyTranslation ['='] = KEY_EQUAL;
	keyTranslation ['+'] = KEY_EQUAL;
	keyTranslation ['?'] = KEY_SLASH;
	keyTranslation ['/'] = KEY_SLASH;
	keyTranslation ['.'] = KEY_DOT;
	keyTranslation ['>'] = KEY_DOT;
	keyTranslation [','] = KEY_COMMA;
	keyTranslation ['<'] = KEY_COMMA;


    keyTranslation [61192] = KEY_BACKSPACE;
    keyTranslation [8] = KEY_BACKSPACE;

    keyTranslation [61197] = KEY_ENTER;
    keyTranslation [10] = KEY_ENTER;
    keyTranslation [13] = KEY_ENTER;
    keyTranslation [32] = KEY_SPACE;

    // Arrows
    keyTranslation [61265] = KEY_LEFT;
    keyTranslation [61266] = KEY_UP;
    keyTranslation [61267] = KEY_RIGHT;
    keyTranslation [61268] = KEY_DOWN;

    keyTranslation [kKeyTab] = KEY_TAB;

    keyTranslation [kKeyShift_R] = KEY_RIGHTSHIFT;
    keyTranslation [kKeyShift_L] = KEY_LEFTSHIFT;
    keyTranslation [kKeySuper_R] = KEY_RIGHTMETA;
    keyTranslation [kKeySuper_L] = KEY_LEFTMETA;
    keyTranslation [kKeyAlt_R] = KEY_RIGHTALT;
    keyTranslation [kKeyAlt_L] = KEY_LEFTALT;
    keyTranslation [kKeyControl_R] = KEY_RIGHTCTRL;
    keyTranslation [kKeyControl_L] = KEY_LEFTCTRL;

    keyTranslation [27] = KEY_ESC;
    keyTranslation [kKeyEscape] = KEY_ESC;
    keyTranslation [kKeyHome] = KEY_HOME;
    keyTranslation [kKeyEnd] = KEY_END;
    keyTranslation [kKeyInsert] = KEY_INSERT;
    keyTranslation [kKeyDelete] = KEY_DELETE;
    keyTranslation [kKeyPageUp] = KEY_PAGEUP;
    keyTranslation [kKeyPageDown] = KEY_PAGEDOWN;

    for (int i = 0; i < 10; ++i)
        keyTranslation [kKeyF1 + i] = KEY_F1 + i;
    keyTranslation [kKeyF11] = KEY_F11;
    keyTranslation [kKeyF12] = KEY_F12;
}
// }
