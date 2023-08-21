/*
In cont.c
*/
struct rb_fiber_struct {
    rb_context_t cont;
    VALUE first_proc;
    struct rb_fiber_struct *prev;
    struct rb_fiber_struct *resuming_fiber;

    BITFIELD(enum fiber_status, status, 2);
    /* Whether the fiber is allowed to implicitly yield. */
    unsigned int yielding : 1;
    unsigned int blocking : 1;

    unsigned int killed : 1;

    struct coroutine_context context;
    struct fiber_pool_stack stack;

    struct fiber_record_struct fiber_record;

};

void rb_stack_barrier_set(struct fiber_record_struct *fiber_record, void * destination);
void rb_stack_barrier(void (*yield)(rb_fiber_t *, rb_fiber_t *), rb_fiber_t *new_fiber, rb_fiber_t *old_fiber);
void rb_stack_barrier_check(void *ptr);

/*
Added debug counters
*/
static void
cont_mark(void *ptr)
{
    rb_context_t *cont = ptr;

    RUBY_MARK_ENTER("cont");
    if (cont->self) {
        rb_gc_mark_movable(cont->self);
    }
    rb_gc_mark_movable(cont->value);

    rb_execution_context_mark(&cont->saved_ec);
    rb_gc_mark(cont_thread_value(cont));

    

    
    RB_DEBUG_COUNTER_INC(fiber_full_stack_scan);
    

   

    if (cont->saved_vm_stack.ptr) {
#ifdef CAPTURE_JUST_VALID_VM_STACK
        rb_gc_mark_locations(cont->saved_vm_stack.ptr,
                             cont->saved_vm_stack.ptr + cont->saved_vm_stack.slen + cont->saved_vm_stack.clen);
#else
        rb_gc_mark_locations(cont->saved_vm_stack.ptr,
                             cont->saved_vm_stack.ptr, cont->saved_ec.stack_size);
#endif
        
    }

    RUBY_MARK_LEAVE("cont");
}


/*
modify stack barrier during fiber control transfer
*/

void 
rb_stack_barrier(void (*yield)(rb_fiber_t *, rb_fiber_t *), rb_fiber_t *new_fiber, rb_fiber_t *old_fiber) {
    rb_stack_barrier_set(&(old_fiber->fiber_record), old_fiber->stack.current);
    rb_stack_barrier_set(&(new_fiber->fiber_record), NULL);
    yield(new_fiber, old_fiber); //transfer control
}


//Set the stack barrier to some location in the stack
void 
rb_stack_barrier_set(struct fiber_record_struct *fiber_record, void * destination) {
    fiber_record->stack_barrier = destination;
}

void
rb_stack_barrier_check(void *ptr) {
    rb_fiber_t *fiber = ptr;
    if (FIBER_SUSPENDED_P(fiber)) {
        fiber->fiber_record.stack_barrier = fiber->stack.current;
    }
    else {
        fiber->fiber_record.stack_barrier = NULL;
    }
    RB_DEBUG_COUNTER_INC(stack_barrier_met);
}

void 
fiber_record_init(struct fiber_record_struct *new_record, void *ptr) 
{
    rb_fiber_t *fiber = ptr;
    new_record->head = NULL;
    new_record->tail = NULL;
    new_record->fiber = (void *)fiber;
    new_record->stack_barrier = NULL;
}

struct fiber_record_struct* get_fiber_record(const rb_execution_context_t* ec) {
    rb_fiber_t *fiber = ec->fiber_ptr;
    if (fiber->cont.type == CONTINUATION_CONTEXT) return NULL;
    else return &fiber->fiber_record;
}