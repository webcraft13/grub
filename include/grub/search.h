/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GRUB_SEARCH_HEADER
#define GRUB_SEARCH_HEADER 1

void grub_search_fs_file (const char *key, const char *var, int no_floppy,
			  char **hints, unsigned nhints);
void grub_search_fs_uuid (const char *key, const char *var, int no_floppy,
			  char **hints, unsigned nhints);
void grub_search_label (const char *key, const char *var, int no_floppy,
			char **hints, unsigned nhints);
void grub_search_part_uuid (const char *key, const char *var, int no_floppy,
			    char **hints, unsigned nhints);
void grub_search_part_label (const char *key, const char *var, int no_floppy,
			     char **hints, unsigned nhints);

#endif
