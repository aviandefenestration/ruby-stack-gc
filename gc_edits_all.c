/*
In gc.h
*/
/*Fiber stuff*/
bool rb_gc_is_full_marking(void);
struct fiber_stack_object 
{
    //a pointer to a object pointer on a fiber stack
    VALUE *stack_obj;
    struct fiber_stack_object *next;
};

struct fiber_record_struct 
{
    //the head of a linked list storing pointer values found on a fiber's stack.
    //any pointers found on the stack are *VALUE type. gc_mark_maybe is given VALUE, which is a pointer to an object
    struct fiber_stack_object *head;
    struct fiber_stack_object *tail;

    // Inform gc of how to scan stack
    void * stack_barrier;

    void *fiber;

};

void fiber_record_init(struct fiber_record_struct *new_record, void *ptr);
void fiber_record_free(struct fiber_record_struct *fiber_record);
struct fiber_record_struct* get_fiber_record(const rb_execution_context_t* ec);


/*
In gc.c
*/


/*
Debug counters
*/

static void
each_location(rb_objspace_t *objspace, register const VALUE *x, register long n, void (*cb)(rb_objspace_t *, VALUE))
{
    VALUE v;
    while (n--) {
        v = *x;
        cb(objspace, v);
        x++;
        RB_DEBUG_COUNTER_ADD(stack_scan_bytes, 8); //8 bytes per 1 address on a 64-bit machine
        RB_DEBUG_COUNTER_INC_IF(stack_object_count, is_pointer_to_heap(objspace, (void *)v));
    }
}

gc_mark_stack_values(rb_objspace_t *objspace, long n, const VALUE *values)
{
    long i;

    for (i=0; i<n; i++) {
        if (is_markable_object(values[i])) {
            gc_mark_and_pin(objspace, values[i]);
            RB_DEBUG_COUNTER_ADD(stack_scan_bytes, 8);
        }
    }
}


//I don't think I ended up using this function
bool
rb_gc_is_full_marking(void) {
    rb_objspace_t *objspace = GET_VM()->objspace;
    return is_full_marking(objspace);
}


void
fiber_record_mark_and_add_locations(rb_objspace_t *objspace, struct fiber_record_struct *fiber_record, const VALUE *start, const VALUE *end,
                        void (*cb)(rb_objspace_t *, VALUE));

void 
fiber_record_mark(rb_objspace_t *objspace, const rb_execution_context_t *ec, const VALUE *start, const VALUE *end,
                 void (*cb)(rb_objspace_t *, VALUE));

void
fiber_record_mark_list(rb_objspace_t *objspace, struct fiber_record_struct *fiber_record, void (*cb)(rb_objspace_t *, VALUE));

void 
fiber_record_free(struct fiber_record_struct *fiber_record);

static void
each_machine_stack_value(const rb_execution_context_t *ec, void (*cb)(rb_objspace_t *, VALUE))
{
    rb_objspace_t *objspace = &rb_objspace;
    VALUE *stack_start, *stack_end;

    GET_STACK_BOUNDS(stack_start, stack_end, 0);
    RUBY_DEBUG_LOG("ec->th:%u stack_start:%p stack_end:%p", rb_ec_thread_ptr(ec)->serial, stack_start, stack_end);
    if (is_full_marking(objspace)) {
        each_stack_location(objspace, ec, stack_start, stack_end, cb);
    }
    else {
        fiber_record_mark(objspace, ec, stack_start, stack_end, cb);
    }
}

//use to create new list
void
fiber_record_mark_and_add_locations(rb_objspace_t *objspace, struct fiber_record_struct *fiber_record, const VALUE *start, const VALUE *end,
                        void (*cb)(rb_objspace_t *, VALUE))
{
    register long n;
    register VALUE *x = start;

    if (end <= start) return;
    n = end - start;

    VALUE v;
    while (n--) {
        v = *x;
        if (is_pointer_to_heap(objspace, (void *)v)) {
            struct fiber_stack_object *new_node = malloc(sizeof(struct fiber_stack_object));
            if (new_node == NULL) return; //no memory
            new_node->stack_obj = x;
            new_node->next = NULL;

            if (fiber_record->head) {
                //add to the tail
                if (fiber_record->tail) {
                    //none null
                    fiber_record->tail->next = new_node;
                    fiber_record->tail = new_node;
                }
                else {
                    //tail null
                    fiber_record->head->next = new_node;
                    fiber_record->tail = new_node;
                }
            } 
            else {
                if (fiber_record->tail) {
                    //head is null
                    fiber_record->head = fiber_record->tail;
                    fiber_record->tail->next = new_node;
                    fiber_record->tail = new_node;
                }
                else {
                    //both null
                    fiber_record->head = new_node;
                    fiber_record->tail = new_node;
                    
                }
            }
            RB_DEBUG_COUNTER_INC(stack_object_count);
            RB_DEBUG_COUNTER_ADD(stack_scan_bytes, 8); //8 bytes per 1 address on a 64-bit machine
        }
        cb(objspace, v);
        x++;
    }
}

void 
fiber_record_mark(rb_objspace_t *objspace, const rb_execution_context_t *ec, const VALUE *start, const VALUE *end,
                 void (*cb)(rb_objspace_t *, VALUE)) 
{
    struct fiber_record_struct *fiber_record = get_fiber_record(ec);

    if (!fiber_record) {
        each_stack_location(objspace, ec, start, end, cb);
        RB_DEBUG_COUNTER_INC(no_fiber_record);

    }
    else if (fiber_record->stack_barrier == NULL) {
        each_stack_location(objspace, ec, start, end, cb);
        fiber_record_free(fiber_record);
    }
    else { 
        if (fiber_record->head) {
            //inactive with created record
            fiber_record_mark_list(objspace, fiber_record, cb);
        }
        else {   
            //inactive without record
            VALUE *stack_start = start;
            VALUE *stack_end = end;
            #if defined(__mc68000__)
                VALUE *stack_start = (VALUE*)((char*)stack_start + 2);
                VALUE *stack_end = (VALUE*)((char*)stack_end - 2);
            #endif

            fiber_record_mark_and_add_locations(objspace, fiber_record, stack_start, stack_end, cb);
        }
    }
}

void
fiber_record_mark_list(rb_objspace_t *objspace, struct fiber_record_struct *fiber_record, void (*cb)(rb_objspace_t *, VALUE)) {
    
    if (fiber_record->head == NULL) return;

    struct fiber_stack_object *current = fiber_record->head;
    
    while (current != NULL) {
        cb(objspace, *(current->stack_obj));
        current = current->next;
        RB_DEBUG_COUNTER_INC(record_obj_mark);
    }
}

void 
fiber_record_free(struct fiber_record_struct *fiber_record) {

    if (fiber_record->head == NULL) return;

    struct fiber_stack_object *current = fiber_record->head;
    
    while (current != NULL) {
        struct fiber_stack_object *removed = current;
        current = current->next;
        free(removed);
        RB_DEBUG_COUNTER_INC(stack_obj_remove);
    }
    fiber_record->head = NULL; 
    fiber_record->tail = NULL; 
}