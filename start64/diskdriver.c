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
DiskDevice *dd;
FreeSectorDescriptorStore *fsds_pointer;
BoundedBuffer *voucher_mem, *appRequest_mem;

typedef struct voucher {
    char *type;
    Pid pid;
    SectorDescriptor *result;
    int success;
} Voucher;

// Trying approach where i group them and queue this instead for my read/write methods
// DO not think it will fix the problem
typedef struct appRequest
{
    SectorDescriptor *sd;
    Voucher *v;
} AppRequest;

void * read_thread_method(void* args){
    args;
    printf("I have entered the read thread \n");
    while(1){
        // Load request - currently never runs as the queue is empty
        AppRequest * req = blockingReadBB(read_buffer);
        int success = read_sector(dd, (req)->sd);
        // Return indication of success
        req->v->result = req->sd;
        req->v->success = success;
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
        int success = write_sector(dd, item);
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
    write_buffer = createBB(16);
    read_buffer = createBB(16);
    write_results_queue = create_gqueue();
    read_results_queue = create_gqueue();
    appRequest_mem = createBB(10);
    voucher_mem = createBB(10);

    //dd = construct_disk_device();
    dd = dd;

    printf("About to initisalise thread \n");
    pthread_t read_thread, write_thread;
    if(pthread_create(&read_thread, NULL, (void *) &read_thread_method, NULL) != 0){
        printf("Error read thread could not be initalised \n");
    }
    if(pthread_create(&write_thread, NULL, (void *) &write_thread_method, NULL) != 0){
        printf("Error writer thread could not be initiated \n");
    };
}

/**
 * Return promptly, but will have delay whilst it waits for space in buffer
 * 
 * Add sd to queue and return voucher that has pid of process
*/
void blocking_write_sector(SectorDescriptor *sd, Voucher **v){ 
    printf("Entered blocking-write-sector \n");
    v = blockingReadBB(voucher_mem);
    (*v)->type = "W";
    (*v)->pid = sector_descriptor_get_pid(sd);
    AppRequest *ar = blockingReadBB(appRequest_mem);
    ar->sd = sd;
    ar->v = *v;

    blockingWriteBB(write_buffer, ar);
}

/**
 * Return instant, 0 if buffer full
 */
int nonblocking_write_sector(SectorDescriptor *sd, Voucher **v){
    printf("Entered non-blocking-write-sector \n");
    v = blockingReadBB(voucher_mem);
    (*v)->type = "W";
    (*v)->pid = sector_descriptor_get_pid(sd);
    AppRequest *ar = blockingReadBB(appRequest_mem);
    ar->sd = sd;
    ar->v = *v;

    int result = nonblockingWriteBB(write_buffer, ar);
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
    v = blockingReadBB(voucher_mem);
    (*v)->type = "R";
    (*v)->pid = sector_descriptor_get_pid(sd);
    AppRequest *ar = blockingReadBB(appRequest_mem);
    ar->sd = sd;
    ar->v = *v;
    
    blockingWriteBB(read_buffer, ar);
}

int nonblocking_read_sector(SectorDescriptor *sd, Voucher**v){
    v = blockingReadBB(voucher_mem);
    (*v)->type = "R";
    (*v)->pid = sector_descriptor_get_pid(sd);
    AppRequest *ar = blockingReadBB(appRequest_mem);
    ar->sd = sd;
    ar->v = *v;
    int result = nonblockingWriteBB(read_buffer, ar);

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
        sd = &v->result;
        return v->success;
    }

    printf("%li", sector_descriptor_get_pid(*sd));
}