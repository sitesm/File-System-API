////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_netowork.c
//  Description    : This is the network implementation for the FS3 system.

//
//  Author         : Patrick McDaniel
//  Last Modified  : Thu 16 Sep 2021 03:04:04 PM EDT
//

// Includes
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cmpsc311_log.h>

// Project Includes
#include <fs3_driver.h>
#include <fs3_controller.h>
#include <cmpsc311_log.h>
#include <fs3_cache.h>
#include <fs3_common.h>
#include <fs3_network.h>
#include <cmpsc311_util.h>

//
//  Global data

// Defines
#define ALL_BYTES_SIZE 1032

unsigned char     *fs3_network_address = NULL; // Address of FS3 server
unsigned short     fs3_network_port = 22887;          // Port of FS3 server

// Variables for deconstructing the commandblock
uint8_t opval, retval; // Updated 'op' value | Updated 'return' value -> (0 == Passed, 1 == Failed)
uint16_t secval;       // Updated 'sector' value
uint_fast32_t trkval;  // Updated track value

// Buffers
char *allBytes, *bufBytes;

// Network variables
int socket_fh;         // Stores socket file handle
FS3CmdBlk orderedCmd;  // Used to convert data to and from network byte order 
int64_t returnValue;

// File structure for handleing internet addresses (IPv4)
struct sockaddr_in clientAddr; // Extern from <netinet/in.h>

//
// Network functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : network_fs3_syscall
// Description  : Perform a system call over the network
//
// Inputs       : cmd - the command block to send
//                ret - the returned command block
//                buf - the buffer to place received data in (Always FS3_SECTOR_SIZE)
// Outputs      : 0 if successful, -1 if failure

