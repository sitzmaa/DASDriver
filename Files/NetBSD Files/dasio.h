/* dasio.h -- ioctl definitions for CS513 */
/* Sampled data is returned as delta time/data pairs in a 32 bit
* integer. Delta time is in the most significant 16 bits, data in
* the least significant bits. Delta time is in 10E-6 seconds.
*/
#define DAS_START_SAMPLING _IO ('D', 0)
#define DAS_STOP_SAMPLING _IO ('D', 1)
/* Rate ... int is time in units of 10E-5 seconds. So 100000 is 1 second. */
#define DAS_SET_RATE _IOW('D', 2, int)
#define DAS_GET_RATE _IOR('D', 3, int)
/* Channel is a number from 0 to 7. */
#define DAS_SET_CHANNEL _IOW('D', 4, int)
#define DAS_GET_CHANNEL _IOR('D', 5, int)
/* For debugging .. only */
/* Int is regno, register val returned in the int. */
#define DAS_GET_REGISTER _IOWR('D', 30, int)
/* Int in set register is (regno << 16) | data */
#define DAS_SET_REGISTER _IOW('D', 30, int)

/*Default rate and channel (will edit these after more work)*/
#define DAS_DEFAULT_RATE 20
#define DAS_DEFAULT_CHANNEL 0

/*Define PCI address Values -- Need to be set for actual use*/
#define CTR2 0X07
#define CTR1 0x02
#define CLOCK 0x06

//vendor and product number
#define DASVENDOR 0x1307
#define DASPRODUCT 0x0029

//base addresses
#define BAR1 0x14
#define BAR2 0x18

//Counter Definitions -- set the mode of the counter on initialization
#define COUNTER_CONTROL_WORD 0xb0 /* Represents a control word 10110000 */

//Sampling values
#define MAX_SAMPLE_SET 1000

// EOC
#define EOC 0x80

//Clock speed
#define CLOCK_SPEED 4125