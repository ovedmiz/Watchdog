/*--------------------------------------------------------------------------*
 * Author: Oved Mizrahi														*	
 * Project: Shared Watchdog - header										*
 * Last update: 19.01.20  													*
 *--------------------------------------------------------------------------*/

#ifndef __SHARED_WD_H__
#define __SHARED_WD_H__

#include "sched.h"

typedef struct sem_err
{
	const char *err_open;
	const char *err_wait;
	const char *err_post;
	const char *err_unlink;
}sem_err_t;

typedef struct restart_param
{
	char *prog_name;
	char **argv;
	char *sem_name; 
	sem_err_t *msg_err;
	pid_t *pid_other_process;
}restart_param_t;

enum stop_flag
{
	CONTINUE = 0,
	STOP = 1
};

/*----------------------------------------------------------------------------*/

/*	the function create a scheduler and add the shared events,
	if success - return pointer to the scheduler, otherwise -  return NULL	*/
scheduler_t *SchedularHandler(restart_param_t *param);

/*----------------------------------------------------------------------------*/

/*	the function create a process with fork and exec and then wait with 
	semaphore until she get post from the new process.
	if success - return the pid of the new process, otherwise - return -1	*/
pid_t CreateProcessIMP(char *prog_name, char *argv[], char *sem_name, 
			sem_err_t *msg_err, size_t num_of_seconds_for_wait);

/*----------------------------------------------------------------------------*/

/*	the function get semaphore address and wait for post.
	return 0 if post that received, and 1 if num_of_minutes passed.	*/
int SemWaitIMP(sem_t *sem_addr, char *sem_name, sem_err_t *msg_err, 
			size_t num_of_seconds);

/*----------------------------------------------------------------------------*/		
/* 	The function open the semaphore with sem_name and post him. 
	return 0 if success and 1 if fail. */
int SemPostIMP(char *sem_name, sem_err_t *msg_err);

/*----------------------------------------------------------------------------*/

/*	the function get a restart_param struct and initialized him.	*/
void InitializedRestartParam(restart_param_t *restart_param, char *prog_name, 
			char *argv[], char *sem_name, sem_err_t *msg_err, 
					pid_t *pid_process);

/*----------------------------------------------------------------------------*/

/*	signal hendler function. the function restet the global couner signals.	*/
void ResetCounterSig(int sig);

/*----------------------------------------------------------------------------*/

#endif /* __SHARED_WD_H__ */
