// Include Files
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

// Project Include Files
#include <smsa_driver.h>
#include <smsa_cache.h>
#include <smsa_network.h>
#include <smsa_internal.h>
#include <cmpsc311_log.h>

// Defines
#define SMSA_DRVR_READ   0 // Tell the system to read from the virtual memory
#define SMSA_DRVR_WRITE  1 // Tell the system to read from the virtual memory

////////////////////////////////////////////////////////////////////////////////
//
// Function     : op_generate
// Description  : Creates the opcode for smsa_client_operation
//
// Inputs       : opcode, drum ID, Block ID
// Outputs      : the op for smsa_client_operation
int op_generate(int  opcode, SMSA_DRUM_ID drum, SMSA_BLOCK_ID block){
	// Create the output 	
	int output = 0;
	
	// Shift it to put it within the first 6 bits
	output = opcode << (32-6);

	// Shift it 22 bytes to plaec the drum in the correct spot
	int drum_temp = drum << 22;

	// Combine them and return it	
	output += drum_temp;
	output += block;

	return output;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : drum_number
// Description  : finds the drum number given the SMSA_VIRTUAL_ADDRESS
//
// Inputs       : SMSA_VIRTUAL_ADDRESS addr
// Outputs      : the drum number
int drum_number( SMSA_VIRTUAL_ADDRESS addr ) {
	return addr >> SMSA_DISK_ARRAY_SIZE;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_number
// Description  : finds the block number given the SMSA_VIRTUAL_ADDRESS
//
// Inputs       : SMSA_VIRTUAL_ADDRESS addr
// Outputs      : the block number
int block_number( SMSA_VIRTUAL_ADDRESS addr ) {
	
	// syphon out the drum numbers to be left over with the bytes within one block	
	int byte = addr % SMSA_DISK_SIZE;
	// Find the block
	return byte / SMSA_BLOCK_SIZE;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_vmount
// Description  : Mount the SMSA disk array virtual address space
//
// Inputs       : none
// Outputs      : -1 if failure or 0 if successful
int smsa_vmount( int lines ) {
	
	// Initializes SMSA
	int op = op_generate(SMSA_MOUNT, (SMSA_DRUM_ID)0, (SMSA_BLOCK_ID)0);	
	int err = smsa_client_operation(op, NULL);
	
	// Initialize the cache
	err = smsa_init_cache(lines);		
	
	// If there is an error return it
	if( err == (-1) ){
		return (-1);
	}

	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_vunmount
// Description  :  Unmount the SMSA disk array virtual address space
//
// Inputs       : none
// Outputs      : -1 if failure or 0 if successful
int smsa_vunmount( void )  {
	
	// UNMOUNTS the SMSA
	int op = op_generate(SMSA_UNMOUNT, (SMSA_DRUM_ID)0, (SMSA_BLOCK_ID)0);

	int err = smsa_client_operation(op, NULL);
	
	// Closes the cache out
	err = smsa_close_cache();

	// If there is an error return it
	if( err == (-1)){
		return (-1);
	}

	return (0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_vread
// Description  : Read from the SMSA virtual address space
//
// Inputs       : addr - the address to read from
//                len - the number of bytes to read
//                buf - the place to put the read bytes
// Outputs      : -1 if failure or 0 if successful
int smsa_vread( SMSA_VIRTUAL_ADDRESS addr, uint32_t len, unsigned char *buf ) {
	int bytes_left = 0; // The bytes being read through
	SMSA_DRUM_ID drum = drum_number(addr); // Drum number
	SMSA_BLOCK_ID block = block_number(addr); // Block number 
	int offset = addr & 0xFF; // Offset for a particular block
	int i; // counter

	// loop through untill we are done going through each byte neccessary
	while( bytes_left != len ) {
		unsigned char* temp = (unsigned char *)malloc(SMSA_BLOCK_SIZE); // Temporary storage
		// places and offset if neccessary or leaves it 0
		if ( block_number(addr) == block ){
			i = offset;
		} else {
			i = 0;
		}
		
		// Move the head to the correct drum, block
		smsa_client_operation( op_generate( SMSA_SEEK_DRUM, drum, block ), NULL);
		smsa_client_operation( op_generate( SMSA_SEEK_BLOCK, drum, block ), NULL);
		// Reads the data from SMSA DISK and places it into temp		
		smsa_client_operation( op_generate( SMSA_DISK_READ, drum, block), temp);
		smsa_put_cache_line(drum, block, temp);
		
		// Transfer temp into buf, byte by byte
		do {
			buf[bytes_left] = temp[i];
			bytes_left++;
			i++;
		}while( bytes_left < len && i < SMSA_BLOCK_SIZE);

		// Set the temp to null
		temp = NULL;

		// If i > block size we increment a block		
		if( i >= SMSA_BLOCK_SIZE ) {
			block++;
		}

		// If the block is greater then block size we need to increment the drum and set block to 0
		if( block >= SMSA_BLOCK_SIZE ){
			drum++;
			block = 0;
		}
	}

	return 0;

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_vwrite
// Description  : Write to the SMSA virtual address space
//
// Inputs       : addr - the address to write to
//                len - the number of bytes to write
//                buf - the place to read the read from to write
// Outputs      : -1 if failure or 0 if successful
int smsa_vwrite( SMSA_VIRTUAL_ADDRESS addr, uint32_t len, unsigned char *buf )  {
	int bytes_left = 0; // The bytes being read through
	SMSA_DRUM_ID drum = drum_number(addr); // drum number
	SMSA_BLOCK_ID block = block_number(addr); // block number
	int offset = addr & 0xFF; // offset 
	int i; // counter
	
	// loop through until we are done byte by byte
	while( bytes_left != len ) {
		// Checks if wee need to add an offset or not
		if ( block_number(addr) == block ){
			i = offset;
		} else {
			i = 0;
		}
		
		unsigned char* temp = (unsigned char*)malloc(SMSA_BLOCK_SIZE); // Temporary storage
		temp = smsa_get_cache_line(drum,block);	
		// Checks to see if already in cache, if not you can seek and read.
		if(temp == NULL){
			// Seek to the drum and block and place the head there
			smsa_client_operation( op_generate( SMSA_SEEK_DRUM, drum, block ), NULL);
			smsa_client_operation( op_generate( SMSA_SEEK_BLOCK, drum, block ), NULL);

			free(temp); // Free's it so we don't dereference null pointer
			temp = (unsigned char*)malloc(SMSA_BLOCK_SIZE); // Remalloc it

			// Reads from SMSA and puts it into temp storage
			smsa_client_operation( op_generate( SMSA_DISK_READ, drum, block), temp);
			
			// Places/updates into cache line everytime you read
			smsa_put_cache_line(drum,block, temp);
		}

		// Takes it out from temp and puts it into buf per byte
		do{
			temp[i] = buf[bytes_left];
			bytes_left++;
			i++;
		}while( bytes_left < len && i < SMSA_BLOCK_SIZE);	

		// Reset the head to drum and block
		smsa_client_operation( op_generate( SMSA_SEEK_DRUM, drum, block ), NULL);
		smsa_client_operation( op_generate( SMSA_SEEK_BLOCK, drum, block ), NULL);
		
		// Write to the temp
		smsa_client_operation( op_generate( SMSA_DISK_WRITE, drum, block), temp);

		// Set temp to null
		temp = NULL;
		// If i is past the block size we need to increment by a block
		if( i >= SMSA_BLOCK_SIZE ) {
			block++;
		}
		// If the block ahas gone through the block size then we need to increment the drum and reset block to 0
		if( block >= SMSA_BLOCK_SIZE ) {
			drum++;
			block = 0;
		}
	}

	return 0;	
}
