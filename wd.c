/*--------------------------------------------------------------------------*
 * Author: Oved Mizrahi														*	
 * Project: Watchdog - functions											*
 * Last update: 20.01.20  													*
 *--------------------------------------------------------------------------*/

#define _POSIX_C_SOURCE 200112L

#include <stdio.h>		/* perror */
#include <stdlib.h>		/* getenv, malloc */
#include <assert.h>		/* assert */
#include <unistd.h>		/* fork, getppid */
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

struct wd
{
	char **argv;
	pid_t wd_pid;
	pthread_t thread;
};

/*----------------------Declaration auxiliary functions-----------------------*/

static void *ThreadFunctionIMP(void *wd_param);
static int CreateThreadIMP(wd_t *wd_param);
static wd_t *CreateWdParamIMP(char *argv[]);
static int RunSchedularIMP(wd_t *wd_param, restart_param_t *restart_param);
static int CreateWdProcessIfNeededIMP(wd_t *wd_param);

/* tasks functions */
static int StopSchedIfNeededIMP(void *wd_pid);

/*-----------------------------Global variables-------------------------------*/

static size_t g_stop_flag = CONTINUE;
static scheduler_t *g_ptr_sched = NULL;

/*-------------------------------API functions--------------------------------*/

wd_t *MMI(char *argv[])
{
	int status = 0;
	sem_t *sem_mmi_addr = NULL;
	wd_t *wd_param = NULL;
	sem_err_t msg_err_mmi = {NULL};

	assert(NULL != argv);
	
	wd_param = CreateWdParamIMP(argv);
	if (NULL == wd_param)
	{
		return wd_param;
	}

	sem_mmi_addr = sem_open("/sem_mmi", O_CREAT, S_IRWXU ,0);
	if (NULL == sem_mmi_addr)	 
	{
#ifndef NDEBUG
		perror("sem_mmi_open: "); 
#endif
		free(wd_param);
		wd_param = NULL;
		
		return wd_param;
	}

	status = CreateThreadIMP(wd_param);
	if (SUCCESS != status)
	{
		free(wd_param);
		wd_param = NULL;
		
		return wd_param;
	}
	
	msg_err_mmi.err_wait = "sem_mmi_wait";
	msg_err_mmi.err_unlink = "sem_mmi_unlink";
	SemWaitIMP(sem_mmi_addr, "/sem_mmi", &msg_err_mmi, 5);
	
	return wd_param;
}

/*----------------------------------------------------------------------------*/

int DNR(wd_t *param)
{
	int status = 0;

	__sync_lock_test_and_set (&g_stop_flag, STOP);

	status = pthread_join(param->thread, NULL);
	
	free(param);
	
	return status;
}


/*----------------------------Auxiliary functions-----------------------------*/

/* 	The fucntion create wd_process if needed, and then create and run 
	the schedular.	*/
static void *ThreadFunctionIMP(void *wd_param)
{
	int status = 0;
	struct sigaction sig = {0};
	sem_err_t msg_err_mmi = {NULL};
	sem_err_t msg_err_wd = {NULL};
	sem_err_t msg_err_th = {NULL};
	wd_t *wd_parameter = wd_param;
	restart_param_t restart_param = {0};
	char *env_flag = getenv("wd_is_exist");

	assert(NULL != wd_param);
	
	status = CreateWdProcessIfNeededIMP(wd_param);
	if (SUCCESS != status)
	{
		return NULL;
	}

	sig.sa_handler = ResetCounterSig;
	sigaction(SIGUSR1, &sig, NULL);
	
	msg_err_mmi.err_open = "sem_mmi_open2";
	msg_err_mmi.err_post = "sem_mmi_post";
	status = SemPostIMP("/sem_mmi", &msg_err_mmi);
	if (SUCCESS != status)
	{
		kill(wd_parameter->wd_pid, SIGTERM);
		return NULL;
	}
	
	InitializedRestartParam(&restart_param, "./process_wd.out", 
			wd_parameter->argv, "/sem_th", &msg_err_th, &wd_parameter->wd_pid);
 	status = RunSchedularIMP(wd_param, &restart_param);
	if (SUCCESS != status)
	{
		kill(wd_parameter->wd_pid, SIGTERM);
		return NULL;
	}
	
	return NULL;
}

/*----------------------------------------------------------------------------*/

