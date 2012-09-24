/* Author:          Terence Parr 
 * Description:     An example of how to use the garbage collector (gc.c). Also tests the
                    functionality. A successful run will have no output.
 * Compile:         gcc -g -Wall -o gc gc.c test.c
 * Usage:           ./gc
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "gc.h"

#define ASSERT(EXPECTED, RESULT)\
  if(EXPECTED != RESULT) { printf("\n%-30s failure on line %d; expecting %d found %d\n", \
        __func__, __LINE__, EXPECTED, RESULT); }
#define STR_ASSERT(EXPECTED, RESULT)\
  if(strcmp(EXPECTED,RESULT)!=0) { printf("\n%-30s failure on line %d; expecting:\n%s\nfound:\n%s\n", \
        __func__, __LINE__, EXPECTED, RESULT); }

typedef struct User /* extends Object */ {
    ClassDescriptor *class;
    byte marked;
    Object *forwarded; // where we've moved this object

    int userid;
    int parking_sport;
    float salary;
    String *name;
} User;

ClassDescriptor User_class = {
    "User",
    sizeof (struct User),
    1, /* name field */
    (int []) {offsetof(struct User, name)} /* offset of 2nd field ignoring class descr. */
};

typedef struct Employee /* extends Object */ {
    ClassDescriptor *class;
    byte marked;
    Object *forwarded; // where we've moved this object
    
    int ID;
    String *name;
    struct Employee *mgr;
} Employee;

ClassDescriptor Employee_class = {
    "Employee",
    sizeof (struct Employee),
    2, /* name, mgr fields */
    (int []) {
        offsetof(struct Employee, name), /* offset of 2nd field ignoring class descr. */
        offsetof(struct Employee, mgr)  // 3rd field
    }
};

void check_state(char *expected) {
    char *found = gc_get_state();
    STR_ASSERT(expected, found);
    free(found);
}

