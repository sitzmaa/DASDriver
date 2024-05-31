/* $NetBSD: toy.c Alex Sitzman*/
/*CSCI462*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: toy.c,v 1.0 $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/tty.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

// #define DATA = "This a string of 30 characters";
#define DATALEN 30
#define TOYIOCTL _IO('Y',0)
#define TOYF_ISOPEN  0x01 /* device is open*/
#define TOYF_DATA    0x02 /* device is not ready to transmit data*/

/* Softc for toy driver -- see satlink driver*/
/* Satlink does not have device name as part -- investigate*/
struct toy_softc {
	bus_space_tag_t sc_iot;		/* space tag */
	bus_space_handle_t sc_ioh;	/* space handle */
	isa_chipset_tag_t sc_ic;	/* ISA chipset info */
	int	sc_drq;			/* the DRQ we're using */
	void *	sc_data;			/* ring buffer for incoming data */
	int	sc_uptr;		/* user index into ring buffer */
	int	sc_sptr;		/* toy index into ring buffer */
	int	sc_flags;		/* misc. flags. */
	int	sc_lastresid;		/* residual */
	callout_t sc_ch;		/* callout pseudo-interrupt */
};


/* Pseudo interrupt taken from satlink
*  ***Delete if never used***
*/
// #define TOY_TIMEOUT hz/10



int	toymatch(device_t, cfdata_t, void *);
void	toyattach(device_t, device_t, void *);
void	toytimeout(void *);

bool isMatched = false;

CFATTACH_DECL_NEW(toy, sizeof(struct toy_softc),
    toymatch, toyattach, NULL, NULL);


/* create cfdriver struct */

extern struct cfdriver toy_cd;

dev_type_open(toyopen);
dev_type_close(toyclose);
dev_type_read(toyread);
dev_type_write(toywrite);
dev_type_ioctl(toyioctl);

const struct cdevsw toy_cdevsw = {
    .d_open = toyopen,
    .d_close = toyclose,
    .d_read = toyread,
    .d_write = toywrite,
    .d_ioctl = toyioctl,
    .d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

int toymatch (device_t parent, cfdata_t match, void *aux)
{
    /* Basic call out and claim*/
    /* May need to add functionality later*/
    if (isMatched)
        return (0);

    isMatched = true;
    return (1);
}

void toyattach(device_t parent, device_t self, void *aux)
{
    /* Find out if any flags need to be sent here */

    /* First Draft attach -- look into this more*/
    struct toy_softc *sc = device_private(self);
    sc->sc_sptr = DATALEN;
    sc->sc_flags = 0;
    // sc->sc_data = DATA;
    return;

}

/* Check if open flag
*  if not then set it and return success
*/
int toyopen(dev_t dev, int flags, int fmt,
    struct lwp *l)
{
    struct toy_softc *sc;    
    sc = device_lookup_private(&toy_cd, minor(dev));

    if (sc == NULL)
        return (ENXIO);

    if (sc->sc_flags & TOYF_ISOPEN)
        return (EBUSY);
    
    sc->sc_flags = TOYF_ISOPEN;

    // callout_reset(&sc->sc_ch, TOY_TIMEOUT, toytimeout, sc);

    return (0);
}

/* unset is open flag*/
int toyclose(dev_t dev, int flags, int fmt,
    struct lwp *l)
{
    isMatched = false;

    return (0);
}

int toyread(dev_t dev, struct uio *uio, int flags)
{
    struct toy_softc *sc;
    int error, s, count, sptr;
    char data[] = "This a string of 30 characters";
    sc = device_lookup_private(&toy_cd, minor(dev));

    s = splsoftclock();

    /* Wait for data (maybe not needed?) 
    while (sc->sc_sptr == sc->sc_uptr) {
		if (flags & O_NONBLOCK) {
			splx(s);
			return (EWOULDBLOCK);
		}
		sc->sc_flags |= TOYF_DATA;
		if ((error = tsleep(sc, TTIPRI | PCATCH, "toyio", 0)) != 0) {
			splx(s);
			return (error);
		}
	} */
    if (sc->sc_sptr == sc->sc_uptr) {
        /* No infomation to read */
        return (-1);
    }

    sptr = sc->sc_sptr;
	splx(s);

    /* Get count of to read */
	count = sptr - sc->sc_uptr;

	if (count > uio->uio_resid)
		count = uio->uio_resid;


    /* cases
     * Easy - no wrap around 
     * - user pointer is behind toy pointer
     * Wrapped -
     * - user pointer is in front of toy pointer (not used in this implementation)
     */    

    /* Easy Case */
    error = uiomove((char*)data + sc->sc_uptr, count, uio);
    if (error == 0) {
        sc->sc_uptr += count;
        if (sc->sc_uptr == DATALEN)
            sc->sc_uptr = DATALEN - 1;
        return (count);
    }
    return (error);
}

int toywrite(dev_t dev, struct uio *uio, int flags)
{
    /* reset uptr */
    struct toy_softc *sc;
    sc = device_lookup_private(&toy_cd, minor(dev));
    sc->sc_uptr = 0;
    return (0);
}

int toyioctl(dev_t dev, u_long cmd, void *data, int flags,
    struct lwp *l)
{
    if (cmd == TOYIOCTL) {

        printf("Hello\n");
    }
    
    return (0);
}
