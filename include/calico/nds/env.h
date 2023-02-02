#pragma once
#include "../types.h"
#include "mm_env.h"

#define g_envAppNdsHeader  ((EnvNdsHeader*)         MM_ENV_APP_NDS_HEADER)
#define g_envCardNdsHeader ((EnvNdsHeader*)         MM_ENV_CARD_NDS_HEADER)
#define g_envAppTwlHeader  ((EnvTwlHeader*)         MM_ENV_APP_TWL_HEADER)
#define g_envCardTwlHeader ((EnvTwlHeader*)         MM_ENV_CARD_TWL_HEADER)
#define g_envNdsArgvHeader ((EnvNdsArgvHeader*)     MM_ENV_ARGV_HEADER)
#define g_envNdsBootstub   ((EnvNdsBootstubHeader*) MM_ENV_HB_BOOTSTUB)
#define g_envUserSettings  ((EnvUserSettings*)      MM_ENV_USER_SETTINGS)
#define g_envTwlDeviceList ((EnvTwlDeviceList*)     MM_ENV_TWL_DEVICE_LIST)

typedef struct EnvNdsHeader {
	char title[12];
	char gamecode[4];
	char makercode[2];
	u8 unitcode;
	u8 encryption_seed_select;
	u8 device_capacity;
	u8 _pad_0x15[7];
	u8 twl_flags;
	u8 ntr_region_flags;
	u8 rom_version;
	u8 ntr_flags;

	u32 arm9_rom_offset;
	u32 arm9_entrypoint;
	u32 arm9_ram_address;
	u32 arm9_size;

	u32 arm7_rom_offset;
	u32 arm7_entrypoint;
	u32 arm7_ram_address;
	u32 arm7_size;

	u32 fnt_rom_offset;
	u32 fnt_size;
	u32 fat_rom_offset;
	u32 fat_size;
	u32 arm9_ovl_rom_offset;
	u32 arm9_ovl_size;
	u32 arm7_ovl_rom_offset;
	u32 arm7_ovl_size;

	u32 cardcnt_normal;
	u32 cardcnt_key1;

	u32 banner_rom_offset;

	u16 secure_area_crc16;
	u16 secure_area_delay;

	u32 arm9_loadlist_hook;
	u32 arm7_loadlist_hook;

	u8 secure_area_disable_magic[8];

	u32 ntr_rom_size;
	u32 rom_header_size;

	u32 arm9_param_rom_offset;
	u32 arm7_param_rom_offset;

	u32 _uninteresting_0x90[0x30/4];

	u8 nintendo_logo[0x9C];
	u16 nintendo_logo_crc16;
	u16 header_crc16;
} EnvNdsHeader;

typedef struct EnvNdsHeaderDebugFields {
	u32 debug_rom_offset;
	u32 debug_size;
	u32 debug_ram_address;
	u32 _pad_0x0C;
} EnvNdsHeaderDebugFields;

typedef struct EnvNdsHeaderDebug {
	EnvNdsHeader base;
	EnvNdsHeaderDebugFields debug;
} EnvNdsHeaderDebug;

typedef struct EnvTwlHeader {
	EnvNdsHeader base;
	EnvNdsHeaderDebugFields debug;

	u8 _pad_0x170[0x10];

	u32 global_wram_setting[5];
	u32 arm9_wram_setting[3];
	u32 arm7_wram_setting[3];
	struct {
		u32 mbk9_setting : 24;
		u32 wramcnt_setting : 8;
	};

	u32 twl_region_flags;
	u32 twl_access_control;
	u32 scfg_ext7_setting;

	u8 _pad_0x1BC[3];
	u8 twl_flags2;

	u32 arm9i_rom_offset;
	u32 _unused_0x1C4;
	u32 arm9i_ram_address;
	u32 arm9i_size;

	u32 arm7i_rom_offset;
	u32 device_list_ram_address;
	u32 arm7i_ram_address;
	u32 arm7i_size;

	u32 ntr_digest_rom_start;
	u32 ntr_digest_size;
	u32 twl_digest_rom_start;
	u32 twl_digest_size;
	u32 digest_level1_rom_offset;
	u32 digest_level1_size;
	u32 digest_level0_rom_offset;
	u32 digest_level0_size;
	u32 digest_sector_size;
	u32 digest_sectors_per_block;

	u32 twl_banner_size;
	u32 _uninteresting_0x20C;
	u32 twl_rom_size;
	u32 _uninteresting_0x214;

	u32 arm9i_param_rom_offset;
	u32 arm7i_param_rom_offset;

	u32 arm9_modcrypt_rom_start;
	u32 arm9_modcrypt_size;
	u32 arm7_modcrypt_rom_start;
	u32 arm7_modcrypt_size;

	union {
		u64 title_id;
		struct {
			u32 title_id_low;
			u32 title_id_high;
		};
	};
	u32 public_sav_size;
	u32 private_sav_size;

	u8 _pad_0x240[0xb0];

	u8 age_ratings[0x10];

	u8 hmac_arm9[20];
	u8 hmac_arm7[20];
	u8 hmac_digest_level0[20];
	u8 hmac_banner[20];
	u8 hmac_arm9i[20];
	u8 hmac_arm7i[20];

	u8 hmac_old_ntr_1[20];
	u8 hmac_old_ntr_2[20];
	u8 hmac_arm9_no_secure[20];

	u8 _pad_0x3B4[0xa4c];

	//-------------------------------------------------------------------------

	u8 twl_debug_args[0x180];
	u8 rsa_sha1_signature[0x80];
} EnvTwlHeader;

