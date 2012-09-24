/* Author:          Terence Parr 
 * Description:     Interface for gc.c
*/

typedef unsigned char byte;

typedef struct ClassDescriptor {
    char *name;
    int size;        /* size in bytes of struct */
    int num_fields;
    /* offset from ptr to object of only fields that are managed ptrs
        e.g., don't want to gc ptrs to functions, say */
    int *field_offsets;
} ClassDescriptor;

typedef struct Object {
	ClassDescriptor *class;
	byte marked;
	struct Object *forwarded; /* where we've moved this object */
} Object;

typedef struct String /* extends Object */ {
	ClassDescriptor *class;
	byte marked;
	Object *forwarded;

	int length;
	char str[];        
        /* the string starts at the end of fixed fields; this field
         * does not take any room in the structure; it's really just a
         * label for the element beyond the length field. So, there is no
         * need set this field. You must, however, copy strings into it.
         * You can't set p->str = "foo";
         */
} String;

#define MAX_ROOTS 100

extern ClassDescriptor String_class;
extern Object **_roots[MAX_ROOTS];
extern int _rp;

/* GC interface */
extern void gc_init(int size);
extern void gc();
extern void gc_done();
extern Object *gc_alloc(ClassDescriptor *class);
extern String *gc_alloc_string(int size);
extern char *gc_get_state();
extern int gc_num_roots();

#define gc_save_rp          int __rp = _rp;
#define gc_add_root( p )    _roots[_rp++] = (Object **)(&(p));
#define gc_restore_roots    _rp = __rp;