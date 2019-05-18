
#define DEBUG(fmt, ...) printk(KERN_INFO "hd2: " fmt "\n", ##__VA_ARGS__)
#define ERROR(fmt, ...) printk(KERN_ERR "hd2: " fmt "\n", ##__VA_ARGS__)

#define DRV_NANE "HardDoom_][™"
#define MAX_BUFFER_SIZE (4 * 1024 * 1024)
