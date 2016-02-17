#include "../include/bcm4339.h"
#include "../include/wrapper.h"
#include "../include/structs.h"
#include "../include/helper.h"
#include "../include/types.h" /* needs to be included before bcmsdpcm.h */
#include "../include/bcmdhd/bcmsdpcm.h"
#include "../include/bcmdhd/bcmcdc.h"

#include "bdc_ethernet_ipv6_udp_header_array.h"

#define SDIO_SEQ_NUM 0x108

extern void *dngl_sendpkt_alternative(void *sdio, void *p, int chan);
struct sk_buff *create_frame(unsigned int hooked_fct, unsigned int arg0, unsigned int arg1, unsigned int arg2, void *start_address, unsigned int length);

__attribute__((naked)) void 
wlc_txfifo_hook(void)
{
	asm("push {r0-r2, r4-r11, lr}\n"		// lr is not included, as it was already pushed before
		"push {r0-r3,lr}\n"					// save the registers that might change as well as the link register
		"bl testprint\n"
		"pop {r0-r3,lr}\n"					// restore the saved registers
		"b wlc_txfifo+4\n"					// jump to the original function
		);
}

void
testprint(void)
{
	printf("wlc_txfifo\n");
}

__attribute__((naked)) void
interrupt_enable_hook(void)
{
	asm("push {r0-r3,lr}\n"					// save the registers that might change as well as the link register
		"bl testprint2\n"
		"pop {r0-r3,lr}\n"					// restore the saved registers
		"b sub_166b4\n"						// call the function that was supposed to be called
		);
}

__attribute__((naked)) void
setup_some_stuff_hook(void)
{
	asm("push {r0-r3,lr}\n"					// save the registers that might change as well as the link register
		"bl testprint3\n"
		"pop {r0-r3,lr}\n"					// restore the saved registers
		"b setup_some_stuff\n"				// call the function that was supposed to be called
		);
}

void
testprint3(void)
{
	unsigned int *sp = get_stack_ptr();

	for (; (unsigned int) sp < BOTTOM_OF_STACK - 16; sp+=4)
		printf("%08x: %08x %08x %08x %08x \n", sp, *(sp), *(sp+1), *(sp+2), *(sp+3));

}

void
testprint2(void)
{
	struct sk_buff *p = 0;

	p = create_frame((unsigned int) &sub_166b4, 0, 0, 0, get_stack_ptr(), BOTTOM_OF_STACK - (unsigned int) get_stack_ptr());

	dngl_sendpkt_alternative(SDIO_INFO_ADDR, p, SDPCM_DATA_CHANNEL);
}

int
bus_binddev_rom_hook(void *sdiodev, void *d11dev)
{
	int x;
	int *sdio_hw = (int *) *(*(((int **) sdiodev)+6)+6);
	void *z;
	void *console_buf;

	// enlage console
	console_buf = malloc(0x2000, 0);
	memset(console_buf, 0, 0x2000);
	*(void **) 0x1EBDDC = console_buf;
	*(int *) 0x1EBDE0 = 0x2000;
	*(int *) 0x1EBDE4 = 0;
	*(void **) 0x1EBDE8 = console_buf;

	//x = bus_binddev_rom(sdiodev, d11dev);
	//x = bus_binddev(sdio_hw, sdiodev, d11dev);
	sdio_hw[14] = (int) sdiodev;
	z = sub_1831A0((void *) sdio_hw[1], sdio_hw, sdio_hw[0], sdiodev);
	sdio_hw[3] = (int) z;

	sdio_hw[15] = (int) d11dev;
//	sdio_hw[15] = 0x100; // hinders the chip from initializing, as sub_14C54 tries to call a callback referenced through this pointer

//	struct sk_buff *p = 0;
//	p = create_frame(0xdddddddd, 0, 0, 0, get_stack_ptr(), BOTTOM_OF_STACK - (unsigned int) get_stack_ptr());
//	dngl_sendpkt_alternative(SDIO_INFO_ADDR, p, SDPCM_DATA_CHANNEL);

	*(((int *) sdiodev)+9) = (int) d11dev;
	*(((int *) d11dev)+36/4) = (int) sdiodev;

	if(z) {
		sub_16D8C(0x1D3CB8, 0x183715, sdio_hw);
		sub_16D8C(0x93B8D, 0x14B8D, sdio_hw);
		x = 0;
	}

	
	return x;
}

