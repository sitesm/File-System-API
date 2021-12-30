#ifndef FS3_DRIVER_INCLUDED
#define FS3_DRIVER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_driver.h
//  Description    : This is the header file for the standardized IO functions
//                   for used to access the FS3 storage system.
//
//  Author         : Patrick McDaniel | Matthew Sites
//  Last Modified  : Fri 19 Nov 2021 05:57:00 PM 
//

// Include files
#include <stdint.h>

// Project includes
#include <fs3_controller.h>

// Defines
#define FS3_MAX_TOTAL_FILES 1024 // Maximum number of files ever
#define FS3_MAX_PATH_LENGTH 128 // Maximum length of filename length


//Type Definitions / Internal Data Structures

// Permanent file structure | Tracks the metadata
typedef struct FS3File{
	char fname[128]; // Files Permanent filename
	int32_t flength; // Length of the file 
	uint_fast32_t floc[FS3_MAX_TRACKS][FS3_TRACK_SIZE]; // What track the file is on 
	char fstate[6]; // "opened" if open, "closed" if closed
	int32_t numsec; // Nuber of sectors the file takes up
} FS3File;
 
// Temporary data to track current state of the file | Only valid when a file is open
typedef struct FS3OpenFile{
	char ofname[128]; // Name of the file (Might not have to track this here)
	int32_t oflength; // Length of the file 
	int16_t ofhandle;// File Handle (Unique number) | Only valid while the file is open
	uint32_t ofpos; // Current position of the file 
	uint_fast32_t ofloc[FS3_MAX_TRACKS][FS3_TRACK_SIZE]; // What track the file is on 
	int32_t numsec; // Number of sectors the file takes up
} FS3OpenFile;

FS3OpenFile oftable[FS3_MAX_TOTAL_FILES];
FS3File ftable[FS3_MAX_TOTAL_FILES];

//
// Interface functions
int8_t switchTrack(int16_t trk);
	// Switches the current track to "trk"

int8_t findFreeLoc(int16_t *trkidx, int16_t *secidx);
	// Finds the indexs of the next free track and sector based on the globalLoc array

int16_t idxByHandle(int16_t fd, int16_t *ofidx, int16_t *fidx);
	// Finds the indexs of both the open and permanant files based on a given file handle 

FS3CmdBlk construct_fs3_cmdblock(uint8_t op, uint16_t sec, uint_fast32_t trk, uint8_t ret);
	// Creates a commandblock to do the requested operation at the correct location

void deconstruct_fs3_cmdblock(FS3CmdBlk cmdblk, uint8_t *op, uint16_t *sec, uint_fast32_t *trk, uint8_t *ret);
	// Deconstructs the newly encoded commandblock from a syscall return

int32_t fs3_mount_disk(void);
	// FS3 interface, mount/initialize filesystem

int32_t fs3_unmount_disk(void);
	// FS3 interface, unmount the disk, close all files

int16_t fs3_open(char *path);
	// This function opens a file and returns a file handle

int16_t fs3_close(int16_t fd);
	// This function closes a file

int32_t fs3_read(int16_t fd, void *buf, int32_t count);
	// Reads "count" bytes from the file handle "fh" into the buffer  "buf"

int32_t fs3_write(int16_t fd, void *buf, int32_t count);
	// Writes "count" bytes to the file handle "fh" from the buffer  "buf"

int32_t fs3_seek(int16_t fd, uint32_t loc);
	// Seek to specific point in the file

#endif