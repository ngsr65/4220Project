#ifndef MODULE
#define MODULE
#endif
#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>	// for kthreads
#include <linux/sched.h>	// for task_struct
#include <linux/time.h>		// for using jiffies 
#include <linux/timer.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#define MSG_SIZE 40
#define CDEV_NAME "buffer"

MODULE_LICENSE("GPL");
int mydev_id;
unsigned long *ptr;
static int major;
static char msg[MSG_SIZE];

enum Event{ NONE, BTN4, BTN5, SW1, SW2, RL, YL, GL};			//enum to specify event 
enum Event now;

//function that will write to the character device so the user space program can read it 
static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset){
 
		ssize_t dummy = copy_to_user(buffer, msg, length);
		if(dummy < 0){
			printk("Error Copying to User Space in device_read\n");
		}

	//	memset(buffer, '\0', MSG_SIZE);
	return length;

}
//function that will read in from the user 
static ssize_t device_write(struct file *filp, const char __user *buff, size_t len, loff_t *off){
	
	ssize_t dummy;
	
	if(len > MSG_SIZE)						//if the length is too long we have an error 
		return -EINVAL;

	dummy = copy_from_user(msg, buff, len);				//copy message over from the user 
	if(len == MSG_SIZE)						//if the length is the same we need to add a null terminator
		msg[len -1] = '\0';
	else								//if its not the same add it to the end of the message 
		msg[len] = '\0';

	printk("%s\n", msg);						//print message to the user 
	return len;
}
//file structure 
static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
};
//ISR function to check if any events happened 
static irqreturn_t button_isr(int irq, void *dev_id){

	disable_irq_nosync(79);						//disable before we begin 
	unsigned long event;						//variable to hold our derefrenced GPEDS0 in 
	event = *(ptr + 16) & 0x0018301C;
	now = NONE;

	switch(event){										//enter switch statement to find out what kind of event occured
		case 0x4:										//RED LED EVENT OCCURED
			now = RL;
			sprintf(msg, "5\n");
			printk("Red Light Event Detected\n");
			break;
		case 0x8:										//YELLOW LED EVENT OCCURED
			now = YL;
			sprintf(msg, "6\n");
			printk("Yellow Light Event Detected\n");
			break;
		case 0x10:										//GREEN LED EVENT DETECTED
			now = GL;
			sprintf(msg, "7\n");
			printk("Green Light Event Detected\n");
			break;
		case 0x1000:									//SWITCH 1 EVENT OCCURED
			now = SW1;
			sprintf(msg, "3\n");
			printk("Switch 1 Event Detected\n");
			break;
		case 0x2000:									//SWITCH 2 EVENT OCCURED
			now = SW2;
			sprintf(msg, "4\n");
			printk("Switch 2 Event Detected\n");
			break;
		case 0x80000:									//BUTTON 4 EVENT OCCURED
			now = BTN4;
			sprintf(msg, "1\n");
			printk("Button 4 Event Detected\n");
			break;
		case 0x100000:									//BUTTON 5 EVENT OCCURED 
			now = BTN5;
			sprintf(msg, "2\n");
			printk("Button 5 Event Detected\n");
			break;
		default:										//default case, no event occured 
			now = NONE;
			break;
	}

	*(ptr + 16) = *(ptr + 16) | 0x0018300C;				//clear register after a detection 

	printk("Interrupt Handled\n");
	enable_irq(79);										//enable again after we are done 
	
	return IRQ_HANDLED;
}
//called when the kernel module is installed 
int init_module(void){
	
	int dummy = 0;
	ptr = (unsigned long *)ioremap(0x3f200000, 4096);	//create memory map so we can initialize our pins 
	
	*(ptr + 1) = *(ptr + 1) | 0x00000000;				//set switches and buttons as inputs 
	*(ptr + 2) = *(ptr + 2) | 0b000;
	*ptr = *ptr | 0x09240;								//set LED's as outputs 
		
	major = register_chrdev(0, CDEV_NAME, &fops);		//register character device and obtain the major 
	if(major < 0){
		printk("Registering the character device failed with %d\n", major);
		return major;
	}
	printk("Major Number is: %d\n", major);				//print major number to the user 

	*(ptr + 37) = *(ptr + 37) | 0x155;					//configure pull up down control 
	udelay(100);										//delay for 150 clock cycles 

	*(ptr + 38) = *(ptr + 38) | 0x001f0000;				
	udelay(100); 										//delay again
	
	*(ptr + 37) = *(ptr + 37) & (~0x155);				//set back to zero 
	*(ptr + 38) = *(ptr + 38) & (~0x001f0000);

	*(ptr + 31) = *(ptr + 31) | 0x0018301C;				//set rising edge detection for buttons, switches and LEDs 
	*(ptr + 34) = *(ptr + 34) | 0x0000301C;				//set falling edge detection for the switches and LED's


	dummy = request_irq(79, button_isr, IRQF_SHARED, "Event_handler", &mydev_id);

	return 0;
}

//called when the kernel module is removed 
void cleanup_nodule(void){

	*(ptr + 31) = *(ptr + 31) & (~0x0018301c);			//clear rising edge detection for the buttons 
	*(ptr + 34) = *(ptr + 34) & (~0x0000301c);			//clear falling edge detection for switches 
	*(ptr + 16) = *(ptr + 16) | 0x0018301C;				//clear event detect register 

	unregister_chrdev(major, CDEV_NAME);				//unregister the charcter device 
	printk("Kernel Module Uninstalled\n");				//print message to user 
}
