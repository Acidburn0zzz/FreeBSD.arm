/* i915_irq.c -- IRQ support for the I915 -*- linux-c -*-
 */
/*-
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/dev/drm/i915_irq.c 194986 2009-06-25 18:27:08Z rnoland $");

#include "dev/drm/drmP.h"
#include "dev/drm/drm.h"
#include "dev/drm/i915_drm.h"
#include "dev/drm/i915_drv.h"

#define MAX_NOPID ((u32)~0)

/**
 * Interrupts that are always left unmasked.
 *
 * Since pipe events are edge-triggered from the PIPESTAT register to IIR,
 * we leave them always unmasked in IMR and then control enabling them through
 * PIPESTAT alone.
 */
#define I915_INTERRUPT_ENABLE_FIX	(I915_DISPLAY_PIPE_A_EVENT_INTERRUPT | \
				   	 I915_DISPLAY_PIPE_B_EVENT_INTERRUPT)

/** Interrupts that we mask and unmask at runtime. */
#define I915_INTERRUPT_ENABLE_VAR	(I915_USER_INTERRUPT)

/** These are all of the interrupts used by the driver */
#define I915_INTERRUPT_ENABLE_MASK	(I915_INTERRUPT_ENABLE_FIX | \
				    	 I915_INTERRUPT_ENABLE_VAR)

#define DRM_I915_VBLANK_PIPE_ALL	(DRM_I915_VBLANK_PIPE_A | \
					 DRM_I915_VBLANK_PIPE_B)

static inline void
i915_enable_irq(drm_i915_private_t *dev_priv, u32 mask)
{
	mask &= I915_INTERRUPT_ENABLE_VAR;
	if ((dev_priv->irq_mask_reg & mask) != 0) {
		dev_priv->irq_mask_reg &= ~mask;
		I915_WRITE(IMR, dev_priv->irq_mask_reg);
		(void) I915_READ(IMR);
	}
}

static inline void
i915_disable_irq(drm_i915_private_t *dev_priv, u32 mask)
{
	mask &= I915_INTERRUPT_ENABLE_VAR;
	if ((dev_priv->irq_mask_reg & mask) != mask) {
		dev_priv->irq_mask_reg |= mask;
		I915_WRITE(IMR, dev_priv->irq_mask_reg);
		(void) I915_READ(IMR);
	}
}

static inline u32
i915_pipestat(int pipe)
{
	if (pipe == 0)
	    return PIPEASTAT;
	if (pipe == 1)
	    return PIPEBSTAT;
	return -EINVAL;
}

void
i915_enable_pipestat(drm_i915_private_t *dev_priv, int pipe, u32 mask)
{
	if ((dev_priv->pipestat[pipe] & mask) != mask) {
		u32 reg = i915_pipestat(pipe);

		dev_priv->pipestat[pipe] |= mask;
		/* Enable the interrupt, clear any pending status */
		I915_WRITE(reg, dev_priv->pipestat[pipe] | (mask >> 16));
		(void) I915_READ(reg);
	}
}

void
i915_disable_pipestat(drm_i915_private_t *dev_priv, int pipe, u32 mask)
{
	if ((dev_priv->pipestat[pipe] & mask) != 0) {
		u32 reg = i915_pipestat(pipe);

		dev_priv->pipestat[pipe] &= ~mask;
		I915_WRITE(reg, dev_priv->pipestat[pipe]);
		(void) I915_READ(reg);
	}
}

/**
 * i915_pipe_enabled - check if a pipe is enabled
 * @dev: DRM device
 * @pipe: pipe to check
 *
 * Reading certain registers when the pipe is disabled can hang the chip.
 * Use this routine to make sure the PLL is running and the pipe is active
 * before reading such registers if unsure.
 */
static int
i915_pipe_enabled(struct drm_device *dev, int pipe)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	unsigned long pipeconf = pipe ? PIPEBCONF : PIPEACONF;

	if (I915_READ(pipeconf) & PIPEACONF_ENABLE)
		return 1;

	return 0;
}

/* Called from drm generic code, passed a 'crtc', which
 * we use as a pipe index
 */
