/**
 * Process scheduling routines for WINIX.
 *
 * Revision History:
 *  2016-09-19		Paul Monigatti			Original
 *  2016-11-20		Bruce Tan			Modified
 **/

#include "winix.h"

//Linked lists are defined by a head and tail pointer.

//Process table
struct proc proc_table[NUM_PROCS];

//Scheduling queues
struct proc *ready_q[NUM_QUEUES][2];

//blocking queues
struct proc *block_q[2];

//Entries in the process table that are not in use
static struct proc *free_proc[2];

//The currently-running process
struct proc *current_proc;


/**
 * How is the process image aligned?
 *
 * Each process image is aligned as shown below
 *
 * Text segment
 * Data segment
 * Bss segment
 * Stack
 * Heap
 *
 * In the struct proc, rbase points to the beggining of 
 * the process image.
 *
 * stack_top is the physical pointer that points to the 
 * physical address of the beginning of the stack. Stack
 * cannot be extended
 *
 * heap_break points to the physical address of the current
 * user heap break
 *
 * heap_bottom, points to the end of the
 * process image. Heap can be extended by extending heap_bottom.
 *
 * Note that all those segments are continous, so whenever a fork
 * is called, we can simply compute the number of pages this process
 * is occuping by doing heap_bottom + 1 - rbase. NB heap_bottom points
 * to the end of the page.
 * 
 */



//print out the list of processes currently in the ready_q
//and the currently running process
void print_runnable_procs() {
	int i;
	struct proc *curr;
	kprintf("NAME     PID PPID RBASE      PC         STACK      HEAP       PROTECTION   FLAGS\n");
	for (i = 0; i < NUM_PROCS; i++) {
		curr = &proc_table[i];
		if(curr->i_flags & PROC_IN_USE && curr->state != ZOMBIE)
			printProceInfo(curr);
	}
}

//print the process state
void printProceInfo(struct proc* curr) {
	int ptable_idx = PADDR_TO_PAGED(curr->rbase)/32;
	kprintf("%-08s %-03d %-04d 0x%08x 0x%08x 0x%08x 0x%08x %d 0x%08x %d\n",
	        curr->name,
	        curr->proc_nr,
			curr->parent,
	        curr->rbase,
	        get_physical_addr(curr->pc,curr),
	        get_physical_addr(curr->sp,curr),
	        curr->heap_break,
			ptable_idx,
	        curr->ptable[ptable_idx],
			curr->s_flags);
}

/**
 * Gets a pointer to a process.
 *
 * Parameters:
 *   proc_nr		The process to retrieve.
 *
 * Returns:			The relevant process, or NULL if it does not exist.
 **/
struct proc *get_proc(int proc_nr) {
	struct proc *p;
	if (IS_PROCN_OK(proc_nr)){
		p = &proc_table[proc_nr];
		return p;
		// if(p->state != ZOMBIE && p->state != DEAD)
		// 	return p;
	}
	return NULL;
}

/**
 * similar to get_proc(), but this one makes sure the 
 * returning proc is runnable
 * @param  proc_nr 
 * @return         
 */
struct proc *get_running_proc(int proc_nr){
	struct proc *p = get_proc(proc_nr);
	if(p->state != RUNNABLE)
		return NULL;
	return p;
}

/**
 * Adds a proc to the tail of a list.
 *
 * Parameters:
 *   q		An array containing a head and tail pointer of a linked list.
 *   proc	The proc struct to add to the list.
 **/
void enqueue_tail(struct proc **q, struct proc *proc) {
	if (q[HEAD] == NULL) {
		q[HEAD] = q[TAIL] = proc;
	}
	else {
		q[TAIL]->next = proc;
		q[TAIL] = proc;
	}
	proc->next = NULL;
}

/**
 * Adds a proc to the head of a list.
 *
 * Parameters:
 *   q		An array containing a head and tail pointer of a linked list.
 *   proc	The proc struct to add to the list.
 **/
void enqueue_head(struct proc **q, struct proc *proc) {
	struct proc *curr = NULL;
	if (q[HEAD] == NULL) {
		proc->next = NULL;
		q[HEAD] = q[TAIL] = proc;
	}
	else {
		proc->next = q[HEAD];
		q[HEAD] = proc;
	}
}

/**
 * Removes the head of a list.
 *
 * Parameters:
 *   q		An array containing a head and tail pointer of a linked list.
 *
 * Returns:
 *   The proc struct that was removed from the head of the list
 *   NULL if the list is empty.
 **/
struct proc *dequeue(struct proc **q) {
	struct proc *p = q[HEAD];

