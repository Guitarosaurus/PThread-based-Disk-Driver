#include "diskdevice.h"
#include "freesectordescriptorstore.h"
#include "freesectordescriptorstore_full.h"
#include "BoundedBuffer.h"
#include "voucher.h"
#include <pthread.h>

BoundedBuffer *write_queue, *read_queue;

void init_disk_driver(DiskDevice *dd, void *mem_start, unsigned long mem_length, FreeSectorDescriptorStore **fsds){
    // First initalise fsds
    *fsds = create_fsds();
    // Then populate with sector descriptors - like this
    // Think this needs to be a loop for the mem section, each sector descriptor = 64 bytes = 512 bytes
    for(int i = mem_start; i < (mem_length + mem_start); i+=512){
        SectorDescriptor *sd;
        blocking_get_sd(fsds, sd);
        blocking_put_sd(fsds, sd);
    }
    // Initialise two bounded buffer queues - no. of items can change
    write_queue = createBB(20);
    read_queue = create_BB(20);
}


/**
 * Think this is a lazy way to do this and may not even be correct for the threads
 */
void* blockingWriteBBThreadEdition(BoundedBuffer *bb, SectorDescriptor *sd){
    blockingWriteBB(write_queue, sd);
    return;
}

/**
 * Return promptly, but will have delay whilst it waits for space in buffer
 * 
 * Voucher = threadid and can be used to reference in further functions
*/
void blocking_write_sector(SectorDescriptor *sd, Voucher **v){ 
    pthread_create(v, NULL, blockingWriteBBThreadEdition(write_queue, sd), NULL);
}

/**
 * Return instant, 0 if buffer full
 */
int nonblocking_write_sector(SectorDescriptor *sd, Voucher **v){
    pthread_create(v, NULL, nonblockingWriteBB(read_queue, sd), NULL);
    // If unsuccessful reset v and return 0
    if(v == 0) {
        v = NULL;
        return 0;
    }
    // Else return success
    else { return 1; }
}

void blocking_read_sector(SectorDescriptor *sd, Voucher**v){
    pthread_create(v, NULL, blockingReadBB(read_queue), NULL);
}

int nonblocking_read_sector(SectorDescriptor *sd, Voucher**v){
    pthread_create(v, NULL, nonblockingReadBB(read_queue, v), NULL);

    if(v == 0){
        v = NULL;
        return 0;
    } else {
        return 1;
    }
}