/*--------------------------------------------------------------------------*
 * Author: Oved Mizrahi														*	
 * Project: Shared Watchdog - functions										*
 * Last update: 20.01.20  													*
 *--------------------------------------------------------------------------*/
 
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>		/* errno */
#include <stdlib.h>		/* getenv */
#include <assert.h>		/* assert */
#include <unistd.h>		/* fork */
#include <semaphore.h>	/* sem functions */
#include <fcntl.h>		/* pid_t, sem_open flags */
#include <sys/stat.h>	/* sem_open mode */
#include <time.h>		/* struct timespec */
#include <signal.h>		/* struct sigaction */
#include <pthread.h>	/* thread functions */
#include <sys/wait.h>	/* waitpid */

#include "shared_wd.h"

#define MAX_SIG 5

enum status
{
	SUCCESS = 0,
	FAIL = 1
};

/*------------------------Declaration tasks functions-------------------------*/

static int SendSignalToParamPid(void *wd_param);
static int RestartProcessIfNeeded(void *restart_param);

/*-----------------------------Global variables-------------------------------*/

static size_t g_count_signals = 0;

/*----------------------------------------------------------------------------*/

scheduler_t *SchedularHandler(restart_param_t *param)
{
	ilrd_uid_t task_uid = {0};
	scheduler_t *sched = SchedulerCreate();
	
	assert(NULL != param);
	
	if (NULL == sched)
	{
#ifndef NDEBUG
		write(0, "Error in create schedular.\n\n", 28);
#endif
		return NULL;
	}
	
	task_uid = SchedulerAdd(sched, SendSignalToParamPid, 1, 
					param->pid_other_process);
	if (UIDIsBad(task_uid))
	{
		SchedulerDestroy(sched);
		
		return NULL;
	}

	task_uid = SchedulerAdd(sched, RestartProcessIfNeeded, 1, param);
	if (UIDIsBad(task_uid))
	{
		SchedulerDestroy(sched);
		
		return NULL;
	}
		
	return sched;
}

/*----------------------------------------------------------------------------*/

pid_t CreateProcessIMP(char *prog_name, char *argv[], char *sem_name, 
			sem_err_t *msg_err, size_t num_of_seconds_for_wait)
{
	int status = 0;
	sem_t *sem_addr = NULL;
	pid_t wd_pid = 0;

	assert(NULL != prog_name);
	assert(NULL != argv);
	assert(NULL != sem_name);
	assert(NULL != msg_err);

	wd_pid = fork();
	if (0 == wd_pid)
	{
		execvp(prog_name, argv);
	}
	else if (-1 == wd_pid)
	{
		return -1;
	}

	sem_addr = sem_open(sem_name, O_CREAT, S_IRWXU ,0);
	if (NULL == sem_addr)
	{
#ifndef NDEBUG
		perror(msg_err->err_open); 
#endif
		return -1;
	}

	status = SemWaitIMP(sem_addr, sem_name, msg_err, num_of_seconds_for_wait);
	if (SUCCESS != status)
	{
		return -1;
	}
	
	return wd_pid;
}

/*----------------------------------------------------------------------------*/

int SemWaitIMP(sem_t *sem_addr, char *sem_name, sem_err_t *msg_err, 
			size_t num_of_seconds)
{
	int status = 0;
	struct timespec time = {0};

	assert(NULL != sem_addr);
	assert(NULL != msg_err);
	assert(NULL != sem_name);
	
	clock_gettime(CLOCK_REALTIME, &time);
	time.tv_sec += num_of_seconds;
	status = sem_timedwait(sem_addr, &time);
	if (0 != status)
	{
#ifndef NDEBUG
		perror(msg_err->err_wait);
#endif
		return FAIL;
	}

	status = sem_unlink(sem_name);
	if (0 != status)
	{
#ifndef NDEBUG
		perror(msg_err->err_unlink);
#endif
		return FAIL;
	}
	
	return SUCCESS;
}

/*----------------------------------------------------------------------------*/

int SemPostIMP(char *sem_name, sem_err_t *msg_err)
{
	int status = 0;
	sem_t *sem_addr = sem_open(sem_name, O_CREAT, S_IRWXU ,0);
	
	assert(NULL != sem_name);
	assert(NULL != msg_err);
	
	if (SEM_FAILED == sem_addr)
	{
#ifndef NDEBUG
		perror(msg_err->err_open);
#endif	
		return FAIL;
	}
	
	status = sem_post(sem_addr);
	if (0 != status)
	{
#ifndef NDEBUG
		perror(msg_err->err_post);
#endif		
		return FAIL;
	}
	
	return SUCCESS;
}

/*----------------------------------------------------------------------------*/

void InitializedRestartParam(restart_param_t *restart_param, char *prog_name, 
			char *argv[], char *sem_name, sem_err_t *msg_err, 
					pid_t *pid_process)
{
	assert(NULL != restart_param);

	restart_param->prog_name = prog_name;
	restart_param->argv = argv;
	restart_param->sem_name = sem_name;
	restart_param->msg_err = msg_err;
	restart_param->pid_other_process = pid_process;
}

/*----------------------------------------------------------------------------*/

void ResetCounterSig(int sig)
{
	(void)sig;

	__sync_lock_test_and_set (&g_count_signals, 0);
}

/*------------------------------Tasks functions-------------------------------*/

/*	This function it's task for schedular. 
	the function send signal to the pid process and add one to the 
	counter signal. the function return 1 - return the task to the schedular. */
static int SendSignalToParamPid(void *pid)
{
	assert(NULL != pid);
	
	kill(*(pid_t *)pid, SIGUSR1);
	
	__sync_fetch_and_add (&g_count_signals, 1);
 	
	return 1;
}

/*----------------------------------------------------------------------------*/

/*	This function it's task for schedular. 
	The function check if need to restart the process with the 
	'pid_other_process'. the function return 1 - return the task to 
	the schedular. */
static int RestartProcessIfNeeded(void *restart_param)
{
	restart_param_t *param = restart_param;

	assert(NULL != restart_param);
	
	if (MAX_SIG <= g_count_signals)
	{
		*(param->pid_other_process) = CreateProcessIMP(param->prog_name, 
					param->argv, param->sem_name, param->msg_err, 7);
		
		if (-1 == *(param->pid_other_process))
		{
			kill(getpid(), SIGTERM);
		}
	}
	
	return 1;
}


/*----------------------------------------------------------------------------*/


