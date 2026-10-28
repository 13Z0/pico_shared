#ifndef __PIO_USB_FRAME_H__
#define __PIO_USB_FRAME_H__
/*
 * pio_usb_frame - the 1 ms PIO-USB frame, run by pico_shared instead of by
 * Pico-PIO-USB, with a workaround for erratum RP2350-E9.
 *
 * pio_usb_board_init() sets skip_alarm_pool in the PIO-USB configuration, and
 * pio_usb_frame_start() then runs the frame (pio_usb_host_frame: SOF, transfers
 * and the connection check) from the same timer Pico-PIO-USB would have used:
 * an alarm pool on hardware alarm 2, every 1000 us, on the calling core.
 *
 * RP2350-E9 (chips up to A2, such as the Fruit Jam's): a pad with its input
 * enabled keeps a line that was high at about 2.2 V, and the pull-down cannot
 * bring it back. Pico-PIO-USB switches the inputs off for 8 NOPs before it reads
 * the line state, too short to drain a line (21 ns at 378 MHz), and reads right
 * after switching them back on. A USB device then often stayed unseen: D- was
 * latched, so D+ and D- both read high (SE1), never an idle state.
 * So while nothing is attached, the inputs stay off between frames (the lines
 * drain through the pull-downs) and the line state is also read here, with the
 * inputs settled. While a device is attached, the lines are drained for a few
 * microseconds every 16 frames, so that Pico-PIO-USB sees an unplug as SE0.
 */

#ifdef __cplusplus
extern "C" {
#endif

// Call once, after tusb_init() has brought up the PIO-USB host.
void pio_usb_frame_start(void);

#ifdef __cplusplus
}
#endif

#endif
