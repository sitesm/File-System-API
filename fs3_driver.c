////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the FS3 storage system.
//
//   Author        : Matthew Sites
//   Last Modified : Fri 19 Nov 2021 09:36:52 AM EDT
//

// Includes
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

// Project Includes
#include <fs3_driver.h>
#include <fs3_controller.h>
#include <cmpsc311_log.h>
#include <fs3_cache.h>
#include <fs3_common.h>
#include <fs3_network.h>

//
// Defines
#define SECTOR_INDEX_NUMBER(x) ((int)(x/FS3_SECTOR_SIZE)) // Gets what sector the bufWrite begins in
#define MAX_FILES FS3_MAX_TOTAL_FILES   // Max files
#define MAX_FILE_SIZE 10000000 // 1 MB

//
// Static Global Variables
// Internal file structures 
extern FS3OpenFile oftable[FS3_MAX_TOTAL_FILES]; // Open file table
extern FS3File ftable[FS3_MAX_TOTAL_FILES];      // Permanent file table (metadata)

// Variables for deconstructing the commandblock
uint8_t opval, retval; // Updated 'op' value | Updated 'return' value -> (0 == Passed, 1 == Failed)
uint16_t secval;       // Updated 'sector' value
uint_fast32_t trkval;  // Updated track value

// Arrays
char mountState[10] = "unmounted";             // == "mounted" if mounted, "unmounted"if not
int globalLoc[FS3_MAX_TRACKS][FS3_TRACK_SIZE]; // 0 if not used, 1 if used

// Used to keep track of what file data is next avalible
int freeOFile  =  0; // Next free open file that can be used
int freeFile   =  0; // Next free premanant file inxed that can be used (Only used when making a brand new file) [Max 10]
int freeHandle =  1; // Next free handle
int16_t curTrk  = -1; // Current track

//
// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : switchTrack
// Description  : switches the track to the requested track
//
// Inputs       : trk - the track to be switched to 
//
// Outputs      : 0 if success, -1 if failure