u32 i915_get_vblank_counter(struct drm_device *dev, int pipe)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	unsigned long high_frame;
	unsigned long low_frame;
	u32 high1, high2, low, count;

	high_frame = pipe ? PIPEBFRAMEHIGH : PIPEAFRAMEHIGH;
	low_frame = pipe ? PIPEBFRAMEPIXEL : PIPEAFRAMEPIXEL;

	if (!i915_pipe_enabled(dev, pipe)) {
		DRM_DEBUG("trying to get vblank count for disabled pipe %d\n", pipe);
		return 0;
	}

	/*
	 * High & low register fields aren't synchronized, so make sure
	 * we get a low value that's stable across two reads of the high
	 * register.
	 */
	do {
		high1 = ((I915_READ(high_frame) & PIPE_FRAME_HIGH_MASK) >>
			 PIPE_FRAME_HIGH_SHIFT);
		low =  ((I915_READ(low_frame) & PIPE_FRAME_LOW_MASK) >>
			PIPE_FRAME_LOW_SHIFT);
		high2 = ((I915_READ(high_frame) & PIPE_FRAME_HIGH_MASK) >>
			 PIPE_FRAME_HIGH_SHIFT);
	} while (high1 != high2);

	count = (high1 << 8) | low;

	return count;
}

u32 g45_get_vblank_counter(struct drm_device *dev, int pipe)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	int reg = pipe ? PIPEB_FRMCOUNT_GM45 : PIPEA_FRMCOUNT_GM45;

	if (!i915_pipe_enabled(dev, pipe)) {
		DRM_DEBUG("trying to get vblank count for disabled pipe %d\n", pipe);
		return 0;
	}

	return I915_READ(reg);
}

irqreturn_t i915_driver_irq_handler(DRM_IRQ_ARGS)
{
	struct drm_device *dev = (struct drm_device *) arg;
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	u32 iir, new_iir;
	u32 pipea_stats, pipeb_stats;
	u32 vblank_status;
	u32 vblank_enable;
	int irq_received;

	iir = I915_READ(IIR);

	if (IS_I965G(dev)) {
		vblank_status = PIPE_START_VBLANK_INTERRUPT_STATUS;
		vblank_enable = PIPE_START_VBLANK_INTERRUPT_ENABLE;
	} else {
		vblank_status = I915_VBLANK_INTERRUPT_STATUS;
		vblank_enable = I915_VBLANK_INTERRUPT_ENABLE;
	}

	for (;;) {
		irq_received = iir != 0;

		/* Can't rely on pipestat interrupt bit in iir as it might
		 * have been cleared after the pipestat interrupt was received.
		 * It doesn't set the bit in iir again, but it still produces
		 * interrupts (for non-MSI).
		 */
		DRM_SPINLOCK(&dev_priv->user_irq_lock);
		pipea_stats = I915_READ(PIPEASTAT);
		pipeb_stats = I915_READ(PIPEBSTAT);

		/*
		 * Clear the PIPE(A|B)STAT regs before the IIR
		 */
		if (pipea_stats & 0x8000ffff) {
			I915_WRITE(PIPEASTAT, pipea_stats);
			irq_received = 1;
		}

		if (pipeb_stats & 0x8000ffff) {
			I915_WRITE(PIPEBSTAT, pipeb_stats);
			irq_received = 1;
		}
		DRM_SPINUNLOCK(&dev_priv->user_irq_lock);

		if (!irq_received)
			break;

		I915_WRITE(IIR, iir);
		new_iir = I915_READ(IIR); /* Flush posted writes */

		if (dev_priv->sarea_priv)
			dev_priv->sarea_priv->last_dispatch =
			    READ_BREADCRUMB(dev_priv);

		if (iir & I915_USER_INTERRUPT) {
			DRM_WAKEUP(&dev_priv->irq_queue);
		}

		if (pipea_stats & vblank_status)
			drm_handle_vblank(dev, 0);

		if (pipeb_stats & vblank_status)
			drm_handle_vblank(dev, 1);

		/* With MSI, interrupts are only generated when iir
		 * transitions from zero to nonzero.  If another bit got
		 * set while we were handling the existing iir bits, then
		 * we would never get another interrupt.
		 *
		 * This is fine on non-MSI as well, as if we hit this path
		 * we avoid exiting the interrupt handler only to generate
		 * another one.
		 *
		 * Note that for MSI this could cause a stray interrupt report
		 * if an interrupt landed in the time between writing IIR and
		 * the posting read.  This should be rare enough to never
		 * trigger the 99% of 100,000 interrupts test for disabling
		 * stray interrupts.
		 */
		iir = new_iir;
	}
}

