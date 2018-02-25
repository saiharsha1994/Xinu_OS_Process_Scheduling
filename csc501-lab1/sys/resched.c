/* resched.c  -  resched */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>

unsigned long currSP;	/* REAL sp of current process */
extern int ctxsw(int, int, int, int);
int resched_class = 0;
int epoch = 0;
/*-----------------------------------------------------------------------
 * resched  --  reschedule processor to highest priority ready process
 *
 * Notes:	Upon entry, currpid gives current process id.
 *		Proctab[currpid].pstate gives correct NEXT state for
 *			current process if other than PRREADY.
 *------------------------------------------------------------------------
 */
 int ori_sched();
 int random_sched();
 int linux_sched();
 int procproritysum();
 int procrand(int sum);
 
 void setschedclass(int sched_class){
	 resched_class = sched_class;
 }
 
 int getschedclass(){
	return resched_class;
 }
 
int resched(){
	switch(getschedclass()){
		case RANDOMSCHED :
			random_sched();
			break;
		case LINUXSCHED :
			linux_sched();
			break;	
		default :
			ori_sched();
	}
}

int ori_sched(){
	register struct	pentry	*optr;	/* pointer to old process entry */
	register struct	pentry	*nptr;	/* pointer to new process entry */

	/* no switch needed if current process priority higher than next*/

	if ( ( (optr= &proctab[currpid])->pstate == PRCURR) &&
	   (lastkey(rdytail)<optr->pprio)) {
		return(OK);
	}
	
	/* force context switch */

	if (optr->pstate == PRCURR) {
		optr->pstate = PRREADY;
		insert(currpid,rdyhead,optr->pprio);
	}

	/* remove highest priority process at end of ready list */

	nptr = &proctab[ (currpid = getlast(rdytail)) ];
	nptr->pstate = PRCURR;		/* mark it currently running	*/
#ifdef	RTCLOCK
	preempt = QUANTUM;		/* reset preemption counter	*/
#endif
	
	ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
	
	/* The OLD process returns here when resumed. */
	return OK;
}

int random_sched(){
	register struct	pentry	*optr;	/* pointer to old process entry */
	register struct	pentry	*nptr;	/* pointer to new process entry */
        
	if((optr= &proctab[currpid])->pstate == PRCURR){
		optr->pstate = PRREADY;
	        insert(currpid,rdyhead,optr->pprio);
	}
	int sum = procproritysum();
	currpid = procrand(sum);
	dequeue(currpid);
	nptr = &proctab[currpid];
	nptr->pstate = PRCURR;		/* mark it currently running	*/
#ifdef	RTCLOCK
	preempt = QUANTUM;		/* reset preemption counter	*/
#endif	
	
	ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
	
	/* The OLD process returns here when resumed. */
	return OK;
}

int linux_sched(){
	register struct	pentry	*optr;	/* pointer to old process entry */
	register struct	pentry	*nptr;	/* pointer to new process entry */
	int i = 0, maxgoodness = 0;
	optr= &proctab[currpid];
	if((optr->counter - preempt) >= 0){
		epoch -=  (optr->counter - preempt);
		optr->goodness -= (optr->counter - preempt);
		if(preempt < 0)	optr->counter = 0;
		else optr->counter = preempt;
		optr->quantum = optr->counter;
	}
	if(optr->goodness <= optr->pprio)	optr->goodness = 0;	
	if((optr= &proctab[currpid])->pstate == PRCURR){
		optr->pstate = PRREADY;	
	    insert(currpid,rdyhead,optr->pprio);
	}
	int pree = q[rdytail].qprev;
	int epoch_check = 0;
	while(pree != rdyhead){
			struct pentry *proc = &proctab[pree];
			if(proc->epochflag == 1 && proc->goodness>0)	epoch_check++;
			pree = q[pree].qprev;
		}
	if(epoch_check == 0)	epoch = 0;	
	if(epoch == 0){
		for(i=0; i<NPROC; i++){
			struct pentry *proc = &proctab[i];
			if(proc->pstate != PRFREE){
				int a = (proc->counter)/2;
				proc->quantum = a+proc->pprio;
				proc->counter = proc->quantum;
				proc->epochflag = 1;
				proc->goodness = proc->counter + proc->pprio;
				epoch += proc->counter;
			}
				else	proc->epochflag = 0;
		}
		int pre = q[rdytail].qprev;
		currpid = 0;
		while(pre != rdyhead){
			struct pentry *proc = &proctab[pre];
			if(proc->epochflag == 1){
				if(proc->goodness > 0){
					if(maxgoodness < proc->goodness){
						maxgoodness = proc->goodness;
						currpid = pre;
					}
				}
			}
			pre = q[pre].qprev;
		}
		dequeue(currpid);
		nptr = &proctab[currpid];
		nptr->pstate = PRCURR;		/* mark it currently running	*/
#ifdef	RTCLOCK
	preempt = nptr->counter;		/* reset preemption counter	*/
#endif	
	ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
	/* The OLD process returns here when resumed. */
	return OK;
	}	
	else if(epoch > 0){
		int pre = q[rdytail].qprev;
		currpid = 0;
		while(pre != rdyhead){
			struct pentry *proc = &proctab[pre];
			if(proc->epochflag == 1){
				if(proc->goodness > 0){
					//printf("inside %d",pre);
					if(maxgoodness < proc->goodness){
						maxgoodness = proc->goodness;
						currpid = pre;
					}
				}
			}
			pre = q[pre].qprev;
		}
		dequeue(currpid);
		nptr = &proctab[currpid];
		nptr->pstate = PRCURR;		/* mark it currently running	*/
#ifdef	RTCLOCK
	preempt = nptr->counter;		/* reset preemption counter	*/
#endif	
	ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
	
	/* The OLD process returns here when resumed. */
	return OK;
	}
}

int procproritysum(){
	int pre = q[rdytail].qprev;
	int sum = 0;
	while(pre != rdyhead){
		sum += q[pre].qkey;
		pre = q[pre].qprev;
	}
	return sum;	
}

int procrand(int sum){
	int curpid = 0;
	if(sum > 0){
		int ran = rand()%sum;
		int pre = q[rdytail].qprev;
		while(pre != rdyhead){
			curpid = pre;
                	if(ran > q[pre].qkey)	ran -= q[pre].qkey;
			else break;
                	pre = q[pre].qprev;
        	}	
	}
	return curpid;
}