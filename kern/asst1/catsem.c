/*
 * catsem.c
 *
 * 30-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: Please use SEMAPHORES to solve the cat syncronization problem in 
 * this file.
 */


/*
 * 
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>

int volatile CatsEating = 0;
int volatile MiceEating = 0;
int volatile CatsWaiting = 0;
int volatile MiceWaiting = 0;
int volatile Bowl[2] = {0, 0};
int volatile CatIteration[6] = {-1, -1, -1, -1, -1, -1};
int volatile MiceIteration[2] = {-1, -1};
struct semaphore*    CatLimiter;
struct semaphore*    CatMutex;
struct semaphore*    MiceMutex;
struct semaphore*    CatMouseContention;
struct semaphore*    SimulationCounter;




/*
 * 
 * Constants
 *
 */

/*
 * Number of food bowls.
 */

#define NFOODBOWLS 2

/*
 * Number of cats.
 */

#define NCATS 6

/*
 * Number of mice.
 */

#define NMICE 2


/*
 * 
 * Function Definitions
 * 
 */

/* who should be "cat" or "mouse" */
static void sem_eat(const char *who, int num, int bowl, int iteration) {
  kprintf("%s: %d starts eating: bowl %d, iteration %d\n", who, num, bowl, iteration);
  clocksleep(1);
  kprintf("%s: %d ends eating: bowl %d, iteration %d\n", who, num, bowl, iteration);
}

/*
 * catsem()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds the cat identifier from 0 to NCATS - 1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using semaphores.
 *
 */

static void catsem(void * unusedpointer, unsigned long catnumber) {

  (void) unusedpointer;

  int bowl;

  int i;
  for (i = 0; i < 4; i++) {

    P(CatLimiter);

    P(CatMutex);
    CatsEating++;
    if (CatsEating == 1)
      P(CatMouseContention);
    if (CatIteration[catnumber] != 3)
      CatIteration[catnumber]++;
    if (Bowl[0] == 0) {
      Bowl[0] = 1;
      bowl = 1;
    }
    else {
      Bowl[1] = 1;
      bowl = 2;
    }
    V(CatMutex);

    sem_eat("cat",catnumber, bowl,CatIteration[catnumber]);

    P(CatMutex);
    CatsEating--;
    if (bowl == 1) {
      Bowl[0] = 0;
    }
    else {
      Bowl[1] = 0;
    }
    if (CatsEating == 0)
      V(CatMouseContention);
    V(CatMutex);
    V(CatLimiter);

  }



  V(SimulationCounter);




}
        

/*
 * mousesem()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long mousenumber: holds the mouse identifier from 0 to 
 *              NMICE - 1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using semaphores.
 *
 */

static void mousesem(void * unusedpointer, unsigned long mousenumber) {

  (void) unusedpointer;

  int bowl;

  int i;
  for (i = 0; i < 4; i++) {
    P(MiceMutex);
    MiceEating++;
    if (MiceEating == 1)
      P(CatMouseContention);
    if (MiceIteration[mousenumber] != 3)
      MiceIteration[mousenumber]++;
    if (Bowl[0] == 0) {
      Bowl[0] = 1;
      bowl = 1;
    }
    else {
      Bowl[1] = 1;
      bowl = 2;
    }
    V(MiceMutex);

    sem_eat("mouse", mousenumber, bowl, MiceIteration[mousenumber]);

    P(MiceMutex);
    MiceEating--;
    if (bowl == 1) {
      Bowl[0] = 0;
    }
    else {
      Bowl[1] = 0;
    }
    if (MiceEating == 0)
      V(CatMouseContention);
    V(MiceMutex);

  }

  V(SimulationCounter);




}


/*
 * catmousesem()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up catsem() and mousesem() threads.  Change this 
 *      code as necessary for your solution.
 */

int catmousesem(int nargs, char ** args) { 

//  int i;
//  for (i = 0; i < 6; i++) CatIteration[i] = -1;
//  for (i = 0; i < 2; i++) MiceIteration[i] = -1;


  if (CatLimiter == NULL) {
    CatLimiter = sem_create("CatLimiter", 2);
    if (CatLimiter == NULL) {
      panic("CatLimiter: sem_create failed\n");
    }
  }
  if (CatMutex == NULL) {
    CatMutex = sem_create("CatMutex", 1);
    if (CatMutex == NULL) {
      panic("CatMutex: sem_create failed\n");
    }
  }
  if (MiceMutex == NULL) {
    MiceMutex = sem_create("MiceMutex", 1);
    if (MiceMutex == NULL) {
      panic("MiceMutex: sem_create failed\n");
    }
  }
  if (CatMouseContention == NULL) {
    CatMouseContention = sem_create("CatMouseContention", 1);
    if (CatMouseContention == NULL) {
      panic("CatMouseContention: sem_create failed\n");
    }
  }
  if (SimulationCounter == NULL) {
    SimulationCounter = sem_create("SimulationCounter", 0);
    if (SimulationCounter == NULL) {
      panic("SimulationCounter: sem_create failed\n");
    }
  }

  int index, error;
   
  /*
   * Avoid unused variable warnings.
   */

  (void) nargs;
  (void) args;

   
  /*
   * Start NCATS catsem() threads.
   */

  for (index = 0; index < NCATS; index++) {
    error = thread_fork("catsem Thread", NULL, index, catsem, NULL);
                
    /*
     * panic() on error.
     */

    if (error) {
      panic("catsem: thread_fork failed: %s\n", strerror(error));
    }
  }

  /*
   * Start NMICE mousesem() threads.
   */

  for (index = 0; index < NMICE; index++) {
    error = thread_fork("mousesem Thread", NULL, index, mousesem, NULL);
                
    /*
     * panic() on error.
     */

     if (error) {
       panic("mousesem: thread_fork failed: %s\n", strerror(error));
     }
  }

  int i;
  for (i = 0; i < 8; i++) P(SimulationCounter);

  sem_destroy(CatLimiter);
  sem_destroy(CatMutex);
  sem_destroy(MiceMutex);
  sem_destroy(CatMouseContention);
        




  return 0;
}

/*
 * End of catsem.c
 */
