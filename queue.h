/*
  (c) Mikhail Tentyukov
  Based on the Smirnov & Tentyukov code of FIESTA.

  Inline implementation of the generic queues, both LIFO and FIFO.
  Queues base type is some basic type (an arithmentic type or a pointer),
  by default void*, may be changed setting the macro QFL_TYPE _BEFORE_ including
  queue.h.

  All public functions have some prefix (<prefix> below), by default voidptr_,
  may be changed setting the macro QFL_PREFIX _BEFORE_ including queue.h.
  See example below.

  A queue is a cyclic buffer with two pointers, "head" and "tail".
  if head==tail then the queue is empty. If head+1==tail, the queue is full.
  Elements are always extracted from the tail.
  If new elements are placed to the head, then the queue is a FIFO queue,
  if new elements are placed to the tail, then the queue is a LIFO queue.

  The queue is created by the constructor <prefix>qFLInit. The last argument of
  the constructor "isFixed", defines the behaviour of the queue when the
  buffer is full:

  If isFixed ==0, the queue  autmatically reallocates the full buffer.
  If isFixed>0, the queue overwrites the buffer when it is full.
  If isFixed <0, the push operation fails with -2 when the buffer is full.
  Corresponding macros may be used:
  QFL_REALLOC_IF_FULL, QFL_OVERWRITE_IF_FULL and QFL_FAIL_IF_FULL.

  Public functions (here we omit <prefix>):
  qFLInit -- constructor;
  qFLDestroy -- destructor;
  qFLIsFull -- chech whether a queue is full;
  qFLIsEmpty -- chech whether a queue is empty;
  qFLReset -- empty the queue without destroying it;
  qFLPop -- extract one element from the queue;
  qFLPushFifo -- put a new element to the FIFO queue;
  qFLPushLifo -- put a new element to the LIFO queue.

  The macro QFL0 is a static initializer for the structure qFL_struct,
  which may be used to prevent comiler warnings:
  qFL_t q=QFL0;

  Example:
  #include <stdio.h>

  // one wants reallocatable queue of generic pointers:
  #include "queue.h"

  // one wants cyclic buffer of integers > 0:
  #define QFL_TYPE int
  #define QFL_PREFIX int_
  #define QFL_NULL (-1)
  #include "queue.h"

  int main(void)
  {
     voidptr_qFL_t ptrQueue = QFL0;
     int_qFL_t intQueue = QFL0;

     voidptr_qFLInit(NULL, 0, &ptrQueue, QFL_REALLOC_IF_FULL);

     voidptr_qFLPushLifo(&ptrQueue,(void*)"first in");
     voidptr_qFLPushLifo(&ptrQueue,(void*)"last in");

     int_qFLInit(NULL, 15, &intQueue, QFL_OVERWRITE_IF_FULL);

     int_qFLPushFifo(&intQueue, 1);
     int_qFLPushFifo(&intQueue, 2);

     printf("First out: '%s', ", (char*)voidptr_qFLPop(&ptrQueue));
     printf("last out: '%s'\n", (char*)voidptr_qFLPop(&ptrQueue));

     printf("First out: '%d', ", int_qFLPop(&intQueue));
     printf("last out: '%d'\n", int_qFLPop(&intQueue));

     voidptr_qFLDestroy(&ptrQueue);
     int_qFLDestroy(&intQueue);
     return 0;
  }
*/

#ifndef QFL_TYPE
#define QFL_TYPE void*
#endif

#ifndef QFL_PREFIX
#define QFL_PREFIX voidptr_
#endif

/*for return nothing:*/
#ifndef QFL_NULL
#define QFL_NULL NULL
#endif

#ifdef DECLARE1
#undef DECLARE1
#endif

#ifdef _DODECLARE1
#undef _DODECLARE1
#endif


#define DECLARE1(name) _DODECLARE1(QFL_PREFIX, name)
#define _DODECLARE1(pref, name) _CAT2(pref, name)
#ifdef _CAT2
#undef _CAT2
#endif
#define _CAT2(arg1, arg2) arg1##arg2

#ifdef DECLARE2
#undef DECLARE2
#endif
#ifdef _DODECLARE2
#undef _DODECLARE2
#endif

