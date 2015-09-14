#include "dummy.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//@ import_module public_invariant;

/*@
predicate dummy_proof_pred() = true;

predicate_family_instance pthread_run_pre(attacker_t)(void *data, any info) =
    [_]public_invar(dummy_pub) &*&
    public_invariant_constraints(dummy_pub, dummy_proof_pred) &*&
    principals(_);
@*/

void *attacker_t(void* data) //@ : pthread_run_joinable
  //@ requires pthread_run_pre(attacker_t)(data, ?info);
  //@ ensures  false;
{
  while(true)
    //@ invariant pthread_run_pre(attacker_t)(data, info);
  {
    //@ open pthread_run_pre(attacker_t)(data, info);
    //@ close dummy_proof_pred();
    attacker();
    //@ open dummy_proof_pred();
    //@ close pthread_run_pre(attacker_t)(data, info);
  }
   
  return 0;
}

/*@

inductive info =
  | int_value(int v)
  | char_list_value(list<char> p)
;

predicate_family_instance pthread_run_pre(sender_t)(void *data, any info) =
  principal(?sender, _) &*&
  chars(data, PACKAGE_SIZE, ?cs) &*&
  info == cons(int_value(sender),
            cons(char_list_value(cs),
                 nil))
;

predicate_family_instance pthread_run_post(sender_t)(void *data, any info) =
  principal(?sender, _) &*&
  chars(data, PACKAGE_SIZE, ?cs) &*&
  info == cons(int_value(sender),
            cons(char_list_value(cs),
                 nil))
;

@*/

void *sender_t(void *data) //@ : pthread_run_joinable
  //@ requires pthread_run_pre(sender_t)(data, ?x);
  //@ ensures pthread_run_post(sender_t)(data, x) &*& result == 0;
{
  //@ open pthread_run_pre(sender_t)(data, _);
  sender(data);
  //@ close pthread_run_post(sender_t)(data, x);
  return 0;
}

/*@
predicate_family_instance pthread_run_pre(receiver_t)(void *data, any info) = 
  principal(?receiver, _) &*&
  chars(data, PACKAGE_SIZE, _) &*&
  info == cons(int_value(receiver), nil)
;

predicate_family_instance pthread_run_post(receiver_t)(void *data, any info) = 
  principal(?receiver, _) &*&
  chars(data, PACKAGE_SIZE, _) &*&
  info == cons(int_value(receiver), nil)
;
@*/

void *receiver_t(void* data) //@ : pthread_run_joinable
  //@ requires pthread_run_pre(receiver_t)(data, ?x);
  //@ ensures pthread_run_post(receiver_t)(data, x) &*& result == 0;
{
  //@ open pthread_run_pre(receiver_t)(data, _);
  receiver(data);
  //@ close pthread_run_post(receiver_t)(data, x);
  return 0;
}

int main(int argc, char **argv) //@ : main_full(main_app)
    //@ requires module(main_app, true);
    //@ ensures true;
{
  //@ open_module();
  //@ assert module(public_invariant, true);
  
  pthread_t a_thread;
  havege_state havege_state;
  
  printf("\n\tExecuting \"");
  printf("dummy protocol");
  printf("\" ... \n\n");
  
  //@ PUBLIC_INVARIANT_CONSTRAINTS(dummy)
  //@ public_invariant_init(dummy_pub);
  
  //@ principals_init();
  //@ int attacker = principal_create();
  //@ int sender = principal_create();
  //@ int receiver = principal_create();
  
  //@ close pthread_run_pre(attacker_t)(NULL, nil);
  pthread_create(&a_thread, NULL, &attacker_t, NULL);
  
  //@ close havege_state(&havege_state);
  havege_init(&havege_state);
  int i = 0;
#ifdef EXECUTE
  while (i++ < 10)
#else
  while (true)
#endif
    /*@ invariant [_]public_invar(dummy_pub) &*&
                  havege_state_initialized(&havege_state) &*&
                  principal(sender, _) &*& principal(receiver, _);
    @*/
  {
    char msg1[PACKAGE_SIZE];
    //@ close random_request(sender, 0, false);
    if (havege_random(&havege_state, msg1, PACKAGE_SIZE) != 0) abort();
    //@ assert cryptogram(msg1, PACKAGE_SIZE, ?cs, ?cg);
    //@ close dummy_pub(cg);
    //@ leak dummy_pub(cg);
    //@ public_cryptogram(msg1, cg);
    char msg2[PACKAGE_SIZE];
    {
      pthread_t s_thread, c_thread;
      
      //@ close pthread_run_pre(sender_t)(msg1, _);
      pthread_create(&s_thread, NULL, &sender_t, msg1);
      //@ close pthread_run_pre(receiver_t)(msg2, _);
      pthread_create(&c_thread, NULL, &receiver_t, msg2);
      
      pthread_join(s_thread, NULL);
      //@ open pthread_run_post(sender_t)(msg1, _);
      pthread_join(c_thread, NULL);
      //@ open pthread_run_post(receiver_t)(msg2, _);
      
      //@ close optional_crypto_chars(false, msg1, PACKAGE_SIZE, _);
      //@ close optional_crypto_chars(false, msg2, PACKAGE_SIZE, _);
      if (memcmp(msg1, msg2, PACKAGE_SIZE) != 0)
        abort();
        
      printf(" |%i| ", i);
    }
  }
  
  printf("\n\n\t\tDone\n");
  return 0;
}
