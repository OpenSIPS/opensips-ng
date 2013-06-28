/*
 * Copyright (C) OpenSIPS Solutions
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2013-06-28  first version (razvancrainea)
 */


#ifndef _MI_LISTENER_H_
#define _MI_LISTENER_H_

int mi_get_next_line(char **dest, int max, char *source,int *read);
int mi_read_and_append( char *b, int fd,int *current_command_len);
int mi_is_valid(char *b,int len);
int mi_listener(void *param);

#endif /* _MI_LISTENER_H_ */


