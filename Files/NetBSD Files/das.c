
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: das.c,v 1.0.0.0 2024/01/27 15:50:13 jenson Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/callout.h>
#include <sys/envsys.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <sys/bus.h>
#include <sys/intr.h>
#include <sys/tty.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/conf.h>

// for current condvar implementation
#include <sys/condvar.h>
#include <sys/mutex.h>

//das header file
#include <dev/pci/dasio.h>

//pci
#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>


#include <uvm/uvm_extern.h>

#include "ioconf.h"


static dev_type_open(das_open);
static dev_type_close(das_close);
static dev_type_ioctl(das_ioctl);
static dev_type_write(das_write);
static dev_type_read(das_read);

struct das_softc {
  struct device sc_dev;
  pci_intr_handle_t *	sc_ih;
  pci_intr_handle_t * sc_isave;
  bus_space_tag_t sc_iot;
  bus_space_handle_t sc_ioh;
  
  uint8_t ctrl_word;
  int  sc_flags;		/* misc. flags. */
  callout_t sc_ch;    /* callout pseudo-interrupt */
  int sc_open;
  int sc_rate;
  int sc_channel;
  int sc_samp;

  // data buffers
  uint32_t* sc_buf;
  uint16_t sc_time_offset;
  int sc_intr_ptr;
  int sc_reader_ptr;
  
  uint8_t sc_ad_high;
  uint8_t sc_ad_low;
  uint16_t sc_sample;
  // condvar
  kcondvar_t sc_cv;
  kmutex_t sc_mtx;
};

//dispatch table
const struct cdevsw das_cdevsw = {
	.d_open = das_open,
	.d_close = das_close,
	.d_read = das_read,
	.d_write = das_write,
	.d_ioctl = das_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};


static int das_match(device_t, cfdata_t, void *);
static int das_open(dev_t, int, int, struct lwp *);
static void das_attach(device_t, device_t, void *);
static int das_close(dev_t, int, int, struct lwp *);
static int das_read(dev_t, struct uio *, int);
static int das_write(dev_t, struct uio *, int);
static int das_ioctl(dev_t, u_long, void*, int, struct lwp *);
static int das_intr(void *p);


CFATTACH_DECL_NEW(
    das,
    sizeof(struct das_softc),
    das_match,
    das_attach,
    NULL,
    NULL
);

static int das_match(device_t parent, cfdata_t cf, void *aux){
  struct pci_attach_args *pa = aux;
  
  if (PCI_VENDOR(pa->pa_id) != DASVENDOR)
    return 0;
  if (PCI_PRODUCT(pa->pa_id) == DASPRODUCT){
    return 1;
  }
  // else
  return 0;
    
 }

