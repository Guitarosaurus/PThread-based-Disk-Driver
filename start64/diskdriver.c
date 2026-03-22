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
GQueue *write_results_queue, *read_results_queue;
DiskDevice *diskDevice;
FreeSectorDescriptorStore *fsds_pointer;
BoundedBuffer *voucher_mem, *appRequest_mem;

// Trying approach where i group them and queue this instead for my read/write methods
// This could have been achieved using just voucher I believe.
typedef struct voucher {
    char *type;
    SectorDescriptor *sd;
    int *success;
} Voucher;

// Static remains outside of functions
static Voucher vouchers[20];

void * read_thread_method(void* args){
    args;
    printf("I have entered the read thread \n");
    while(1){
        printf("Waiting for items to be added to readBuffer \n");
        // Load request - currently never runs as the queue is empty
        Voucher * req = blockingReadBB(read_buffer);
        printf("Acquired the latest voucher \n");
        int success = read_sector(diskDevice, (req)->sd);
        // Return indication of success
        req->success = success;
        printf("Request completed, success: %i, result returned to voucher \n", success);
    }
    pthread_exit(NULL);
}

void * write_thread_method(void* args){
    args;
    printf("I have entered the write thread method \n");
    while(1){
        printf("Waiting for an item to enter the buffer\n");
        void * item = blockingReadBB(write_buffer);
        int success = write_sector(diskDevice, item);
        unsigned long pid = sector_descriptor_get_pid(item);
        // Free sector descriptor and return to store, set pointer to NULL
        init_sector_descriptor(item);
        blocking_put_sd(fsds_pointer, item);
        item = NULL;

        void * tuple = (void *)(pid, success);
        // Return success to buffer to be accessed by app
        gqueue_enqueue(write_results_queue, tuple);
        printf("%lu \n", pid);
    }
    pthread_exit(NULL);
}

void init_disk_driver(DiskDevice *dd, void *mem_start, unsigned long mem_length, FreeSectorDescriptorStore **fsds){
    // First initalise fsds
    printf("I have entered the disk_driver_initalisation \n");
    fsds_pointer = create_fsds();
    *fsds = fsds_pointer;
    // Then populate with sector descriptors
    create_free_sector_descriptors(*fsds, mem_start, mem_length);
    // Initialise two bounded buffer queues - no. of items can change
    write_buffer = createBB(20);
    read_buffer = createBB(20);
    write_results_queue = create_gqueue();
    read_results_queue = create_gqueue();
    appRequest_mem = createBB(20);
    voucher_mem = createBB(20);

    for(int i = 0; i < 20; i++){
        vouchers[i].sd = NULL;
        vouchers[i].success = NULL;
        vouchers[i].type = NULL;

        printf("I am attempting to write a vouchr to BB \n");

        blockingWriteBB(voucher_mem, &vouchers[i]);

        printf("Written voucher to BB \n");
    }

    //dd = construct_disk_device();
    dd = diskDevice;

    printf("About to initisalise thread \n");
    pthread_t read_thread, write_thread;
    if(pthread_create(&read_thread, NULL, (void *) &read_thread_method, NULL) != 0){
        printf("Error read thread could not be initalised \n");
    }
    if(pthread_create(&write_thread, NULL, (void *) &write_thread_method, NULL) != 0){
        printf("Error writer thread could not be initiated \n");
    };

    // Detach threads so they return resources when finished?
    pthread_detach(read_thread);
    pthread_detach(write_thread);
}

/**
 * Return promptly, but will have delay whilst it waits for space in buffer
 * 
 * Add sd to queue and return voucher that has pid of process
*/
void blocking_write_sector(SectorDescriptor *sd, Voucher **v){ 
    printf("Entered blocking-write-sector \n");
    Voucher *voucher = blockingReadBB(voucher_mem);
    // Believe this way of creating them is causing a seg fault
    voucher->type = "W";
    voucher->sd = sd;
    voucher->success = NULL;

    *v = voucher;
    
    printf("Voucher created \n");

    blockingWriteBB(write_buffer, v);
    printf("Blocking write complete for process id: %lu \n", sector_descriptor_get_pid(sd));
}

/**
 * Return instant, 0 if buffer full
 */
int nonblocking_write_sector(SectorDescriptor *sd, Voucher **v){
    printf("Entered non-blocking-write-sector \n");
    Voucher *voucher = blockingReadBB(voucher_mem);
    // Believe this way of creating them is causing a seg fault
    voucher->type = "W";
    voucher->sd = sd;
    voucher->success = NULL;

    *v = voucher;
    int result = nonblockingWriteBB(write_buffer, v);
    printf("Complete non-blocking write sector \n");
    // If unsuccessful dont return voucher and return 0
    if(result == 0) {
        v = NULL;
        return 0;
    }
    // Else return success and set voucher
    else { 
        return 1; 
    }
}

void blocking_read_sector(SectorDescriptor *sd, Voucher**v){
    Voucher *voucher = blockingReadBB(voucher_mem);
    // Believe this way of creating them is causing a seg fault
    voucher->type = "R";
    voucher->sd = sd;
    voucher->success = NULL;

    *v = voucher;
    
    blockingWriteBB(read_buffer, v);
    printf("Blocking read complete for process id: %lu \n", sector_descriptor_get_pid(sd));
}

int nonblocking_read_sector(SectorDescriptor *sd, Voucher**v){
    Voucher *voucher = blockingReadBB(voucher_mem);
    // Believe this way of creating them is causing a seg fault
    voucher->type = "R";
    voucher->sd = sd;
    voucher->success = NULL;

    *v = voucher;
    int result = nonblockingWriteBB(read_buffer, v);

    if(result == 0){
        return 0;
    } else {
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

    //int pid = v->pid;
    char *type = v->type;
    printf("Voucher being attempted to be redeemed \n");
    if(type == 'W'){
        return v->success;
    } else {
        sd = &v->sd;
        return v->success;
    }

    printf("%li", sector_descriptor_get_pid(*sd));
}