void test_alloc_str_gc_compact_does_nothing() {
    gc_init(1000);
    String *a;
    gc_save_rp;
    gc_add_root(a);
    ASSERT(1, gc_num_roots());

    a = gc_alloc_string(10);
    strcpy(a->str, "hi mom");

    {
        char *expected =
                "next_free=43\n"
                "objects:\n"
                "  0000:String[32+11]=\"hi mom\"\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    gc();

    {
        char *expected =
                "next_free=43\n"
                "objects:\n"
                "  0000:String[32+11]=\"hi mom\"\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    gc_restore_roots;
    gc_done();
}

void test_alloc_str_set_null_gc() {
    gc_init(1000);
    String *a;
    gc_save_rp;
    gc_add_root(a);
    ASSERT(1, gc_num_roots());

    a = gc_alloc_string(10);
    strcpy(a->str, "hi mom");

    {
        char *expected =
                "next_free=43\n"
                "objects:\n"
                "  0000:String[32+11]=\"hi mom\"\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    a = NULL;

    gc();

    {
        char *expected = // compacts out dead stuff
                "next_free=0\n"
                "objects:\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    gc_restore_roots;
    gc_done();
}

void test_alloc_2_str_overwrite_first_one_gc() {
    gc_init(1000);
    String *a;
    gc_save_rp;
    gc_add_root(a);
    ASSERT(1, gc_num_roots());

    a = gc_alloc_string(10);
    strcpy(a->str, "hi mom");

    {
        char *expected =
                "next_free=43\n"
                "objects:\n"
                "  0000:String[32+11]=\"hi mom\"\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    a = gc_alloc_string(10);
    strcpy(a->str,"hi dad");

    gc();

    {
        char *expected = // compacts out dead stuff
                "next_free=43\n"
                "objects:\n"
                "  0000:String[32+11]=\"hi dad\"\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    gc_restore_roots;
    gc_done();
}

// user->name points at string

void test_alloc_user() {
    gc_init(1000);
    gc_save_rp;
    User *u = (User *) gc_alloc(&User_class);
    gc_add_root(u);

    u->name = gc_alloc_string(20);
    strcpy(u->name->str, "parrt");

    {
        char *expected = // compacts out dead stuff
                "next_free=101\n"
                "objects:\n"
                "  0000:User[48]->[48]\n"
                "  0048:String[32+21]=\"parrt\"\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    u = NULL; // should free user and string

    gc();

    {
        char *expected = // compacts out dead stuff
                "next_free=0\n"
                "objects:\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    gc_restore_roots;
    gc_done();
}

void test_alloc_user_after_string() {
    gc_init(1000);
    gc_save_rp;

    String * s = gc_alloc_string(20);
    gc_add_root(s);
    strcpy(s->str, "parrt");

    User *u = (User *) gc_alloc(&User_class);
    gc_add_root(u);
    u->name = s;

    {
        char *expected = // compacts out dead stuff
                "next_free=101\n"
                "objects:\n"
                "  0000:String[32+21]=\"parrt\"\n"
                "  0053:User[48]->[0]\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    u = NULL; // should free user but NOT string

    gc();

    {
        char *expected = // compacts out dead stuff
                "next_free=53\n"
                "objects:\n"
                "  0000:String[32+21]=\"parrt\"\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    gc_restore_roots;
    gc_done();
}

void test_alloc_obj_with_two_ptr_fields() {
    gc_init(1000);
    gc_save_rp;

    Employee *tombu = (Employee *) gc_alloc(&Employee_class);
    String *s = gc_alloc_string(3);
    strcpy(s->str, "Tom");
    tombu->name = s;

    Employee *parrt = (Employee *) gc_alloc(&Employee_class);
    parrt->name = gc_alloc_string(10);
    strcpy(parrt->name->str, "Terence");
    parrt->mgr = tombu;

    gc_add_root(parrt); // just one root
    
    gc();

    {
        char *expected = // compacts out dead stuff
            "next_free=175\n"
            "objects:\n"
            "  0000:Employee[48]->[48,NULL]\n"
            "  0048:String[32+4]=\"Tom\"\n"
            "  0084:Employee[48]->[132,0]\n"
            "  0132:String[32+11]=\"Terence\"\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    gc_restore_roots;
    gc_done();
}

void test_alloc_obj_kill_mgr_ptr() {
    gc_init(1000);
    gc_save_rp;

    Employee *tombu = (Employee *) gc_alloc(&Employee_class);
    String *s = gc_alloc_string(3);
    strcpy(s->str, "Tom");
    tombu->name = s;

    Employee *parrt = (Employee *) gc_alloc(&Employee_class);
    parrt->name = gc_alloc_string(10);
    strcpy(parrt->name->str, "Terence");
    parrt->mgr = tombu;

    gc_add_root(parrt); // just one root
    
    parrt->mgr = NULL; // 2 objects live
    
    gc();

    {
        char *expected = // compacts out dead stuff
            "next_free=91\n"
            "objects:\n"
            "  0000:Employee[48]->[48,NULL]\n"
            "  0048:String[32+11]=\"Terence\"\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    gc_restore_roots;
    gc_done();
}

void test_mgr_cycle() {
    gc_init(1000);
    gc_save_rp;

    Employee *tombu = (Employee *) gc_alloc(&Employee_class);
    String *s = gc_alloc_string(3);
    strcpy(s->str, "Tom");
    tombu->name = s;

    Employee *parrt = (Employee *) gc_alloc(&Employee_class);
    parrt->name = gc_alloc_string(10);
    strcpy(parrt->name->str, "Terence");

    // CYCLE
    parrt->mgr = tombu;
    tombu->mgr = parrt;

    gc_add_root(parrt); // just one root; can it find everyone and not freak out?
    
    gc();

    {
        char *expected = // compacts out dead stuff
            "next_free=175\n"
            "objects:\n"
            "  0000:Employee[48]->[48,84]\n"
            "  0048:String[32+4]=\"Tom\"\n"
            "  0084:Employee[48]->[132,0]\n"
            "  0132:String[32+11]=\"Terence\"\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    gc_restore_roots;
    gc_done();
}

void test_mgr_cycle_kill_one_link() {
    gc_init(1000);
    gc_save_rp;

    Employee *tombu = (Employee *) gc_alloc(&Employee_class);
    String *s = gc_alloc_string(3);
    strcpy(s->str, "Tom");
    tombu->name = s;

    Employee *parrt = (Employee *) gc_alloc(&Employee_class);
    parrt->name = gc_alloc_string(10);
    strcpy(parrt->name->str, "Terence");

    // CYCLE
    parrt->mgr = tombu;
    tombu->mgr = parrt;

    gc_add_root(parrt); // just one root; can it find everyone and not freak out?
    
    parrt->mgr = NULL;  // can't see tombu from anywhere
    
    gc();

    {
        char *expected = // compacts out dead stuff
            "next_free=91\n"
            "objects:\n"
            "  0000:Employee[48]->[48,NULL]\n"
            "  0048:String[32+11]=\"Terence\"\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    gc_restore_roots;
    gc_done();
}

void test_template() {
    gc_init(1000);
    gc_save_rp;

    // gc_add_root(s);

    gc_restore_roots;
    gc_done();
}

void test_automatic_gc() {
    gc_init(90);
    gc_save_rp;

    User *u = (User *) gc_alloc(&User_class);
    gc_add_root(u);

    u->name = gc_alloc_string(5);
    strcpy(u->name->str, "parrt");

    {
        char *expected = // compacts out dead stuff
                "next_free=86\n"
                "objects:\n"
                "  0000:User[48]->[48]\n"
                "  0048:String[32+6]=\"parrt\"\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    u = NULL; // should free user and string
    
    User *q = (User *) gc_alloc(&User_class);
    gc_add_root(q);
    
    q->name = gc_alloc_string(6);
    strcpy(q->name->str, "steely");
    
    {
        char *expected = // compacts out dead stuff
                "next_free=87\n"
                "objects:\n"
                "  0000:User[48]->[48]\n"
                "  0048:String[32+7]=\"steely\"\n";;
                
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }

    gc_restore_roots;
    gc_done();
}

void test_loop() {
    gc_init(500);
    gc_save_rp;
   
	Employee *parrt;
   	gc_add_root(parrt);

	int i;
	parrt = (Employee *) gc_alloc(&Employee_class);
	
	for (i=0;i<1000;i++){
      printf("\n*****************iteration %d*****************\n\n", i);
      // parrt = NULL; 
		parrt = (Employee *) gc_alloc(&Employee_class);
		parrt->name = gc_alloc_string(10);
      printf("parrt->name: %s\n", parrt->class->name);	    
	}
    strcpy(parrt->name->str, "Terence");
       
    gc();
   
    {
        char *expected = // compacts out dead stuff
            "next_free=91\n"
            "objects:\n"
            "  0000:Employee[48]->[48,NULL]\n"
            "  0048:String[32+11]=\"Terence\"\n";
        char *found = gc_get_state();
        STR_ASSERT(expected, found);
        free(found);
    }
   
    gc_restore_roots;
    gc_done();
}

int main(int argc, char *argv[]) {
   test_alloc_str_gc_compact_does_nothing();
   test_alloc_str_set_null_gc();
   test_alloc_2_str_overwrite_first_one_gc();
   test_alloc_user();
   test_alloc_user_after_string();
   test_alloc_obj_with_two_ptr_fields();
   test_alloc_obj_kill_mgr_ptr();
   test_mgr_cycle();
   test_mgr_cycle_kill_one_link();
   test_automatic_gc();
   return 0;
}
