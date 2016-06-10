/*******************************************************************************
    OpenAirInterface
    Copyright(c) 1999 - 2014 Eurecom

    OpenAirInterface is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.


    OpenAirInterface is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OpenAirInterface.The full GNU General Public License is
   included in this distribution in the file called "COPYING". If not,
   see <http://www.gnu.org/licenses/>.

  Contact Information
  OpenAirInterface Admin: openair_admin@eurecom.fr
  OpenAirInterface Tech : openair_tech@eurecom.fr
  OpenAirInterface Dev  : openair4g-devel@lists.eurecom.fr

  Address      : Eurecom, Campus SophiaTech, 450 Route des Chappes, CS 50193 - 06904 Biot Sophia Antipolis cedex, FRANCE

*******************************************************************************/

#ifndef __PHY_INTERFACE_VARS_H__
#define __PHY_INTERFACE_VARS_H__

//#include "SIMULATION/PHY_EMULATION/spec_defs.h"
#include "defs.h"

#ifdef PHY_EMUL
#include "SIMULATION/PHY_EMULATION/DEVICE_DRIVER/defs.h"
#include "SIMULATION/simulation_defs.h"
#endif


unsigned int mac_debug;

//MAC_xface *mac_xface;

//MACPHY_PARAMS MACPHY_params;

unsigned int mac_registered;


#endif

#ifndef USER_MODE
EXPORT_SYMBOL(mac_xface);
#endif //PHY_EMUL

