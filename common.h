#include "harddoom2.h"

// #define DEBUG(fmt, ...) printk(KERN_INFO "hd2: " fmt "\n", ##__VA_ARGS__)
#define DEBUG(fmt, ...)

#define ERROR(fmt, ...) printk(KERN_ERR "hd2: " fmt "\n", ##__VA_ARGS__)

#define DRV_NAME "HardDoom_][™"
#define DEVICES_LIMIT 256
