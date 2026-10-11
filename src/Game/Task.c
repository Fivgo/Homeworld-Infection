/*=============================================================================
    Name    : task.c
    Purpose : Functions for implementing a non-preemptive task manager.

    Created 6/18/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "Debug.h"
#include "Memory.h"
#include "Demo.h"
#include "TitanInterfaceC.h"
#include "Task.h"
#include <math.h>      // CAT: time compression
#include "CommandWrap.h"      // CAT: time compression
#include "Globals.h"      // CAT: time compression
#include "MultiplayerGame.h"      // CAT: time compression
#include "Tutor.h"      // CAT: time compression

/*=============================================================================
    Data:
=============================================================================*/
//task structures
taskdata *taskData[TSK_NumberTasks];            //pointers to active tasks
sdword taskMaxTask;                             //highest indexed task enabled
sdword taskModuleInit = FALSE;

CallBacks callbacks;

//speed of task timer, in Hz
real32 taskFrequency;

//global handle of task currently being executed
taskhandle taskCurrentTask = -1;


real32 taskTimeDelta   = 0.0f;
real32 taskTimeElapsed = 0.0f;                  //time elapsed, in seconds, since program started

// CAT (M2-R4 rest-oracle): Cataclysm's time compression (cataclysm.exe 0x0052a350-0x0052a4c0), which replaces HW1's
//      universeTurbo: every tick count taskExecuteAllPending receives is scaled by cat_timeCompressionFactor
//      (0x00bb3268), the table entry at cat_timeCompressionIndex (0x00bb326c; 0x0072bd14: 1/4, 1/2, 1, 2, 4, 6, 8; the
//      default index 0x008aa6f0 is 2 = 1.0). The unscaled ticks also feed cat_taskRealTimeElapsed (0x00bb3270), the
//      wall-clock task time the construction manager's build timers read.
static const real32 cat_timeCompressionFactorTable[7] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 6.0f, 8.0f };
static const ubyte cat_timeCompressionDefaultIndex = 2;
static ubyte  cat_timeCompressionIndex = 2;
real32 cat_timeCompressionFactor = 1.0f;
real32 cat_taskRealTimeDelta = 0.0f;
real32 cat_taskRealTimeElapsed = 0.0f;

udword taskProcessIndex = 0;                    //number of calls being executed on this task
sdword taskNumberCalls;                         //number of times to call the task

/*=============================================================================
    Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Test task function
-----------------------------------------------------------------------------*/
#if TASK_TEST
DEFINE_TASK(taskTestFunction)
{
    taskVar;

    sdword index;
    sdword aba;

    taskProg(state);

    state->index = 300;
    state->aba = 24;

    taskYield(0);

    for (state->index = 0; state->index < 4; state->index++)
    {
        dbgMessagef("%s: Task test function called %d times.", __func__, index);
        aba++;
        taskYield(0);
    }
    taskEnd;
}
#endif //TASK_TEST

/*-----------------------------------------------------------------------------
    Name        : taskStartup
    Description : Initialize the task module.
    Inputs      : frequency - frequency, in Hz of the timer to be used.
    Outputs     : All taskData structures cleared
    Return      : OKAY on success, ERROR if failure
----------------------------------------------------------------------------*/
sdword taskStartup(udword frequency)
{
    sdword index;

#if TASK_VERBOSE_LEVEL >= 1
    dbgMessagef("%s: Task module started using a frequency of %dHz", __func__, frequency);
#endif

    dbgAssertOrIgnore(taskModuleInit == FALSE);

    for (index = 0; index < TSK_NumberTasks; index++)
    {
        taskData[index] = NULL;
    }

    taskMaxTask = 0;                                        //no tasks
    taskFrequency = (real32)frequency;                      //save the frequency
    taskModuleInit = TRUE;                                  //and say module init

    //call the task handler dispatcher once so that it will set return pointers OK
    taskExecuteAllPending(0);

#if TASK_TEST                                               //test this crazy module
    {
        taskhandle handle;
        handle = taskStart(taskTestFunction, 1, TF_OncePerFrame);
        taskRename(handle, "Task this you commie bastards!");
        taskExecuteAllPending(1);
        taskExecuteAllPending(1);
        taskExecuteAllPending(1);
        taskExecuteAllPending(1);
        taskExecuteAllPending(1);
        taskExecuteAllPending(1);
    }
#endif //TASK_TEST

    return(OKAY);
}

