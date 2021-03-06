/* $NetBSD: lpt_acpi.c,v 1.23 2021/01/29 15:49:55 thorpej Exp $ */

/*
 * Copyright (c) 2002 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lpt_acpi.c,v 1.23 2021/01/29 15:49:55 thorpej Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/termios.h>
#include <sys/systm.h>

#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_intr.h>

#include <dev/ic/lptvar.h>

#include <dev/isa/isadmavar.h>

static int	lpt_acpi_match(device_t, cfdata_t, void *);
static void	lpt_acpi_attach(device_t, device_t, void *);

struct lpt_acpi_softc {
	struct lpt_softc sc_lpt;
};

CFATTACH_DECL_NEW(lpt_acpi, sizeof(struct lpt_acpi_softc), lpt_acpi_match,
    lpt_acpi_attach, NULL, NULL);

/*
 * Supported device IDs
 */

static const struct device_compatible_entry compat_data[] = {
	{ .compat = "PNP04??" },	/* Standard LPT printer port */
	DEVICE_COMPAT_EOL
};

/*
 * lpt_acpi_match: autoconf(9) match routine
 */
static int
lpt_acpi_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	return acpi_compatible_match(aa, compat_data);
}

/*
 * lpt_acpi_attach: autoconf(9) attach routine
 */
static void
lpt_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct lpt_acpi_softc *asc = device_private(self);
	struct lpt_softc *sc = &asc->sc_lpt;
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_io *io;
	ACPI_STATUS rv;

	sc->sc_dev = self;

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	/* parse resources */
	rv = acpi_resource_parse(sc->sc_dev, aa->aa_node->ad_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	/* find our i/o registers */
	io = acpi_res_io(&res, 0);
	if (io == NULL) {
		aprint_error_dev(self,
		    "unable to find i/o register resource\n");
		goto out;
	}

	sc->sc_iot = aa->aa_iot;
	if (bus_space_map(sc->sc_iot, io->ar_base, io->ar_length,
		    0, &sc->sc_ioh)) {
		aprint_error_dev(self, "can't map i/o space\n");
		goto out;
	}

	lpt_attach_subr(sc);

	sc->sc_ih = acpi_intr_establish(self,
	    (uint64_t)(uintptr_t)aa->aa_node->ad_handle,
	    IPL_TTY, false, lptintr, sc, device_xname(self));
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "unable to establish interrupt\n");
		goto out;
	}

 out:
	acpi_resource_cleanup(&res);
}
