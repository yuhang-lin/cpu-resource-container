//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

// Project 1: Yuhang Lin, ylin34; Yuanchao Xu, yxu47; 

#include "processor_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/types.h>

typedef struct node {
	struct task_struct *task;
	struct node *next;
} node_t;

typedef struct head {
	struct node *node;
	struct head *next;
	struct node *lastRunNode;
	__u64 cid;
	int numTask;
} head_t;

static DEFINE_MUTEX(lock);
EXPORT_SYMBOL(lock);
static head_t *containerHead = NULL;
int numContainer = 0;

/**
 * Search for the head for the current container using the given container id.
 */
head_t * searchHead(head_t *head, __u64 cid) {
	head_t *curr;
	
	curr = head;
	while (curr != NULL) {
		if (curr->cid == cid) {
			return curr;
		}
		curr = curr->next;
	}
	return NULL;
}

/**
 * Search for the task using the current thread id.
 */
node_t * searchTask(node_t *head) {
	node_t *node;

	node = head;
	while (node != NULL) {
		if (node->task->pid == current->pid) {
			return node;
		}
		node = node->next;
	}
	return NULL;
}

/**
 * Search for the head for the current container using the current thread id.
 */
head_t * searchContainer(head_t *head) {
	head_t *nodeHead;
	node_t *result;
	
	nodeHead = head;
	while (nodeHead != NULL) {
		result = searchTask(nodeHead->node);
		if (result != NULL) {
			return nodeHead;
		}
		nodeHead = nodeHead->next;
	}
	return NULL;
}



/**
 * Insert a node to the container linked list.
 */
head_t * insertHead(head_t **head, __u64 cid) {
	head_t *node;
	head_t *prev;
	head_t *new_node;

	node = *head;
	prev = node;
	new_node = kmalloc(sizeof(head_t), GFP_KERNEL);
	new_node->node = NULL;
	new_node->next = NULL;
	new_node->lastRunNode = NULL;
	new_node->cid = cid;
	new_node->numTask = 0;
	if (*head == NULL) {
		*head = new_node;
		return new_node;
	}
	while (node != NULL) {
		if (node->cid == cid) {
			kfree(new_node);
			return node;
		}
		prev = node;
		node = node->next;
	}
	prev->next = new_node;
	return new_node;
}

/**
 * Insert a node to the task linked list. 
 */
void insertTask(node_t **head) {
	node_t *node;

	node = *head;
	if (*head == NULL) {
		node_t *new_node = kmalloc(sizeof(node_t), GFP_KERNEL);
		new_node->task = current;
		new_node->next = NULL;
		*head = new_node;
		return;
	}
	while (node->next != NULL) {
		node = node->next;
	}
	node->next = kmalloc(sizeof(node_t), GFP_KERNEL);
	node->next->task = current;
	node->next->next = NULL;
}

/**
 * Print all the nodes from the container linked list. 
 */
void printContainer(head_t *head) {
	head_t *node;

	node = head;
	if (head == NULL) {
		return;
	}
	while (node != NULL) {
		printk(KERN_ERR "List: container id is: %llu", node->cid);
		node = node->next;
	}
}

/**
 * Print all the nodes from the linked list. 
 */
void print_all(node_t *head) {
	node_t *node;

	node = head;
	if (head == NULL) {
		return;
	}
	while (node != NULL) {
		printk(KERN_ERR "List: thread id is: %d", node->task->pid);
		node = node->next;
	}
}

/**
 * Free the current node from the container linked list. 
 */
void freeHead(head_t **head, __u64 cid) {
	head_t *node;
	head_t *prev;

	node = *head;
	if (*head == NULL) {
		return;
	}
	if (node->cid == cid) {
		if (node->node == NULL) {
			*head = node->next;
			kfree(node);
		}
		return;
	}
	prev = node;
	node = node->next;
	while (node != NULL) {
		if (node->cid == cid) {
			if (node->node == NULL) {
				prev->next = node->next;
				kfree(node);
			}
			return;
		}
		prev = node;
		node = node->next;
	}
}