#define DECLARE2(name, suffix) _DODECLARE2(QFL_PREFIX, name, suffix)
#define _DODECLARE2(pref, name, suffix) _CAT3(pref, name, suffix)
#ifdef _CAT3
#undef _CAT3
#endif
#define _CAT3(arg1, arg2, arg3) arg1##arg2##arg3

#include <stdlib.h>
#include <string.h>

#include "comdef.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef QUEUE_H

#ifdef COM_INLINE
#define QFL_INLINE COM_INLINE
#else
#define QFL_INLINE inline
#endif

#ifdef COM_INT
#define FL_INT COM_INT
#else
#define FL_INT int
#endif

#endif

/*
  If isFixed>0, the queue overwrites the buffer when it is full.
  If isFixed <0 -- when the buffer is full, push operation fails with -2:
*/

#ifndef QUEUE_H
#define QFL_FAIL_IF_FULL (-1)
#define QFL_OVERWRITE_IF_FULL 1
#define QFL_REALLOC_IF_FULL 0

#define QFL_MAX_LENGTH 2147483647 /*2^31 - 1*/

#define QFL_INI_LENGTH 1024

#endif /*#ifndef QUEUE_H*/

#ifndef qFLAlloc
#define qFLAlloc(theSize) malloc(theSize)
#define qFLFree(theScratch,theLength) free(theScratch)
#define qFLAllocFail NULL
#endif

typedef struct {
   QFL_TYPE *scratch;
   FL_INT len;
   /*if head==tail then the queue is empty. If head+1==tail, the queue is full.*/
   FL_INT head;/*Put new cells for Fifo*/
   FL_INT tail;/*Pop cells and put Lifo*/
   int isFixed;/*0 if reallocatable, otherwise, non-rellocatable:
                 >0 -- overwrite when full, <0 -- fail with -2*/
}DECLARE2(qFL,_t);

/*Static initializer for the structure qFL_t struct:*/
#define QFL0 {NULL,0,0,0,0}

/*Expects theFL->head == theFL->tail:*/
static QFL_INLINE 
int DECLARE1(qFLRealloc)(DECLARE2(qFL,_t) *theFL)
{
int oldLen=theFL->len;
QFL_TYPE *tmp;

   if(theFL->len >= QFL_MAX_LENGTH )
      return -1;

   theFL->len *= 2;

   tmp=(QFL_TYPE*) qFLAlloc(theFL->len*sizeof(QFL_TYPE));
   if(tmp == qFLAllocFail)
       return -1;
   memcpy(tmp,theFL->scratch,theFL->head*sizeof(QFL_TYPE));
   memcpy(tmp+theFL->head+oldLen,theFL->scratch+theFL->tail,
           (oldLen-theFL->tail)*sizeof(QFL_TYPE));
   theFL->tail+=oldLen;
   qFLFree(theFL->scratch,oldLen*sizeof(QFL_TYPE));
   theFL->scratch=tmp;
   return 0;
}/*qFLRealloc*/

/*Returns 0 if after nex push a queue is still NOT full, otherwise !0:*/
static QFL_INLINE 
FL_INT DECLARE1(qFLIsFull)(DECLARE2(qFL,_t) *theFL)
{
FL_INT wrk = theFL->head + 1;
   if (wrk == theFL->len)
      wrk = 0;
   return (wrk == theFL->tail);
}/*qFLIsFull*/

/*Returns !0 if a queue is empty, otherwise 0:*/
static QFL_INLINE 
FL_INT DECLARE1(qFLIsEmpty)(DECLARE2(qFL,_t) *theFL)
{
   return (theFL->head == theFL->tail);
}/*qFLIsFull*/


/*Puts the cell to the head:*/
static QFL_INLINE 
FL_INT DECLARE1(qFLPushFifo)(DECLARE2(qFL,_t) *theFL, QFL_TYPE cell)
{
FL_INT ret=theFL->head;
   theFL->scratch[ret]=cell;
   theFL->head++;
   if(theFL->head == theFL->len)
      theFL->head=0;
    if(theFL->head == theFL->tail){/*The buffer is full*/
      if(theFL->isFixed == 0){
         if(DECLARE1(qFLRealloc)(theFL))
            return -1;
         /*recalc ret*/
         ret=theFL->head-1;
         if(ret < 0)
            ret=theFL->len-1;         
         return ret;
      }/*if(theFL->isFixed == 0)*/
      if(theFL->isFixed>0){/*next time overwrite the tail*/
         if(++theFL->tail== theFL->len)
           theFL->tail=0;
         return ret;
      }
      /*theFL->isFixed<0 -- rollback head and fail with -2:*/
      if(--theFL->head<0)
         theFL->head=theFL->len-1;
      return -2;
   }/*if(theFL->head == theFL->tail)*/
   return ret;
}/*qFLPushFifo*/

