#include <calico/types.h>
#include <calico/arm/common.h>
#include <calico/dev/tmio.h>

#define TMIO_DEBUG

#ifdef TMIO_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif

#define REG_TMIO(_type,_name) (*(_type volatile*)(ctl->reg_base + TMIO_##_name))

#define REG_TMIO_CMD        REG_TMIO(u16, CMD)
#define REG_TMIO_PORTSEL    REG_TMIO(u16, PORTSEL)
#define REG_TMIO_CMDARG     REG_TMIO(u32, CMDARG)
#define REG_TMIO_STOP       REG_TMIO(u16, STOP)
#define REG_TMIO_BLKCNT     REG_TMIO(u16, BLKCNT)
#define REG_TMIO_CMDRESP    REG_TMIO(TmioResp, CMDRESP)
#define REG_TMIO_STAT       REG_TMIO(u32, STAT)
#define REG_TMIO_MASK       REG_TMIO(u32, MASK)
#define REG_TMIO_CLKCTL     REG_TMIO(u16, CLKCTL)
#define REG_TMIO_BLKLEN     REG_TMIO(u16, BLKLEN)
#define REG_TMIO_OPTION     REG_TMIO(u16, OPTION)
#define REG_TMIO_ERROR      REG_TMIO(u32, ERROR)
#define REG_TMIO_FIFO16     REG_TMIO(u16, FIFO16)
#define REG_TMIO_SDIOCTL    REG_TMIO(u16, SDIOCTL)
#define REG_TMIO_SDIO_STAT  REG_TMIO(u16, SDIO_STAT)
#define REG_TMIO_SDIO_MASK  REG_TMIO(u16, SDIO_MASK)
#define REG_TMIO_FIFOCTL    REG_TMIO(u16, FIFOCTL)
#define REG_TMIO_RESET      REG_TMIO(u16, RESET)
#define REG_TMIO_VERSION    REG_TMIO(u16, VERSION)
#define REG_TMIO_POWER      REG_TMIO(u16, POWER)
#define REG_TMIO_EXT_SDIO   REG_TMIO(u16, EXT_SDIO)
#define REG_TMIO_EXT_WRPROT REG_TMIO(u16, EXT_WRPROT)
#define REG_TMIO_EXT_STAT   REG_TMIO(u32, EXT_STAT)
#define REG_TMIO_EXT_MASK   REG_TMIO(u32, EXT_MASK)
#define REG_TMIO_CNT32      REG_TMIO(u16, CNT32)
#define REG_TMIO_BLKLEN32   REG_TMIO(u16, BLKLEN32)
#define REG_TMIO_BLKCNT32   REG_TMIO(u16, BLKCNT32)

#define ERROR_IRQ_BITS ( \
	TMIO_STAT_BAD_CMD_INDEX | \
	TMIO_STAT_BAD_CRC | \
	TMIO_STAT_BAD_STOP_BIT | \
	TMIO_STAT_DATA_TIMEOUT | \
	TMIO_STAT_RX_OVERFLOW | \
	TMIO_STAT_TX_UNDERRUN | \
	TMIO_STAT_CMD_TIMEOUT | \
	TMIO_STAT_ILL_ACCESS )

#define TX_IRQ_BITS ( \
	TMIO_STAT_CMD_RESPEND | \
	TMIO_STAT_CMD_DATAEND | \
	ERROR_IRQ_BITS )

static ThrListNode s_tmioIrqQueue;
static ThrListNode s_tmioTxEndQueue;