/**
 * Free the current node from the task linked list. 
 */
void free_node(node_t **head) {
	node_t *node;
	node_t *prev;
	int target;

	node = *head;
	if (*head == NULL) {
		return;
	}
	target = current->pid;
	if (node->task->pid == target) {
		*head = node->next;
		kfree(node);
		return;
	}
	prev = node;
	node = node->next;
	while (node != NULL) {
		if (node->task->pid == target) {
			prev->next = node->next;
			kfree(node);
			return;
		}
		prev = node;
		node = node->next;
	}
}

/**
 * Free all the nodes in the task linked list. 
 */
void freeAll(node_t **head) {
	node_t *node;

	node = *head;
	if (*head == NULL) {
		return;
	}
	while (node != NULL) {
		node_t *temp = node->next;
		kfree(node);
		node = temp;
	}
	*head = NULL;
}

/**
 * Find the previous node of a given node. 
 */
node_t * findPrevNode(node_t *head, int target) {
	node_t *node;
	node_t *prev;

	if (head == NULL) {
		return NULL;
	}
	prev = NULL;
	node = head;
	while (node != NULL) {
		if (node->task->pid == target) {
			return prev;
		}
		prev = node;
		node = node->next;
	}
	return NULL;
}

/**
 * Delete the task in the container.
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), 
 */
int processor_container_delete(struct processor_container_cmd __user *user_cmd)
{
	struct processor_container_cmd kernel_cmd;
	struct task_struct *task = current;
	head_t *headNode;
	node_t *sleepNode;
	head_t *wakeContainer;

	mutex_lock(&lock);
	printk(KERN_ERR "Delete: current thread id: %d", task->pid);
	if (copy_from_user(&kernel_cmd, user_cmd, sizeof(kernel_cmd)) > 0) {
		printk(KERN_ERR "Delete: failed to copy from user_cmd");
	} else {
		printk(KERN_ERR "Delete Copied container id: %llu", kernel_cmd.cid);
	}
	sleepNode = NULL;
	headNode = searchHead(containerHead, kernel_cmd.cid);
	if (headNode->lastRunNode != NULL && headNode->lastRunNode->task->pid == current->pid) {
		headNode->lastRunNode = findPrevNode(headNode->node, current->pid);
	}
	free_node(&(headNode->node));
	headNode->numTask = headNode->numTask - 1;
	if (headNode->next == NULL) {
		wakeContainer = containerHead;
	}
	else {
		wakeContainer = headNode->next;
	}
	if (headNode->numTask == 0) {
		// free this head
		freeHead(&containerHead, kernel_cmd.cid);
		numContainer--;
	}
	if (wakeContainer->numTask > 0) {
		if (wakeContainer->lastRunNode == NULL || wakeContainer->lastRunNode->next == NULL) {
			sleepNode = wakeContainer->node; // just pick the first one in the list to run
		}
		else {
			sleepNode = wakeContainer->lastRunNode->next; // pick the next one in the list to run
		}
		if (sleepNode != NULL && sleepNode->task->pid == current->pid) {
			sleepNode = sleepNode->next;
			//printk(KERN_ERR "Switch same node: %d", current->pid);
		}
		if (sleepNode == NULL && wakeContainer->node->task->pid != current->pid) {
			sleepNode = wakeContainer->node;
			//printk(KERN_ERR "Head node pid: %d", headNode->node->task->pid);
		}
	}
	wakeContainer->lastRunNode = sleepNode;
	if (sleepNode != NULL) {
		wake_up_process(sleepNode->task);
	}
	else {
		printk(KERN_ERR "Delete: sleep node is NULL");
	}
	mutex_unlock(&lock);
    return 0;
}

