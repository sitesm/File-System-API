#ifndef FS3_CACHE_INCLUDED
#define FS3_CACHE_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_cache.h
//  Description    : This is the interface for the sector cache in the FS3
//                   filesystem.
//
//  Author         : Patrick McDaniel | Matthew Sites
//  Last Modified  : Sun 19 Nov 2021 09:36:52 AM EDT
//

// Include
#include <fs3_driver.h>
#include <fs3_controller.h>
#include <cmpsc311_log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Defines
#define FS3_DEFAULT_CACHE_SIZE 0x8; // 8 cache entries, by default
// 
// Typedef structures
typedef struct FS3Cache{
    int16_t csec;      // Keeps track of what sector in the cache 'dataBuf' is in
    int16_t ctrk;      // Keeps track of what track in the cache 'dataBuf' is in
    char *dataBuf;      // Sector data being held in the cache
    int32_t lastAccess; // Keeps track of the alst time a cache line was used
}FS3Cache;

//
// Cache Functions
int16_t fs3_lru_idx(int16_t cachelines);
    // Find the lru idx of a cache

int fs3_init_cache(uint16_t cachelines);
    // Initialize the cache with a fixed number of cache lines

int fs3_close_cache(void);
    // Close the cache, freeing any buffers held in it

int fs3_put_cache(FS3TrackIndex trk, FS3SectorIndex sct, void *buf);
    // Put an element in the cache

void * fs3_get_cache(FS3TrackIndex trk, FS3SectorIndex sct);
    // Get an element from the cache (returns NULL if not found)

int fs3_log_cache_metrics(void);
    // Log the metrics for the cache 

#endif