bool tmioInit(TmioCtl* ctl, uptr reg_base, uptr fifo_base, u32* mbox_slots, unsigned num_mbox_slots)
{
	*ctl = (TmioCtl){0};
	ctl->reg_base = reg_base;
	ctl->fifo_base = fifo_base;

	// Fast fail if the TMIO controller is either inaccessible
	// (SCFG-blocked) or no ports are present
	unsigned num_ports = TMIO_PORTSEL_NUMPORTS(REG_TMIO_PORTSEL);
	dietPrint("TMIO @ %#.8x - %u ports\n", reg_base, num_ports);
	if (num_ports == 0) {
		return false;
	}

	// Initialize dummy current port information
	ctl->cur_port.clock = 0xffff;
	ctl->cur_port.num = 0xff;
	ctl->cur_port.width = 0xff;

	// Initialize mailbox
	mailboxPrepare(&ctl->mbox, mbox_slots, num_mbox_slots);

	// Perform a soft reset. Resets the following regs:
	//   TMIO_STOP      = 0
	//   TMIO_CMDRESP   = 0
	//   TMIO_ERROR     = 0
	//   TMIO_CLKCTL   &= ~(TMIO_CLKCTL_ENABLE | TMIO_CLKCTL_UNK10)
	//   TMIO_OPTION    = 0x40ee
	//   TMIO_STAT      = 0 (everything acknowledged)
	//   TMIO_SDIO_STAT = 0
	//   Presumably:
	//     TMIO_EXT_SDIO irq bits?
	//     TMIO_EXT_STAT?
	REG_TMIO_RESET &= ~1;
	REG_TMIO_RESET |= 1;

	// Mask all interrupts
	REG_TMIO_MASK = ~0;
	REG_TMIO_SDIOCTL = 0; // port0 disable sdio
	REG_TMIO_SDIO_MASK = ~0;
	REG_TMIO_EXT_SDIO = 7<<8; // also: port1..3 disable sdio, acknowledge irqs
	//REG_TMIO_EXT_MASK = ~0; // no$ barfs at this: "notyet32 write"
	REG_TMIO(u16, EXT_MASK+0) = ~0;
	REG_TMIO(u16, EXT_MASK+2) = ~0;

	// Initialize 32-bit FIFO
	REG_TMIO_CNT32 = TMIO_CNT32_ENABLE | TMIO_CNT32_FIFO_CLEAR;
	REG_TMIO_FIFOCTL = TMIO_FIFOCTL_MODE32;

	// Initialize options (not technically needed)
	REG_TMIO_OPTION = TMIO_OPTION_DETECT(14) | TMIO_OPTION_TIMEOUT(14) | TMIO_OPTION_NO_C2;

	return true;
}

void tmioIrqHandler(TmioCtl* ctl)
{
	TmioTx* tx = ctl->cur_tx;
	if (tx && tx->xfer_isr) {
		u32 fifostat = REG_TMIO_CNT32 ^ TMIO_CNT32_STAT_NOT_SEND;
		fifostat &= fifostat>>3; // mask out if IRQs are disabled
		if (fifostat & (TMIO_CNT32_STAT_RECV|TMIO_CNT32_STAT_NOT_SEND)) {
			tx->xfer_isr(ctl, tx);
		}
	}

	threadUnblockOneByValue(&s_tmioIrqQueue, (u32)ctl);
}

