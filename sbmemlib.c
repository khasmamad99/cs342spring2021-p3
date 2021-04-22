
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>

// Define a name for your shared memory; you can give any name that start with a slash character; it will be like a filename.
#define MEMNAME "/buddymem"
#define MAX_M 18
#define MIN_M 13
#define MAX_PROC 10

// Define semaphore(s)


// Define your stuctures and variables.
struct proc_info {
    int id;
    void *start_addr;
};
struct proc_info processes[MAX_PROC];
struct block_attr {
    int tag;
    int kval;
    uintptr_t loc;
    uintptr_t next;
    uintptr_t prev;
};

struct block_attr avail[MAX_M];
void *start_ptr;
int fd, m;
int check_access();
int sbmem_remove();


// int main() {
//     int fd;
//     if ((fd = shm_open(MEMNAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | O_EXCL)) == -1) {
//         if (errno == EEXIST) {
//             remove(MEMNAME);
//             if ((fd = shm_open(MEMNAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | O_EXCL)) == -1) {
//                 printf("shm_open failed\n");
//                 return -1;
//             }
//         } else {
//             printf("shm_open failed\n");
//             return -1;
//         }
//     }
//     printf("shm_open finished\n");
//     int size = 256;
//     if (ftruncate(fd, size) == -1) {
//         printf("ftrunctate failed\n");
//         return -1;
//     }
//     printf("ftruncate finished\n");


//     struct block_attr *addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
//     if (addr == (void *) -1) {
//         printf("mmap failed\n");
//         return -1;
//     }

//     addr->tag = 1;
//     printf("addr->tag = %d\n", addr->tag);
//     printf("addr = %p\n", addr);

//     sbmem_remove();
//     printf("removed\n");

//     return 0;    
// }


int sbmem_init(int segmentsize)
{
    printf ("sbmem init called"); // remove all printfs when you are submitting to us.  

    if (segmentsize < pow(2, MIN_M) || segmentsize > pow(2, MAX_M)) {
        printf("segmentsize should be between 32KB (2^13B) and 256KB (2^18B)\n");
        return -1;
    }

    if ((fd = shm_open(MEMNAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | O_EXCL)) == -1) {
        if (errno == EEXIST) {
            shm_unlink(MEMNAME);
            if ((fd = shm_open(MEMNAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | O_EXCL)) == -1) {
                printf("shm_open failed\n");
                return -1;
            }
        } else {
            printf("shm_open failed\n");
            return -1;
        }
    }

    m = (int) log2(segmentsize) + 1;
    int size = (int) pow(2, m);
    if (ftruncate(fd, size) == -1) {
        printf("ftrunctate failed\n");
        return -1;
    }

    // initialize the list of processes to -1 (no processes currently)
    for (int i = 0; i < MAX_PROC; i++) {
        processes[i].id = -1;
        processes[i].start_addr = NULL;
    }

    for (int i = 0; i <= m; i++) {
        avail[i].tag = 0;
        avail[i].kval = i;
        avail[i].loc = -1;
        avail[i].prev = avail[i].next = avail[i].loc;
    }

    start_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (start_ptr == (void *) -1) {
        printf("mmap failed\n");
        return -1;
    }

    // struct block_attr *cur_block_ptr;
    struct block_attr *cur_block_ptr = start_ptr;
    cur_block_ptr->tag = 1;
    cur_block_ptr->kval = m;
    // cur_block_ptr->addr = cur;
    cur_block_ptr->prev = cur_block_ptr->next = avail[m].loc;
    avail[m].next = 0;
    avail[m].prev = 0;
    
    return (0);
}

int sbmem_remove()
{
    shm_unlink(MEMNAME);

    // remove the semaphores

    return (0); 
}

int sbmem_open()
{
    // find an empty slot
    printf("ALALAALLA\n");
    int i;
    for (i = 0; i < MAX_PROC && processes[i].id != -1; i++)
        ;
    if (i == MAX_PROC) {
        printf("i == MAX_PRO\n");
        return -1;
    }
    
    processes[i].id = getpid();
    void *addr = mmap(NULL, (int) pow(2, m), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == (void *) -1) {
        printf("mmap failed\n");
        return -1;
    }
    processes[i].start_addr = addr;

    return (0); 
}


void *sbmem_alloc (int size)
{
    int i, j;
    if ((i = check_access()) == -1)
        return NULL;

    void *start_addr = processes[i].start_addr;

    int k = (int) log2(size) + 1;
    // find block
    for (j = m; j >= k && avail[j].next != avail[j].loc; j--)
        ;
    
    // no such block exists
    if (j < k)
        return NULL;

    // remove from the list
    uintptr_t L, P;
    struct block_attr *L_ptr, *P_ptr;
    L = avail[j].prev;
    L_ptr = start_addr + L;
    P = L_ptr->prev;
    P_ptr = start_addr + P;
    avail[j].prev = P;
    P_ptr->next = avail[j].loc;
    L_ptr->tag = 0;

    while (1) {
        if (j == k) {
            // no split required
            // void *addr = mmap(NULL, (int) pow(2, m), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            // if (addr == (void *) -1) {
            //     printf("mmap failed\n");
            //     return NULL;
            // }
            // do not allow the process to access the block information
            void *return_addr = start_addr + L + sizeof(struct block_attr);
            return return_addr;
        }
        // split required
        j--;
        P = L + (int) pow(2, j);
        struct block_attr *P_ptr = start_addr + P;
        P_ptr->tag = 1;
        P_ptr->kval = j;
        P_ptr->next = P_ptr->prev = avail[j].loc;
        avail[j].next = avail[j].prev = P;
    }

    return (NULL);
}


void sbmem_free (void *p)
{
    // account for the block information
    int i;
    if ((i = check_access()) != -1) {
        void *start_ptr = processes[i].start_addr;
        uintptr_t L, P;
        int k;
        struct block_attr *L_ptr, *P_ptr, *P_prev_ptr, *P_next_ptr;

        L = (uintptr_t) p - (uintptr_t) start_ptr - sizeof(struct block_attr);
        L_ptr = p - sizeof(struct block_attr);
        k = L_ptr->kval;

        while(1) {
            // is buddy available?
            P = L ^ (int) pow(2, k);
            P_ptr = start_ptr + P;

            if (k == m || P_ptr->tag == 0 || (P_ptr->tag == 1 && P_ptr->kval != k)) {
                // put L on list
                L_ptr->tag = 1;
                P = avail[k].next;
                P_ptr = start_ptr + P;
                L_ptr->next = P;
                P_ptr->prev = L;
                L_ptr->kval = k;
                L_ptr->prev = avail[k].loc;
                avail[k].next = L;
                break;
            } else {
                // combine with buddy
                // remove P
                P_prev_ptr = start_ptr + P_ptr->prev;
                P_prev_ptr->next = P_ptr->next;

                P_next_ptr = start_ptr + P_ptr->next;
                P_next_ptr->prev = P_ptr->prev;

                k++;
                if (P < L)
                    L_ptr->loc = P;
            }
        }
    }
 
}

int sbmem_close()
{
    int i = check_access();
    if (i == -1)
        return -1;

    processes[i].id = -1;
    processes[i].start_addr = NULL;

    return (0); 
}

int check_access() {
    int i;
    int pid = getpid();
    for (i = 0; i < MAX_PROC && processes[i].id != pid; i++)
        ;
    if (i == MAX_PROC)
        return -1;

    return i;
}