	if (p == NULL)
		return NULL;

	if (q[HEAD] == q[TAIL]) { //Last item
		q[HEAD] = q[TAIL] = NULL;
	}
	else { //At least one remaining item
		q[HEAD] = p->next;
	}
	p->next = NULL;
	return p;
}

/**
 * remove the process from the scheduling queue
 * @param  h process to be removed
 * @return   0 on success, -1 if not
 */
int dequeue_schedule( struct proc *h) {
	struct proc *curr;
	struct proc *prev = NULL;
	struct proc ** q = ready_q[h->priority];

	curr = q[HEAD];

	if (curr == NULL)
		return ERR;

	while (curr != h && curr != NULL) {
		prev = curr;
		curr = curr->next;
	}

	if (curr == NULL)
		return ERR;

	if (prev == NULL) {
		q[HEAD] = curr->next;
		if(curr->next == NULL){
			q[TAIL] = curr->next;
		}
	} else {
		prev->next = curr->next;
	}
	return OK;
}

/**
 * enqueue the process to the scheduling queue
 * according to its priority
 * @param p 
 */
void enqueue_schedule(struct proc* p) {
	p->state = RUNNABLE;
	enqueue_tail(ready_q[p->priority], p);
}

/**
 * unsched the process, the process will be removed from the
 * ready_q, and memory will be released by the system
 * @param p 
 */
void unseched(struct proc *p){
	release_proc_mem(p);
	dequeue_schedule(p);
}

/**
 * add this struct to the free_proc list. This method should
 * only be called when a zombie process is released
 * @param p 
 */
void free_slot(struct proc *p){
	p->state = DEAD;
	p->i_flags &= ~PROC_IN_USE;
	enqueue_head(free_proc, p);
}

/**
 * Exits a process, release memory, unschedule the process
 * and frees its slot in the process table.
 *
 * Note:
 *   The process must not currently belong to any linked list.
 *
 * Side Effects:
 *   Process state is set to DEAD, and is returned to the free_proc list.
 **/
void end_process(struct proc *p) {
	unseched(p);
	free_slot(p);
}

/**
 * get a free struct proc from the system proc table
 * @return pointer to the free slot, or NULL
 */
struct proc *get_free_proc_slot() {
	int i;
	struct proc *p = dequeue(free_proc);
	size_t *sp = NULL;

	if (p) {
		proc_set_default(p);
		p->i_flags |= PROC_IN_USE;
		return p;
	}
	return NULL;
}

/**
 * set the process struct to default values
 * @param p 
 */
void proc_set_default(struct proc *p) {
	int pnr_bak = p->proc_nr;
	memset(p, 0, sizeof(struct proc));
	p->proc_nr = pnr_bak;

	memset(p->regs, DEFAULT_REG_VALUE, NUM_REGS);
	p->cctrl = DEFAULT_CCTRL;

	p->quantum = DEFAULT_USER_QUANTUM;
	p->state = INITIALISING;
	p->ptable = p->protection_table;
	p->alarm.proc_nr = p->proc_nr;
}

/**
 * allocate stack for kernel processes or kernel thread
 * stack size is defined by KERNEL_STACK_SIZE
 * this method can be used for creating kernel process 
 * or kernel threads'  stack
 * @param  who 
 * @return     virtual address of the stack
 */
reg_t* alloc_kstack(struct proc *who){
	int page_size;
	int index;
	ptr_t *addr, *stack_top;

	stack_top = user_get_free_pages(who, KERNEL_STACK_SIZE, GFP_HIGH);

	ASSERT(stack_top != NULL);
	
	addr = stack_top + KERNEL_STACK_SIZE - 1;
	who->stack_top = stack_top;
	return get_virtual_addr(addr, who);
}

/**
 * set corressponding fields of struct pro
 */
int set_proc(struct proc *p, void (*entry)(), int priority, const char *name) {
	p->priority = priority;
	p->pc = entry;

	//NB: this may result in buffer overflow
	strcpy(p->name, name);
	return OK;
}

/**
 * Creates a new kernel process and adds it to the runnable queue
 *
 * Parameters:
 *   entry		A pointer to the entry point of the new process.
 *   priority	The scheduling priority of the new process.
 *   name		The name of the process, up to PROC_NAME_LEN characters long.
 *
 * Returns:
 *   The newly-created proc struct.
 *   NULL if the priority is not valid.
 *   NULL if the process table is full.
 *
 * Side Effects:
 *   A proc is removed from the free_proc list, reinitialised, and added to ready_q.
 */
struct proc *start_kernel_proc(void (*entry)(), int priority, const char *name) {
	struct proc *p;

