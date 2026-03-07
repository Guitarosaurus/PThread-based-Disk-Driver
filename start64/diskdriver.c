#include "diskdevice.h"
#include "freesectordescriptorstore.h"
#include "freesectordescriptorstore_full.h"
#include "BoundedBuffer.h"
#include "voucher.h"

BoundedBuffer *write_queue, *read_queue;

void init_disk_driver(DiskDevice *dd, void *mem_start, unsigned long mem_length, FreeSectorDescriptorStore **fsds){
    // First initalise fsds
    *fsds = create_fsds();
    SectorDescriptor *sd;
    // Then populate with sector descriptors - like this?
    blocking_get_sd(fsds, sd);
    blocking_put_sd(fsds, sd);
    // Initialise two bounded buffer queues - no. of items can change
    write_queue = createBB(20);
    read_queue = create_BB(20);
}

/**
 * Return promptly, but will have delay whilst it waits for space in buffer
*/
void blocking_write_sector(SectorDescriptor *sd, Voucher **v){
    int outcome = nonblockingWriteBB(write_queue, sd);
    // No space in buffer if this is false
    if(outcome == 0){
        //Make a blocking call then
        blockingWriteBB(write_queue, sd);
    }

    // Not sure how to do voucher
}

/**
 * Return instant, 0 if buffer full
 */
void nonblocking_write_sector(SectorDescriptor *sd, Voucher **v){
    int outcome = nonblocking_writeBB(write_queue, sd);

    // Generate voucher

    return outcome;
}