/**
 * Create a task in the corresponding container.
 * external functions needed:
 * copy_from_user(), mutex_lock(), mutex_unlock(), set_current_state(), schedule()
 * 
 * external variables needed:
 * struct task_struct* current  
 */
int processor_container_create(struct processor_container_cmd __user *user_cmd)
{
	struct task_struct *task = current;
	struct processor_container_cmd kernel_cmd;
	head_t *headNode;

	mutex_lock(&lock);
	printk(KERN_ERR "Create: current thread id: %d", task->pid);
	if (copy_from_user(&kernel_cmd, user_cmd, sizeof(kernel_cmd)) > 0) {
		printk(KERN_ERR "Create: failed to copy from user_cmd");
	} else {
		printk(KERN_ERR "Create copied container id: %llu", kernel_cmd.cid);
	}
	headNode = insertHead(&containerHead, kernel_cmd.cid);
	insertTask(&(headNode->node));
	headNode->numTask = headNode->numTask + 1;
	if (headNode->numTask == 1) {
		numContainer++;
	}
	printk(KERN_ERR "Insert: current thread id: %d, current num tasks: %d", task->pid, headNode->numTask);
	//print_all(headNode->node);
	//printContainer(containerHead);
	if (numContainer > 1 || headNode->numTask > 1) {
		set_current_state(TASK_INTERRUPTIBLE);
	    mutex_unlock(&lock);
		schedule();
	}
    else
    {
        mutex_unlock(&lock);
    }
	return 0;
}

/**
 * switch to the next task within the same container
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), set_current_state(), schedule()
 */
int processor_container_switch(struct processor_container_cmd __user *user_cmd)
{
	head_t *headNode;
	node_t *sleepNode;

	sleepNode = NULL;
	printk(KERN_ERR "Switch: current thread id: %d", current->pid);
	mutex_lock(&lock);
	// find the container head node for the current pid
	headNode = searchContainer(containerHead);
	mutex_unlock(&lock);
	if (headNode == NULL) {
		printk(KERN_ERR "No head node found");
		//set_current_state(TASK_INTERRUPTIBLE);
        //schedule();
        return 0;
	}
	mutex_lock(&lock);
	if (headNode->next == NULL) {
		headNode = containerHead;
	}
	else {
		headNode = headNode->next;
	}

	if (headNode->lastRunNode == NULL || headNode->lastRunNode->next == NULL) {
		sleepNode = headNode->node; // just pick the first one in the list to run
	} else {
		sleepNode = headNode->lastRunNode->next; // pick the next one in the list to run
	}
	if (sleepNode != NULL && sleepNode->task->pid == current->pid) {
		sleepNode = sleepNode->next;
		printk(KERN_ERR "Switch same node: %d", current->pid);
	}
	if (sleepNode == NULL && headNode->node->task->pid != current->pid) {
		sleepNode = headNode->node;
		printk(KERN_ERR "Head node pid: %d", headNode->node->task->pid);
	}
    headNode->lastRunNode = sleepNode;
	
	if (sleepNode != NULL && sleepNode->task->pid != current->pid) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (wake_up_process(sleepNode->task) == 1) {
			printk(KERN_ERR "%d is woken up: ", sleepNode->task->pid);
		} else {
			printk(KERN_ERR "%d is already running", sleepNode->task->pid);
		}
		printk(KERN_ERR "current thread: %d, switch to: %d", current->pid, sleepNode->task->pid);
	} else {
		set_current_state(TASK_RUNNING);
		printk(KERN_ERR "Switch: Sleep node is NULL");
	}
	mutex_unlock(&lock);
	schedule();
    return 0;
}

/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int processor_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case PCONTAINER_IOCTL_CSWITCH:
        return processor_container_switch((void __user *)arg);
    case PCONTAINER_IOCTL_CREATE:
        return processor_container_create((void __user *)arg);
    case PCONTAINER_IOCTL_DELETE:
        return processor_container_delete((void __user *)arg);
    default:
        return -ENOTTY;
    }
}