void tmioThreadMain(TmioCtl* ctl)
{
	dietPrint("TMIO thread start %p\n", ctl);

	// Disable interrupts in this thread
	armIrqLockByPsr();

	for (;;) {
		// Receive transaction request
		TmioTx* tx = (TmioTx*)mailboxRecv(&ctl->mbox);
		ctl->cur_tx = tx;

		unsigned cmd_id = tx->type & 0x3f;
		dietPrint("TMIO cmd%2u arg=%.8lx\n", cmd_id, tx->arg);

		// Invoke transaction callback if needed
		if (tx->callback) {
			tx->callback(ctl, tx);
		}

		// Apply port number if changed
		if (ctl->cur_port.num != tx->port.num) {
			ctl->cur_port.num = tx->port.num;
			REG_TMIO_PORTSEL = (REG_TMIO_PORTSEL &~ TMIO_PORTSEL_PORT(3)) | TMIO_PORTSEL_PORT(ctl->cur_port.num);
		}

		// Apply bus width if changed
		if (ctl->cur_port.width != tx->port.width) {
			ctl->cur_port.width = tx->port.width;
			REG_TMIO_OPTION = (REG_TMIO_OPTION &~ TMIO_OPTION_BUS_WIDTH1) | (ctl->cur_port.width == 1 ? TMIO_OPTION_BUS_WIDTH1 : TMIO_OPTION_BUS_WIDTH4);
		}

		// Apply clock if changed
		if (ctl->cur_port.clock != tx->port.clock) {
			ctl->cur_port.clock = tx->port.clock;

			u16 clkmask = TMIO_CLKCTL_DIV(0xff) | TMIO_CLKCTL_AUTO;
			u16 clk0 = REG_TMIO_CLKCTL &~ TMIO_CLKCTL_ENABLE;
			u16 clk1 = (clk0 &~ clkmask) | (ctl->cur_port.clock & clkmask);
			u16 clk2 = clk1 | TMIO_CLKCTL_ENABLE;

			REG_TMIO_CLKCTL = clk0; // Disable clock
			REG_TMIO_CLKCTL = clk1; // Apply new clock settings
			REG_TMIO_CLKCTL = clk2; // Reenable clock
		}

		// GO_IDLE requires 1ms delay between applying clock and sending the command
		if (cmd_id == 0) {
			threadSleep(1000);
		}

		// Set up block transfer FIFO if needed
		u32 isr_bits = 0;
		if (tx->type & TMIO_CMD_TX) {
			u16 stop = REG_TMIO_STOP &~ (TMIO_STOP_DO_STOP|TMIO_STOP_AUTO_STOP);
			if (tx->type & TMIO_CMD_TX_MULTI) {
				stop |= TMIO_STOP_AUTO_STOP;
			}

			REG_TMIO_STOP = stop;
			REG_TMIO_BLKLEN = tx->block_size;
			REG_TMIO_BLKCNT = tx->num_blocks;
			REG_TMIO_BLKLEN32 = tx->block_size;
			REG_TMIO_BLKCNT32 = tx->num_blocks; // this is purely informational

			// Enable corresponding interrupt if block transfer callback is used
			if (tx->xfer_isr) {
				isr_bits = (tx->type & TMIO_CMD_TX_READ) ? TMIO_CNT32_IE_RECV : TMIO_CNT32_IE_SEND;
				REG_TMIO_CNT32 |= isr_bits;
			}
		}

		// Enable transaction-related interrupts
		REG_TMIO_STAT = ~TX_IRQ_BITS; // acknowledge them first just in case
		REG_TMIO_MASK &= ~TX_IRQ_BITS;

		// Feed command to the controller
		REG_TMIO_CMDARG = tx->arg;
		REG_TMIO_CMD = tx->type;

		// Wait for the command to be done processing
		do {
			threadBlock(&s_tmioIrqQueue, (u32)ctl);

			u32 stat = REG_TMIO_STAT;
			u32 bits = stat & TX_IRQ_BITS;

			// Update status
			REG_TMIO_STAT = ~bits; // acknowledge bits (by writing 0)
			tx->status |= bits;
			if (!(stat & TMIO_STAT_CMD_BUSY)) {
				// Command is done, clear bits in the transaction struct
				tx->status &= ERROR_IRQ_BITS;
			}

			// Read response if needed
			if (bits & TMIO_STAT_CMD_RESPEND) {
				switch (tx->type & TMIO_CMD_RESP_MASK) {
					default: break;

					case TMIO_CMD_RESP_48:
					case TMIO_CMD_RESP_48_BUSY:
					case TMIO_CMD_RESP_48_NOCRC:
						tx->resp.value[0] = REG_TMIO_CMDRESP.value[0];
						break;

					case TMIO_CMD_RESP_136: {
						TmioResp resp = REG_TMIO_CMDRESP;

						if_likely (cmd_id != 2 && cmd_id != 10) {
							// Regular 136-bit response, needs some shifting...
							tx->resp.value[0] = (resp.value[0]<<8);
							tx->resp.value[1] = (resp.value[1]<<8) | (resp.value[0]>>24);
							tx->resp.value[2] = (resp.value[2]<<8) | (resp.value[1]>>24);
							tx->resp.value[3] = (resp.value[3]<<8) | (resp.value[2]>>24);
						} else {
							// Special case for cmd2/cmd10 (all_get_cid/get_cid)
							tx->resp = resp;
						}

						break;
					}
				}
			}
		} while (tx->status & TMIO_STAT_CMD_BUSY);

		// Disable transaction-related interrupts
		REG_TMIO_MASK |= TX_IRQ_BITS;

		// Disable interrupt if block transfer callback is used
		if (isr_bits) {
			REG_TMIO_CNT32 &= ~isr_bits;
		}

		// Invoke transaction callback if needed
		if (tx->callback) {
			tx->callback(ctl, tx);
		}

		// Notify that we're done with this transaction
		ctl->cur_tx = NULL;
		threadUnblockOneByValue(&s_tmioTxEndQueue, (u32)tx);
	}
}

bool tmioTransact(TmioCtl* ctl, TmioTx* tx)
{
	ArmIrqState st = armIrqLockByPsr();

	tx->status = TMIO_STAT_CMD_BUSY;
	if_unlikely (!mailboxTrySend(&ctl->mbox, (u32)tx)) {
		tx->status = TMIO_STAT_RX_OVERFLOW;
		armIrqUnlockByPsr(st);
		return false;
	}

	if_likely (tx->status & TMIO_STAT_CMD_BUSY) {
		threadBlock(&s_tmioTxEndQueue, (u32)tx);
	}

	armIrqUnlockByPsr(st);
	return tx->status == 0;
}

void tmioXferRecvByCpu(TmioCtl* ctl, TmioTx* tx)
{
	vu32* fifo = (vu32*)ctl->fifo_base;
	u32* buf = (u32*)tx->user;

	for (u32 i = 0; i < tx->block_size; i += 4) {
		*buf++ = *fifo;
	}

	tx->user = buf;
}

void tmioXferSendByCpu(TmioCtl* ctl, TmioTx* tx)
{
	vu32* fifo = (vu32*)ctl->fifo_base;
	u32* buf = (u32*)tx->user;

	for (u32 i = 0; i < tx->block_size; i += 4) {
		*fifo = *buf++;
	}

	tx->user = buf;
}
