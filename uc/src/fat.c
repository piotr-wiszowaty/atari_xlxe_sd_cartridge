/**
 *  Atari XL/XE SD cartridge
 *  Copyright (C) 2013  Piotr Wiszowaty
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see {http://www.gnu.org/licenses/}.
 */

#include <stdint.h>
#include "fat.h"
#include "sdmmc.h"

typedef struct {
	uint8_t active;
	uint8_t start_head;
	uint16_t start_cyl_sec;
	uint8_t fs_type;
	uint8_t end_head;
	uint16_t end_cyl_sec;
	uint32_t first_sector;
	uint32_t size;
} Partition_Info;

typedef struct {
	uint8_t bootloader[0x1be];
	Partition_Info part_info[4];
	uint16_t signature;
} Master_Boot_Record;

typedef struct {
	uint8_t jmp_boot[3];
	uint8_t oem_name[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors_count;
	uint8_t num_fats;
	uint16_t root_entry_count;
	uint16_t total_sectors16;
	uint8_t bpb_media;
	uint16_t fat16_size;
	uint16_t sectors_per_track;
	uint16_t num_heads;
	uint32_t hidden_sectors;
	uint32_t total_sectors32;
	/* FAT32 specific fields follow */
	uint32_t fat32_size;
	uint16_t ext_flags;
	uint16_t fsver;
	uint32_t root_cluster;
} BPB;

typedef struct {
	uint8_t name[11];
	uint8_t attr;
	uint8_t ntres;
	uint8_t crt_time_tenth;
	uint16_t crt_time;
	uint16_t crt_date;
	uint16_t lst_acc_date;
	uint16_t fst_clus_hi;
	uint16_t wrt_time;
	uint16_t wrt_date;
	uint16_t fst_clus_lo;
	uint32_t file_size;
} Dir_Entry;

int fat_equ_filename(char *filename, char *dir_entry_name)
{
	int i;
	int j;

	for (i = 0; i < 8 && filename[i] && filename[i] != '.' && dir_entry_name[i]; i++) {
		if (dir_entry_name[i] >= 'A' && dir_entry_name[i] <= 'Z') {
			if ((dir_entry_name[i] | 0x20) != (filename[i] | 0x20)) {
				return 0;
			}
		} else if (filename[i] != dir_entry_name[i]) {
			return 0;
		}
	}
	i++;
	j = 8;
	while (filename[i] && dir_entry_name[j]) {
		if (dir_entry_name[j] >= 'A' && dir_entry_name[j] <= 'Z') {
			if ((dir_entry_name[j] | 0x20) != (filename[i] | 0x20)) {
				return 0;
			}
		} else if (filename[i] != dir_entry_name[j]) {
			return 0;
		}
		i++;
		j++;
	}

	return 1;
}

uint32_t fat_find_first_sector(char *filename)
{
	int i;
	uint8_t buf[512];
	Master_Boot_Record *mbr;
	uint8_t fs_type;
	uint32_t part0_first_sector;
	BPB *bpb;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors_count;
	uint32_t fat_start;
	uint32_t fat_size;
	uint32_t first_data_sector;
	Dir_Entry *dir_entries;
	uint32_t cluster;

	if (sdmmc_read_block(0, buf) < 0) {
		return 0;
	}
	mbr = (Master_Boot_Record *) buf;
	fs_type = mbr->part_info[0].fs_type;
	part0_first_sector = mbr->part_info[0].first_sector;

	if (sdmmc_read_block(part0_first_sector, buf) < 0) {
		return 0;
	}
	bpb = (BPB *) buf;
	if (fs_type == 0x06) {
		fat_size = bpb->fat16_size;
	} else if (fs_type == 0x0B) {
		fat_size = bpb->fat32_size;
	} else {
		return 0;
	}
	sectors_per_cluster = bpb->sectors_per_cluster;
	reserved_sectors_count = bpb->reserved_sectors_count;
	fat_start = part0_first_sector + reserved_sectors_count;
	first_data_sector = fat_start + (fat_size << 1);

	if (sdmmc_read_block(first_data_sector, buf) < 0) {
		return 0;
	}
	dir_entries = (Dir_Entry *) buf;

	i = 0;
	while (dir_entries[i].name[0]) {
		if (dir_entries[i].name[0] != 0xe5) {
			if (fat_equ_filename(filename, (char *) &(dir_entries[i].name))) {
				cluster = ((uint32_t) dir_entries[i].fst_clus_hi << 16) | (uint32_t) dir_entries[i].fst_clus_lo;
				return first_data_sector + cluster * sectors_per_cluster;
			}
		}
		i++;
	}

	return 0;
}