void
sub_1ECAB0_hook(int a1)
{
	printf("%08x\n", a1);
	sub_1ECAB0(a1);
	struct sk_buff *p = 0;
	p = create_frame(0xdddddddd, 0, 0, 0, get_stack_ptr(), BOTTOM_OF_STACK - (unsigned int) get_stack_ptr());
	dngl_sendpkt_alternative(SDIO_INFO_ADDR, p, SDPCM_DATA_CHANNEL);
}

struct sk_buff *
create_frame(unsigned int hooked_fct, unsigned int arg0, unsigned int arg1, unsigned int arg2, void *start_address, unsigned int length) {
	struct sk_buff *p = 0;
	struct osl_info *osh = OSL_INFO_ADDR;
	struct bdc_ethernet_ipv6_udp_header *hdr;
	struct nexmon_header *nexmon_hdr;

	p = pkt_buf_get_skb(osh, sizeof(struct bdc_ethernet_ipv6_udp_header) - 1 + sizeof(struct nexmon_header) - 1 + length);

	// copy headers to target buffer
	memcpy(p->data, bdc_ethernet_ipv6_udp_header_array, bdc_ethernet_ipv6_udp_header_length);

	hdr = p->data;
	hdr->ipv6.payload_length = htons(sizeof(struct udp_header) + sizeof(struct nexmon_header) - 1 + length);
	nexmon_hdr = (struct nexmon_header *) hdr->payload;

	nexmon_hdr->hooked_fct = hooked_fct;
	nexmon_hdr->args[0] = arg0;
	nexmon_hdr->args[1] = arg1;
	nexmon_hdr->args[2] = arg2;

	memcpy(nexmon_hdr->payload, start_address, length);

	return p;
}

void *
dma_txfast_hook(void *di, struct sk_buff *p, int commit)
{
	struct sk_buff *p1 = 0;
	struct sk_buff *p2 = 0;
	void *ret = 0;
	unsigned char *sdio = (unsigned char *) SDIO_INFO_ADDR;

	if (di == DMA_INFO_SDIODEV_ADDR) {
//		p1 = create_frame((unsigned int) &dma_txfast, (unsigned int) di, 0, 0, get_stack_ptr(), BOTTOM_OF_STACK - (unsigned int) get_stack_ptr());
	} else {
		p1 = create_frame((unsigned int) &dma_txfast, (unsigned int) di, 0, 0, get_stack_ptr(), BOTTOM_OF_STACK - (unsigned int) get_stack_ptr());
		p2 = create_frame((unsigned int) &dma_txfast, (unsigned int) di, 0, 0, p->data, p->len);
	}

//	if (di == DMA_INFO_D11FIFO1_ADDR) {
//		p2 = pkt_buf_get_skb((struct osl_info *) OSL_INFO_ADDR, p->len);
//		memcpy(p2->data, p->data, p->len);
//		dma_txfast(di, p2, commit);
//	}

	// We transmit the original frame first, as it already contains the correct sequence number
        ret = dma_txfast(di, p, commit);

	// Now we check, if an SDIO DMA transfer was done and fix the sequence number that is increased automatically in the dngl_sendpkt function
	if (p1 != 0) {
		if (di == DMA_INFO_SDIODEV_ADDR && ((unsigned char *) p->data)[4] == sdio[SDIO_SEQ_NUM]) {
			sdio[SDIO_SEQ_NUM]++;
			dngl_sendpkt_alternative(SDIO_INFO_ADDR, p1, SDPCM_DATA_CHANNEL);
			dngl_sendpkt_alternative(SDIO_INFO_ADDR, p2, SDPCM_DATA_CHANNEL);
			sdio[SDIO_SEQ_NUM]--;
		} else {
			dngl_sendpkt_alternative(SDIO_INFO_ADDR, p1, SDPCM_DATA_CHANNEL);
			dngl_sendpkt_alternative(SDIO_INFO_ADDR, p2, SDPCM_DATA_CHANNEL);
		}
	}

	//printf("X %08x %08x %08x %d\n", (unsigned int) di, (unsigned int) p, (unsigned int) p->data, p->len);

	return ret;
}