/* Author:                Steely Morneau 
 * Description:           A mark-n-compact single-heap garbage collector
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "gc.h"

void mark(Object* obj);
void setForwarding();
void changePointers(Object* obj);
char *printObjectsFromRoots();
void moveObjects();
char *doFields(Object* obj, char* buf);

void* heap;
Object **_roots[MAX_ROOTS];
int _rp;
int nextFree;
int heapSize;

/* initialize the garbage collector and a static-sized heap */
void gc_init(int size) {
   _rp = 0;
   heap = (void*) malloc(size);
   memset(heap, 0, size);
   nextFree = 0;
   heapSize = size;
}

/* garbage collection on the heap */
void gc() {
   int i;
   
   for (i = 0; i < _rp; i++) { 
      mark(*_roots[i]);
   }
      
   setForwarding();
      
   for (i = 0; i < _rp; i++) {
      if(*_roots[i] == NULL) {
         continue;
      }

      changePointers(*_roots[i]);
      *_roots[i] = (*_roots[i])->forwarded;
   }
   
   moveObjects();
   
}

/* mark live objects */
void mark(Object* obj) {
   int i;
   
   if(obj == NULL || obj->marked == 1 || (void*)obj < heap || 
         (void*)obj > heap + heapSize) {
      return;
   }
   
   obj->marked = 1;
   
   for(i = 0; i < obj->class->num_fields; i++) {
      mark( *((Object**) (obj->class->field_offsets[i] + (void*)obj)) );   
   }

}

/* walk live and compute forwarding addresses */
void setForwarding() {
   int i = 0, off = 0, step;
   Object* o;
   
   while(i < nextFree) {

      o = (Object*) (heap + i);
      if(o == NULL) {
         break;
      }
   
      if( (char*)(o->class->name) == NULL ) {
         break;
      }
   
      if(strcmp("String", (char*)(o->class->name)) == 0) {
         step = ((String*)o)->length + String_class.size;
      } else {
         step = o->class->size;
      }
      
      // set forwarding address of live objects and ignore dead ones
      if(o->marked == 1) {
         
         o->forwarded = (Object*) (heap + off);
         off += step;
         o->marked = 0;
      } else {
         o->forwarded = NULL;
      }
      i += step;
   }

}

/* change pointer field addresses and root addresses */
void changePointers(Object* obj) {
   int i;
   Object** field;
   
   if(obj == NULL || obj->marked == 1 || (void*)obj < heap || 
         (void*)obj > heap + heapSize) {
      return;
   }
   
   obj->marked = 1;
      
   for(i = 0; i < obj->class->num_fields; i++) {
      
      field = ((Object**) (obj->class->field_offsets[i] + (void*)obj));
      
      if(*field == NULL) {
         continue;
      }
      changePointers(*field); 
      *field = (*field)->forwarded;
   }
}

/* move objects */
void moveObjects() {
   int i = 0, newNextFree = 0, step;
   Object* o;
   
   while(i < nextFree) {
      
      o = (Object*) (heap + i);
      
      if(o == NULL) {
         break;
      }
      
      if(strcmp("String", (char*)(o->class->name)) == 0) {
         step = ((String*)o)->length + String_class.size;
      } else {
         step = o->class->size;
      }
      
      if(o->marked == 1) {
         memcpy(o->forwarded, o, step);
         newNextFree += step;
         o->marked = 0;
         o->forwarded = NULL;
      }
      
      i += step;
   }
   
   nextFree = newNextFree;
  
}

/* free the heap */
void gc_done() {
   free(heap);
}

/* allocate an object */
Object *gc_alloc(ClassDescriptor *class) {
   int old_offset;
   int i;
   Object* o;
   
   if(nextFree + class->size > heapSize) {
      gc();
      if(nextFree + class->size > heapSize) {
         printf("No more space after garbage collection.");
         return NULL;
      }
   }
   old_offset = nextFree;
   
   nextFree += class->size;
   o = (Object*) (old_offset + heap);
   o->class = class;
   o->forwarded = NULL;
   o->marked = 0;
   
   /* force all object pointer fields to be null */
   for(i = 0; i < o->class->num_fields; i++) {
      *( (Object**) (o->class->field_offsets[i] + (void*)o) ) = NULL; 
      
   }
   
   return o;
}