static int i915_emit_irq(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;

	i915_kernel_lost_context(dev);

	if (++dev_priv->counter > 0x7FFFFFFFUL)
		dev_priv->counter = 0;
	if (dev_priv->sarea_priv)
		dev_priv->sarea_priv->last_enqueue = dev_priv->counter;

	DRM_DEBUG("emitting: %d\n", dev_priv->counter);

	BEGIN_LP_RING(4);
	OUT_RING(MI_STORE_DWORD_INDEX);
	OUT_RING(I915_BREADCRUMB_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
	OUT_RING(dev_priv->counter);
	OUT_RING(MI_USER_INTERRUPT);
	ADVANCE_LP_RING();

	return dev_priv->counter;
}

void i915_user_irq_get(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	if (dev->irq_enabled == 0)
		return;

	DRM_DEBUG("\n");
	DRM_SPINLOCK(&dev_priv->user_irq_lock);
	if (++dev_priv->user_irq_refcount == 1)
		i915_enable_irq(dev_priv, I915_USER_INTERRUPT);
	DRM_SPINUNLOCK(&dev_priv->user_irq_lock);
}

void i915_user_irq_put(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	if (dev->irq_enabled == 0)
		return;

	DRM_SPINLOCK(&dev_priv->user_irq_lock);
	KASSERT(dev_priv->user_irq_refcount > 0, ("invalid refcount"));
	if (--dev_priv->user_irq_refcount == 0)
		i915_disable_irq(dev_priv, I915_USER_INTERRUPT);
	DRM_SPINUNLOCK(&dev_priv->user_irq_lock);
}

static int i915_wait_irq(struct drm_device * dev, int irq_nr)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	int ret = 0;

	if (READ_BREADCRUMB(dev_priv) >= irq_nr) {
		if (dev_priv->sarea_priv) {
			dev_priv->sarea_priv->last_dispatch =
				READ_BREADCRUMB(dev_priv);
		}
		return 0;
	}

	if (dev_priv->sarea_priv)
		dev_priv->sarea_priv->perf_boxes |= I915_BOX_WAIT;

	DRM_DEBUG("irq_nr=%d breadcrumb=%d\n", irq_nr,
		  READ_BREADCRUMB(dev_priv));

	i915_user_irq_get(dev);
	DRM_WAIT_ON(ret, dev_priv->irq_queue, 3 * DRM_HZ,
		    READ_BREADCRUMB(dev_priv) >= irq_nr);
	i915_user_irq_put(dev);

	if (ret == -ERESTART)
		DRM_DEBUG("restarting syscall\n");

	if (ret == -EBUSY) {
		DRM_ERROR("EBUSY -- rec: %d emitted: %d\n",
			  READ_BREADCRUMB(dev_priv), (int)dev_priv->counter);
	}

	return ret;
}

/* Needs the lock as it touches the ring.
 */
int i915_irq_emit(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_irq_emit_t *emit = data;
	int result;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	RING_LOCK_TEST_WITH_RETURN(dev, file_priv);

	result = i915_emit_irq(dev);

	if (DRM_COPY_TO_USER(emit->irq_seq, &result, sizeof(int))) {
		DRM_ERROR("copy_to_user\n");
		return -EFAULT;
	}

	return 0;
}

/* Doesn't need the hardware lock.
 */
int i915_irq_wait(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_irq_wait_t *irqwait = data;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	return i915_wait_irq(dev, irqwait->irq_seq);
}

/* Called from drm generic code, passed 'crtc' which
 * we use as a pipe index
 */
int i915_enable_vblank(struct drm_device *dev, int pipe)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	if (!i915_pipe_enabled(dev, pipe))
		return -EINVAL;

	DRM_SPINLOCK(&dev_priv->user_irq_lock);
	if (IS_I965G(dev))
		i915_enable_pipestat(dev_priv, pipe,
				     PIPE_START_VBLANK_INTERRUPT_ENABLE);
	else
		i915_enable_pipestat(dev_priv, pipe,
				     PIPE_VBLANK_INTERRUPT_ENABLE);
	DRM_SPINUNLOCK(&dev_priv->user_irq_lock);
	return 0;
}

