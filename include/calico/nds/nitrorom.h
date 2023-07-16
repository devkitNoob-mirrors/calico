#pragma once
#include "../types.h"
#include "env.h"

#define NITROROM_ROOT_DIR 0xf000
#define NITROROM_NAME_MAX 0x7f

MK_EXTERN_C_START

typedef struct NitroRom          NitroRom;
typedef struct NitroRomParams    NitroRomParams;
typedef struct NitroRomIface     NitroRomIface;
typedef struct NitroRomIter      NitroRomIter;
typedef struct NitroRomIterEntry NitroRomIterEntry;
typedef EnvNdsFileTableEntry     NitroRomFile;
typedef EnvNdsDirTableEntry      NitroRomDir;

struct NitroRomParams {
	u32 fat_offset;
	u32 fat_sz;
	u32 fnt_offset;
	u32 fnt_sz;
	u32 img_offset;
};

struct NitroRom {
	const NitroRomIface* iface;
	void* user;

	u32 fnt_offset;
	u32 img_offset;
	u16 num_files;
	u16 num_dirs;

	NitroRomFile* file_table;
	NitroRomDir*  dir_table;
};

struct NitroRomIface {
	bool (*read)(void* user, u32 offset, void* buf, u32 size);
	void (*close)(void* user);
};

struct NitroRomIter {
	NitroRom* nr;
	u32 start;
	u32 cursor;
	u16 first_file_id;
	u16 cur_file_id;
};

struct NitroRomIterEntry {
	char name[NITROROM_NAME_MAX+1];
	u16  id;
	u16  name_len;
};

#ifdef ARM9
NitroRom* nitroromGetSelf(void);
#endif

bool nitroromOpen(NitroRom* nr, const NitroRomParams* params, const NitroRomIface* iface, void* user);

void nitroromClose(NitroRom* nr);

MK_INLINE bool nitroromRead(NitroRom* nr, u32 offset, void* buf, u32 size)
{
	return nr->iface->read(nr->user, offset, buf, size);
}

MK_INLINE u32 nitroromGetFileOffset(NitroRom* nr, u16 file_id)
{
	return nr->img_offset + nr->file_table[file_id].start_offset;
}

MK_INLINE u32 nitroromGetFileSize(NitroRom* nr, u16 file_id)
{
	EnvNdsFileTableEntry* f = &nr->file_table[file_id];
	return f->end_offset - f->start_offset;
}

MK_INLINE bool nitroromReadFile(NitroRom* nr, u16 file_id, u32 offset, void* buf, u32 size)
{
	return nitroromRead(nr, nitroromGetFileOffset(nr, file_id) + offset, buf, size);
}

MK_INLINE u16 nitroromGetParentDir(NitroRom* nr, u16 dir_id)
{
	return nr->dir_table[dir_id-NITROROM_ROOT_DIR].parent_id;
}

MK_INLINE void nitroromOpenIter(NitroRom* nr, u16 dir_id, NitroRomIter* iter)
{
	NitroRomDir* d      = &nr->dir_table[dir_id-NITROROM_ROOT_DIR];
	iter->nr            = nr;
	iter->start         = nr->fnt_offset + d->subtable_offset;
	iter->cursor        = iter->start;
	iter->first_file_id = d->file_id_base;
	iter->cur_file_id   = iter->first_file_id;
}

bool nitroromReadIter(NitroRomIter* iter, NitroRomIterEntry* entry);

MK_INLINE void nitroromRewindIter(NitroRomIter* iter)
{
	iter->cursor      = iter->start;
	iter->cur_file_id = iter->first_file_id;
}

int nitroromResolvePath(NitroRom* nr, u16 base_dir, const char* path);

MK_EXTERN_C_END
