/****************************************
* Author: ol78							*
* Last Update: 15/01/2020				*
* Data structure: Watchdog.				*
****************************************/
#ifndef __WD_H__
#define __WD_H__

/*----------------------------------------------------------------------------*/

typedef struct wd wd_t;

/*----------------------------------------------------------------------------*/

/**/
wd_t *MMI(char *argv[]);

/*----------------------------------------------------------------------------*/

/**/
int DNR(wd_t *param);

/*----------------------------------------------------------------------------*/

#endif /* __WD_H__ */
