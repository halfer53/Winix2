/**
 * Process Management for WINIX.
 *
 * Revision History:
 *  2016-09-19		Paul Monigatti			Original
 *  2016-11-20		Bruce Tan			Modified
 **/

#ifndef _PROC_H_
#define _PROC_H_ 1

//Process & Scheduling
#define PROC_NAME_LEN			20
#define NUM_PROCS				20
#define NUM_QUEUES				5
#define IDLE_PRIORITY			4
#define USER_PRIORITY			3
#define KERNEL_PROCESS_PRIORITY			1
#define SYSTEM_PRIORITY			0
#define PROCESS_STATE_LEN		18

//Process Defaults
#define DEFAULT_FLAGS			0
#define PROTECTION_TABLE_LEN	32
#define DEFAULT_STACK_SIZE		1024
#define DEFAULT_HEAP_SIZE		1024
#define DEFAULT_CCTRL			0xff9
#define DEFAULT_STACK_POINTER			0x00000
#define USER_CCTRL			0x8 //OKU is set to 0
#define DEFAULT_RBASE			0x00000
#define DEFAULT_PTABLE			0x00000
#define DEFAULT_QUANTUM			100
#define DEFAULT_REG_VALUE		0xffffffff
#define DEFAULT_MEM_VALUE		0xffffffff
#define DEFAULT_RETURN_ADDR		0x00000000
#define DEFAULT_PROGRAM_COUNTER	0x00000000

//Process Flags
#define SENDING					0x0001
#define RECEIVING				0x0002
#define REJECT				0x0003

/**
 * State of a process.
 **/
typedef enum { DEAD, INITIALISING, RUNNABLE, ZOMBIE } proc_state_t;

#define PROC_FILEP_NR	8

/**
 * Process structure for use in the process table.
 *
 * Note: 	Do not update this structure without also
 * 			updating the definitions in "wramp.s"
 **/
typedef struct proc {
	/* Process State */
	unsigned long regs[14];	//Register values
	unsigned long *sp;
	void *ra;
	void (*pc)();
	void *rbase;
	unsigned long *ptable;
	unsigned long cctrl;

	/* Protection */
	unsigned long protection_table[PROTECTION_TABLE_LEN];

	/* Scheduling */
	int priority;		//Default priority
	int quantum;		//Timeslice length
	int ticks_left;		//Timeslice remaining
	struct proc *next;	//Next pointer

	/* IPC */
	struct proc *sender_q;	//Head of process queue waiting to send to this process
	struct proc *next_sender; //Link to next sender in the queue
	// message_t *message;	//Message buffer;

	/* Accounting */
	int time_used;		//CPU time used

	/* Metadata */
	char name[PROC_NAME_LEN];		//Process name
	proc_state_t state;	//Current process state
	int flags;

	/* Process Table Index */
	int pid;		//Index in the process table

	unsigned long length;

	int parent_pid;

	void *heap_break;

	filp_t* fp_filp[PROC_FILEP_NR];
	inode_t *fp_rootdir;
	inode_t *fp_workdir;
} proc_t;

extern proc_t proc_table[NUM_PROCS];
extern proc_t *ready_q[NUM_QUEUES][2];


/**
 * Pointer to the current process.
 **/
extern proc_t *current_proc;

#endif