//Need to set up bus stuf
static void das_attach(device_t parent, device_t self, void *aux)
{
  
  //setting up device
  //printf("%s\n",info);
  struct das_softc *sc = device_private(self);
  struct pci_attach_args *pa = aux;
  //bus_space_tag_t iot = pa->pa_iot;
  //bus_space_handle_t ioh;
  pci_chipset_tag_t pc = pa->pa_pc;
  pci_intr_handle_t ih;
  uint32_t cmd;

  const char *intrstr;
  char intrbuf[PCI_INTRSTR_LEN];
  sc->sc_intr_ptr =0;
  sc->sc_reader_ptr = 0;
  sc->sc_ad_high = BAR2;
  sc->sc_ad_low = BAR2 + 0x1;
  sc->sc_sample = 0;
  sc->sc_samp = 0;
  /* Map I/O registers <-- confirm if needed*/
  // This is pulled from oboe.c, might need ajusting for register num
  
  //need to map BADR1 and set values
  if (pci_mapreg_map(pa,BAR1, PCI_MAPREG_TYPE_IO, 0,
		     &sc->sc_iot, &sc->sc_ioh, NULL, NULL)) {
    printf("Could not map I/O space\n");
    return;
  }
  //read in 32 bits from BADDR1 +0x4c, set bits, then write back
  cmd = bus_space_read_4(sc->sc_iot,sc->sc_ioh,0x4c);
  //printf("CMD initial:%d\n",cmd);
  cmd = (cmd & ~0xff)^0x41;
  //printf("CMD after mod :%d\n",cmd);
  bus_space_write_4(sc->sc_iot,sc->sc_ioh,0x4c,cmd);
  //read in 32 bits from baddr1+0x50, set bits, then write back
  cmd = bus_space_read_4(sc->sc_iot,sc->sc_ioh,0x50);
  cmd = (cmd & ~0x7)^0x6;
  bus_space_write_4(sc->sc_iot,sc->sc_ioh,0x50,cmd);
  
  //unmap badr1
  //probably won't worry about unmapping it.
  //bus_space_unmap(sc->sc_iot,sc->sc_ioh,32); //need to ask about size
  //map BADR2
  
  if (pci_mapreg_map(pa, BAR2, PCI_MAPREG_TYPE_IO,0,
  		     &sc->sc_iot,&sc->sc_ioh,NULL,NULL)){
    printf("Could not map I/O space\n");
    return;
  }
  // //disable interupts before open.
  // uint8_t word = bus_space_read_1(sc->sc_iot,sc->sc_ioh,CTR1);
  // word = word & 7;
  // printf("word= %d\n",word);
  // bus_space_write_1(sc->sc_iot,sc->sc_ioh,CTR1,word);
  
  //printf("Debug1\n");
   // example pci_intr_map given by if_le_pci.c
   
   if (pci_intr_map(pa, &ih)) {
     printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
     return;
   }
   //printf("debug2\n");
   cv_init(&sc->sc_cv, "condvar");
   //printf("cv_init success\n");
   mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_NONE);
   
   // establish inturrupts based on if_le_pci.c
   intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
   //printf("Inbetween intrstr and establish\n");
   sc->sc_ih = pci_intr_establish(pc, ih, IPL_TTY, das_intr, sc);
   //printf("Debug3\n");
   if (sc->sc_ih == NULL) {
     printf("%s: couldn't establish interrupt",
     sc->sc_dev.dv_xname);
     if (intrstr != NULL)
     printf(" at %s", intrstr);
     printf("\n");
     return;
     }
  printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);
   
   // IMPORTANT**** das_intr is a function that will be pointed to in th event of an interrupt,
   //currently the sc is being passed as the arg. must revist before functional
  printf("\n");
}
//
static int das_open(dev_t dev, int oflags, int devtype, struct lwp *l)
{
  printf("Attempting to open DAS\n");
  struct das_softc * sc;
  sc = device_lookup_private(&das_cd, minor(dev));
  int error = 0; uint8_t cmd_high,cmd_low, ctrcmd;
  uint16_t cmd;
  sc->sc_rate = DAS_DEFAULT_RATE;
  sc->sc_channel = DAS_DEFAULT_CHANNEL;
  if (sc == NULL){
    printf("Softc is null\n");
    error = ENXIO;
    return error;
  }
  if(sc->sc_open > 0){
    printf("Already one open\n");
    error = EBUSY;
    return error;
  }

  // Set up Counter 2
  cmd = DAS_DEFAULT_RATE; // Provide Default rate
  ctrcmd = COUNTER_CONTROL_WORD; // control register command
  //control register is BADDR2+7, Counter2 is BADDR2+6
  bus_space_write_1(sc->sc_iot,sc->sc_ioh, CTR2,ctrcmd); // write the control command
  /* Write the counter information
  * Information is an 16 bit value split into two 8 bit messages,
  * first low bits then high*/
  bus_space_write_1(sc->sc_iot,sc->sc_ioh,sc->sc_ad_low, cmd);
  cmd_high = (uint8_t)(cmd>>8);
  cmd_low = (uint8_t)(cmd&225);
  sc->sc_buf = malloc(sizeof(int)*MAX_SAMPLE_SET,M_DEVBUF,M_ZERO);
  for (int i = 0; i < MAX_SAMPLE_SET; i++) {
    sc->sc_buf[i] = 0;
  }
  bus_space_write_1(sc->sc_iot,sc->sc_ioh, CLOCK,cmd_low);
  bus_space_write_1(sc->sc_iot,sc->sc_ioh, CLOCK,cmd_high);
   sc->sc_open +=1;
  return error;
}
static int das_close(dev_t dev, int fflag, int devtype, struct lwp *p)
{
  struct das_softc *sc;
  sc = device_lookup_private(&das_cd, minor(dev));
  if (sc == NULL)
    {
      return ENXIO;
    }
  // close stuff here
  sc->sc_open = 0;
  free(sc->sc_buf,M_DEVBUF);
  mutex_destroy(&sc->sc_mtx);
  cv_destroy(&sc->sc_cv);
  return 0;
}

