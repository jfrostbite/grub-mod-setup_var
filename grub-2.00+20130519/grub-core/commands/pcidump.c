/* lspci.c - List PCI devices.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2013  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/pci.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/extcmd.h>
#include <grub/env.h>
#include <grub/mm.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_uint32_t pciid_check_mask, pciid_check_value;
static int bus, device, function;
static int check_bus, check_device, check_function;

static const struct grub_arg_option options[] =
  {
    {0, 'd', 0, N_("Select device by vendor and device IDs."),
     N_("[vendor]:[device]"), ARG_TYPE_STRING},
    {0, 's', 0, N_("Select device by its position on the bus."),
     N_("[bus]:[slot][.func]"), ARG_TYPE_STRING},
    {0, 0, 0, 0, 0, 0}
  };

static int
grub_pcidump_iter (grub_pci_device_t dev, grub_pci_id_t pciid,
		  void *data __attribute__ ((unused)))
{
  grub_pci_address_t addr;
  int i;

  if ((pciid & pciid_check_mask) != pciid_check_value)
    return 0;

  if (check_bus && grub_pci_get_bus (dev) != bus)
    return 0;

  if (check_device && grub_pci_get_device (dev) != device)
    return 0;

  if (check_function && grub_pci_get_function (dev) != function)
    return 0;

  for (i = 0; i < 256; i += 4)
    {
      addr = grub_pci_make_address (dev, i);
      grub_printf ("%08x ", grub_pci_read (addr));
      if ((i & 0xc) == 0xc)
	grub_printf ("\n");
    }

  return 0;
}

static grub_err_t
grub_cmd_pcidump (grub_extcmd_context_t ctxt,
		  int argc __attribute__ ((unused)),
		  char **argv __attribute__ ((unused)))
{
  const char *ptr;

  pciid_check_value = 0;
  pciid_check_mask = 0;

  if (ctxt->state[0].set)
    {
      ptr = ctxt->state[0].arg;
      pciid_check_value |= (grub_strtoul (ptr, (char **) &ptr, 16) & 0xffff);
      if (grub_errno == GRUB_ERR_BAD_NUMBER)
	{
	  grub_errno = GRUB_ERR_NONE;
	  ptr = ctxt->state[0].arg;
	}
      else
	pciid_check_mask |= 0xffff;
      if (grub_errno)
	return grub_errno;
      if (*ptr != ':')
	return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("missing `%c' symbol"), ':');
      ptr++;
      pciid_check_value |= (grub_strtoul (ptr, (char **) &ptr, 16) & 0xffff)
	<< 16;
      if (grub_errno == GRUB_ERR_BAD_NUMBER)
	grub_errno = GRUB_ERR_NONE;
      else
	pciid_check_mask |= 0xffff0000;
    }

  pciid_check_value &= pciid_check_mask;

  check_bus = check_device = check_function = 0;

  if (ctxt->state[1].set)
    {
      const char *optr;
      
      ptr = ctxt->state[1].arg;
      optr = ptr;
      bus = grub_strtoul (ptr, (char **) &ptr, 16);
      if (grub_errno == GRUB_ERR_BAD_NUMBER)
	{
	  grub_errno = GRUB_ERR_NONE;
	  ptr = optr;
	}
      else
	check_bus = 1;
      if (grub_errno)
	return grub_errno;
      if (*ptr != ':')
	return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("missing `%c' symbol"), ':');
      ptr++;
      optr = ptr;
      device = grub_strtoul (ptr, (char **) &ptr, 16);
      if (grub_errno == GRUB_ERR_BAD_NUMBER)
	{
	  grub_errno = GRUB_ERR_NONE;
	  ptr = optr;
	}
      else
	check_device = 1;
      if (*ptr == '.')
	{
	  ptr++;
	  function = grub_strtoul (ptr, (char **) &ptr, 16);
	  if (grub_errno)
	    return grub_errno;
	  check_function = 1;
	}
    }

  grub_pci_iterate (grub_pcidump_iter, NULL);
  return GRUB_ERR_NONE;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(setpci)
{
  cmd = grub_register_extcmd ("pcidump", grub_cmd_pcidump, 0,
			      N_("[-s POSITION] [-d DEVICE]"),
			      N_("Dump PCI configuration space."), options);
}

GRUB_MOD_FINI(setpci)
{
  grub_unregister_extcmd (cmd);
}
