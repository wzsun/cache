// Include Files
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>

// Project Include Files
#include <smsa_network.h>
#include <smsa.h>
#include <smsa_internal.h>
#include <smsa_server.c>

// Global variables
int server_socket;
// Functional Prototypes

//
// Functions
int clientConnect();
int clientDisconnect();

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_client_operation
// Description  : This the client operation that sends a reques to the SMSA
//                server.   It will:
//
//                1) if mounting make a connection to the server 
//                2) send any request to the server, returning results
//                3) if unmounting, will close the connection
//
// Inputs       : op - the operation code for the command
//                block - the block to be read/writen from (READ/WRITE)
// Outputs      : 0 if successful, -1 if failure
int smsa_client_operation( uint32_t op, unsigned char *block ) {
	unsigned char buf[SMSA_NET_HEADER_SIZE + SMSA_BLOCK_SIZE] = {0};
	int len = SMSA_NET_HEADER_SIZE; // Length
	int ret; // Return value
	int op_; // Alternate OP Code

	// If the opcode is mount we mount it
	if(SMSA_OPCODE(op) == SMSA_MOUNT){
		if(clientConnect() != 0){
			return(-1);	
		}	
	}
	
	// If op is write we write w/ buffer
	if(SMSA_DISK_WRITE == SMSA_OPCODE(op)){

		// Putting it into network form
		len += SMSA_BLOCK_SIZE;
		len = htons(len);
		ret = htons(0);
		op_ = htonl(op);
	
		// Placing the data in
		*(uint16_t*)buf = len;
		*(uint32_t*)(buf+2) = op_;
		*(uint16_t*)(buf+6) = ret;

		// Put block into the buffer
		int i;
		for(i = SMSA_NET_HEADER_SIZE; i < SMSA_NET_HEADER_SIZE + SMSA_BLOCK_SIZE; i++){
			buf[i] = block[i-SMSA_NET_HEADER_SIZE];		
		}
	
		// Write to the server, if it errors return -1
		if(write(server_socket, buf, SMSA_BLOCK_SIZE+SMSA_NET_HEADER_SIZE) != SMSA_BLOCK_SIZE+SMSA_NET_HEADER_SIZE){
			return(-1);
		}	

	} else {
		// This occurs when we write w/o a buffer, you always want to senf a packet
		len = htons(len);
		ret = htons(0);
		op_ = htonl(op);

		*(uint16_t*)buf = len;
		*(uint32_t*)(buf+2) = op_;
		*(uint16_t*)(buf+6) = ret;
	
		write(server_socket, buf, SMSA_NET_HEADER_SIZE);
	}

	// When we send a packet we want to recieve a packet back
	// Recieve packet
	// If the op code is read we read it with a buffer
	if(SMSA_DISK_READ == SMSA_OPCODE(op)){

		// We first read for SMSA_NET_HEADER_SIZE, if the length is > SMSA_NET_HEADER_SIZE we read the block
		if(read(server_socket, buf, SMSA_NET_HEADER_SIZE) != SMSA_NET_HEADER_SIZE){
			return(-1);	
		}
		
		// Takes out the data from buffer that we got from the socket
		len = ntohs(*(uint16_t*)buf);
		op_ = ntohl(*(uint32_t*)(buf+sizeof(uint16_t)));
		ret = ntohs(*(uint16_t*)(buf+sizeof(uint16_t)+sizeof(uint32_t)));
		
		// If the length is > SMSA_NET_HEADER_SIZE we have to read the block
		if(len > SMSA_NET_HEADER_SIZE){
			if(read(server_socket, block, len-SMSA_NET_HEADER_SIZE) != SMSA_BLOCK_SIZE){
				return(-1);		
			}
		}
	} else {
		// If the opcode is not a disk read then we still want to read a packet
		
		// We read the packet of SMSA_NET_HEADER_SIZE sent to us
		if(read(server_socket, buf, SMSA_NET_HEADER_SIZE) != SMSA_NET_HEADER_SIZE){
			return(-1);	
		}		
		
		// Take out the data from the buffer
		op_ = ntohl(*(uint32_t*)(buf+sizeof(uint16_t)));
		ret = ntohs(*(uint16_t*)(buf+sizeof(uint16_t)+sizeof(uint32_t)));
		block = NULL;
	} 

	// If return value is not 0 we errored somewhere
	if(ret != 0){
		return(-1);	
	}

	// If the op didn't match then somewhere between sending and recieving we messed up
	if(op != op_){
		return(-1);	
	}

	// if opcode is SMSA_UNMOUNT then we just unmount
	if(SMSA_OPCODE(op) == SMSA_UNMOUNT){
		if(clientDisconnect() != 0  ){
			return(-1);		
		}	
	}

	return(0);
}

int clientConnect(){
	// Variables
	char *ip = SMSA_DEFAULT_IP;
	unsigned short port = SMSA_DEFAULT_PORT;
	struct sockaddr_in caddr;

	// Setup the address information
	caddr.sin_family = AF_INET;
	caddr.sin_port = htons(port);
	if ( inet_aton(ip, &caddr.sin_addr) == 0 ) {
		return( -1 );
	}

	// Creates a socket
	server_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (server_socket == -1) {
		//printf( "Error on socket creation [%s]\n", strerror(errno));
		return(-1);
	}

	if ( connect(server_socket, (const struct sockaddr *)&caddr, sizeof(struct sockaddr)) == -1 ) { 
 		return( -1 );
	}

	return(0);
}

int clientDisconnect(){
	close(server_socket);
	server_socket = -1;
	return(0);
}