/* Called from drm generic code, passed 'crtc' which
 * we use as a pipe index
 */
void i915_disable_vblank(struct drm_device *dev, int pipe)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	DRM_SPINLOCK(&dev_priv->user_irq_lock);
	i915_disable_pipestat(dev_priv, pipe,
			      PIPE_VBLANK_INTERRUPT_ENABLE |
			      PIPE_START_VBLANK_INTERRUPT_ENABLE);
	DRM_SPINUNLOCK(&dev_priv->user_irq_lock);
}

/* Set the vblank monitor pipe
 */
int i915_vblank_pipe_set(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	return 0;
}

int i915_vblank_pipe_get(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_vblank_pipe_t *pipe = data;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	pipe->pipe = DRM_I915_VBLANK_PIPE_A | DRM_I915_VBLANK_PIPE_B;

	return 0;
}

/**
 * Schedule buffer swap at given vertical blank.
 */
int i915_vblank_swap(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	/* The delayed swap mechanism was fundamentally racy, and has been
	 * removed.  The model was that the client requested a delayed flip/swap
	 * from the kernel, then waited for vblank before continuing to perform
	 * rendering.  The problem was that the kernel might wake the client
	 * up before it dispatched the vblank swap (since the lock has to be
	 * held while touching the ringbuffer), in which case the client would
	 * clear and start the next frame before the swap occurred, and
	 * flicker would occur in addition to likely missing the vblank.
	 *
	 * In the absence of this ioctl, userland falls back to a correct path
	 * of waiting for a vblank, then dispatching the swap on its own.
	 * Context switching to userland and back is plenty fast enough for
	 * meeting the requirements of vblank swapping.
	 */
	return -EINVAL;
}

/* drm_dma.h hooks
*/
void i915_driver_irq_preinstall(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	I915_WRITE(HWSTAM, 0xeffe);
	I915_WRITE(PIPEASTAT, 0);
	I915_WRITE(PIPEBSTAT, 0);
	I915_WRITE(IMR, 0xffffffff);
	I915_WRITE(IER, 0x0);
	(void) I915_READ(IER);
}

int i915_driver_irq_postinstall(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	dev_priv->vblank_pipe = DRM_I915_VBLANK_PIPE_A | DRM_I915_VBLANK_PIPE_B;

	/* Unmask the interrupts that we always want on. */
	dev_priv->irq_mask_reg = ~I915_INTERRUPT_ENABLE_FIX;

	/* Disable pipe interrupt enables, clear pending pipe status */
	I915_WRITE(PIPEASTAT, I915_READ(PIPEASTAT) & 0x8000ffff);
	I915_WRITE(PIPEBSTAT, I915_READ(PIPEBSTAT) & 0x8000ffff);

	/* Clear pending interrupt status */
	I915_WRITE(IIR, I915_READ(IIR));

	I915_WRITE(IER, I915_INTERRUPT_ENABLE_MASK);
	I915_WRITE(IMR, dev_priv->irq_mask_reg);
	I915_WRITE(PIPEASTAT, dev_priv->pipestat[0] |
	    (dev_priv->pipestat[0] >> 16));
	I915_WRITE(PIPEBSTAT, dev_priv->pipestat[1] |
	    (dev_priv->pipestat[1] >> 16));
	(void) I915_READ(IER);

	return 0;
}

void i915_driver_irq_uninstall(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	if (!dev_priv)
		return;

	dev_priv->vblank_pipe = 0;

	I915_WRITE(HWSTAM, 0xffffffff);
	I915_WRITE(PIPEASTAT, 0);
	I915_WRITE(PIPEBSTAT, 0);
	I915_WRITE(IMR, 0xffffffff);
	I915_WRITE(IER, 0x0);

	I915_WRITE(PIPEASTAT, I915_READ(PIPEASTAT) & 0x8000ffff);
	I915_WRITE(PIPEBSTAT, I915_READ(PIPEBSTAT) & 0x8000ffff);
	I915_WRITE(IIR, I915_READ(IIR));
}
