/*--------------------------------------------------------------------------*
 * Author: Oved Mizrahi														*	
 * Project: Process Watchdog - functions									*
 * Last update: 20.01.20  													*
 *--------------------------------------------------------------------------*/

#define _POSIX_C_SOURCE 200112L

#include <stdio.h>		/* errno */
#include <stdlib.h>		/* getenv */
#include <assert.h>		/* assert */
#include <unistd.h>		/* fork */
#include <signal.h>		/* struct sigaction */
#include <semaphore.h>	/* sem functions */
#include <fcntl.h>		/* pid_t, sem_open flags */
#include <sys/stat.h>	/* sem_open mode */
#include <time.h>		/* struct timespec */
#include <signal.h>		/* struct sigaction */
#include <pthread.h>	/* thread functions */

#include "wd.h"
#include "shared_wd.h"

enum status
{
	SUCCESS = 0,
	FAIL = 1
};

/*----------------------Declaration auxiliary functions-----------------------*/

static int StopSchedIfNeededIMP(void *wd_pid);
static void TurnOnStopFlagIMP(int sig);

/*-----------------------------Global variables-------------------------------*/

static size_t g_stop_flag = CONTINUE;

/*----------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	int status = 0;
	pid_t pid_thread = getppid();
	scheduler_t *sched = NULL;
	struct sigaction sig1 = {0};
	struct sigaction sig2 = {0};
	restart_param_t restart_param= {0};
	sem_err_t msg_err_th = {NULL};
	sem_err_t msg_err_wd = {NULL};
	(void)argc;
	
	status = setenv("wd_is_exist", "alive", 1);	
	if (0 != status)
	{
		return 0;
	}
	
	sig1.sa_handler = ResetCounterSig;
	sigaction(SIGUSR1, &sig1, NULL);
	
	sig2.sa_handler = TurnOnStopFlagIMP;
	sigaction(SIGUSR2, &sig2, NULL);
	
	msg_err_wd.err_open = "sem_wd_open";
	msg_err_wd.err_wait = "sem_wd_wait";
	msg_err_wd.err_unlink = "sem_wd_unlink";
	InitializedRestartParam(&restart_param, argv[0], argv, "/sem_wd", 
				&msg_err_wd, &pid_thread);
	
	sched = SchedularHandler(&restart_param);
	if (NULL == sched)
	{
		return 0;
	}
	SchedulerAdd(sched, StopSchedIfNeededIMP, 1, sched);
	
	msg_err_th.err_open = "sem_th_open2";
	msg_err_th.err_post = "sem_th_post";
	
	status = SemPostIMP("/sem_th", &msg_err_th);
	if (SUCCESS != status)
	{
		return 0;
	}

	status = SchedulerRun(sched);
	/* I don't care about return value from SchedulerRun() */
	SchedulerDestroy(sched);
	
	return 0;
}

/*----------------------------Auxiliary functions-----------------------------*/

/*	this function is signal hendler - the function change the stop-flag 
	to STOP. */
static void TurnOnStopFlagIMP(int sig)
{
	(void)sig;
	
	g_stop_flag = STOP;
}

/*------------------------------Tasks functions-------------------------------*/

/*	This function it's task for schedular. 
	the function check all the time if the schedular need to stop.
	if need to stop, stop the schedular and send sem_post to the thread.
	the function return 1 - return the task to the schedular. */
static int StopSchedIfNeededIMP(void *sched)
{	
	if (CONTINUE != g_stop_flag)
	{
		int status = 0;
		sem_t *sem_stop_addr = sem_open("/sem_stop", O_CREAT, S_IRWXU ,0);
		
		if (NULL == sem_stop_addr)	 
		{
#ifndef NDEBUG
			perror("sem_stop_open: "); 
#endif	
			return 1;
		}
		
		SchedulerStop(sched);
		
		status = sem_post(sem_stop_addr);
		if (0 != status)
		{
#ifndef NDEBUG
			perror("sem_stop_post: ");
#endif		
			return FAIL;
		}
	}

	return 1;
}

/*----------------------------------------------------------------------------*/