/*-----------------------------------------------------------------------------
    Name        : taskShutdown
    Description : Close the task manager by clearing all tasks.
    Inputs      : void
    Outputs     : Clears all task structures to unused and frees stack memory (if any)
    Return      : void
----------------------------------------------------------------------------*/
void taskShutdown(void)
{
    sdword index;

#if TASK_VERBOSE_LEVEL >= 1
    dbgMessage("taskClose: closing task module");
#endif

    if (!taskModuleInit)
    {
        return;
    }
    for (index = 0; index < taskMaxTask; index++)
    {
        if (taskData[index] != NULL)
        {
            if (taskData[index]->context != NULL)
                free(taskData[index]->context);
            //free structure and stack associated with this task
            memFree(taskData[index]);
            taskData[index] = NULL;
        }
    }
    taskModuleInit = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : taskPointerAlloc
    Description : Allocate a task pointer out of global task data list
    Inputs      : void
    Outputs     : sets allocated task pointer to -1
    Return      : index of newly allocated task
    Note        : generates a fatal error if no task found
----------------------------------------------------------------------------*/
sdword taskPointerAlloc(void)
{
    sdword index;

    for (index = 0; index < TSK_NumberTasks; index++)       //look for a free task
    {
        if (taskData[index] == NULL)                        //if free
        {
            taskData[index] = (void *)0xffffffff;
            taskMaxTask = max(taskMaxTask, index + 1);      //update max task if needed
            return(index);
        }
    }

#if TASK_ERROR_CHECKING
    dbgFatalf(DBG_Loc, "All %d task pointers in use.", taskMaxTask);
#endif
    return(ERROR);
}

/*-----------------------------------------------------------------------------
    Name        : taskStartName
    Description : Start a specific task
    Inputs      : function - entry point of task
                  name - a string describing the task for debugging
                  period - period between consecutive calls
                  stacksize - size of task's local stack
                  flags - control execution of task
    Outputs     : taskdata structure allocated and initialized
    Return      : handle to task for later manipulation of task
    Note        : if a taskdata structure or stack RAM cannot be allocated, the
                    function will generate a fatal error.
                  name must be caller-allocated and outlive the task.
                  This is usually invoked with the taskStart macro.
                  The task is executed once immediately and must yield
                  rather than exit at this first call.
----------------------------------------------------------------------------*/
taskhandle taskStartName(taskfunction function, char *name,
                         real32 period, udword flags)
{
    static taskhandle handle = ERROR;
    taskdata *newTask;
    
    taskInitCheck();
    dbgAssertOrIgnore(function != NULL);
    dbgAssertOrIgnore(period > 0.0f);

    newTask = memAlloc(sizeof(taskdata), "taskData", NonVolatile);
    handle = taskPointerAlloc();
    taskData[handle] = newTask;

#if TASK_VERBOSE_LEVEL >= 2
    dbgMessagef("%s: starting task at 0x%x at %d Hz, flags 0x%x using handle %d at 0x%x",
                __func__, function, 1.0f/period, flags, handle, taskData[handle]);
#endif

    //make task in use and running
    newTask->flags = flags | TF_Allocated;
    newTask->function = function;
    newTask->context = NULL;
    newTask->name = name;
    newTask->ticks = 0;         //no residual ticks
    dbgAssertOrIgnore(period * taskFrequency < (real32)SDWORD_Max);
    newTask->ticksPerCall = (udword)(period * taskFrequency);

    function(&newTask->context);

    if (newTask->context == NULL)
    {
        // Exited already?
#if TASK_ERROR_CHECKING
        dbgFatalf(DBG_Loc, "taskStart: stillborn task %s", name);
#endif
        taskStop(handle);
        return ERROR;
    }

    return handle;
}

/*-----------------------------------------------------------------------------
    Name        : taskStop
    Description : Destroy the specified task (stop and deallocate)
    Inputs      : handle - handle of task returned from taskStart()
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void taskStop(taskhandle handle)
{
    taskInitCheck();
    dbgAssertOrIgnore(handle >= 0);
    dbgAssertOrIgnore(handle < taskMaxTask);
    dbgAssertOrIgnore(taskData[handle] != NULL);

#if TASK_VERBOSE_LEVEL >= 2
    dbgMessagef("%s: destroying task %d", __func__, handle);
#endif

    if (taskData[handle]->context != NULL)
    {
        free(taskData[handle]->context);
    }
    memFree(taskData[handle]);
    taskData[handle] = NULL;
    if (handle == taskMaxTask - 1)
    {
        taskMaxTask--;
    }
}

/*-----------------------------------------------------------------------------
    Name        : taskExecuteAllPending
    Description : Execute all pending tasks
    Inputs      : ticks - number of system timer ticks since the last time called
    Outputs     : all pending tasks executed
    Return      : ?
    Note        : Because the stack is corrupted in this function, we can use
        no local variables or passed parameters.
----------------------------------------------------------------------------*/
static sdword tTicks, tLocalTicks;
sdword taskExecuteAllPending(sdword ticks)
{
    sdword realTicks;
#if TASK_VERBOSE_LEVEL >= 2
    if (ticks > 0)
    {
        dbgMessagef("%s: executing tasks for %d ticks", __func__, ticks);
    }
#endif

    /* Don't do a taskInitCheck because this sometimes gets called on
       exiting, after all the tasks are shut down. */
    if (taskModuleInit == FALSE)
    {
        return(OKAY);
    }
    //taskInitCheck();

    // CAT: the ticks are scaled by the time-compression factor and rounded to the nearest integer (the exe adds 1.5 x 2^52
    //      in x87 and reads the low dword: round to nearest even) before the demo record/playback and the task clock
    //      (taskExecuteAllPending 0x0052a840)
    realTicks = ticks;
    ticks = (sdword)nearbyintf((real32)ticks * cat_timeCompressionFactor);

    if (demDemoRecording)
    {
        demNumberTicksSave(ticks);
    }
    else if (demDemoPlaying)
    {
        ticks = demNumberTicksLoad(ticks);
    }

    taskTimeDelta = ticks / taskFrequency;
    taskTimeElapsed += taskTimeDelta;
    tTicks = ticks;             //save the ticks parameter
    // CAT: the real (unscaled) task time (taskExecuteAllPending 0x0052a840)
    cat_taskRealTimeDelta = (real32)realTicks / taskFrequency;
    cat_taskRealTimeElapsed += cat_taskRealTimeDelta;

    // Verify we're not in any task currently.
    dbgAssertOrIgnore(taskCurrentTask == -1);

    for (taskCurrentTask = 0; taskCurrentTask < taskMaxTask; taskCurrentTask++)
    {
        if (taskData[taskCurrentTask] == NULL
            || bitTest(taskData[taskCurrentTask]->flags, TF_Paused))
            continue;

        if (TitanActive)
            titanPumpEngine();

        //calculate number of calls
        if (bitTest(taskData[taskCurrentTask]->flags, TF_OncePerFrame))
        {
/* Don't do this if check, because if you do it is possible that
   hundreds of other tasks will get executed and this task will starve
   when the game AI gets heavy.  Luke, later check if there's a better
   way to do this. */
/*
            if (taskData[taskCurrentTask]->ticks == 0)  //see if time to execute
            {
*/
            taskNumberCalls = 1; //flag to call once
            taskData[taskCurrentTask]->ticks =
                taskData[taskCurrentTask]->ticksPerCall;
/*
            }
*/
            taskData[taskCurrentTask]->ticks--;
        }
        else
        {                       //else it's a real-time task
            //add leftover ticks from last frame
            tLocalTicks = tTicks + taskData[taskCurrentTask]->ticks;
            //save leftover ticks for next frame
            taskData[taskCurrentTask]->ticks =
                tLocalTicks % taskData[taskCurrentTask]->ticksPerCall;
            taskNumberCalls =
                tLocalTicks / taskData[taskCurrentTask]->ticksPerCall;
#if TASK_MAX_TICKS
            taskNumberCalls = min(taskNumberCalls, TSK_MaxTicks);
#endif //TASK_MAX_TICKS
            }
#if TASK_VERBOSE_LEVEL >= 3
        if (taskNumberCalls)
        {
            dbgMessagef(
                "%s: task %d (%s) to run %d times, "
                "%d ticks elapsed / %d per call = %d",
                __func__,
                taskCurrentTask, taskData[taskCurrentTask]->name,
                taskNumberCalls,
                tLocalTicks, taskData[taskCurrentTask]->ticksPerCall,
                tLocalTicks / taskData[taskCurrentTask]->ticksPerCall);
        }
#endif
        taskProcessIndex++;
        for (; taskNumberCalls > 0; taskNumberCalls--)
        {
            taskData[taskCurrentTask]->function(
                &taskData[taskCurrentTask]->context);
            if (taskData[taskCurrentTask]->context == NULL) {
                // Task has exited.
                taskStop(taskCurrentTask);
                break;
            }
        }
    }
    taskCurrentTask = -1;       //not currently executing any tasks
    return(OKAY);
}

/*-----------------------------------------------------------------------------
    Name        : taskPause
    Description : Pauses selected task.
    Inputs      : handle - handle of task
    Outputs     : Sets the paused bit of specified taskData structure
    Return      : void
----------------------------------------------------------------------------*/
void taskPause(taskhandle handle)
{
    taskInitCheck();
    dbgAssertOrIgnore(handle >= 0);
    dbgAssertOrIgnore(handle < taskMaxTask);
    dbgAssertOrIgnore(taskData[handle] != NULL);

    bitSet(taskData[handle]->flags, TF_Paused);
}

/*-----------------------------------------------------------------------------
    Name        : taskResume
    Description : Resumes selected task.
    Inputs      : handle - handle of task
    Outputs     : Clears the paused bit of specified taskData structure
    Return      : void
----------------------------------------------------------------------------*/
void taskResume(taskhandle handle)
{
    taskInitCheck();
    dbgAssertOrIgnore(handle >= 0);
    dbgAssertOrIgnore(handle < taskMaxTask);
    dbgAssertOrIgnore(taskData[handle] != NULL);

    bitClear(taskData[handle]->flags, TF_Paused);
}

/*-----------------------------------------------------------------------------
    Name        : taskSavePauseStatus
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void taskSavePauseStatus(void)
{
    sdword index;

    taskInitCheck();
    for (index = 0; index < taskMaxTask; index++)           //for all tasks
    {
        if (taskData[index] != NULL)                        //if task active
        {
            if (bitTest(taskData[index]->flags, TF_Paused)) //save the pause bit
            {
                bitSet(taskData[index]->flags, TF_PauseSave);
            }
            else
            {
                bitClear(taskData[index]->flags, TF_PauseSave);
            }
            // deliberately do not pause task, just make sure TF_PauseSave gets set correctly
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : taskFreezeAll
    Description : Freeze all tasks.
    Inputs      : void
    Outputs     : Copies paused bit into pause save bit, then sets pause bit
                    of all active tasks.
    Return      : void
----------------------------------------------------------------------------*/
void taskFreezeAll(void)
{
    sdword index;

    taskInitCheck();
    for (index = 0; index < taskMaxTask; index++)           //for all tasks
    {
        if (taskData[index] != NULL)                        //if task active
        {
            if (bitTest(taskData[index]->flags, TF_Paused)) //save the pause bit
            {
                bitSet(taskData[index]->flags, TF_PauseSave);
            }
            else
            {
                bitClear(taskData[index]->flags, TF_PauseSave);
            }
            bitSet(taskData[index]->flags, TF_Paused);      //pause this task
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : taskResumesAll
    Description : Resumes all previously unpaused tasks.
    Inputs      : void
    Outputs     : Copies pause save bit into paused bit.
    Return      : void
    Note        : It is up to the calling party to ensure that the next call to
                    taskExecuteAllPending() does not try to execute all the task
                    ticks which would have happended while the tasks were paused.
----------------------------------------------------------------------------*/
void taskResumeAll(void)
{
    sdword index;

    taskInitCheck();
    for (index = 0; index < taskMaxTask; index++)           //for all tasks
    {
        if (taskData[index] != NULL)                        //if task active
        {
            if (bitTest(taskData[index]->flags, TF_PauseSave)) //save the pause bit
            {
                bitSet(taskData[index]->flags, TF_Paused);
            }
            else
            {
                bitClear(taskData[index]->flags, TF_Paused);
            }
        }
    }
}

/*=============================================================================
    CAT: time compression (0x0052a350 - 0x0052a4c0)
=============================================================================*/

// 0x0052a350
sdword cat_taskTimeCompressionMaxIndex(void)
{
    return 6;
}

// 0x0052a360
ubyte cat_taskTimeCompressionDefaultIndex(void)
{
    return cat_timeCompressionDefaultIndex;
}

// 0x0052a370: the factor from the table, and the tutorial hears about it
void cat_taskTimeCompressionApply(void)
{
    cat_timeCompressionFactor = cat_timeCompressionFactorTable[cat_timeCompressionIndex];
    tutGameMessage("TimeCompress");
}

// 0x0052a3a0: "1/%d %s" below 1, "%d %s" otherwise, as a command message. CAT-TODO(stage 3): the %s argument (a string
// the decompiler does not show; the exe's clCommandMessage also takes the colour 0xffffffff)
void cat_taskTimeCompressionPrint(void)
{
    char buffer[64];

    if (cat_timeCompressionFactor < 1.0f)
    {
        sprintf(buffer, "1/%d %s", (sdword)(1.0f / cat_timeCompressionFactor), "");
    }
    else
    {
        sprintf(buffer, "%d %s", (sdword)cat_timeCompressionFactor, "");
    }
    clCommandMessage(buffer);
}

// 0x0052a420: back to the default speed (universeReset, universeClose, nisStartup, animBinkPlay)
void cat_taskTimeCompressionReset(void)
{
    if (cat_timeCompressionIndex != cat_timeCompressionDefaultIndex)
    {
        cat_timeCompressionIndex = cat_timeCompressionDefaultIndex;
        cat_taskTimeCompressionApply();
    }
}

// 0x0052a450
void cat_taskTimeCompressionResetAndPrint(void)
{
    if (cat_timeCompressionIndex != cat_timeCompressionDefaultIndex)
    {
        cat_timeCompressionIndex = cat_timeCompressionDefaultIndex;
        cat_taskTimeCompressionApply();
        cat_taskTimeCompressionPrint();
    }
}

// 0x0052a480 / 0x0052a4c0: one step up or down, outside a multiplayer game (the Accelerate_Time / Decelerate_Time keys)
void cat_taskTimeCompressionIncrease(void)
{
    if (((!multiPlayerGame) || (singlePlayerGame)) && (cat_timeCompressionIndex < 6))
    {
        cat_timeCompressionIndex++;
        cat_taskTimeCompressionApply();
        cat_taskTimeCompressionPrint();
    }
}

void cat_taskTimeCompressionDecrease(void)
{
    if (((!multiPlayerGame) || (singlePlayerGame)) && (cat_timeCompressionIndex != 0))
    {
        cat_timeCompressionIndex--;
        cat_taskTimeCompressionApply();
        cat_taskTimeCompressionPrint();
    }
}

/*-----------------------------------------------------------------------------
    Name        : taskPeriodSet
    Description : Set frequency for task
    Inputs      : handle - handle of task to adjust
                  period - period of task
    Outputs     :
    Return      : old period
----------------------------------------------------------------------------*/
real32 taskPeriodSet(taskhandle handle, real32 period)
{
    real32 old;

    taskInitCheck();
    dbgAssertOrIgnore(handle >= 0);
    dbgAssertOrIgnore(handle < taskMaxTask);
    dbgAssertOrIgnore(taskData[handle] != NULL);
    old = (real32)taskData[handle]->ticksPerCall / taskFrequency;
    taskData[handle]->ticksPerCall = (udword)(period * taskFrequency);
    return(old);
}

////////////////
///  CALLBACK TASK HANDLING
////////////////

taskhandle babyTaskHandle = -1;

DECLARE_TASK(taskCallBackProcess);

void taskCallBackInit()
{
    listInit(&callbacks.babies);        //initialize linked list of babies
    babyTaskHandle = taskStart(taskCallBackProcess, BABY_CallBackPeriod, 0);
}

void taskCallBackShutDown()
{
    if (babyTaskHandle != -1)
    {
        taskStop(babyTaskHandle);
    }

    listDeleteAll(&callbacks.babies);
}

BabyCallBack *taskCallBackRegister(babyFuncCB callback, udword num,
                                   void *data, real32 callintime)
{
    BabyCallBack *baby;
    //(maybe do check of time to see if it is volatile?)
    baby = memAlloc(sizeof(BabyCallBack), "BabyCallBack", Volatile);
    baby->babyCallBackFunc = callback;
    baby->numparm = num;
    baby->dataparm = data;
    baby->timeToExecute = callintime;
    baby->timeStarted = taskTimeElapsed;
    listAddNode(&callbacks.babies, &baby->babylink,baby);
    return(baby);
}

void taskCallBackRemove(BabyCallBack *babytogobyebye)
{
    BabyCallBack *baby;
    Node *babynode;

    babynode = callbacks.babies.head;
    while(babynode != NULL)
    {
        baby = (BabyCallBack *) listGetStructOfNode(babynode);
        if(baby == babytogobyebye)
        {
            //returned TRUE, so delete
            listDeleteNode(babynode);
            return;
        }
        babynode = babynode->next;
    }
}

DEFINE_TASK(taskCallBackProcess)
{
    static BabyCallBack *baby;
    static Node *babynode,*tempnode;

    taskBegin;

    taskYield(0);

    while(1)
    {
        babynode = callbacks.babies.head;
        while(babynode != NULL)
        {
            baby = (BabyCallBack *) listGetStructOfNode(babynode);
            if(taskTimeElapsed - baby->timeStarted >= baby->timeToExecute)
            {
                //time elapsed so execute callback
                if((baby->babyCallBackFunc)(baby->numparm, baby->dataparm, baby))
                {
                    //returned TRUE, so delete
                    tempnode = babynode->next;
                    listDeleteNode(babynode);
                    babynode = tempnode;
                    continue;
                }
                //reset task time
                baby->timeStarted = taskTimeElapsed;
            }
            babynode = babynode->next;
        }
        //break;
        taskYield(0);
    }
    taskEnd;
}
