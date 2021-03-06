#ifndef _COMMON__H_
#define _COMMON__H_

#include "ppu.h"
#include "integer.h"

// The default header that comes with SNESC specifies a LOROM configuration. Remove this define if HIROM is used.
//#define LOROM

#ifdef LOROM
#define GAME_LIST_BANK 1
#else
#define GAME_LIST_BANK 0
#endif

#define LAST_GAME_MAGIC 			  0x4C617354
#define LAST_GAME_SRAM_BANK 		  0x03
#define LAST_GAME_SRAM_OFFS 		  0xF800
#define LAST_GAME_SRAM_BACKUP_PENDING 0xBAAD
#define LAST_GAME_SRAM_BACKUP_DONE    0x5786

#define RAM_CODE_BANK 0x7e
#define RAM_CODE_OFFSET 0x8000


// Offset a function pointer so that it points to a copy of the function in RAM
#define MAKE_RAM_FPTR(fptr, fun) fptr = fun & 0x7fff; add_full_pointer((void**)&fptr, RAM_CODE_BANK-1, RAM_CODE_OFFSET)
#define MAKE_PSRAM_FPTR(fptr, fun) fptr = fun & 0x7fff; add_full_pointer((void**)&fptr, 0xdf-1, RAM_CODE_OFFSET)

// Maximum number of Game Genie codes allowed
#define MAX_GG_CODES 4
// Metastring IDs
#define MS_VERSION_COPYRIGHT		    0
#define MS_GAME_NUMBER				    4
#define MS_MAIN_MENU_INSTRUCTIONS	   75
#define MS_GG_ENTRY_MENU_INSTRUCTIONS  76
#define MS_GG_EDIT_MENU_INSTRUCTIONS   77
#define MS_ROM_INFO_MENU_INSTRUCTIONS  80
#define MS_SD_ERROR_MENU_INSTRUCTIONS  81
#define MS_SD_INFO_MENU_INSTRUCTIONS   82
#define MS_VGM_PLAY_MENU_INSTRUCTIONS  83
#define MS_CART_TEST_MENU_INSTRUCTIONS 84
#define MS_DUMP_MENU_INSTRUCTIONS      85

// Masks for the joypad data returned by read_joypad
#define JOY_R 		0x0010
#define JOY_L 		0x0020
#define JOY_X		0x0040
#define JOY_A 		0x0080
#define JOY_RIGHT 	0x0100
#define JOY_LEFT 	0x0200
#define JOY_DOWN 	0x0400
#define JOY_UP 		0x0800
#define JOY_START	0x1000
#define JOY_SELECT 	0x2000
#define JOY_Y		0x4000
#define JOY_B 		0x8000

#define SHELL_BGPAL_WHITE       2
#define SHELL_BGPAL_LIGHT_GREEN 3
#define SHELL_BGPAL_TOS_GREEN   4
#define SHELL_BGPAL_LIGHT_BLUE  5
#define SHELL_BGPAL_DARK_OLIVE  6
#define SHELL_BGPAL_OLIVE 		7
#define SHELL_OBJPAL_DARK_OLIVE 0
#define SHELL_OBJPAL_WHITE      1


#define KEY_REPEAT_INITIAL_DELAY 32
#define KEY_REPEAT_DELAY 7


#define GAME_MODE_NORMAL_ROM  4
#define GAME_MODE_SPC 		 32
#define GAME_MODE_VGM 		 33
#define GAME_MODE_PACKED_VGM 34
#define GAME_MODE_ZIPPED_ROM 35
#define GAME_MODE_VGZ 		 36

////////////////////////////////////////////////////////////////////////////////

enum
{
	SD_OP_MOUNT,
	SD_OP_OPEN_FILE,
	SD_OP_READ_FILE,
	SD_OP_WRITE_FILE,
	SD_OP_SEEK,
	SD_OP_OPEN_DIR,
	SD_OP_READ_DIR,
	SD_OP_UNKNOWN
};
enum
{
	DUMP_ROM=0,
	DUMP_SRAM=1,
	DUMP_NEO_SRAM=2
};

