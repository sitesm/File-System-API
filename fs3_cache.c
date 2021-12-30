////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_cache.c
//  Description    : This is the implementation of the cache for the 
//                   FS3 filesystem interface.
//
//  Author         : Patrick McDaniel | Matthew Sites
//  Last Modified  : Sun 17 Oct 2021 09:36:52 AM EDT
//

// Includes
#include <cmpsc311_log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Project Includes
#include <fs3_driver.h>
#include <fs3_controller.h>
#include <cmpsc311_log.h>
#include <fs3_cache.h>
#include <fs3_common.h>
#include <fs3_network.h>

// 
// Support Macros/Data

//
// Global Variables
FS3Cache *cache       = NULL; // Pointer to the cache memory location
int16_t cacheSize = -1, cacheItems =  0;  // Cache parameters
int32_t nextAccess = 0, cacheGets = 0, cacheInserts = 0, cacheMisses = 0, cacheHits = 0; // Cache statistics   

//
// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_init_cache
// Description  : Returns the least reacently used cache line index
//
// Inputs       : cachelines - the number of cache lines to include in cache
// Outputs      : Least Recently Used Index if successful, -1 if failure

int16_t fs3_lru_idx(int16_t cachelines){ // lastAccess will never be -1

    // Local variables
    int32_t smallestAccess = nextAccess; // The first iteration will always be smaller than nextAccess
    int16_t LRUidx = -1;

    // Loop over every cache line
    for(int i = 0; i < cachelines; i++){
        
        // If the lastAccess at cache line "i" is smaller than the smallest index
        if( ((cache + i) -> lastAccess) < smallestAccess){ 
            
            //Update smallestIDX
            smallestAccess = (cache + i) -> lastAccess;
            LRUidx = i;
        }
    }

    logMessage(LOG_INFO_LEVEL, "LRU idx = %d", LRUidx);
    return(LRUidx);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_init_cache
// Description  : Initialize the cache with a fixed number of cache lines
//
// Inputs       : cachelines - the number of cache lines to include in cache
// Outputs      : 0 if successful, -1 if failure

int fs3_init_cache(uint16_t cachelines) {

    //Failure condition
    if(cache != NULL || cacheSize != -1){ // Cache is already initalized
        logMessage(LOG_INFO_LEVEL, "Cache already initalized, exiting program.");
        return(-1);
    }

    if(cachelines == 0){ // 0 line cache is pointless
        logMessage(LOG_INFO_LEVEL, "Cache with 0 cache lines NOT created, a 0 line cache is useless");
        return(-1);
    }else{       
        //Allocate area for the cache in the heap
        cache = malloc(sizeof(FS3Cache)*cachelines);

        // Check for success
        if(cache == NULL){ // Cache memory not allocated
            logMessage(LOG_INFO_LEVEL, "Initalization of a %d cache line cache failed, exiting program.", cachelines);
            return(-1);
        }else{
            // Initalize all variables
            for(int i = 0; i<cachelines; i++){
                (cache + i) -> csec = -1;        // Set sector to 0
                (cache + i) -> ctrk = -1;        // Set track to 0
                (cache + i) -> lastAccess = -1; // Set last access to -1
                (cache + i) -> dataBuf = NULL;  // Initalize dataBuf
            }

            //Log info
            logMessage(LOG_INFO_LEVEL, "Cache successfully initalized.");
            logMessage(LOG_INFO_LEVEL, "Cache state [%d items, %d bytes used]", cacheItems, cacheItems*FS3_SECTOR_SIZE);
            cacheSize = cachelines; // Upate global variable for use elsewhere
            return(0); // Indicate success
        }
    }

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_close_cache
// Description  : Close the cache, freeing any buffers held in it
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int fs3_close_cache(void)  {

    // Failure condition
    if(cache == NULL || cacheSize == -1){ // Cache not initalized
        logMessage(LOG_INFO_LEVEL, "Cache was never inilaized, cannot close. Exiting program.");
        return(-1);
    }

    for(int i = 0; i<cacheSize; i++){
        // Reset all cache values
        (cache + i) -> csec       =  0; // Set sector to 0
        (cache + i) -> ctrk       =  0; // Set track to 0
        (cache + i) -> lastAccess = -1; // Set last access to -1

        if( (cache + i) -> dataBuf != NULL){
            free((cache + i) -> dataBuf);   // Free dataBuf if its still allocated
        }
    }

    // Reset
    cacheSize  = -1; // Update global variable
    cacheItems = 0;  // Reset cache item

    // Free cache pointer
    free(cache);

    // Reset pointer
    cache = NULL;

    //Log info
    logMessage(LOG_INFO_LEVEL, "Cache successfully un-initalized.");
    logMessage(LOG_INFO_LEVEL, "Cache state [%d items, %d bytes used]", cacheItems, cacheItems*FS3_SECTOR_SIZE);
    return(0); // Indicate success
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_put_cache
// Description  : Put an element in the cache
//
// Inputs       : trk - the track number of the sector to put in cache
//                sct - the sector number of the sector to put in cache
// Outputs      : 0 if inserted, -1 if not inserted

int fs3_put_cache(FS3TrackIndex trk, FS3SectorIndex sct, void *buf) {

    // Failure condition
    if(cache == NULL){
        // If the cache was never allocated in the heap
        logMessage(LOG_INFO_LEVEL, "Cache never allocated, cannot put cache line into cache.");
        return(-1);
    }

    // Check to see if track / sector data is already in the cache
    for(int i = 0; i < cacheSize; i++){
        // Walk the cache looking for the track / sector data
        if( (cache + i) -> ctrk == trk && (cache+ i) -> csec == sct){

            // Track & sector found -> Update dataBuf
            memcpy((cache + i) -> dataBuf, buf, FS3_SECTOR_SIZE); // Update dataBuf

            // Update access time 
            (cache + i) -> lastAccess = nextAccess;

            // Increment nextAccess / cacheInserts
            nextAccess++;
            cacheInserts++;

            // Log info
            logMessage(LOG_INFO_LEVEL, "[Trk %d, Sec %d] found in cache, overwriting data buffer", trk, sct);

            // Indicate success
            return(0);
        }
    }

    // Trk / Sct not found, fill all unused cache lines first(lastAccess == -1)
    for(int i = 0; i < cacheSize; i++){
        // If the last access at 'i' is -1 (Initalized but not used), place the sector there
        if( (cache + i) -> lastAccess == -1 ){ 
            // Set cache variables
            (cache + i) -> csec = sct;                            // Set sector to sct
            (cache + i) -> ctrk = trk;                            // Set track to trk
            (cache + i) -> lastAccess = nextAccess;               // Set last access to the next free access time 
            (cache + i) -> dataBuf = malloc(FS3_SECTOR_SIZE);     // Allocate area for the dataBuf
            memcpy((cache + i) -> dataBuf, buf, FS3_SECTOR_SIZE); // Update dataBuf

            // Update variables
            nextAccess++;
            cacheItems++;
            cacheInserts++;

            // Log info 
            logMessage(LOG_INFO_LEVEL, "[Trk %d, Sec %d] placed in cache.", trk, sct);
            logMessage(LOG_INFO_LEVEL, "[Trk %d, Sec %d] replaced cache line with last access of -1.[Cold Miss]", trk, sct);
            logMessage(LOG_INFO_LEVEL, "Cache state [%d items, %d bytes used]", cacheItems, cacheItems*FS3_SECTOR_SIZE);
            return(0); // Indicate success / Exit from function
        }
    }

    // Will only run if all cache lines are already filled (lastAccess != -1)

    // Find the LRU cache index
    int LRUidx = fs3_lru_idx(cacheSize);

    if(LRUidx == -1){

        // Log failure
        logMessage(LOG_INFO_LEVEL, "Could not find LRUidx (LRUidx == -1)");
        return(-1);
    }

    // Update LRU cache line to new parameters
    (cache + LRUidx) -> ctrk = trk;              // Update track
    (cache + LRUidx) -> csec = sct;              // Update sector 
    (cache + LRUidx) -> lastAccess = nextAccess; // Update access time

    // Update dataBuf with new data 
    memcpy((cache + LRUidx) -> dataBuf, buf, FS3_SECTOR_SIZE); // Update dataBuf

    // Update
    nextAccess++; 
    cacheInserts++;

    // Log info / Update nextAccess
    logMessage(LOG_INFO_LEVEL, "[Trk %d, Sec %d] placed in cache.", trk, sct);
    logMessage(LOG_INFO_LEVEL, "[Trk %d, Sec %d] replaced cache line with least recent access.", trk, sct);
    return(0); // Indicate success / Exit from function
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_get_cache
// Description  : Get an element from the cache 
//
// Inputs       : trk - the track number of the sector to find
//                sct - the sector number of the sector to find
// Outputs      : returns NULL if not found or failed, pointer to buffer if found

void * fs3_get_cache(FS3TrackIndex trk, FS3SectorIndex sct)  {
    // Increment how many times get has been called
    cacheGets++;

    // Failure condition
    if(cache == NULL){
        // If the cache was never allocated in the heap
        logMessage(LOG_INFO_LEVEL, "Cache never allocated, cannot put cache line into cache.");
        return(NULL);
    }

    // Check to see if track / sector data is already in the cache
    for(int i = 0; i < cacheSize; i++){

        // Walk the cache looking for the track / sector data
        if( (cache + i) -> ctrk == trk && (cache + i) -> csec == sct){
            // Track & sector found: Update access time 
            (cache + i) -> lastAccess = nextAccess;

            // Update
            nextAccess++;
            cacheHits++;

            // Log info
            logMessage(LOG_INFO_LEVEL, "[Trk %d, Sec %d] found in cache. Cache hits = %d", trk, sct, cacheHits);

            // Indicate success
            return((cache + i) -> dataBuf);
        }
    }

    // Cache not found, return null / upate cache misses
    cacheMisses++;
    logMessage(LOG_INFO_LEVEL, "[Trk %d, Sec %d] not found in cache. Cache misses = %d", trk, sct, cacheMisses);
    return(NULL);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_log_cache_metrics
// Description  : Log the metrics for the cache 
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int fs3_log_cache_metrics(void) {
    // Calculate the hit ratio
    float hitRatio = 100.0*((float)cacheHits / ((float)cacheHits + (float)cacheMisses));

    // Log block
    logMessage(LOG_OUTPUT_LEVEL, "** FS3 Cache Metrics **");
    logMessage(LOG_OUTPUT_LEVEL, "Cache Inserts   [%d]", cacheInserts);
    logMessage(LOG_OUTPUT_LEVEL, "Cache Gets      [%d]", cacheGets);
    logMessage(LOG_OUTPUT_LEVEL, "Cache Hits      [%d]", cacheHits);
    logMessage(LOG_OUTPUT_LEVEL, "Cache Misses    [%d]", cacheMisses);
    logMessage(LOG_OUTPUT_LEVEL, "Cache Hit Ratio [%.2f%%]", hitRatio);
    
    return(0);
}