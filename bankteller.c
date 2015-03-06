#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

static pthread_mutex_t mutex;
static pthread_cond_t teller_available;

typedef struct teller_info_t {
   int id;
   int checked_in;
   int doing_service;
   pthread_cond_t done;
   pthread_t thread;
   struct teller_info_t *next;
   struct teller_info_t *end;
} *p_teller;

static p_teller teller_list = NULL;


void teller_check_in(p_teller teller) 
{
   pthread_mutex_lock(&mutex);
   teller->checked_in = 1; 
   teller->doing_service = 0; 
   // add teller to list
   pthread_cond_init(&teller->done, NULL);
   
   if(teller_list == NULL){
      teller_list = teller;
      teller_list->next = NULL;
   }else{
      teller->next = teller_list;
      teller_list = teller;
   }
   pthread_cond_signal(&teller_available);
   pthread_mutex_unlock(&mutex);
}

void teller_check_out(p_teller teller)
{
  // check if the teller is done
   // if not, wait for teller to be done
   pthread_mutex_lock(&mutex);
   while(teller->doing_service){
      pthread_cond_wait(&teller->done, &mutex);
   }
 
   // remove teller from list
   if(teller == teller_list){
      teller_list = teller_list->next;
      teller->next = NULL;
   }else{
      //we have to search for it
      p_teller tmp;
      tmp = teller_list;
      while(tmp->next != teller){
      tmp =  tmp->next;
      }
      tmp->next = teller->next;
      teller->next = NULL;
   }
   pthread_mutex_unlock(&mutex);
 
   teller->checked_in = 0; 
}

p_teller do_banking(int customer_id)
{
   // check if list contains a teller (=teller is available)
   // if not, wait for teller to become available 
   // TEST
   pthread_mutex_lock(&mutex);
   while(teller_list == NULL){
      pthread_cond_wait(&teller_available, &mutex);
   }
   //now we know that a teller is available
   p_teller teller;
   teller = teller_list;
   teller_list = teller_list->next;
   teller->next = NULL;
   printf("Customer %d is served by teller %d\n", customer_id, teller->id);
   teller->doing_service = 1;
   pthread_mutex_unlock(&mutex);

   return teller;
}

void finish_banking(int customer_id, p_teller teller)
{
   pthread_mutex_lock(&mutex);
   printf("Customer %d is done with teller %d\n", customer_id, teller->id);

   teller->doing_service = 0;
   
   teller->next = teller_list;
   teller_list = teller;
   
   pthread_cond_signal(&teller_available);
   pthread_cond_signal(&teller->done); 
   pthread_mutex_unlock(&mutex);
}

void* teller(void *arg)
{
   p_teller me = (p_teller) arg;
  
   // perform an initial checkin
   teller_check_in(me);

   while(1)
   {
      // in 5% of the cases toggle status
      int r = rand();
      if (r < (0.05 * RAND_MAX))
      {
         if (me->checked_in) 
         {
            printf("teller %d checks out\n", me->id); 
            teller_check_out(me);

            // uncomment line below to let program terminate for testing
             //return 0;
         } else {
            printf("teller %d checks in\n", me->id);
            teller_check_in(me);
         }
      }
   }
}

void* customer(void *arg)
{
   int id = (uintptr_t) arg;

   while (1)
   {
      // in 10% of the cases enter bank and do business
      int r = rand();
      if (r < (0.1 * RAND_MAX))
      {
         p_teller teller = do_banking(id);
         // sleep up to 3 seconds
         sleep(r % 4);
         finish_banking(id, teller);
      }
   }
}

#define NUM_TELLERS 3
#define NUM_CUSTOMERS 5

int main(void)
{
   srand(time(NULL));
   struct teller_info_t tellers[NUM_TELLERS];
   int i;

   // initialize mutexes/condition variables
   pthread_mutex_init(&mutex, NULL);
   pthread_cond_init(&teller_available, NULL);


   for (i=0; i<NUM_TELLERS; i++) 
   { 
      tellers[i].id = i;
      tellers[i].next = NULL;
      pthread_create(&tellers[i].thread, NULL, teller, (void*) &tellers[i]);
   }

   pthread_t thread;

   for (i=0; i<NUM_CUSTOMERS; i++) 
   {
      pthread_create(&thread, NULL, customer, (void*) (uintptr_t) i);
   }

   for (i=0; i<NUM_TELLERS; i++) 
   {
     pthread_join(tellers[i].thread, NULL);
   }

   return 0;
}