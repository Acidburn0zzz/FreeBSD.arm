/*-
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/dev/isci/scil/scif_sas_timer.c 231136 2012-02-07 17:43:58Z jimharris $");

/**
 * @file
 *
 * @brief This file contains core callback notificiations implemented by
 *        the framework for timers.
 */

#include <dev/isci/scil/sci_types.h>
#include <dev/isci/scil/scif_user_callback.h>
#include <dev/isci/scil/scic_user_callback.h>

#include <dev/isci/scil/scif_sas_controller.h>

void * scic_cb_timer_create(
   SCI_CONTROLLER_HANDLE_T   controller,
   SCI_TIMER_CALLBACK_T      timer_callback,
   void                    * cookie
)
{
   SCIF_SAS_CONTROLLER_T * fw_controller = (SCIF_SAS_CONTROLLER_T *)
                                         sci_object_get_association(controller);

   return scif_cb_timer_create(fw_controller, timer_callback, cookie);
}

// -----------------------------------------------------------------------------

void scic_cb_timer_destroy(
   SCI_CONTROLLER_HANDLE_T   controller,
   void                    * timer
)
{
   SCIF_SAS_CONTROLLER_T * fw_controller = (SCIF_SAS_CONTROLLER_T *)
                                         sci_object_get_association(controller);
   if (timer != NULL)
   {
      scif_cb_timer_destroy(fw_controller, timer);
      timer = NULL;
   }
}

// -----------------------------------------------------------------------------

void scic_cb_timer_start(
   SCI_CONTROLLER_HANDLE_T   controller,
   void                    * timer,
   U32                       milliseconds
)
{
   SCIF_SAS_CONTROLLER_T * fw_controller = (SCIF_SAS_CONTROLLER_T *)
                                         sci_object_get_association(controller);

   scif_cb_timer_start(fw_controller, timer, milliseconds);
}

// -----------------------------------------------------------------------------

void scic_cb_timer_stop(
   SCI_CONTROLLER_HANDLE_T   controller,
   void                    * timer
)
{
   SCIF_SAS_CONTROLLER_T * fw_controller = (SCIF_SAS_CONTROLLER_T *)
                                         sci_object_get_association(controller);

   scif_cb_timer_stop(fw_controller, timer);
}

