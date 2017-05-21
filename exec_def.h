/*
This file is part of NppEventExec
Copyright (C) 2016-2017 Mihail Ivanchev

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __EXEC_DEF_H__
#define __EXEC_DEF_H__

/*
This header file outlines the communication interface between the execution
queue and the queue dialog.
*/

typedef enum
{
    STATE_QUEUED,
    STATE_WAITING,
    STATE_EXECUTING
} ExecState;

typedef enum
{
    QUEUE_ADD_BACKGROUND,
    QUEUE_ADD_FOREGROUND,
    QUEUE_REMOVE_BACKGROUND,
    QUEUE_REMOVE_FOREGROUND,
    QUEUE_STATUS_UPDATE
} QueueEvent;

void updateQueue(void);
void stopQueue(void);
unsigned int getQueueSize(unsigned int *foregroundCnt);
void abortExecs(int *positions,
                unsigned int *queueSize,
                unsigned int *foregroundCnt);
const wchar_t* getExecRule(unsigned int pos);
ExecState getExecState(unsigned int pos);
const wchar_t* getExecPath(unsigned int pos);
int isExecForeground(unsigned int pos);

int isQueueDlgVisible(void);
void processQueueEvent(QueueEvent event);

#endif /* __EXEC_DEF_H__ */
