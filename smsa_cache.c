// Include Files
#include <stdint.h>
#include <stdlib.h>

// Project Include Files
#include <smsa_cache.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>

//
// Static Global Variables
SMSA_CACHE_LINE  *smsa_cache = NULL;	    // This is the cache itself
uint32_t	  smsa_cache_entries = 0;   // This is the size of the cache

// Functional Prototypes
int least_recently_used( void );

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_init_cache
// Description  : Setup the block cache
//
// Inputs       : lines - the number of cache entries to create
// Outputs      : 0 if successful test, -1 if failure

int smsa_init_cache( uint32_t lines ) {

    // Create and initialize the cache
    logMessage( LOG_INFO_LEVEL, "Cache initialization with [%d] lines", lines );
    smsa_cache = calloc( lines, sizeof(SMSA_CACHE_LINE) );
    smsa_cache_entries = lines;

    // Return succesfully
    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_close_cache
// Description  : Clear cache and free associated memory
//
// Inputs       : none
// Outputs      : 0 if successful test, -1 if failure

int smsa_close_cache( void ) {

    // Setup local variables
    int i;

    // Walk the cache and clean up the entries
    for ( i=0; i<smsa_cache_entries; i++ ) {

	// If there is a cache line present, release it
	if ( smsa_cache[i].line != NULL ) {
	    free( smsa_cache[i].line );
	    smsa_cache[i].line = NULL;
	}
    }

    // Now free the cache itself, log it
    free( smsa_cache );
    smsa_cache_entries = 0;
    logMessage( LOG_INFO_LEVEL, "Cache closed : [%d] lines", smsa_cache_entries );

    // Return succesfully
    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_get_cache_line
// Description  : Check to see if the cache entry is available
//
// Inputs       : drm - the drum ID to look for
//                blk - the block ID to lookm for
// Outputs      : pointer to cache entry if found, NULL otherwise

unsigned char *smsa_get_cache_line( SMSA_DRUM_ID drm, SMSA_BLOCK_ID blk ) {

    // Setup local variables
    int i;

    // Walk the cache and clean up the entries
    for ( i=0; i<smsa_cache_entries; i++ ) {

	// If there is a cache line present and right one
	if  ( (smsa_cache[i].line != NULL) && 
	      (smsa_cache[i].drum == drm) && (smsa_cache[i].block == blk) ) {

	    // Reset the "recently used timer", return cache line
	    gettimeofday( &smsa_cache[i].used, NULL );
	    logMessage( LOG_INFO_LEVEL, "Cache hit [%lu,%lu]", drm, blk );
	    return( smsa_cache[i].line );

	}
    }

    // Cache miss log 
    logMessage( LOG_INFO_LEVEL, "Cache miss [%lu,%lu]", drm, blk );

    // Return succesfully
    return( NULL );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_put_cache_line
// Description  : Put a new line into the cache
//
// Inputs       : drm - the drum ID to place
//                blk - the block ID to lplace
//                buf - the buffer to put into the cache
// Outputs      : 0 if successful, -1 otherwise

int smsa_put_cache_line( SMSA_DRUM_ID drm, SMSA_BLOCK_ID blk, unsigned char *buf ) {

    // Setup some local variables
    int lru;

    // First check to make sure it is not already in cache
    if ( smsa_get_cache_line( drm, blk ) != NULL ) {
	logMessage( LOG_INFO_LEVEL, "Attempting to cache alread cached line [%lu,%lu], ignoring.", drm, blk );
	return( 0 );
    }

    // Log the cache add
    logMessage( LOG_INFO_LEVEL, "Caching line [%lu,%lu]", drm, blk );

    // Get the LRU slot to place this in, free as needed
    lru = least_recently_used();
    if ( smsa_cache[lru].line != NULL ) {

	// Log then eject cache entry
	logMessage( LOG_INFO_LEVEL, "Ejecting cache line [%lu,%lu]", 
		    smsa_cache[lru].drum,  smsa_cache[lru].block);
	free( smsa_cache[lru].line );
	smsa_cache[lru].line = NULL;
    }

    // Set the new entry
    smsa_cache[lru].drum = drm;
    smsa_cache[lru].block = blk;
    smsa_cache[lru].line = buf;
    gettimeofday( &smsa_cache[lru].used, NULL );

    // Return succesfully
    return( 0 );
}

//
// Local Support Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_least_recently_used
// Description  : Find the least recently used cache entry (or unused as need)
//
// Inputs       : none
// Outputs      : index if least recently used 

int least_recently_used( void ) {

    // Setup local variables
    int i, lru;

    // Walk the cache and see if any index unused
    for ( i=0; i<smsa_cache_entries; i++ ) {

	// Current index unused, use it
	if ( smsa_cache[i].line == NULL ) {
	    return( i );
	}
    }

    // Walk again looking for LRU
    lru = 0;
    for ( i=1; i<smsa_cache_entries; i++ ) {
	if ( compareTimes(&smsa_cache[lru].used, &smsa_cache[i].used) < 0 ) {
	    lru = i;
	}
    }

    // Return the index of the least recently used 
    return( lru );
}