	if (!IS_PRIORITY_OK(priority)) 
		return NULL;
	
	if (!(p = get_free_proc_slot()))
		return NULL;
	
	set_proc(p, entry, priority, name);
	bitmap_fill(p->ptable, PTABLE_LEN);
	p->sp = alloc_kstack(p);
	enqueue_schedule(p);
	p->quantum = DEFAULT_KERNEL_QUANTUM;
	return p;
}

/**
 * start a new user process
 * @param  lines    array containing the binary image of the process
 * @param  length   length of the lines
 * @param  entry    entry point of the process
 * @param  priority 
 * @param  name     
 * @return          
 */
struct proc *start_user_proc(size_t *lines, size_t length, size_t entry, int priority, const char *name){
	struct proc *p;
	if(!(p = get_free_proc_slot()))
		return NULL;

	
	if(exec_proc(p,lines,length,entry,priority,name) == ERR)
		return NULL;

	return p;
}

/**
 * process memory control, 
 * @param  who       
 * @param  page_addr the virtual address memory
 * @param  flags     PROC_ACCESS OR PROC_NO_ACCESS
 * @return           
 */
int proc_memctl(struct proc* who ,vptr_t* page_addr, int flags){
	int paged = PADDR_TO_PAGED(get_physical_addr(page_addr,who)); //get page descriptor

	if(flags == PROC_ACCESS){
		return bitmap_set_bit(who->ptable, PTABLE_LEN, paged);
	}else if(flags == PROC_NO_ACCESS){
		return bitmap_clear_bit(who->ptable, PTABLE_LEN, paged);
	}
	return ERR;
}

/**
 * allocate memory for the given process
 * @param  who        
 * @param  text_data_length total of text and data size
 * @param  stack_size 
 * @param  heap_size  
 * @param  flags      PROC_SET_SP or/and PROC_SET_HEAP	
 * @return            
 */
int alloc_proc_mem(struct proc *who, int text_data_length, int stack_size, int heap_size, int flags){
	int proc_len;
	int td_aligned;
	ptr_t *bss_start;
	int bss_size;

	//make sizes page aligned
	//text_Data_length is the length of text plus data.
	//Since srec file does not include the size of bss segment by
	//default, so we have to manually set it. By default, bss segment
	//extends from the end of the data segment. So the initial bss 
	//size is simply aligned size minus exact size
	td_aligned = align_page(text_data_length);
	bss_size = td_aligned - text_data_length;

	stack_size = align_page(stack_size);
	heap_size = align_page(heap_size);
	
	//give bss an extra page if its not enough. not that if bss size 
	//is not enough, it could extend to the stack segment, which could
	//potentially corrupt the stack 
	if(bss_size < MIN_BSS_SIZE)
		bss_size += PAGE_LEN;

	proc_len = text_data_length + bss_size + stack_size + heap_size;
	who->rbase = user_get_free_pages(who, proc_len, GFP_NORM);
	if(who->rbase == NULL)
		return ERR;

	//set bss segment to 0
	bss_start = who->rbase + text_data_length;
	memset(bss_start, 0, bss_size);

	//for information on how process memory are structured, 
	//look at the first line of this file
	if(flags & PROC_SET_SP){
		who->stack_top = who->rbase + text_data_length + bss_size;
		who->sp = get_virtual_addr(who->stack_top + stack_size - 1,who);
		*(who->stack_top) = STACK_MAGIC;
	}

	if(flags & PROC_SET_HEAP){
		who->heap_break = who->rbase + text_data_length + bss_size + stack_size;
		who->heap_bottom = who->heap_break + heap_size - 1;
	}
	return OK;
}


/**
 * Initialises the process table
 *
 * Side Effects:
 *   ready_q is initialised to all empty queues.
 *   free_proc queue is initialised and filled with processes.
 *   proc_table is initialised to all DEAD processes.
 *   current_proc is set to NULL.
 **/
void init_proc() {
	int i;
	struct proc *p;
	//Initialise queues

	for (i = 0; i < NUM_QUEUES; i++) {
		ready_q[i][HEAD] = NULL;
		ready_q[i][TAIL] = NULL;
	}

	free_proc[HEAD] = free_proc[TAIL] = NULL;
	//Add all proc structs to the free list
	for ( i = 0; i < NUM_PROCS; i++) {
		p = &proc_table[i];
		proc_set_default(p);
		p->state = DEAD;
		p->proc_nr = i;
		enqueue_tail(free_proc, p);
	}

	//No current process yet.
	current_proc = NULL;
}



