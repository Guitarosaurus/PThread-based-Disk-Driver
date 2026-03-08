#include "diskdevice.h"
#include "freesectordescriptorstore.h"
#include "freesectordescriptorstore_full.h"
#include "BoundedBuffer.h"
#include "voucher.h"
#include "diskdevice.h"
#include "diskdevice_full.h"
#include <pthread.h>
#include <stdio.h>

BoundedBuffer *write_queue, *read_queue;
DiskDevice *dd;

void init_disk_driver(DiskDevice *dd, void *mem_start, unsigned long mem_length, FreeSectorDescriptorStore **fsds){
    // First initalise fsds
    printf("I have entered the disk_driver_initalisation");
    *fsds = create_fsds();
    // Then populate with sector descriptors - like this
    // Think this needs to be a loop for the mem section, each sector descriptor = 64 bytes = 512 bytes
    for(int i = mem_start; i < (mem_length + mem_start); i+=512){
        SectorDescriptor **sd;
        blocking_get_sd(*fsds, sd);
        blocking_put_sd(*fsds, *sd);
        printf("This is the value of sd: %i", sector_descriptor_get_pid(sd));
    }
    // Initialise two bounded buffer queues - no. of items can change
    write_queue = createBB(200);
    read_queue = createBB(200);

    dd = construct_disk_device();

    // Create a thread that is always reading and one which is always writing
    pthread_t w;
    pthread_create(&w, NULL, write_sector(dd, blockingReadBB(write_queue)), NULL);
    pthread_t r;
    pthread_create(&r, NULL, read_sector(dd, blockingReadBB(read_queue)), NULL);
}


/**
 * Think this is a lazy way to do this and may not even be correct for the threads
 */
void* blockingWriteBBThreadEdition(BoundedBuffer *bb, SectorDescriptor *sd){
    blockingWriteBB(bb, sd);
    return NULL;
}

/**
 * Return promptly, but will have delay whilst it waits for space in buffer
 * 
 * Voucher = threadid and can be used to reference in further functions
*/
void blocking_write_sector(SectorDescriptor *sd, Voucher **v){ 
    pthread_t t_id;
    pthread_create(&t_id, NULL, blockingWriteBBThreadEdition(write_queue, sd), NULL);
}

/**
 * Return instant, 0 if buffer full
 */
int nonblocking_write_sector(SectorDescriptor *sd, Voucher **v){
    pthread_t t;
    pthread_create(&t, NULL, nonblockingWriteBB(read_queue, sd), NULL);
    // If unsuccessful release thread and return 0
    if(t == 0) {
        pthread_join(t, NULL);
        return 0;
    }
    // Else return success and set voucher
    else { 
        v = t;
        return 1; 
    }
}

void blocking_read_sector(SectorDescriptor *sd, Voucher**v){
    pthread_t t;
    pthread_create(&t, NULL, blockingReadBB(read_queue), NULL);
}

int nonblocking_read_sector(SectorDescriptor *sd, Voucher**v){
    pthread_t t;
    pthread_create(&t, NULL, nonblockingReadBB(read_queue, sd), NULL);

    if(t == 0){
        pthread_join(t, NULL);
        return 0;
    } else {
        v = t;
        return 1;
    }
}

/*
 * the following call is used to retrieve the status of the read or write
 * the return value is 1 if successful, 0 if not
 * the calling application is blocked until the read/write has completed
 * regardless of the status of a read, the associated SectorDescriptor is
 * returned in *sd
 * 
 * Read will return a value, write will not but will require the sector be added back to free sector descriptor store
 */
int redeem_voucher(Voucher *v, SectorDescriptor **sd){
    // pthread_join will wait for termination of thread
    void ** status;
    pthread_join(v, &status);
    
    // If status = 0, then success, determine whether it was a read or write
    
}