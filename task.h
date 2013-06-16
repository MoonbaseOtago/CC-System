// (c) Copyright Paul Campbell paul@taniwha.com 2013
#ifndef _TASK_H
#define _TASK_H

__sfr __at (0x93) _XPAGE;

typedef unsigned char u8;

typedef struct task {
	void 	(*callout)(struct task __xdata*);	// offset 0
	unsigned int	time;				// offset 2
	struct task     __xdata*next;			// offset 4
	u8		state;				// offset 6
} task;

void queue_task(task __xdata *, unsigned int);
void cancel_task(task __xdata * );

#define HZ (32000/256)


#define TASK_IDLE 	0
#define TASK_CALLOUT	1
#define TASK_QUEUED	2

extern void wait_us(u8);

#endif