static int
das_read(dev_t dev, struct uio *uio, int ioflag){
    /*read() should return an integral number of "samples". 
    (each read should return a multiple of 4 bytes). 
    Each sammple is returned as a 4 byte integer value. 
    the most signifiant two bytes is a 16 bit integer 
    representating the time offset from the interrupt for whent the sample was taken. 
    The value should be in units of 10^-6 seconds. (It should be a small number.) 
    the least significant two bytes is athe 12 bit data value. if o data is available and the
    driver is sampling, 
    the user procss  should block until there is at least one sample available. 
    It should return EOF (zero bytes read) when there is no more data in your driver's 
    buffer and the driver is not sampleing. it should return EINVAL if the request is
    less than 4 bytes.*/
  struct das_softc *sc;
  int error = 0;
  if (uio->uio_resid < 4)
    return EINVAL;
  
  sc = device_lookup_private(&das_cd, minor(dev));
  if(sc == NULL)
    return ENXIO;

  while (uio->uio_resid > 0) {


    if (sc->sc_reader_ptr == sc->sc_intr_ptr) {

      if (sc->sc_samp == 0) {
        return error;
      }
        cv_wait(&sc->sc_cv,&sc->sc_mtx);
    }
    error = uiomove((char*) (sc->sc_buf+((sc->sc_reader_ptr)*(sizeof(int)))), sizeof(int), uio);
    if (error <0) {
      return error;
    }
    sc->sc_reader_ptr++;
    if (sc->sc_reader_ptr >= MAX_SAMPLE_SET) {
      sc->sc_reader_ptr = 0;
    }
  }
  
  return error;
}
static int das_write(dev_t dev, struct uio *uio, int ioflag){
    //returns ENODEV
    return ENODEV;
}
static int das_ioctl(dev_t dev, u_long cmd, void* data, int fflag, struct lwp *p)
{
    //ioctl value??
    //define io_ and call it in our program

    // satlink example of extracting data from ioctl
    // memcpy(data, &sc->sc_id, sizeof(sc->sc_id));
  struct das_softc * sc;
  sc = device_lookup_private(&das_cd, minor(dev));
  uint16_t ch_holder = sc->sc_channel;
  uint8_t sample_cmd = 0;
  uint8_t stat_reg = 0;
  switch(cmd){
    case DAS_START_SAMPLING:
    // set OP1 to 1 -- add in the header file the value for op1 as it is an offset from base address
    sample_cmd = 24|sc->sc_channel;
    sc->sc_samp = 1;
    bus_space_write_1(sc->sc_iot,sc->sc_ioh,CTR1, sample_cmd);
    return 0;
    break;
    case DAS_STOP_SAMPLING:
    // Set OP1 to 0
    sc->sc_samp = 0;
    sample_cmd = 8|sc->sc_channel;
    bus_space_write_1(sc->sc_iot,sc->sc_ioh, CTR1, sample_cmd);
    return 0;
    break;
      case DAS_SET_RATE:
      memcpy(&sc->sc_rate, data, sizeof(int)); // set the rate to the ctr
      if(sc->sc_rate < 0 || sc->sc_rate > UINT16_MAX) {
        sc->sc_rate = DAS_DEFAULT_RATE;
	      return -1;
      }
      uint16_t _cmd = (sc->sc_rate);
      uint8_t cmd_h,cmd_l;
      cmd_h = (uint8_t)(_cmd>>8);
      cmd_l = (uint8_t)(_cmd&225);
      bus_space_write_1(sc->sc_iot,sc->sc_ioh, CLOCK,cmd_l);
      bus_space_write_1(sc->sc_iot,sc->sc_ioh, CLOCK,cmd_h);
      return 0;
      break;
      case DAS_GET_RATE:
      memcpy(data, &sc->sc_rate, sizeof(sc->sc_rate));
 // I imagine it's more complicated than this but for now
    return 0;
    break;
    case DAS_SET_CHANNEL:
      memcpy(&sc->sc_channel, data, sizeof(int)); // need to figure out if sc needs reference
      if (sc->sc_channel <0 || sc->sc_channel >7) {
        sc->sc_channel = ch_holder;
        return -1;
      }
      
      stat_reg = (stat_reg|sc->sc_channel); // input channel num into phrase
      stat_reg = (stat_reg|8); // make sure interrupt bit is enabled before output
      bus_space_write_1(sc->sc_iot,sc->sc_ioh, CTR1, stat_reg); //write it back
      return 0;
    break;
      case DAS_GET_CHANNEL:
     
      stat_reg = bus_space_read_1(sc->sc_iot,sc->sc_ioh, CTR1);
      stat_reg = stat_reg&7;
      memcpy(data, &stat_reg, sizeof(stat_reg));

    return 0;
    break;
      case DAS_GET_REGISTER:
    
    return 0;
    break;
      case DAS_SET_REGISTER:
    
    return 0;
    break;
  default:
    printf("Invalid IOCTL value\n");
    return -1;
    break;
  }
}

