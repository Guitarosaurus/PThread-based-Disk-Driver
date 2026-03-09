#include "diskdevice.h"
#include "freesectordescriptorstore.h"
#include "freesectordescriptorstore_full.h"
#include "sectordescriptorcreator.h"
#include "BoundedBuffer.h"
#include "voucher.h"
#include "diskdevice.h"
#include "diskdevice_full.h"
#include "generic_queue.h"
#include <pthread.h>
#include <stdio.h>

BoundedBuffer *write_buffer, *read_buffer;
GQueue *write_queue, *read_queue;
DiskDevice *dd;

typedef struct voucher {
    Pid pid;
} Voucher;


void init_disk_driver(DiskDevice *dd, void *mem_start, unsigned long mem_length, FreeSectorDescriptorStore **fsds){
    // First initalise fsds
    printf("I have entered the disk_driver_initalisation");
    *fsds = create_fsds();
    // Then populate with sector descriptors - like this
    // Uses methof in sectordescriptorcreator
    create_free_sector_descriptors(*fsds, mem_start, mem_length);
    // Initialise two bounded buffer queues - no. of items can change
    write_buffer = createBB(64);
    read_buffer = createBB(64);

    dd = construct_disk_device();
}

/**
 * Return promptly, but will have delay whilst it waits for space in buffer
 * 
 * Add sd to queue and return voucher that has pid of process
*/
void blocking_write_sector(SectorDescriptor *sd, Voucher **v){ 
    gqueue_enqueue(write_queue, sd);
    (*v)->pid = sector_descriptor_get_pid(sd);
}

/**
 * Return instant, 0 if buffer full
 */
int nonblocking_write_sector(SectorDescriptor *sd, Voucher **v){
    int result = gqueue_enqueue(write_queue, sd);
    // If unsuccessful dont return voucher and return 0
    if(result == 0) {
        return 0;
    }
    // Else return success and set voucher
    else { 
        (*v)->pid = sector_descriptor_get_pid(sd);
        return 1; 
    }
}

void blocking_read_sector(SectorDescriptor *sd, Voucher**v){
    gqueue_enqueue(read_queue, sd);
    (*v)->pid = sector_descriptor_get_pid(sd);
}

int nonblocking_read_sector(SectorDescriptor *sd, Voucher**v){
    int result = gqueue_enqueue(read_queue, sd);

    if(result == 0){
        return 0;
    } else {
        (*v)->pid = sector_descriptor_get_pid(sd);
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

    printf("%li", sector_descriptor_get_pid(*sd));
}