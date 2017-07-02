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

//OLD Process Stacks
//static size_t proc_stacks[NUM_PROCS][DEFAULT_STACK_SIZE];

//Limits for memory allocation
size_t FREE_MEM_BEGIN = 0;
size_t FREE_MEM_END = 0;



size_t sizeof_proc_t() {
	hole_t arr[2];
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
static proc_t *dequeue(proc_t **q) {
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

//return -1 if nothing found
int delete_proc(proc_t **q, proc_t *h) {
	proc_t *curr = q[HEAD];
	proc_t *prev = NULL;

	if (curr == NULL) { //Empty list
		assert(q[TAIL] == NULL, "delete: tail not null");
		return -1;
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
		return 1;
	} else {
		return -1;
	}

}

void add_to_scheduling_queue(proc_t* p) {
	enqueue_tail(ready_q[p->priority], p);
}


proc_t *get_idle(){
	static proc_t *idle;
	kprintf("get idle\n");
	if(idle == NULL){
		idle = new_proc(idle_main, IDLE_PRIORITY, "IDLE");
	}
	return idle;
}


proc_t *get_free_proc() {
	int i;
	proc_t *p = dequeue(free_proc);
	size_t *sp = NULL;

	if (p) {

		proc_set_default(p);
		//malloced_sp
	}
	return p;
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
	//strcpy(p->name,"Unkonwn Name");
	p->state = INITIALISING;
	p->flags = DEFAULT_FLAGS;

	p->sender_q = NULL;
	p->next_sender = NULL;
	p->message = NULL;

	p->length = 0;
	p->parent_proc_index = 0;
	p->heap_break = NULL;

	p->ptable = p->protection_table;
	bitmap_clear(p->ptable, PROTECTION_TABLE_LEN);

	memset(p->sig_table,0,_NSIG * 3); //3: sizeof struct sigaction
}


/**
 * Chooses a process to run.
 *
 * Returns:
 *   The process that is runnable with the highest priority.
 *   NULL if no processes are runnable (should never happen).
 *
 * Side Effects:
 *   A proc is removed from a ready_q.
 **/
proc_t *pick_proc() {
	int i;

	//Find the highest-priority non-empty queue
	for (i = 0; i < NUM_QUEUES; i++) {
		if (ready_q[i][HEAD] != NULL) {
			return dequeue(ready_q[i]);
		}
	}

	return get_idle();
}

/**
 * fork the calling process
 *
 * Returns:
 *   proc_index of the newly forked process
 *
 * Side Effects:
 *   a new process forked onto the a new memory space, but not yet added to the scheduling queue
 **/
proc_t* do_fork(proc_t *original) {
	proc_t *p = NULL;
	void *ptr_base = NULL;
	int len = 0;
	int nstart = 0;
	int pbak,index,j;
	unsigned long *sp;
	uint32_t *src, *dest;
	pattern_t ptn;

	if (original->length == 0 || (size_t)(original->rbase) == 0) {
		//we can't fork p1 if it's a system task
		kprintf("%s can't be forked\n", original->name );
		return NULL;
	}

	if (p = get_free_proc()) {
		pbak = p->proc_index;
		*p = *original;
		p->proc_index = pbak;
		p->ptable = p->protection_table;

		if(extract_pattern(p->ptable, MEM_MAP_LEN, (int)p->heap_break, &ptn) != 0){
			kprintf("pat search failed");
		}

		index = bitmap_search_pattern(mem_map, MEM_MAP_LEN, BSS_END / 1024, ptn.pattern, ptn.size);
		bitmap_clear(p->ptable, PROTECTION_TABLE_LEN);

		bitmap_set_pattern(mem_map, 32, index, ptn.pattern, ptn.size);
		bitmap_set_pattern(p->ptable, 32, index, ptn.pattern, ptn.size);

		ptr_base = (void *)(index * 1024);
		p->rbase = ptr_base;

		sp = (size_t *)((size_t)(original->sp) + (size_t)(original->rbase));
		p->message = (message_t *)(*(sp+ 2) + (size_t)p->rbase);

		for( src = (uint32_t *)original->rbase, dest = (uint32_t *)p->rbase, j=0; j < ptn.size ; src+=1024,dest+=1024, j++){
			if((0x80000000 >> j) & ptn.pattern){
				memcpy(dest,src,1024);
			}
		}
		p->parent_proc_index = original->proc_index;
		p->state = RUNNABLE;
	}
	// process_overview();
	// printProceInfo(original);
	// printProceInfo(p);
	assert(p != NULL, "Fork");
	return p;
}



void *kset_proc(proc_t *p, void (*entry)(), int priority, const char *name) {
	void *ptr = NULL;
	int i = 0, len = 0;
	p->priority = priority;
	p->pc = entry;

	len = DEFAULT_STACK_SIZE / 1024;
	strcpy(p->name, name);

	// p->ptable = p->protection_table;

	// ptr = proc_malloc(DEFAULT_STACK_SIZE);
	ptr = get_free_pages(len,__GFP_NORM);
	i = get_page_index(ptr);
	bitmap_set_nbits(p->ptable, PROTECTION_TABLE_LEN, i, len);
	bitmap_set_nbits(mem_map, PROTECTION_TABLE_LEN, i, len);
	// kprintf("kset %x\n",p->ptable[0]);
	p->sp = (size_t *)ptr + DEFAULT_STACK_SIZE - 100;
	p->heap_break = p->sp + 1;

	p->length = DEFAULT_STACK_SIZE;

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
	// kprintf("kset %x\n",mem_map[0]);
	p->rbase = DEFAULT_RBASE;
	bitmap_fill(p->ptable, PROTECTION_TABLE_LEN);
	add_to_scheduling_queue(p);
	return p;
}

proc_t *start_system(void (*entry)(), int priority, const char *name) {
	proc_t *p = NULL;
	int stack_size = 1024 * 1;
	int *ptr = NULL;
	if (p = get_free_proc()) {
		p->priority = priority;
		p->pc = entry;

		strcpy(p->name, name);

		// p->ptable = p->protection_table;

		// ptr = proc_malloc(DEFAULT_STACK_SIZE);
		ptr = get_free_pages(stack_size / 1024,__GFP_NORM);

		p->sp = (size_t *)ptr + stack_size - 128;
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
	p->state = DEAD;
	// process_overview();
	for(i=0; i< NUM_QUEUES; i++){
		if(ready_q[i] != NULL){
			ret = delete_proc(ready_q[i], p);
			if(ret == 0){
				break;
			}
		}
	}
	// process_overview();
	enqueue_tail(free_proc, p);
}

//print out the list of processes currently in the ready_q
//and the currently running process
//return 0;

int process_overview() {
	int i = 0;
	proc_t *curr = NULL;
	//kprintf("|| PS %s ",current_proc->name );
	for (i = 0; i < NUM_QUEUES; i++) {
		if (ready_q[i][HEAD] != NULL) {
			curr = ready_q[i][HEAD];
			while (curr != NULL) {
				printProceInfo(curr);
				//kprintf("%s ",curr->name);
				curr = curr->next;
			}
		}
	}
	//kprintf("||\n" );
	return 0;
}

//print the process state given
void printProceInfo(proc_t* curr) {
	kprintf("%s %d rbase %x pc %x, sp %x, heap %x, pt %x pri %d\r\n",
	        curr->name,
	        curr->proc_index,
	        curr->rbase,
	        curr->pc,
	        curr->sp,
	        curr->heap_break,
	        curr->ptable[0],
			curr->priority);
}

//return the strign value of state name give proc_state_t state
char* getStateName(proc_state_t state) {
	switch (state) {
		case DEAD: return "DEAD";
		case INITIALISING: return "INITIALISING";
		case RUNNABLE: return "RUNNABLE";
		case ZOMBIE: return "ZOMBIE";
		default: return "none";
	}
}


/**
 * The Scheduler.
 *
 * Notes:
 *   Context of current_proc must already be saved.
 *   If successful, this function does not return.
 *
 * Side Effects:
 *   current_proc has its accounting fields updated, and is reinserted to ready_q.
 *   current_proc is updated to point to the next runnable process.
 *   Context of the new proc is loaded.
 **/
void sched() {

	proc_t *curr = ready_q[3][HEAD];
	int nextpick = 0;
	static int count = 0;

	if (current_proc != NULL && !current_proc->flags) {
		//Accounting
		current_proc->time_used++;

		if (--current_proc->ticks_left) {
			enqueue_head(ready_q[current_proc->priority], current_proc);
		}
		else { //Re-insert process at the tail of its priority queue
			enqueue_tail(ready_q[current_proc->priority], current_proc);
		}
	}

	current_proc = pick_proc();
	
	//Reset quantum if needed
	if (current_proc->ticks_left <= 0) {
		current_proc->ticks_left = current_proc->quantum;
	}

	//Load context and run
	wramp_load_context();
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
	if (0 <= proc_nr && proc_nr < NUM_PROCS) {
		proc_t *p = &proc_table[proc_nr];
		if (p->state != DEAD)
			return p;
	}
	return NULL;
}

/**
 * sends a message.
 *
 * Parameters:
 *   m				Pointer to write the message to.
 *
 * Returns:
 *   0 on success
 *   -1 if destination is invalid
 **/

int wini_send(int dest, message_t *m) {
	proc_t *pDest;

	current_proc->message = m; //save for later

	//Is the destination valid?
	if (pDest = get_proc(dest)) {
		// if (debug)
		// 	kprintf("%d ", pDest->proc_index);
		//If destination is waiting, deliver message immediately.
		if (pDest->flags & RECEIVING) {
			// if (debug)
			// 	kprintf("| data sent from %d\n", current_proc->proc_index);
			//Copy message to destination
			*(pDest->message) = *m;
			//Unblock receiver
			pDest->flags &= ~RECEIVING;
			enqueue_head(ready_q[pDest->priority], pDest);
			// if(pDest->proc_index == 0)
				// kprintf(" | send t %d %x| ",m->type,m);
		}

		else {
			//Otherwise, block current process and add it to head of sending queue of the destination.
			current_proc->flags |= SENDING;
			current_proc->next_sender = pDest->sender_q;
			pDest->sender_q = current_proc;
			// kprintf(" | send t %d nexts %d senderh %d| ",m->type,current_proc->next_sender->proc_index,current_proc->sender_q->proc_index);
		}

		return 0;
	} else {
		// if (debug)
		// 	kprintf("pid %d not found from %d\n", dest, current_proc->proc_index);
	}

	return -1;
}

/**
 * Receives a message.
 *
 * Parameters:
 *   m				Pointer to write the message to.
 *
 * Returns:			0
 **/
int wini_receive(message_t *m) {
	proc_t *p = current_proc->sender_q;

	//If a process is waiting to send to this process, deliver it immediately.
	if (p != NULL) {

		//Dequeue head node
		current_proc->sender_q = p->next_sender;

		//Copy message to this process
		*m = *(p->message);

		//Unblock sender
		p->flags &= ~SENDING;
		enqueue_head(ready_q[p->priority], p);
		//kprintf("$at receive sender %d enqueued || ",p->proc_index);
	}
	else {
		current_proc->message = m;
		current_proc->flags |= RECEIVING;
	}
	return 0;
}
/**
 * non-block send
 *
 * Parameters:
 *   m				Pointer to write the message to.
 *
 * Returns:
 *   0 on success
 *   -1 if destination is invalid
 **/
int wini_notify(int dest, message_t *m) {
	proc_t *pDest;

	current_proc->message = m; //save for later

	//Is the destination valid?
	if (pDest = get_proc(dest)) {

		//If destination is waiting, deliver message immediately.
		if (pDest->flags & RECEIVING) {

			//Copy message to destination
			*(pDest->message) = *m;

			//Unblock receiver
			pDest->flags &= ~RECEIVING;
			enqueue_head(ready_q[pDest->priority], pDest);
			//if the destination rejects any message it receives,
			//do not deliver the message, but add it to the scheduling queue
		} else {
			//TODO
			enqueue_tail(ready_q[current_proc->priority], current_proc);
		}

		//do nothing if it's not waiting
		return 0;
	}
	return -1;
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
	//Initialise queues
	// proc_t arr[2];
	// int size = (char*)&arr[1] - (char*)&arr[0];
	// kprintf("sizeof proc_t %d\n",size );

	for (i = 0; i < NUM_QUEUES; i++) {
		ready_q[i][HEAD] = NULL;
		ready_q[i][TAIL] = NULL;
	}

	free_proc[HEAD] = free_proc[TAIL] = NULL;
	//Add all proc structs to the free list
	for (i = 0; i < NUM_PROCS; i++) {
		proc_t *p = &proc_table[i];
		p->state = DEAD;

		enqueue_tail(free_proc, p);
		proc_table[i].proc_index = i;
	}

	//No current process yet.
	current_proc = NULL;
}