static int CreateWdProcessIfNeededIMP(wd_t *wd_param)
{
	sem_err_t msg_err_th = {0};
	sem_err_t msg_err_wd = {0};
	int status = 0;
	char *env_flag = getenv("wd_is_exist");
	
	if (NULL == env_flag)
	{	
		msg_err_th.err_open = "sem_th_open";
		msg_err_th.err_wait = "sem_th_wait";
		msg_err_th.err_unlink = "sem_th_unlink";
		wd_param->wd_pid = CreateProcessIMP("./process_wd.out",
						wd_param->argv, "/sem_th", &msg_err_th, 3);
		if (-1 == wd_param->wd_pid)
		{
			return FAIL;
		}
	}
	else
	{
		wd_param->wd_pid = getppid();
		
		msg_err_wd.err_open = "sem_wd_open2";
		msg_err_wd.err_post = "sem_wd_post";
		status = SemPostIMP("/sem_wd", &msg_err_wd);
		if (SUCCESS != status)
		{
			kill(wd_param->wd_pid, SIGTERM);
			return FAIL;
		}
	}
	
	return SUCCESS;
}

/*----------------------------------------------------------------------------*/

/*	the functino create the schedular and add all the tasks.
	and then send to schedularRun() until the function stop and then
	destroy the schedular.	*/
static int RunSchedularIMP(wd_t *wd_param, restart_param_t *restart_param)
{
	scheduler_t *sched = NULL;
	int status = 0;
	
	assert(NULL != wd_param);
	assert(NULL != restart_param);

	sched = SchedularHandler(restart_param); /** need change **/
	if (NULL == sched)
	{
		kill(wd_param->wd_pid, SIGTERM);
		return FAIL;
	}
	SchedulerAdd(sched, StopSchedIfNeededIMP, 1, &wd_param->wd_pid);
	g_ptr_sched = sched;
	
	
	status = SchedulerRun(sched);
	/* I don't care about return value from SchedulerRun() */
	SchedulerDestroy(sched);
	(void)status;
	
	return SUCCESS;
}

/*----------------------------------------------------------------------------*/

/* 	The function create a struct wd_param and initialized argv and thread_pid.
	in success return pointer to the struct , in fail return NULL */
static wd_t *CreateWdParamIMP(char *argv[])
{
	wd_t *wd_param = (wd_t *)malloc(sizeof(wd_t));
	
	assert(NULL != argv);
	
	if (NULL != wd_param)
	{
		wd_param->argv = argv;
	}

	return wd_param;
}

/*----------------------------------------------------------------------------*/

/* The function create thread and return 0 if success and 1 if fail. */
static int CreateThreadIMP(wd_t *wd_param)
{
	int status = pthread_create(&wd_param->thread, NULL, ThreadFunctionIMP, 
			wd_param);
	
	if (0 != status)
	{
#ifndef NDEBUG
		perror("pthread_create: ");
#endif
		return FAIL;
	}
	
	return SUCCESS;
}

/*------------------------------Tasks functions-------------------------------*/

/*	This function it's task for schedular. 
	the function check all the time if the schedular need to stop.
	if need to stop, stop the schedular and wait for the process_wd until the
	process post him, the fucntion continue send signals.
	the function return 1 - return the task to the schedular. */
static int StopSchedIfNeededIMP(void *wd_pid)
{
	assert(NULL != wd_pid);
	
	
	if (CONTINUE != g_stop_flag)
	{
		int status = FAIL;
		sem_t *sem_stop_addr = sem_open("/sem_stop", O_CREAT, S_IRWXU ,0);
		if (NULL == sem_stop_addr)	 
		{
#ifndef NDEBUG
			perror("sem_stop_open: "); 
#endif	
			return 1;
		}
		
		kill(*(pid_t *)wd_pid, SIGUSR2);
		SchedulerStop(g_ptr_sched);
		
		while (SUCCESS != status)
		{
			status = sem_trywait(sem_stop_addr);
			kill(*(pid_t *)wd_pid, SIGUSR1);
		}
		
		status = sem_unlink("/sem_stop");
		if (0 != status)
		{
#ifndef NDEBUG
			perror("sem_stop_unlink");
#endif
			return 1;
		}
	}
	
	return 1;
}

/*----------------------------------------------------------------------------*/