/*Do LIFO placement (puts the cell to the tail):*/
static QFL_INLINE 
FL_INT DECLARE1(qFLPushLifo)(DECLARE2(qFL,_t) *theFL, QFL_TYPE cell)
{
   if(--theFL->tail <0)
      theFL->tail=theFL->len-1;
   if(theFL->head == theFL->tail){/*The buffer is full*/
      if(theFL->isFixed == 0){/*realloc*/
         if(DECLARE1(qFLRealloc)(theFL))
            return -1;
      }/*if(theFL->isFixed == 0)*/
      else if(theFL->isFixed>0){/*overwrite the head*/
         if(--theFL->head<0)
            theFL->head = theFL->len-1;
      }/*else if(theFL->isFixed>0)*/
      else{/*theFL->isFixed<0, rollback the tail and fail*/
         if(++theFL->tail == theFL->len)
            theFL->tail = 0;
         return -2;
      }
   }/*if(theFL->head == theFL->tail)*/
   theFL->scratch[theFL->tail]=cell;
   return theFL->tail;
}/*qFLPushLifo*/

static QFL_INLINE 
QFL_TYPE DECLARE1(qFLPop)(DECLARE2(qFL,_t) *theFL)
{
   QFL_TYPE ret;

   if(theFL->head == theFL->tail)
      return QFL_NULL;/*empty queue*/

   ret=theFL->scratch[theFL->tail];
   if(++theFL->tail == theFL->len)
      theFL->tail=0;
   return ret;
}/*qFLPop*/

/*Initializes the queue theFL. If it is NULL, allocates it.
  If scratch==NULL, allocates the scratch, otherwise, use the given array.
  DO NOT CALL destroy, DO NOT USE isFixed == 0 for the external array!
  If iniLength> 0, uses this value for initialization, otherwise, uses default value.
  If isFixed ==0, the queue is autmatically reallocates the full buffer.
  If isFixed>0, the queue overwrites the buffer when it is full.
  If isFixed <0 -- when the buffer is full, push operation fails with -2:
*/
static QFL_INLINE 
DECLARE2(qFL,_t) *DECLARE1(qFLInit)(QFL_TYPE *scratch,FL_INT iniLength, DECLARE2(qFL,_t) *theFL, int isFixed)
{
  if(theFL == NULL){
     theFL=malloc(sizeof(DECLARE2(qFL,_t)));
     if(theFL == NULL)
        return NULL;
  }
  theFL->isFixed=isFixed;
  if(iniLength<=0)
     theFL->len=QFL_INI_LENGTH;
  else
     theFL->len=iniLength;
  theFL->head=theFL->tail=0;
  if(scratch!=NULL)
     theFL->scratch=scratch;
  else{
     theFL->scratch=(QFL_TYPE*) qFLAlloc( (theFL->len)*sizeof(QFL_TYPE));
     if(theFL->scratch == qFLAllocFail)
        return NULL;
  }
  return theFL;
}/*qFLInit*/

static QFL_INLINE 
void DECLARE1(qFLReset)(DECLARE2(qFL,_t) *theFL)
{
   theFL->head=theFL->tail=0;
}/*qFLReset*/

static QFL_INLINE 
void DECLARE1(qFLDestroy)(DECLARE2(qFL,_t) *theFL)
{
   qFLFree(theFL->scratch,theFL->len*sizeof(QFL_TYPE));
   theFL->scratch=NULL;
   theFL->len=0;
   theFL->head=theFL->tail=0;
}/*qFLDestroy*/

#undef QFL_TYPE
#undef QFL_PREFIX
#undef DECLARE1
#undef DECLARE2
#undef QFL_NULL


#ifndef QUEUE_H
#define QUEUE_H 1
#endif

#ifdef __cplusplus
}
#endif

