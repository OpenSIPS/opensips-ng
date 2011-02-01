/*
 * Copyright (C) 2010 OpenSIPS Project
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 */


#ifndef _CORE_CONFIG_H
#define _CORE_CONFIG_H

#define CFG_FILE CFG_DIR "opensips.cfg"

#define SIP_PORT  5060
#define SIPS_PORT 5061

#define SHM_MEM_SIZE 32

#define USER_AGENT "User-Agent: OpenSIPS (" VERSION " (" ARCH "/" OS"))"
#define USER_AGENT_LEN (sizeof(USER_AGENT)-1)

#define SERVER_HDR "Server: OpenSIPS (" VERSION " (" ARCH "/" OS"))"
#define SERVER_HDR_LEN (sizeof(SERVER_HDR)-1)

#define SRV_UDP_PREFIX "_sip._udp."
#define SRV_UDP_PREFIX_LEN (sizeof(SRV_UDP_PREFIX) - 1)

#define SRV_TCP_PREFIX "_sip._tcp."
#define SRV_TCP_PREFIX_LEN (sizeof(SRV_TCP_PREFIX) - 1)

#define SRV_SCTP_PREFIX "_sip._sctp."
#define SRV_SCTP_PREFIX_LEN (sizeof(SRV_SCTP_PREFIX) - 1)

#define SRV_TLS_PREFIX "_sips._tcp."
#define SRV_TLS_PREFIX_LEN (sizeof(SRV_TLS_PREFIX) - 1)

#define SRV_MAX_PREFIX_LEN SRV_TLS_PREFIX_LEN

/*!< magic cookie for transaction matching as defined in RFC3261 */
#define MCOOKIE "z9hG4bK"
#define MCOOKIE_LEN (sizeof(MCOOKIE)-1)




#endif