typedef struct EnvNdsBanner {
	u16 version;
	u16 crc16_v1;
	u16 crc16_v2;
	u16 crc16_v3;
	u16 crc16_v3_twl;
	u8 _pad_0xa[0x16];

	u8 icon_tiles[0x200];
	u16 icon_pal[16];

	u16 title_utf16[16][128];

	u8 twl_icon_tiles[8][0x200];
	u16 twl_icon_pal[8][16];
	u16 twl_anim_seq[64];
} EnvNdsBanner;

typedef struct EnvNdsFileNameTableDirEntry {
	u32 file_table_offset; // relative to self
	u16 file_id_base;
	union {
		u16 num_dirs;
		u16 parent_id;
	};
} EnvNdsFileNameTableDirEntry;

typedef struct EnvNdsFileTableEntry {
	u32 rom_start_offset;
	u32 rom_end_offset;
} EnvNdsFileTableEntry;

typedef struct EnvNdsOverlay {
	u32 overlay_id;
	u32 ram_address;
	u32 load_size;
	u32 bss_size;
	u32 ctors_start;
	u32 ctors_end;
	u32 file_id;
	u32 reserved;
} EnvNdsOverlay;

#define ENV_NDS_ARGV_MAGIC 0x5f617267 // '_arg'

typedef struct EnvNdsArgvHeader {
	u32 magic; // ENV_NDS_ARGV_MAGIC
	char* args_str;
	u32 args_str_size;
	int argc;
	char** argv;
	char** argv_end;
	u32 dslink_host_ipv4;
} EnvNdsArgvHeader;

#define ENV_NDS_BOOTSTUB_MAGIC 0x62757473746f6f62ULL // 'bootstub'

typedef struct EnvNdsBootstubHeader {
	u64 magic; // ENV_NDS_BOOTSTUB_MAGIC

	// Main return-to-hbmenu entrypoint, for use on ARM9.
	MEOW_NORETURN void (*arm9_entrypoint)(void);

	// This entrypoint is intended for requesting return-to-hbmenu directly from ARM7.
	void (*arm7_entrypoint)(void);
} EnvNdsBootstubHeader;

#define ENV_USER_SETTINGS_VERSION     5
#define ENV_USER_SETTINGS_VERSION_EXT 1

#define ENV_USER_NAME_LEN    10
#define ENV_USER_COMMENT_LEN 26

typedef enum EnvLanguage {
	EnvLanguage_Japanese = 0,
	EnvLanguage_English  = 1,
	EnvLanguage_French   = 2,
	EnvLanguage_German   = 3,
	EnvLanguage_Italian  = 4,
	EnvLanguage_Spanish  = 5,
	EnvLanguage_Chinese  = 6,
	EnvLanguage_Korean   = 7,
} EnvLanguage;

typedef struct EnvUserSettings {
	u8 version;
	u8 _pad_0x01;

	struct {
		u8  favorite_color;
		u8  birth_month;
		u8  birth_day;
		u8  _pad_0x05;
		u16 name_ucs2[ENV_USER_NAME_LEN];
		u8  name_len;
		u8  _pad_0x1b;
		u16 comment_ucs2[ENV_USER_COMMENT_LEN];
		u8  comment_len;
		u8  _pad_0x51;
	} user;

	struct {
		u8 hour;
		u8 minute;
		u8 second; // unused?
		u8 _pad_0x55;
		u8 flags;
		u8 _pad_0x57;
	} alarm;

	struct {
		u16 raw_x1, raw_y1;
		u8  lcd_x1, lcd_y1;
		u16 raw_x2, raw_y2;
		u8  lcd_x2, lcd_y2;
	} touch_calib;

	struct {
		u16 language          : 3; // see EnvLanguage
		u16 gba_on_bottom_lcd : 1;
		u16 dslite_brightness : 2;
		u16 autoboot          : 1;
		u16 _pad              : 9;

		u8  unused_maybe1;
		u8  unused_maybe2;
		s32 rtc_offset_lo;
		s32 rtc_offset_hi; // only sign extend/useless (?)
	} config;
} EnvUserSettings;

typedef struct EnvUserSettingsNvram {
	EnvUserSettings base;
	u16 counter;
	u16 crc16;

	struct {
		u8 version;
		u8 language;
		u16 lang_mask;

		u8 _pad_0x78[0x86];
	} ext;

	u16 ext_crc16;
} EnvUserSettingsNvram;

typedef struct EnvTwlDeviceListEntry {
	char drive_letter;
	u8 flags;
	u8 perms;
	u8 _pad_0x3;
	char name[0x10];
	char path[0x40];
} EnvTwlDeviceListEntry;

typedef struct EnvTwlDeviceList {
	EnvTwlDeviceListEntry devices[11];
	u8 _pad_0x39c[0x24];
	char argv0[0x40];
} EnvTwlDeviceList;

#if defined(ARM7)
void envReadNvramSettings(void);
#endif