int network_fs3_syscall(FS3CmdBlk cmd, FS3CmdBlk *ret, void *buf){

    // Local variables 
    char *ip = "127.0.0.1";      // Default loopback address
    unsigned short port = 22887; // Server port

    // Deconstruct to find what syscall is being made
	deconstruct_fs3_cmdblock(cmd, &opval, &secval, &trkval, &retval); 
    
    ////////////////////////////////////////////////////////////////
    // 			 GET ADDRESS/PORT && SETUP STRUCTURE              //
    ////////////////////////////////////////////////////////////////

    if(opval == FS3_OP_MOUNT){
        logMessage(LOG_NETWORK_LEVEL, "Setting up ip and port");

        // Clear garbage data
        memset(&clientAddr, 0, sizeof(clientAddr));

        clientAddr.sin_family = AF_INET;      // Set family (what types of addresses the socket can communicate with)
        clientAddr.sin_port   = htons(port);  // Convert port from host byte order to server byte order

        // Convert string dot address to network address and sets it to clientAddr.sin_addr.s_addr
        if( inet_aton(ip, &(clientAddr.sin_addr)) == 0){ // Check for failure
            logMessage(LOG_NETWORK_LEVEL, "Failed to convert IPv4 address to sin_addr, exiting program");
            return(-1);
        }

        // Allocate memory (deallocated during UNMOUNT)
        allBytes = (char *)malloc(FS3_SECTOR_SIZE + 8);  // Create an area for cmd + buffer bytes (8 bytes + 1024 bytes = 1032 bytes);
        bufBytes = (char *)malloc(FS3_SECTOR_SIZE);      // Big enough for a sector of data
    
        ////////////////////////////////////////////////////////////////
        // 			             CREATE THE SOCKET                    //
        ////////////////////////////////////////////////////////////////

        logMessage(LOG_NETWORK_LEVEL, "Creating a socket");

        // Create the socket (PF_INET: Protocol family; SOCK_STREAM: client/server communication continues until a party terminates)
        socket_fh = socket(PF_INET, SOCK_STREAM, 0);
        
        // Check for failure
        if(socket_fh == -1){
            logMessage(LOG_NETWORK_LEVEL, "Failed to create a socket, exiting the program.");
            return(-1);
        }  

        logMessage(LOG_NETWORK_LEVEL, "socket_fh: %d", socket_fh);
    
        ////////////////////////////////////////////////////////////////
        // 			   CONNECT THE SOCKET TO THE SERVER               //
        ////////////////////////////////////////////////////////////////
    
        // tmp is returning -1 for some reason
        int tmp = connect(socket_fh, (const struct sockaddr *)&clientAddr, sizeof(clientAddr));

        // Connect the socket to the server
        if( tmp == -1 ){
            logMessage(LOG_NETWORK_LEVEL, "Failed to connect the socket to the server, exiting the program");
            logMessage(LOG_NETWORK_LEVEL, "Error: [%s]", strerror(errno));
            return(-1);
        }
        
        ////////////////////////////////////////////////////////////////
        // 			                CALL TO MOUNT                     //
        ////////////////////////////////////////////////////////////////

        logMessage(LOG_NETWORK_LEVEL, "[MOUNT] op code recieved");

        // Order cmdblk bytes in network order
        orderedCmd = htonll64(cmd);

        // Send the data in buf to the server for a write
        returnValue = write(socket_fh, &orderedCmd, sizeof(orderedCmd));

        // Check for failure
        if(returnValue != sizeof(orderedCmd)){
            logMessage(LOG_NETWORK_LEVEL, "Failed to mount filesystem over network, exiting program");
            return(-1);
        }

        ////////////////////////////////////////////////////////////////
        // 			       READ BACK RETURNED CMDBLK                  //
        ////////////////////////////////////////////////////////////////

        // Read back from the server
        returnValue = read(socket_fh, &orderedCmd, sizeof(orderedCmd));

        // Order the returned commandblock in host order
        orderedCmd = ntohll64(orderedCmd);

        // Do I want to do error checking on the returned commandblock here or in driver.c?
        deconstruct_fs3_cmdblock(orderedCmd, &opval, &secval, &trkval, &retval);

        // Check for failure
        if(returnValue != sizeof(orderedCmd)){
            logMessage(LOG_NETWORK_LEVEL, "Short-read of %d bytes from %d requested bytes, exiting program",
                returnValue, sizeof(orderedCmd));
            return(-1);
        }else if(retval != 0){ 
            logMessage(LOG_NETWORK_LEVEL, "Read from server failed, exiting program");
            return(-1);
        } 

        // Send commandblock to output
        *ret = orderedCmd;

        return(0);
    }
    
    ////////////////////////////////////////////////////////////////
    // 			                CALL TO TSEEK                     //
    ////////////////////////////////////////////////////////////////

    if(opval == FS3_OP_TSEEK){     
        logMessage(LOG_NETWORK_LEVEL, "[SEEK] op code recieved");

        ////////////////////////////////////////////////////////////////
        // 			            SEND COMMAND BLOCK                    //
        ////////////////////////////////////////////////////////////////

        // Order in network byte order
        orderedCmd = htonll64(cmd);

        // Send cmdblk to server
        returnValue = write(socket_fh, &orderedCmd, sizeof(orderedCmd));

        // Check for failure
        if(returnValue != sizeof(orderedCmd)){
            logMessage(LOG_NETWORK_LEVEL, "Short-write of %d bytes from %d requested bytes, exiting program",
                returnValue, sizeof(orderedCmd));
            return(-1);
        }

        ////////////////////////////////////////////////////////////////
        // 			       READ BACK RETURNED CMDBLK                  //
        ////////////////////////////////////////////////////////////////

        // Read cmdblk back from server
        returnValue = read(socket_fh, &orderedCmd, sizeof(orderedCmd));

        // Conver to host byte order
        orderedCmd = ntohll64(orderedCmd);

        // Send to output
        *ret = orderedCmd;

        // error check retCmd
        deconstruct_fs3_cmdblock(orderedCmd, &opval, &secval, &trkval, &retval);

        // Check for failure
        if(returnValue != sizeof(orderedCmd)){
            logMessage(LOG_NETWORK_LEVEL, "Short-read of %d bytes from %d requested bytes, exiting program",
                returnValue, sizeof(orderedCmd));
            return(-1);
        }else if(retval != 0){
            logMessage(LOG_NETWORK_LEVEL, "Read from server failed, exiting program");
            return(-1);
        } 

        return(0);
    }

    ////////////////////////////////////////////////////////////////
    // 			                CALL TO WRITE                     //
    ////////////////////////////////////////////////////////////////

    if(opval == FS3_OP_WRSECT){
        logMessage(LOG_NETWORK_LEVEL, "[WRITE] op code recieved");

        // Clear memory
        memset(allBytes, 0x0, ALL_BYTES_SIZE);
        orderedCmd = 0;

        ////////////////////////////////////////////////////////////////
        // 			    SEND COMMAND BLOCK + BUF TO SERVER            //
        ////////////////////////////////////////////////////////////////

        // Order cmdblk bytes in network order
        orderedCmd = htonll64(cmd);

        // Memcpy orderdBytes into allBytes
        memcpy(&allBytes[0], &orderedCmd, 8);

        // Memccpy buffer over into allBytes
        memcpy(&allBytes[8], buf, FS3_SECTOR_SIZE); 

        // Send the cmdblk to the server 
        returnValue = write(socket_fh, allBytes, ALL_BYTES_SIZE);

        // Check for failure
        if(returnValue != ALL_BYTES_SIZE){
            logMessage(LOG_NETWORK_LEVEL, "Short-write of %d bytes from %d requested bytes, exiting program",
                returnValue, ALL_BYTES_SIZE);
            return(-1);
        }

        ////////////////////////////////////////////////////////////////
        // 			       READ BACK RETURNED CMDBLK                  //
        ////////////////////////////////////////////////////////////////

        // Read back from the server
        returnValue = read(socket_fh, &orderedCmd, sizeof(orderedCmd));

        // Order the returned commandblock in host order
        orderedCmd = ntohll64(orderedCmd);

        // Send commandblock output
        *ret = orderedCmd;

        // Do I want to do error checking on the returned commandblock here or in driver.c?
        deconstruct_fs3_cmdblock(orderedCmd, &opval, &secval, &trkval, &retval);

        // Check for failure
        if(returnValue != sizeof(orderedCmd)){
            logMessage(LOG_NETWORK_LEVEL, "Short-read of %d bytes from %d requested bytes, exiting program",
                returnValue, sizeof(orderedCmd));
            return(-1);
        }else if(retval != 0){
            logMessage(LOG_NETWORK_LEVEL, "Read from server in [WRITE] failed, exiting program");
            return(-1);
        } 

        return(0);
    }

    ////////////////////////////////////////////////////////////////
    // 			                 CALL TO READ                     //
    ////////////////////////////////////////////////////////////////

    if(opval == FS3_OP_RDSECT){
        logMessage(LOG_NETWORK_LEVEL, "[READ] opcode recieved");

        // Clear memory
        memset(allBytes, 0x0, ALL_BYTES_SIZE);
        memset(bufBytes, 0x0, FS3_SECTOR_SIZE);
        orderedCmd = 0;

        ////////////////////////////////////////////////////////////////
        // 			      SEND COMMAND BLOCK TO SERVER                //
        ////////////////////////////////////////////////////////////////

        // Order cmdblk bytes in network order
        orderedCmd = htonll64(cmd);

        // Send the data in buf to the server for a write
        returnValue = write(socket_fh, &orderedCmd, sizeof(orderedCmd));

        // Check for failure
        if(returnValue != sizeof(orderedCmd)){
            logMessage(LOG_NETWORK_LEVEL, "Short-read of %d bytes from %d requested bytes, exiting program",
                returnValue, sizeof(orderedCmd));
            return(-1);
        }

        ////////////////////////////////////////////////////////////////
        // 			         READ BACK RETURNED DATA                  //
        ////////////////////////////////////////////////////////////////

        // Make call to read sector
        returnValue = read(socket_fh, allBytes, ALL_BYTES_SIZE);

        // Copy over returned commandblock
        memcpy(&orderedCmd, &allBytes[0], 8);

        // Parse buffer out
        memcpy(buf, &allBytes[8], FS3_SECTOR_SIZE); // Cut the middle man

        // Order the returned commandblock in host order
        orderedCmd = ntohll64(orderedCmd);

        // Send to output
        *ret = orderedCmd;

        // Do I want to do error checking on the returned commandblock here or in driver.c?
        deconstruct_fs3_cmdblock(orderedCmd, &opval, &secval, &trkval, &retval);

        // Check for failure
        if(returnValue != ALL_BYTES_SIZE){
            logMessage(LOG_NETWORK_LEVEL, "Short-read of %d bytes from %d requested bytes, exiting program",
                returnValue, sizeof(orderedCmd));
            return(-1);
        }else if(retval != 0){
            logMessage(LOG_NETWORK_LEVEL, "Read from server in [READ] failed, exiting program");
            return(-1);
        }

        return(0);
    }

    ////////////////////////////////////////////////////////////////
    //           TERMINATE COMMUNICATION / CLOSE SOCKET           //
    ////////////////////////////////////////////////////////////////
    
    if(opval == FS3_OP_UMOUNT){
        logMessage(LOG_NETWORK_LEVEL, "[UNMOUNT] opcode recieved");

        ////////////////////////////////////////////////////////////////
        // 			      SEND COMMAND BLOCK TO SERVER                //
        ////////////////////////////////////////////////////////////////

        // Order cmdblk bytes in network order
        orderedCmd = htonll64(cmd);

        // Send the data in buf to the server for a write
        returnValue = write(socket_fh, &orderedCmd, sizeof(orderedCmd));

        // Check for failure
        if(returnValue != sizeof(orderedCmd)){
            logMessage(LOG_NETWORK_LEVEL, "Short-read of %d bytes from %d requested bytes, exiting program",
                returnValue, sizeof(orderedCmd));
            return(-1);
        }

        ////////////////////////////////////////////////////////////////
        // 			       READ BACK RETURNED CMDBLK                  //
        ////////////////////////////////////////////////////////////////

        // Read back from the server
        returnValue = read(socket_fh, &orderedCmd, sizeof(orderedCmd));

        // Order the returned commandblock in host order
        orderedCmd = ntohll64(orderedCmd);

        // Send commandblock output
        ret = &orderedCmd;

        // Do I want to do error checking on the returned commandblock here or in driver.c?
        deconstruct_fs3_cmdblock(orderedCmd, &opval, &secval, &trkval, &retval);

        // Check for failure
        if(returnValue != sizeof(orderedCmd)){
            logMessage(LOG_NETWORK_LEVEL, "Short-read of %d bytes from %d requested bytes, exiting program",
                returnValue, sizeof(orderedCmd));
            return(-1);
        }else if(retval != 0){
            logMessage(LOG_NETWORK_LEVEL, "Read from server failed, exiting program");
            return(-1);
        }
    
        // Free buffers
        free(allBytes);
        free(bufBytes);

        // Close socket
        close(socket_fh);
        socket_fh = -1;
                
        // Return successfully
        return (0);
    }

    // opval != FS3OpCodes
    return(-1);
}