enum
{
	HIGHLIGHTED_GAME,
	LAST_PLAYED_GAME
};

typedef struct
{
	u16 count;
	u16 firstShown;
	u16 highlighted;
} itemList_t;

typedef struct
{
    char sfn[13];
    char lfn[32];
    DWORD fsize;
    BYTE fattrib;
    BYTE ftype;
} fileInfoTable_t;

typedef struct
{
	DWORD magic;
    char sfnAbs[256];
    DWORD fsize;
    WORD sramSize;
    WORD sramMode;
    WORD sramBank;
    WORD gameMode;
    WORD dspMode;
    WORD extSramMode;
    WORD sramStatus;
} lastPlayedGame_t;

typedef struct
{
	u16 x1, y1;
	u16 x2, y2;
} rect_t;

// For use with can_games_list_scroll
typedef enum
{
	DIRECTION_UP,
	DIRECTION_DOWN
} scrollDirection_t;

typedef enum
{
	SORT_LOGICALLY,
	SORT_ALPHABETICALLY
} sortOrder_t;

typedef enum
{
	SOURCE_GBAC = 0,
	SOURCE_SD
} sourceMedium_t;

typedef enum
{
    LAYOUT_LOROM = 0,
    LAYOUT_HIROM = 1,
    LAYOUT_UNKNOWN = 2
} romLayout_t;

////////////////////////////////////////////////////////////////////////////////

extern u8           romSize, romRunMode, sramSize, sramBank, sramMode;
extern u8           extDsp, extSram;
extern itemList_t   gamesList;
extern itemList_t   cheatList;
extern char         MS4[];
extern const char * const metaStrings[];
extern u16          gbaCardAlphabeticalIdx[500];
extern u8           cheatApplied[128];
extern sortOrder_t  sortOrder;
extern u8           freeCodeSlots;
extern u8           snesRomInfo[0x40];
extern u8           doRegionPatch;
extern int          sramBankOverride;
extern u8           resetType;
extern u16          hasGbacPsram,useGbacPsram;
extern oamEntry_t   marker;
extern sourceMedium_t sourceMedium;
extern char         loadProgress[];
extern DWORD        compressedVgmSize;
extern DWORD        vgzSize;
extern u8           compressVgmBuffer[512];
extern u16          isVgz;
extern u8           diskioPacket[7];
extern u8           diskioResp[17];
extern void         (*keypress_handler)(u16);
extern void         (*recv_sd_psram_multi)(WORD, WORD, WORD);
extern unsigned char sec_buf[520];
extern u16          highlightedIsDir;
extern char         highlightedFileName[100];
extern long long    highlightedFileSize;
extern u8 			sdLoaderMemSel;
extern u8 			useSdAutoBuffer;
extern u8 			allowLastGameSave;
extern lastPlayedGame_t lastPlayedGame;
extern u8 			dumpType;
extern u8 			romDumpLayout;
extern u8 			romDumpSize;
extern u8 			sramDumpAddr;
extern u8 			sramDumpSize;
extern u8 			neoSramDumpAddr;
extern u8 			neoSramDumpSize;

////////////////////////////////////////////////////////////////////////////////

extern void set_full_pointer(void **, u8, u16);
extern void add_full_pointer(void **, u8, u16);
extern void wait_nmi();
extern void update_screen();
extern void run_game_from_gba_card_c();
extern void play_spc_from_gba_card_c();
extern void run_secondary_cart_c();
extern u16 count_games_on_gba_card();
extern void clear_screen();
extern void clear_status_window();
extern void hide_games_list();
extern void hide_cheat_list();
extern void print_meta_string(u16);
extern void print_games_list();
extern void print_cheat_list();
extern void print_hw_card_rev();
extern void set_printxy_clip_rect(u16 x1, u16 y1, u16 x2, u16 y2);
extern void show_scroll_indicators();
extern void update_game_params(int force);
romLayout_t get_rom_info_sd(char *fname, u8 *romInfo);
romLayout_t get_rom_info_psram(char *fname, u8 *romInfo);
extern void run_game_from_sd_card_c(int whichGame);
extern int strstri(char *lookFor, char *lookIn);

#endif


