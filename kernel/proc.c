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
proc_t proc_table[NUM_PROCS];

//Scheduling queues
proc_t *ready_q[NUM_QUEUES][2];

//blocking queues
proc_t *block_q[2];

//Entries in the process table that are not in use
static proc_t *free_proc[2];

//The currently-running process
proc_t *current_proc;

//Limits for memory allocation
size_t FREE_MEM_BEGIN = 0;
size_t FREE_MEM_END = 0;



size_t sizeof_proc_t() {
	proc_t arr[2];
	int size = (char*)&arr[1] - (char*)&arr[0];
	return size;
}


/**
 * Adds a proc to the tail of a list.
 *
 * Parameters:
 *   q		An array containing a head and tail pointer of a linked list.
 *   proc	The proc struct to add to the list.
 **/
void enqueue_tail(proc_t **q, proc_t *proc) {
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
void enqueue_head(proc_t **q, proc_t *proc) {
	proc_t *curr = NULL;
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
proc_t *dequeue(proc_t **q) {
	proc_t *p = q[HEAD];

	if (p == NULL) { //Empty list
		assert(q[TAIL] == NULL, "deq: tail not null");
		return NULL;
	}

	if (q[HEAD] == q[TAIL]) { //Last item
		q[HEAD] = q[TAIL] = NULL;
	}
	else { //At least one remaining item
		q[HEAD] = p->next;
	}
	p->next = NULL;
	return p;
}

//return ERR if nothing found
int delete_proc(proc_t **q, proc_t *h) {
	proc_t *curr = q[HEAD];
	proc_t *prev = NULL;

	if (curr == NULL) { //Empty list
		assert(q[TAIL] == NULL, "delete: tail not null");
		return ERR;
	}

	while (curr != h && curr != NULL) {
		prev = curr;
		curr = curr->next;
	}
	if (curr != NULL) {
		if (prev == NULL) {
			q[HEAD] = curr->next;
			if(curr->next == NULL){
				q[TAIL] = curr->next;
			}
		} else {
			prev->next = curr->next;
		}
		return ERR;
	} else {
		return ERR;
	}

}

void add_to_scheduling_queue(proc_t* p) {
	enqueue_tail(ready_q[p->priority], p);
}

proc_t *get_free_proc() {
	int i;
	proc_t *p = dequeue(free_proc);
	size_t *sp = NULL;

	if (p) {
		proc_set_default(p);
		p->IN_USE = 1;
		return p;
	}
	return NULL;
}


void proc_set_default(proc_t *p) {
	int i = 0;
	for (i = 0; i < NUM_REGS; i++) {
		p->regs[i] = DEFAULT_REG_VALUE;
	}

	p->sp = DEFAULT_STACK_POINTER;
	p->ra = DEFAULT_RETURN_ADDR;
	p->pc = DEFAULT_PROGRAM_COUNTER;
	p->rbase = DEFAULT_RBASE;
	p->ptable = DEFAULT_PTABLE;
	p->cctrl = DEFAULT_CCTRL;

	p->quantum = DEFAULT_QUANTUM;
	p->ticks_left = 0;
	p->time_used = 0;
	//strcpy(p->name,"Unkonwn");
	p->state = INITIALISING;
	p->flags = DEFAULT_FLAGS;

	p->sender_q = NULL;
	p->next_sender = NULL;
	p->message = NULL;

	p->length = 0;
	p->parent = 0;
	p->heap_break = NULL;
	p->pid = p->proc_nr;

	p->ptable = p->protection_table;
	bitmap_clear(p->ptable, PROTECTION_TABLE_LEN);

	p->alarm.time_out = 0;
	p->alarm.next = NULL;
	p->alarm.flags = 0;
	p->alarm.pid = p->pid;

	memset(&p->sig_table,0,_NSIG * 3); //3: sizeof struct sigaction
}


void release_proc_mem(proc_t *who){
    bitmap_xor(mem_map,who->ptable,MEM_MAP_LEN);
}


void *kset_proc(proc_t *p, void (*entry)(), int priority, const char *name) {
	ptr_t *ptr;
	int pg_index;
	p->priority = priority;
	p->pc = entry;

	strcpy(p->name, name);

	ptr = get_free_page(__GFP_NORM);
	p->sp = (reg_t *)(ptr) + PAGE_LEN - 1;

	p->length = DEFAULT_STACK_SIZE; //one page

	//Set the process to runnable, remember to enqueue it after you call this method
	p->state = RUNNABLE;
	return ptr;
}

/**
 * Creates a new process and adds it to the runnable queue
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
proc_t *new_proc(void (*entry)(), int priority, const char *name) {
	proc_t *p = NULL;
	int i;
	size_t *ptr = NULL;
	int n = 0;
	int temp = 0;

	//Is the priority valid?
	if (!(0 <= priority && priority < NUM_QUEUES)) {
		return NULL;
	}
	if (p = get_free_proc()) {
		kset_proc(p, entry, priority, name);
		bitmap_fill(p->ptable, PROTECTION_TABLE_LEN);
		enqueue_tail(ready_q[priority], p);
	}
	return p;
}





proc_t *kexecp(proc_t *p, void (*entry)(), int priority, const char *name) {
	void *ptr = NULL;
	int lower_bound = 0;
	if (p->rbase == DEFAULT_RBASE) {
		bitmap_set_bit(mem_map, MEM_MAP_LEN, get_page_index(p->sp));
	} else {
		bitmap_xor(mem_map, p->ptable, MEM_MAP_LEN);
	}
	ptr = kset_proc(p, entry, priority, name);
	// kprintf("kset 0x%08x\n",mem_map[0]);
	p->rbase = DEFAULT_RBASE;
	bitmap_fill(p->ptable, PROTECTION_TABLE_LEN);
	add_to_scheduling_queue(p);
	return p;
}

proc_t *start_system(void (*entry)(), int priority, const char *name) {
	proc_t *p = NULL;
	int stack_size = 1024 * 1;
	int pg_idx;
	ptr_t *ptr;
	if (p = get_free_proc()) {
		p->priority = priority;
		p->pc = entry;

		strcpy(p->name, name);

		// p->ptable = p->protection_table;

		// ptr = proc_malloc(DEFAULT_STACK_SIZE);
		ptr = get_free_pages(stack_size / 1024,__GFP_NORM);

		p->sp = (reg_t *)ptr + stack_size - 128;
		//TODO: init heap_break using slab
		p->heap_break = p->sp + 1;
		p->length = stack_size * 10;
		//Set the process to runnable, remember to enqueue it after you call this method
		p->state = RUNNABLE;
	}
	bitmap_fill(p->ptable, PROTECTION_TABLE_LEN);
	add_to_scheduling_queue(p);
	return p;
}

proc_t* start_init(size_t *lines, size_t length, size_t entry) {
	void *ptr_base;
	int size = 1024, nstart = 0;
	proc_t *p = NULL;
	if (p = get_free_proc()) {
		ptr_base = kset_proc(p, (void (*)())entry, USER_PRIORITY, "INIT");
		memcpy(ptr_base, lines, length);
		p->rbase = ptr_base;
		p->sp = get_virtual_addr(p->sp, p);
		// bitmap_set_bit(p->ptable,PROTECTION_TABLE_LEN,get_page_index(p->rbase));
		//set protection table TODOK
		add_to_scheduling_queue(p);
	}
	return p;
}


void unseched(proc_t *p){
	release_proc_mem(p);
	delete_proc(ready_q[p->priority], p);
}

void free_slot(proc_t *p){
	p->state = DEAD;
	p->IN_USE = 0;
	enqueue_tail(free_proc, p);
}

/**
 * Exits a process, and frees its slot in the process table.
 *
 * Note:
 *   The process must not currently belong to any linked list.
 *
 * Side Effects:
 *   Process state is set to DEAD, and is returned to the free_proc list.
 **/
void end_process(proc_t *p) {
	int i,ret;
	unseched(p);
	free_slot(p);
}

//print out the list of processes currently in the ready_q
//and the currently running process
//return OK;

void process_overview() {
	int i;
	proc_t *curr;
	kprintf("NAME     PID PPID RBASE      PC         STACK      HEAP       PROTECTION   flags\n");
	for (i = 0; i < NUM_PROCS; i++) {
		curr = &proc_table[i];
		if(curr->IN_USE && curr->state != ZOMBIE)
			printProceInfo(curr);
	}
}

//print the process state given
void printProceInfo(proc_t* curr) {
	int ptable_idx = get_page_index(curr->rbase)/32;
	kprintf("%-08s %-03d %-04d 0x%08x 0x%08x 0x%08x 0x%08x %d 0x%08x %d\n",
	        curr->name,
	        curr->pid,
			curr->parent,
	        curr->rbase,
	        get_physical_addr(curr->pc,curr),
	        get_physical_addr(curr->sp,curr),
	        curr->heap_break,
			ptable_idx,
	        curr->ptable[ptable_idx],
			curr->flags);
}


/**
 * Gets a pointer to a process.
 *
 * Parameters:
 *   proc_nr		The process to retrieve.
 *
 * Returns:			The relevant process, or NULL if it does not exist.
 **/
proc_t *get_proc(int proc_nr) {
	proc_t *p;
	if (isokprocn(proc_nr)){
		p = &proc_table[proc_nr];
		return p;
		// if(p->state != ZOMBIE && p->state != DEAD)
		// 	return p;
	}
	return NULL;
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
	proc_t *p;
	//Initialise queues

	for (i = 0; i < NUM_QUEUES; i++) {
		ready_q[i][HEAD] = NULL;
		ready_q[i][TAIL] = NULL;
	}

	free_proc[HEAD] = free_proc[TAIL] = NULL;
	//Add all proc structs to the free list
	for ( i = 0; i < NUM_PROCS; i++) {
		p = &proc_table[i];
		p->state = DEAD;
		p->IN_USE = 0;
		p->proc_nr = i;
		enqueue_tail(free_proc, p);
	}

	//No current process yet.
	current_proc = NULL;
}



