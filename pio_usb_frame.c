// pio_usb_frame.c - the 1 ms PIO-USB frame with the RP2350-E9 workaround, see pio_usb_frame.h
#include "tusb.h"

#if CFG_TUH_RPI_PIO_USB && PICO_RP2350
#include "pico/stdlib.h"
#include "hardware/structs/pads_bank0.h"
#include "pio_usb.h"
#include "pio_usb_ll.h"
#include "pio_usb_frame.h"

#define FRAME_ALARM 2                   /* the hardware alarm Pico-PIO-USB uses itself */
#define E9_DRAIN_FRAMES 16u             /* frames between drains while a device is attached */
#define E9_DRAIN_US 5u

static repeating_timer_t frame_timer;
static bool e9;                         /* chip up to A2: needs the workaround */
static uint32_t e9_frames;

static void __not_in_flash_func(wait_us)(uint32_t us)
{
    uint32_t start = timer_hw->timerawl;
    while (timer_hw->timerawl - start <= us)
        ;
}

static void __not_in_flash_func(set_inputs)(const root_port_t *root, bool on)
{
    io_rw_32 *dp = &pads_bank0_hw->io[root->pin_dp];
    io_rw_32 *dm = &pads_bank0_hw->io[root->pin_dm];
    if (on)
    {
        hw_set_bits(dp, PADS_BANK0_GPIO0_IE_BITS);
        hw_set_bits(dm, PADS_BANK0_GPIO0_IE_BITS);
    }
    else
    {
        hw_clear_bits(dp, PADS_BANK0_GPIO0_IE_BITS);
        hw_clear_bits(dm, PADS_BANK0_GPIO0_IE_BITS);
    }
    (void)*dp;                          /* read back: the writes have reached the pads */
    (void)*dm;
}

/* Before pio_usb_host_frame: drain the lines of an attached device now and then. */
static void __not_in_flash_func(e9_drain)(void)
{
    root_port_t *root = PIO_USB_ROOT_PORT(0);
    if (!(root->initialized && root->connected && !root->suspended) || ++e9_frames % E9_DRAIN_FRAMES)
        return;
    set_inputs(root, false);
    wait_us(E9_DRAIN_US);
    set_inputs(root, true);
}

/* After pio_usb_host_frame: while nothing is attached, look for a J state (the
   device's pull-up on D+ or D-) as pio_usb_host_frame does, then leave the
   inputs off until the next frame. */
static void __not_in_flash_func(e9_connect)(void)
{
    root_port_t *root = PIO_USB_ROOT_PORT(0);
    if (!root->initialized || root->connected)
        return;
    set_inputs(root, true);
    wait_us(1);
    bool dp = !gpio_get(root->pin_dp);  /* pio_usb_host_add_port inverts the inputs */
    bool dm = !gpio_get(root->pin_dm);
    if (dp != dm)
    {
        root->is_fullspeed = dp;
        root->connected = true;
        root->suspended = true;         /* a bus reset comes first */
        root->ints |= PIO_USB_INTS_CONNECT_BITS;
        return;
    }
    set_inputs(root, false);
}

static bool __not_in_flash_func(frame)(repeating_timer_t *rt)
{
    (void)rt;
    if (e9)
    {
        e9_drain();
        pio_usb_host_frame();
        e9_connect();
    }
    else
        pio_usb_host_frame();
    return true;
}

void pio_usb_frame_start(void)
{
    e9 = rp2350_chip_version() <= 2;
    alarm_pool_add_repeating_timer_us(alarm_pool_create(FRAME_ALARM, 1), -1000, frame, NULL, &frame_timer);
    printf("PIO USB frame: RP2350-E9 workaround %s\n", e9 ? "on" : "off");
}
#endif
