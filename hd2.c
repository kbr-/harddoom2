#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");

int hd2_init(void)
{
	printk(KERN_INFO "Hello from hd2\n");
	return 0;
}

void hd2_cleanup(void)
{
	printk(KERN_INFO "Goodbye from hd2\n");
}

module_init(hd2_init);
module_exit(hd2_cleanup);