int8_t switchTrack(int16_t trk){

	// If the file is not on the correct track, seek to that track
	if(trk != curTrk){  

		logMessage(FS3DriverLLevel,"Driver attempting to seek to track %d", trk);

		// Local variable
		FS3CmdBlk retCmd;

		// Seek to the correct track
		int netSuccess = network_fs3_syscall(construct_fs3_cmdblock(FS3_OP_TSEEK, 0, trk, 0), &retCmd,  NULL);

		// Deconstruct to see if it worked
		deconstruct_fs3_cmdblock(retCmd, &opval, &secval, &trkval, &retval); 

		// Fail condition
		if(retval != 0 || netSuccess == -1){ 
			logMessage(FS3DriverLLevel,"System call to seek to track %d failed, exiting program", trk);
			return(-1);
		} 

		curTrk = trk; // Update the current track 
		logMessage(FS3DriverLLevel, "Driver successfully changed track to %d", trk);
		return(0);
	} else{
		logMessage(FS3DriverLLevel, "File system is already on the correct track.");
		return(0);
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : findFreeLoc
// Description  : returns the file index associated with the next free track / sector
//
// Inputs       : *trkidx - Pointer to the storage variable
//				: *secidx - Pointer to the storage variable
//
// Outputs      : 0 if success, -1 if failure

int8_t findFreeLoc(int16_t *trkidx, int16_t *secidx){

	// Loop through all possible tracks
	for(int trk=0; trk<FS3_MAX_TRACKS; trk++){

		// Loop through every sector in the track
		for(int sec=0; sec<FS3_TRACK_SIZE; sec++){
			
			// If the track & sector are free(0), set the result
			if(globalLoc[trk][sec] == 0){
				
				*trkidx = trk;
				*secidx = sec;
				globalLoc[trk][sec] = 1; // Update global array
				return(0);
			}
		}
	}
	
	// Log info
	logMessage(FS3DriverLLevel, "Could not find a free trk/sec, exiting the program");
	return(-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : idxByHandle
// Description  : returns the file index associated with a file handle
//
// Inputs       : fd - file handle to find index of 
//              : *ofidx - Pointer to the storage variable
//				: *fidx  - Pointer to the storage variable
//
// Outputs      : 0 if success, -1 if failure

int16_t idxByHandle(int16_t fd, int16_t *ofidx, int16_t *fidx){

	// For every possible file 
	for(int i=0; i<MAX_FILES; i++){
		
		if(oftable[i].ofhandle == fd){ // If the file handle is found at index 'i'

			*ofidx = i; // Set pointer to ofidx to the open file index

			// If the open index is found, also return the index of the permanent file
			for(int j=0; j<MAX_FILES; j++){
				
				if(strcmp(oftable[i].ofname, ftable[j].fname) == 0){ // If the names of the files match

					*fidx = j; // Set pointer to fidx to the permanant file index
					return(0);
				}		
			}
		}
	}

	// Log info
	logMessage(FS3DriverLLevel, "File/OFile index not found, exiting program");
	return(-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : construct_fs3_cmdblock
// Description  : Constructs the command block to communicate with the controller
//
// Inputs       : op - operation code to run
//              : sec - sector number to operate on
//              : trk - track number to operate on 
// Outputs      : retval - the constructed commandblock

FS3CmdBlk construct_fs3_cmdblock(uint8_t op, uint16_t sec, uint_fast32_t trk, uint8_t ret){
	
	// Local Variables for the function
	FS3CmdBlk tmpop = 0, tmpsec = 0, tmptrk = 0, tmpret = 0, retval = 0; 
	
	// Opcode cast and shift
	tmpop = (FS3CmdBlk)op << 60; // Shifts it to bits 0-3
	
	// Sector cast and shift
	tmpsec = (FS3CmdBlk)sec << 44; // Shifts it to bits 4-19
	
	// Track cast and shift
	tmptrk = (FS3CmdBlk)trk << 12; // Shifts it to bits 20-51
	
	// Return cast and shift
	tmpret = (FS3CmdBlk)ret << 11; // Shifts it to bit 52
	
	// Bitwise or all shifter bits together
	retval = (FS3CmdBlk)(tmpop | tmpsec | tmptrk | tmpret); 
	
	return(retval);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : deconstruct_fs3_cmdblock
// Description  : Deonstructs the command block to get the changed values from fs3_syscall
//
// Inputs       : *op - pointer to the operation code
//              : *sec - pointer to the sector number to operate on
//              : *trk - pointer to the track number to operate on 
// Outputs      : Void

void deconstruct_fs3_cmdblock(FS3CmdBlk cmdblk, uint8_t *op, uint16_t *sec, uint_fast32_t *trk, uint8_t *ret){
	
	// Mask the command block to isolate the opcode 
	*op = (cmdblk&0xf000000000000000) >> 60; // Set 'op'(&opval) to the least signifigant 4 bits

	// Mask the commandblock to isolate the sector
	*sec = (cmdblk&0x0fff000000000000) >> 44; // Set 'sec'(&secval) to the least signifigant 16 bits

	// Mask the commandblock to isolate the track
	*trk = (cmdblk&0x00001fffffffe000) >> 12; // Set 'trk'(&trkval) to the  least signifigant 32 bits

	// Mask the commandblock to isolate the return value
	*ret = (cmdblk&0x0000000000000800) >> 11; // Set 'ret'(&retval) to the least signifigant  8 bits

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_mount_disk
// Description  : FS3 interface, mount/initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_mount_disk(void) {

	// Check to see if file system is already mounted
	if(strncmp(mountState, "mounted", 7) == 0){
		logMessage(FS3DriverLLevel, "FS3 DRVR: File system already mounted, exiting program");
		return(-1); // FS already mounted, end the program
	}

	// Make commandblock telling the controller to mount to the disk
	FS3CmdBlk retCmd;
	
	int netSuccess = network_fs3_syscall(construct_fs3_cmdblock(FS3_OP_MOUNT,0,0,0), &retCmd, NULL);

	// Deconstructing the fs3_syscall cmdblk to see if it worked
	deconstruct_fs3_cmdblock(retCmd, &opval, &secval, &trkval, &retval);

	if(retval == 0 && netSuccess == 0){ // Test the output of retval
		logMessage(FS3DriverLLevel, "FS3 DRVR: mounted.\n");    // Log success
		memset(ftable,    0x0, sizeof(FS3File)*FS3_MAX_TRACKS); // Initalize ftable to 0
		memset(oftable,   0x0, sizeof(FS3OpenFile)*MAX_FILES);  // Initalize oftable to 0
		memset(globalLoc, 0x0, sizeof(globalLoc));              // Initalize globalLoc to 0
		strcpy(mountState, "mounted");
		return(0); // Passed
	}else{
		logMessage(FS3DriverLLevel, "FS3 DRVR: Systemcall to mount file system failed, exiting program");
		return(-1); // Failed
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_unmount_disk
// Description  : FS3 interface, unmount the disk, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_unmount_disk(void) {

	// Check to make sure FS was already mounted
	if(strncmp(mountState, "mounted", 7) != 0){
		logMessage(FS3DriverLLevel, "FS3 DRVR: Filesystem not mounted, unable to unmount an unmounted system. Exiting the program");
		return(-1); // FS is not mounted, bail out
	}

	// Cleaning up internal data structure
	for(int i = 0; i < freeHandle; i++){
		if(oftable[i].ofhandle != -1){
			fs3_close(oftable[i].ofhandle); // Close the respective file handle
		}
	}
	
	// Local variable
	FS3CmdBlk retCmd;

	// Make commandblock telling the controller to unmount from the disk
	int netSuccess = network_fs3_syscall(construct_fs3_cmdblock(FS3_OP_UMOUNT,0,0,0), &retCmd, NULL);

	// Deconsteruction
	deconstruct_fs3_cmdblock(retCmd, &opval, &secval, &trkval, &retval);

	// Error check
	if(retval != 0 && netSuccess != 0){
		logMessage(FS3DriverLLevel, "Systemcall to unmount file system failed, exiting program ");
		return(-1); // Failed
	}

	 // Successful unmount
	logMessage(FS3DriverLLevel, "FS3 DRVR: unmounted.");
	strcpy(mountState, "unmounted");
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure

int16_t fs3_open(char *path) { // Path is a pointer "assign2/penn-state.txt"

	// Local variables
	int16_t idx =  0; // if idx==1023, all files have been checked

	// Loop through all possible files 
	for(int i=0; i<MAX_FILES; i++){ // i is the ftable we are currently working with

		// Find the file that correponds with the name 'path'
		if(strcmp(ftable[i].fname, path) == 0){

			// File with the 'path' == 'fname' found
			if(strncmp(ftable[i].fstate, "opened", 6) == 0){ // If the corresponding file state is already "opened"
				logMessage(FS3DriverLLevel, "File [%s] already opened, exiting program", path); // Log creation of new file
				return(-1); 
			}else{ // File is closed, initalize values of oftable to those in ftable
				logMessage(FS3DriverLLevel, "Driver opening existing file [%s]", path); // Log creation of new file

				// Pick a unique file handle 
				oftable[freeOFile].ofhandle = freeHandle; // Set file handle to a unique number

				// Update the open file to all the previous declarations in ftable
				strcpy(oftable[freeOFile].ofname, path); // Set open file name 
				strcpy(ftable[i].fstate, "opened");// Set open file state to opened 
				oftable[freeOFile].oflength = ftable[i].flength; // Set open file length  
				oftable[freeOFile].numsec = ftable[i].numsec; // Set number of sectors
				
				// Copy all track/sector combinations over to the open file
				memcpy(&oftable[freeOFile].ofloc[0][0], &ftable[i].floc[0][0], sizeof(oftable[freeOFile].ofloc[0][0])*FS3_MAX_TRACKS*FS3_TRACK_SIZE);

				break; // Break out of for loop because file inialized
			}
		}else{ // If the file at index 'i' does not have fname == path,

			idx++; // Increment idx by one
		}

		// Check to see if all the files have been looped through and the file is not found
		if(idx == MAX_FILES){ // If none of the files have fname == path, make a new file
			logMessage(FS3DriverLLevel, "Driver creating new file [%s]", path); // Log creation of new file
			strcpy(ftable[freeFile].fname, path); // Copy the path name to the permanent file name
			
			// Initialize
			oftable[freeOFile].numsec = 0;

			// Pick a unique file handle 
			oftable[freeOFile].ofhandle = freeHandle;   // Set file handle to a unique number

			// Update the open file to all the previous declarations in ftable
			strcpy(oftable[freeOFile].ofname, path);      // Set open file name 
			strcpy(ftable[freeOFile].fstate, "opened");   // Set permanant file state to opened 
			oftable[freeOFile].oflength = 0;              // Set open file length 
			oftable[freeOFile].ofpos    = 0; 		      // Set position to the first byte

			freeFile++; // Increment freeFile by one to keep it unique
			break; // Breaks out of the for loop
		}
	}

	// Log the info
	logMessage(FS3DriverLLevel, "File [%s] opened in driver, fh = %d.", oftable[freeOFile].ofname, oftable[freeOFile].ofhandle);

	// Increment all used variables to keep them unique
	freeOFile++; // Move free file pointer to the next index
	freeHandle++; // Move free handle pointer to the next index

	// Return the file handle of the file to open (-1 to correct for the above incrementation)
	return(oftable[freeOFile - 1].ofhandle);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful, -1 if failure

int16_t fs3_close(int16_t fd) {

	// Local variables
	int16_t fidx   = -1; // Index of the permanant file corresponding to the file handle
	int16_t ofidx  = -1; // Index of the open file corresponding to the file handle
	int16_t idxRet = -1; // Initial value

	idxRet = idxByHandle(fd, &ofidx, &fidx); // Get file indexes

	if(idxRet == -1){ // Check for success
		return(-1);
	}

	////////////////////////////////////////////////////////////////
	// 						FAILURE CONTITIONS                    //
	////////////////////////////////////////////////////////////////

	if(ofidx == -1 || fidx == -1){
		// File handle not found
		return(0);
	}else if(strncmp(ftable[fidx].fstate, "closed", 6) == 0){ // If the file is not open
		// File is already closed / was never opened
		logMessage(FS3DriverLLevel, "File refrenced by fh %d not open.", fd);
		return(-1);
	}else{

		////////////////////////////////////////////////////////////////
		// 						SAVING NEW DATA                       //
		////////////////////////////////////////////////////////////////

		ftable[fidx].flength = oftable[ofidx].oflength;  // Record new metadata into permanent table
		ftable[fidx].numsec = oftable[ofidx].numsec;     // Record new metadata
		strcpy(ftable[fidx].fstate, "closed"); 	 		 // Set the file to closed

		// Save all track/sector combinations
		memcpy(&ftable[fidx].floc[0][0], &oftable[ofidx].ofloc[0][0], sizeof(oftable[freeOFile].ofloc[0][0])*FS3_MAX_TRACKS*FS3_TRACK_SIZE);

		////////////////////////////////////////////////////////////////
		// 				RESET ALL OPEN FILE PARAMETERS                //
		////////////////////////////////////////////////////////////////

		oftable[ofidx].oflength =  0; // Set back to original value
		oftable[ofidx].ofhandle = -1; // Set back to original value
		oftable[ofidx].ofpos    =  0; // Set back to original value
		oftable[ofidx].numsec   =  0; // Set back to original value
	
		// Set track/sectors to all 0's
		memset(&oftable[ofidx].ofloc[0][0], 0x0, sizeof(oftable[freeOFile].ofloc[0][0])*FS3_MAX_TRACKS*FS3_TRACK_SIZE);

		// Log info
		logMessage(FS3DriverLLevel, "File contents of fh %d, [%s] saved.", fd, ftable[fidx].fname);
		return (0); // Return 0 to indicate success
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_read
// Description  : Reads "count" bytes from the file handle "fh" into the 
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure

int32_t fs3_read(int16_t fd, void *buf, int32_t count) {

	// Variables for file tracking
	int16_t fidx        = -1; // Index of the permanant file corresponding to the file handle
	int16_t ofidx       = -1; // Index of the open file corresponding to the file handle
	int16_t idxRet      = -1; // Initial value

	// Variables for tracking the state of the read call
	int32_t writePos     =  0; // Tracks the position to write into readBuf 
	int32_t sectorsRead  =  0; // Number of sectors that have already been read
	int16_t numToRead = (int)ceil((double)count/(double)1024);

	// Buffers
	char *readBuf, *cachePtr; 
	char *tmpBuf = (char*)malloc(FS3_SECTOR_SIZE); 

	idxRet = idxByHandle(fd, &ofidx, &fidx); // Get file indexes

	// Allocate space for readBuf
	readBuf = (char*)malloc(numToRead*FS3_SECTOR_SIZE); // Willw always be a multiple of 1024

	// Clear memory
	memset(tmpBuf, 0x0, FS3_SECTOR_SIZE);
	memset(readBuf, 0x0, numToRead*FS3_SECTOR_SIZE);

	////////////////////////////////////////////////////////////////
	// 						FAILURE CONTITIONS                    //
	////////////////////////////////////////////////////////////////

	if(ofidx == -1 || fidx == -1){ // End the program if the file is not found in either structure
		logMessage(FS3DriverLLevel, "Failed to find file index, exiting program");
		return(-1);  
	}else if(strcmp(ftable[fidx].fstate, "opened") != 0){ // End the program if the file is  not open
		logMessage(FS3DriverLLevel, "File not opened, exiting program");
		return(-1);  
	}else if(tmpBuf == NULL){ // End program if allocation failed
		logMessage(FS3DriverLLevel,"Memory allocation for tmpBuf failed, exiting program");
		return(-1);
	}else if(readBuf == NULL){ // Check for success
		logMessage(FS3DriverLLevel,"Memory allocation for readBuf failed, exiting program");
		return(-1); 
	}else if(idxRet == -1){ // Check for success
		logMessage(FS3DriverLLevel,"Failed to find file index, exiting program.");
		return(-1); 
	}

	////////////////////////////////////////////////////////////////
	// 				   READ WHOLE FILE CONTENTS                   //
	////////////////////////////////////////////////////////////////

	int sectorsChecked = 0;
	int firstSec = (int)floor((double)oftable[ofidx].ofpos / (double)FS3_SECTOR_SIZE);

	// Only read if there is data
	if(numToRead > 0){

		// Loop through all locations of the currently opened file
		for(int trk = 0; trk<FS3_MAX_TRACKS; trk++){

			if(sectorsRead == numToRead){ // Cascades from sector for loop below
				break;
			}

			// Check every sector in each track
			for(int sec = 0; sec<FS3_TRACK_SIZE; sec++){

				if(sectorsRead == numToRead){ // breakout condition
					break;
				}

				// Check to see if the sector has any information in it
				if(oftable[ofidx].ofloc[trk][sec] == 1){
					
					// If the nuymber of sectors in the file == number of sectors read, exit the loop
					if(sectorsChecked == firstSec){

						// Check if its on the correct track
						if(trk != curTrk){
							// Switch to the correct track
							int8_t switchRet = switchTrack(trk); // Failing here

							if(switchRet == -1){
								return(-1);
							}
						}

						// Give cacheBuf a value
						cachePtr = fs3_get_cache(trk, sec);

						// Check to see if it was found
						if(cachePtr != NULL){ // Cache line was found
							// Copy data over
							memcpy(&readBuf[writePos], cachePtr, FS3_SECTOR_SIZE); //ERROR (8 bytes = a pointer)

						}else{ // Cache line not found
							// Local variable
							FS3CmdBlk retCmd;

							logMessage(FS3DriverLLevel, "[trk = %d, sec = %d] not found in cache", trk, sec);
							
							// Read the 'ith' sector worth of information
							int netSuccess = network_fs3_syscall(construct_fs3_cmdblock(FS3_OP_RDSECT, sec ,0,0), &retCmd, tmpBuf);

							// Deconstruct the command block to see if it worked properly (ret == 0) -> pass, (ret == 1) -> fail.
							deconstruct_fs3_cmdblock(retCmd, &opval, &secval, &trkval, &retval);

							// Read failed, bail
							if(retval != 0 || netSuccess == -1){
								logMessage(FS3DriverLLevel, "Read on track %d, sector %d failed, exiting program", trk, sec);
								return(-1);
							}
							
							// Place data in the cache
							int putRet = fs3_put_cache(trk, sec, tmpBuf);

							if(putRet == -1){
								logMessage(FS3DriverLLevel, "Failed to palce data in cache, exiting program");
								return(-1);
							}

							// Copy a sector worth of old content into the write buf
							memcpy(&readBuf[writePos], tmpBuf, FS3_SECTOR_SIZE);
						}

						// Update
						writePos += FS3_SECTOR_SIZE;
						sectorsRead++;
					}else{
						sectorsChecked++;
					}
				}
			} 
		} 
	}

	////////////////////////////////////////////////////////////////
	// 				   COPY CORRECT AMMOUNT OVER                  //
	////////////////////////////////////////////////////////////////

	// Copy over memory to buf
	memcpy(buf, readBuf, count); 

	// Log info
	logMessage(FS3DriverLLevel, "FS3 DRVR: read on fh %d (%d bytes)", oftable[ofidx].ofhandle, count);

	// Free memory
	free(tmpBuf);
	free(readBuf);

	// Good practice
	tmpBuf   = NULL;
	readBuf  = NULL;
	cachePtr = NULL; // Never malloced, just ned to reset value
	return(count);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_write
// Description  : Writes "count" bytes to the file handle "fh" from the 
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure

int32_t fs3_write(int16_t fd, void *buf, int32_t count) { //buf -> data that shoud be written

	// Variables for file / open location tracking
	int16_t fidx     = -1; // Index of the permanant file corresponding to the file handle
	int16_t ofidx    = -1; // Index of the open file corresponding to the file handle
	int16_t trkidx   = -1; // Index of the next free track location
	int16_t secidx   = -1; // Index of the next free sector location
	int16_t idxRet   = -1; // Initial value

	// Variables for tracking the state of the write call
	int32_t writePos     =  0; // Keeps track of where to write from
	int32_t writeBufSize =  0; // Selects what size writeBuf should be between 'length' and 'pos + count'
	int32_t sectorsWrote =  0; // Number of sectors that have already been wrote

	// Buffers 
	char *writeBuf, *cachePtr, *tmpBuf;

	// Returns the open/permanant file index of a file referd to by the given file handle
	idxRet = idxByHandle(fd, &ofidx, &fidx); 

	// Find the first sector that needs to be changed
	int16_t firstSec = (int)floor((double)oftable[ofidx].ofpos / (double)1024);
	
	// Find the last sector that needs to be changed
	int16_t lastSec = (int)ceil((double)(oftable[ofidx].ofpos + count) / (double)1024);

	// Determine how many sectors need to be changed
	int16_t numToChange = lastSec - firstSec;

	// Determine the correct size for writeBuf
	writeBufSize = numToChange*FS3_SECTOR_SIZE;

	// Dynamically allocates area of oflength in memory to store the file
	writeBuf = (char*)malloc(writeBufSize);
	tmpBuf   = (char*)malloc(FS3_SECTOR_SIZE);

	// Clear memory
	memset(writeBuf, 0x0, writeBufSize);
	memset(tmpBuf, 0x0, FS3_SECTOR_SIZE);

	////////////////////////////////////////////////////////////////
	// 						FAILURE CONTITIONS                    //
	////////////////////////////////////////////////////////////////

	if(ofidx == -1 || fidx == -1){ // End the program if the file is not found in either structure
		logMessage(FS3DriverLLevel, "File index not found in [WRITE] exiting program");
		return(-1);
	}else if(strcmp(ftable[fidx].fstate, "opened") != 0){ // End the program if the file is found but not open
		logMessage(FS3DriverLLevel, "File not opened in [WRITE] exiting program");
		return(-1);
	}else if(MAX_FILE_SIZE < (oftable[ofidx].ofpos + count)){ // End the program if the write is larger than the max file size(10KB)
	logMessage(FS3DriverLLevel, "Write size in [WRITE] excedded the limit, exiting program");
		return(-1);
	}else if(tmpBuf == NULL){ // If tmpBuf == NULL (malloc() failed)
		logMessage(FS3DriverLLevel,"Memory allocation for tmpBuf failed in [WRITE], exiting program");
		return(-1); // Failed 
	}else if(writeBuf == NULL){
		logMessage(FS3DriverLLevel,"Memory allocation for writeBuf failed in [WRITE], exiting program");
		return(-1);
	}else if(idxRet == -1){
		logMessage(FS3DriverLLevel,"File index not found in [WRITE], exiting program");
		return(-1);
	}

	////////////////////////////////////////////////////////////////
	// 		  READ ALL SECTORS FROM THE FILE INTO WRITEBUF        //
	////////////////////////////////////////////////////////////////
	
	// Only update length if the position is going to go past the current length
	if(oftable[ofidx].ofpos + count > oftable[ofidx].oflength){ 

		//Log info
		logMessage(FS3DriverLLevel, "Extending file length to accomadate %d bytes... fh %d length is now %d", 
			count, oftable[ofidx].ofhandle, (oftable[ofidx].ofpos + count));

		// Compute number of new sectors required
		double requiredSectors = ceil((double)(oftable[ofidx].ofpos + count) / (double)FS3_SECTOR_SIZE); // Can only allocate full sectors

		// Check if the file requires another sector  
		if(requiredSectors > oftable[ofidx].numsec){ 

			// How many sectors to add (required sectors - how many sectors are alredy allocated)
			int8_t numSectors = requiredSectors - (oftable[ofidx].numsec);

			// Log info
			logMessage(FS3DriverLLevel, "Required sectors for the file exceeds currently allocated sectors, allocating %d more sectors for fh %d", numSectors, oftable[ofidx].ofhandle);

			// While the required sectors have not been allocated
			while(numSectors > 0){

				// Find a free track/setor combination
				findFreeLoc(&trkidx, &secidx);

				// Update local/global locations
				oftable[ofidx].ofloc[trkidx][secidx] = 1;

				// Decrement numSectors
				numSectors--;

				// Increment number of sectors
				oftable[ofidx].numsec++;
			}

			// Initial update of length
			oftable[ofidx].oflength = oftable[ofidx].numsec*FS3_SECTOR_SIZE;
		}
	}

	// Only read if theres data to read
	if(numToChange > 0 && oftable[ofidx].oflength != 0){
	    
		// Save old position for later
		int32_t tmpPos  = oftable[ofidx].ofpos;

		// Set position to 0 in order to read the whole file
		oftable[ofidx].ofpos = firstSec*FS3_SECTOR_SIZE;

		// Read sectors that need to be altered
		int32_t readRes = fs3_read(fd, writeBuf, writeBufSize); // Reads the whole sector

		// Check for success
		if(readRes != writeBufSize){
			logMessage(FS3DriverLLevel, "Read in [WRITE] Failed, exiting program");
			return(-1);
		}

		// Return file position to previous state
		oftable[ofidx].ofpos = tmpPos;
	}

	////////////////////////////////////////////////////////////////
	// 		  ADD CONTENTS OF BUF INTO THE CURRENT POSITION       //
	////////////////////////////////////////////////////////////////

	// Determine where to write on the new writeBuf
	int whereToWrite = oftable[ofidx].ofpos - (firstSec*FS3_SECTOR_SIZE);

	logMessage(FS3DriverLLevel, "WhereToWrite = %d, Pos = %d, firstSec = %d", whereToWrite,oftable[ofidx].ofpos, firstSec);

	// Move buf data into writeBuf at the current position
	memcpy(&writeBuf[whereToWrite], buf, count);

	////////////////////////////////////////////////////////////////
	// 	  WE NOW HAVE ALL CONTENTS IN WRITEBUF TO MAKE SYSCALL    //
	////////////////////////////////////////////////////////////////

	// Reset the values of curSec and reaminder
	int sectorsPassed = 0;

	// Find the next free trk/sec and write into that 
	for(int trk = 0; trk<FS3_MAX_TRACKS; trk++){ 

		// Check to see if all data has been written
		if(sectorsWrote == numToChange){ // Cacsaces from below loop
			break;
		}

		// Check each sector
		for(int sec = 0; sec<FS3_TRACK_SIZE; sec++){

			// Check to see if all data has been written
			if(sectorsWrote == numToChange){
				break;
			}

			// Want to start sec at firstSec and then write the next numToChange sectors

			// Check for sector owned by the file
			if(oftable[ofidx].ofloc[trk][sec] == 1){
				
				// Get to the first sector to write into
				if(sectorsPassed == firstSec){ 

					// Check track
					if(trk != curTrk){
						// Switch Tracks
						int8_t switchRet = switchTrack(trk);

						// Check for failure
						if(switchRet == -1){
							logMessage(FS3DriverLLevel, "switchTrack return value = %d", switchRet);
							return(-1);
						}
					}

					// Copy one sector worth of data into a tmpBuf buffer
					memcpy(tmpBuf, &writeBuf[writePos], FS3_SECTOR_SIZE);

					// Check to see if the sector is in the cache and the cache data is the same as tmpBuf
					cachePtr = fs3_get_cache(trk, sec); // Probably dont need this or the if statment(only the else)

					// Check to see if it was found
					if(cachePtr != NULL && memcmp(cachePtr, tmpBuf, FS3_SECTOR_SIZE) == 0){ 
						// Local variable
						FS3CmdBlk retCmd;

						// Write the 'ith' sector worth of information
						int netSuccess = network_fs3_syscall(construct_fs3_cmdblock(FS3_OP_WRSECT, sec, 0, 0), &retCmd, cachePtr);

						// Deconstruct the command block to see if it worked properly (ret == 0) -> pass, (ret == 1) -> fail.
						deconstruct_fs3_cmdblock(retCmd, &opval, &secval, &trkval, &retval);

						//Read failed, bail
						if(retval != 0 || netSuccess == -1){
							logMessage(FS3DriverLLevel,"System call to write to sector %d for fh %d failed, exiting program", sec, oftable[ofidx].ofhandle);
							return(-1);
						}
					}else{
						// Local variable
						FS3CmdBlk retCmd;

						// Write the 'ith' sector worth of information
						int netSuccess = network_fs3_syscall(construct_fs3_cmdblock(FS3_OP_WRSECT, sec, 0, 0), &retCmd, tmpBuf);

						// Deconstruct the command block to see if it worked properly (ret == 0) -> pass, (ret == 1) -> fail.
						deconstruct_fs3_cmdblock(retCmd, &opval, &secval, &trkval, &retval);

						//Read failed, bail
						if(retval != 0 || netSuccess == -1){
							logMessage(FS3DriverLLevel,"System call to write to sector %d for fh %d failed, exiting program", sec, oftable[ofidx].ofhandle);
							return(-1);
						}

						// Place data in the cache (write through)
						int putRet = fs3_put_cache(trk, sec, tmpBuf);

						// Failure condition
						if(putRet == -1){
							logMessage(FS3DriverLLevel, "Failed to palce data in cache, exiting program");
							return(-1);
						}
					}

					// Increment
					sectorsWrote++;
					writePos += FS3_SECTOR_SIZE;
				}else{
					// Increment
					sectorsPassed++;
				}
			}
		}
	}

	// Update the new position
	oftable[ofidx].ofpos += count;

	// Log info
	logMessage(FS3DriverLLevel, "FS3 DRVR: write on fh %d (%d bytes) [pos=%d, len=%d]",
		oftable[ofidx].ofhandle, count, oftable[ofidx].ofpos, oftable[ofidx].oflength);

	// Free Buffers 
	free(writeBuf);
	free(tmpBuf);

	// NULL Buffers
	writeBuf = NULL; 
	tmpBuf   = NULL;
	cachePtr = NULL; // Never malloced, just need to reset value
	
	// Indicate success
	return(count);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_seek
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offfset of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_seek(int16_t fd, uint32_t loc) {

	//Local Variables
	int16_t fidx   = -1; // Index of the permanant file corresponding to the file handle
	int16_t ofidx  = -1; // Index of the open file corresponding to the file handle
	int16_t idxRet = -1; // Initial value

	// Get the index of the file handle parameter
	idxRet = idxByHandle(fd, &ofidx, &fidx);

	// Check for success
	if(idxRet == -1){
		return(-1);
	}

	////////////////////////////////////////////////////////////////
	// 						FAILURE CONTITIONS                    //
	////////////////////////////////////////////////////////////////

	if(loc > oftable[ofidx].oflength || // If the location is bigger than the file length
		ofidx == -1 || fidx == -1){ 	// If the file handle was not found in either structure
		logMessage(FS3DriverLLevel,"Failure condition in [SEEK] reached, exiting program");
		return(-1); 
	}

	oftable[ofidx].ofpos = loc; // Set the position of the file to the loc
	logMessage(FS3DriverLLevel, "File seek fh %d to [pos = %d] successful.", oftable[ofidx].ofhandle, loc);
	return(0); // Return 0 to indicate success
}