ClassDescriptor String_class = {
    "String",
    sizeof (struct String), /* size of string obj, not string */
    0, /* fields */
    NULL
};

/* allocate a string */
String *gc_alloc_string(int size) {
   int old_offset;
   String* s;
   
   if(nextFree + String_class.size + size + 1 > heapSize) {
      gc();
      if(nextFree + String_class.size + size + 1 > heapSize) {
         printf("No more space after garbage collection.");
         return NULL;
      }
   }
   
   old_offset = nextFree;
   
   nextFree += String_class.size + size + 1;
   s = (String*) (old_offset + heap);
   s->class = &String_class;
   s->length = size+1;
   s->forwarded = NULL;
   s->marked = 0;

   return s;
}

/* dumps the heap */
char *gc_get_state() {
   char* buf = calloc(1024, sizeof(char));
   Object* obj;
   int i = 0, 
   offset = 0;
   int step;
   Object** field;
   
   sprintf(buf, "next_free=%d\nobjects:\n", nextFree);
   
   while(i < nextFree) {
      
      obj = (Object*) (heap + i);
      if(obj == NULL) {
         break;
      }
      
      offset = (void*)obj - heap;
      
      sprintf(buf, "%s  %04d:%s[", buf, offset, obj->class->name);
      
      /* string */
      if(strcmp(obj->class->name, "String") == 0) {
         step = ((String*)obj)->length + String_class.size;
         
         sprintf(buf, "%s%d+%d]=\"%s\"\n", buf, String_class.size, ((String*) obj)->length, ((String*) obj)->str);
      } 
      else { /* object */
         step = obj->class->size;
         sprintf(buf, "%s%d]->[", buf, step);
         field = NULL;
        
         /* get info on every field object */
         buf = doFields(obj, buf);
         
         sprintf(buf, "%s]\n", buf);
      }
      
      i += step;
   }
   return buf;
   
}

int gc_num_roots() {
   return _rp;
}

char *doFields(Object* obj, char* buf) {
   Object** field;
   int j;

   for(j = 0; j < obj->class->num_fields; j++) {
       
      field = ((Object**) (obj->class->field_offsets[j] + (void*)obj)); 
      
      if(j != 0) {
         sprintf(buf, "%s,", buf);
      }
      
      if(*field == NULL) {
         sprintf(buf, "%sNULL", buf);
      } else {
         sprintf(buf, "%s%ld", buf, ((void*)(*field))-heap);
      }
      
    }
   return buf;
}


char *printObjectsFromRoots() {
   char* buf = calloc(1024, sizeof(char));
   Object* obj;
   int i, j, offset;
   char* objName;
   int objSize;
   void* addr;
   
   sprintf(buf, "next_free=%d\nobjects:\n", nextFree);

   /* get info on every root */
   for (i = 0; i < _rp; i++) { 
      obj = *_roots[i];
      if(obj == NULL) {
         continue;
      }
      
      objName = obj->class->name;
      offset = (void*)obj - heap;
      
      sprintf(buf, "%s  %04d:%s[", buf, offset, objName);
      
      /* string */
      if(strcmp(objName, "String") == 0) {
         objSize = ((String*) obj)->length + 1;
         sprintf(buf, "%s%d+%d]=\"%s\"\n", buf, String_class.size, objSize, ((String*) obj)->str);
      } 
      else { /* object */
         objSize = obj->class->size;
         sprintf(buf, "%s%d]->[", buf, objSize);
         /* get info on every field object */
         for(j = 0; j < obj->class->num_fields; j++) {
            if(j != 0) {
               sprintf(buf, "%s,", buf);
            }
            addr = obj->class->field_offsets[j] + obj;
            offset = addr - heap;
            sprintf(buf, "%s%d", buf, offset);
         }
         sprintf(buf, "%s]\n", buf);
      }
   }
	
   return buf;
}