// Function that interrupts point to
static int das_intr(void *p)
{
  struct das_softc *sc = p;
  uint8_t word =0;
  word = bus_space_read_1(sc->sc_iot,sc->sc_ioh,CTR1);
  if((word&8) == 8){
    if(sc->sc_open != 1) return 1;
    // free the das_read from being blocked
    uint8_t status = 1;
    while (status == 1) {
      status = bus_space_read_1(sc->sc_iot,sc->sc_ioh, CTR1); // read status register
      status = (status&EOC);
    }
    // reset interrupt register
    bus_space_write_1(sc->sc_iot,sc->sc_ioh,CTR1, word);
    
    
    // Experimental
    
    // read counter 2 data
    sc->sc_time_offset = bus_space_read_1(sc->sc_iot,sc->sc_ioh,CLOCK);
    // read the data
    sc->sc_sample = bus_space_read_1(sc->sc_iot,sc->sc_ioh, sc->sc_ad_high);
    sc->sc_sample = (sc->sc_sample >> 4) | (bus_space_read_1(sc->sc_iot,sc->sc_ioh, sc->sc_ad_low)<<4);
    
    /*
     * Find the time offset somewhere and put it here 
     * calculate the seconds, (clock is 4.125mhz and 1mhz = 1 period to 10^-6 sec)
     * calculation:
     * rate - timeoffset = periods since interrupt
     * no floats allowed, multiply periods by 1000
     * multiple 4.125 by 1000 for non float clock speed
     * divide periods by clock speed
     */
    sc->sc_time_offset = sc->sc_rate - sc->sc_time_offset;
    sc->sc_time_offset = sc->sc_time_offset*1000;
    sc->sc_time_offset = sc->sc_time_offset / CLOCK_SPEED;
    
    if (sc->sc_intr_ptr >= MAX_SAMPLE_SET) {
      sc->sc_intr_ptr = 0;
    }
    sc->sc_buf[sc->sc_intr_ptr] = (((uint32_t)sc->sc_time_offset) << 16) | sc->sc_sample;
    sc->sc_intr_ptr++;
    if (sc->sc_intr_ptr == sc->sc_reader_ptr) {
      sc->sc_reader_ptr++;
    }
    if (sc->sc_reader_ptr>=MAX_SAMPLE_SET) {
      sc->sc_reader_ptr = 0;
    }
    cv_signal(&sc->sc_cv);
    bus_space_write_1(sc->sc_iot,sc->sc_ioh,sc->sc_ad_low, word);
    return 0;
  }
  return 0;
}
