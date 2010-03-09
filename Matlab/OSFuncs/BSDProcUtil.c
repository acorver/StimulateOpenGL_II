/*
 *  BSDProcUtil.c
 *  StimulateOpenGL_II
 *
 *  Created by calin on 3/8/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */

#include "BSDProcUtil.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <string.h>

typedef struct kinfo_proc kinfo_proc;

int GetBSDProcessList(struct kinfo_proc **procList, size_t *procCount)
// Returns a list of all BSD processes on the system.  This routine
// allocates the list and puts it in *procList and a count of the
// number of entries in *procCount.  You are responsible for freeing
// this list (use "free" from System framework).
// On success, the function returns 0.
// On error, the function returns a BSD errno value.
{
    int                 err;
    kinfo_proc *        result;
    bool                done;
    static const int    name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    // Declaring name as const requires us to cast it when passing it to
    // sysctl because the prototype doesn't include the const modifier.
    size_t              length;
	
    assert( procList != NULL);
    //assert(*procList == NULL);
    assert(procCount != NULL);
	
    *procCount = 0;
	
    // We start by calling sysctl with result == NULL and length == 0.
    // That will succeed, and set length to the appropriate length.
    // We then allocate a buffer of that size and call sysctl again
    // with that buffer.  If that succeeds, we're done.  If that fails
    // with ENOMEM, we have to throw away our buffer and loop.  Note
    // that the loop causes use to call sysctl with NULL again; this
    // is necessary because the ENOMEM failure case sets length to
    // the amount of data returned, not the amount of data that
    // could have been returned.
	
    result = NULL;
    done = false;
    do {
        assert(result == NULL);
		
        // Call sysctl with a NULL buffer.
		
        length = 0;
        err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1,
					 NULL, &length,
					 NULL, 0);
        if (err == -1) {
            err = errno;
        }
		
        // Allocate an appropriately sized buffer based on the results
        // from the previous call.
		
        if (err == 0) {
            result = malloc(length);
            if (result == NULL) {
                err = ENOMEM;
            }
        }
		
        // Call sysctl again with the new buffer.  If we get an ENOMEM
        // error, toss away our buffer and start again.
		
        if (err == 0) {
            err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1,
						 result, &length,
						 NULL, 0);
            if (err == -1) {
                err = errno;
            }
            if (err == 0) {
                done = true;
            } else if (err == ENOMEM) {
                assert(result != NULL);
                free(result);
                result = NULL;
                err = 0;
            }
        }
    } while (err == 0 && ! done);
	
    // Clean up and establish post conditions.
	
    if (err != 0 && result != NULL) {
        free(result);
        result = NULL;
    }
    *procList = result;
    if (err == 0) {
        *procCount = length / sizeof(kinfo_proc);
    }
	
    assert( (err == 0) == (*procList != NULL) );
	
    return err;
}

int IsBSDProcessRunning(const char *exe_name)
{
	struct kinfo_proc *procs=NULL;
	size_t count=0, i;
	int ret=0;
	
	if (!GetBSDProcessList(&procs, &count)) {
		for (i = 0; i < count && !ret; ++i) {
			if (!strncmp(procs[i].kp_proc.p_comm,exe_name,MAXCOMLEN)) 
				ret = 1;
		}
	}
	free(procs);
	return ret;	
}

#ifdef TEST_PROCUTIL
int main(void) {
	struct kinfo_proc *procs=NULL;
	size_t count=0;
	int ret, i;
	
	if ((ret=GetBSDProcessList(&procs, &count))) {
		fprintf(stderr,"error: %s\n",strerror(ret));
		return ret;
	}
	for (i = 0; i < (int)count; ++i) {
		printf("pid: %d comm: %s\n",(int)procs[i].kp_proc.p_pid,procs[i].kp_proc.p_comm);
	}
	free(procs);
	return 0;
}
#endif
