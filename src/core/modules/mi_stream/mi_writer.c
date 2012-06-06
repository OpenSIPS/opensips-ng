/*
 * Copyright (C) 2006 Voice Sistem SRL
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
 *  2006-09-25  first version (bogdan)
 */


#include <stdio.h>
#include <string.h>
#include "../../str.h"
#include "../../mi/tree.h"
#include "../../globals.h"
#include "../../log.h"
#include "../../utils.h"
#include "mi_stream.h"
#include "fifo_fnc.h"
#include "mi_parser.h"

static str  mi_fifo_indent;

int mi_writer_init(char *indent)
{
	if (indent==NULL || indent[0]==0 ) {
		mi_fifo_indent.s = 0;
		mi_fifo_indent.len = 0;
	} else {
		mi_fifo_indent.s = indent;
		mi_fifo_indent.len = strlen(indent);
	}

	return 0;
}

static inline int mi_write_node(str *buf, struct mi_node *node, int level)
{
	struct mi_attr *attr;
	char *end;
	char *p;

	p = buf->s;
	end = buf->s + buf->len;

	/* write indents */
	if (mi_fifo_indent.s) {
		if (p + level*mi_fifo_indent.len>end)
			return -1;
		for( ; level>0 ; level-- ) {
			memcpy( p, mi_fifo_indent.s, mi_fifo_indent.len);
			p += mi_fifo_indent.len;
		}
	}

	/* name and value */
	if (node->name.s!=NULL) {
		if (p+node->name.len+3>end)
			return -1;
		memcpy(p,node->name.s,node->name.len);
		p += node->name.len;
		*(p++) = MI_ATTR_VAL_SEP1;
		*(p++) = MI_ATTR_VAL_SEP2;
		*(p++) = ' ';
	}
	if (node->value.s!=NULL) {
		if (p+node->value.len>end)
			return -1;
		memcpy(p,node->value.s,node->value.len);
		p += node->value.len;
	}

	/* attributes */
	for( attr=node->attributes ; attr!=NULL ; attr=attr->next ) {
		if (attr->name.s!=NULL) {
			if (p+attr->name.len+2>end)
				return -1;
			*(p++) = ' ';
			memcpy(p,attr->name.s,attr->name.len);
			p += attr->name.len;
			*(p++) = '=';
		}
		if (attr->value.s!=NULL) {
			if (p+attr->value.len>end)
				return -1;
			memcpy(p,attr->value.s,attr->value.len);
			p += attr->value.len;
		}
	}

	if (p+1>end)
		return -1;
	*(p++) = '\n';

	buf->len -= p-buf->s;
	buf->s = p;
	return 0;
}



static int recur_write_tree(int fd, struct mi_node *tree, str *buf,
								char *mi_write_buffer,int level)
{
	for( ; tree ; tree=tree->next ) {	
		if (!(tree->flags & MI_WRITTEN)) {
			if (mi_write_node( buf, tree, level)!=0) {
				/* buffer is full -> write it and reset buffer */
				if (mi_fifofd_reply(fd,mi_write_buffer,buf->s-mi_write_buffer)!=0)
					return -1;
				buf->s = mi_write_buffer;
				buf->len = MAX_MI_FIFO_BUFFER;
				if (mi_write_node( buf, tree, level)!=0) {
					LM_ERR("failed to write - line too long!\n");
					return -1;
				}
			}
		}
		if (tree->kids) {
			if (recur_write_tree(fd, tree->kids, buf,mi_write_buffer,
						level+1)<0)
				return -1;
		}
	}
	return 0;
}



int mi_write_tree(int fd, struct mi_root *tree,char *mi_write_buffer)
{
	str buf;
	str code;
	char buff[INT2STR_MAX_LEN];

	buf.s = mi_write_buffer;
	buf.len = MAX_MI_FIFO_BUFFER;

	if (!(tree->node.flags & MI_WRITTEN)) {
		/* write the root node */
		code.s = int2bstr((unsigned long)tree->code,buff,&code.len);
		if (code.len+tree->reason.len+1>buf.len) {
			LM_ERR("failed to write - reason too long!\n");
			return -1;
		}
		memcpy( buf.s, code.s, code.len);
		buf.s += code.len;
		*(buf.s++) = ' ';
		if (tree->reason.len) {
			memcpy( buf.s, tree->reason.s, tree->reason.len);
			buf.s += tree->reason.len;
		}
		*(buf.s++) = '\n';
		buf.len -= code.len + 1 + tree->reason.len+1;
	}

	if (recur_write_tree(fd, tree->node.kids, &buf,mi_write_buffer, 0)!=0)
		return -1;

	if (buf.len<=0) {
		LM_ERR("failed to write - EOC does not fit in!\n");
		return -1;
	}
	*(buf.s++)='\n';
	buf.len--;

	if (mi_fifofd_reply(fd,mi_write_buffer,buf.s-mi_write_buffer)!=0)
		return -1;

	return 0;
}



static int recur_flush_tree(int fd, struct mi_node *tree, str *buf,
		char *mi_write_buffer,int level)
{
	struct mi_node *kid, *tmp;	
	int ret;

	for(kid = tree->kids ; kid ; ){
		/* write the current kid */
		if (!(kid->flags & MI_WRITTEN)){
			if (mi_write_node( buf, kid, level)!=0) {
				/* buffer is full -> write it and reset buffer */
				if (mi_fifofd_reply(fd,mi_write_buffer,buf->s-mi_write_buffer)!=0)
					return -1;
				buf->s = mi_write_buffer;
				buf->len = MAX_MI_FIFO_BUFFER;
				if (mi_write_node( buf, kid, level)!=0) {
					LM_ERR("failed to write - line too long!\n");
					return -1;
				}
			}

			/* we are sure that this node has been written 
			* => avoid writing it again */
			kid->flags |= MI_WRITTEN;
		}

		/* write the current kid's children */
		if ((ret = recur_flush_tree(fd, kid, buf,mi_write_buffer,level+1))<0)
			return -1;
		else if (ret > 0)
			return ret;

		if (!(kid->flags & MI_NOT_COMPLETED)){
			tmp = kid;
			kid = kid->next;
			tree->kids = kid;

			if(!tmp->kids){
				/* this node does not have any kids */
				free_mi_node(tmp); 
			}
		}
		else{
			/* the node will have more kids => to keep the tree shape, do not
			 * flush any other node for now */
			return 1;
		}
	}

	return 0;
}



int mi_flush_tree(fd_wrapper *wrap, struct mi_root *tree)
{
	str buf;
	str code;
	char buff[INT2STR_MAX_LEN];

	buf.s = wrap->buffer;
	buf.len = MAX_MI_FIFO_BUFFER;

	if (!(tree->node.flags & MI_WRITTEN)) {
		/* write the root node */
		code.s = int2bstr((unsigned long)tree->code,buff,&code.len);
		if (code.len+tree->reason.len+1>buf.len) {
			LM_ERR("failed to write - reason too long!\n");
			return -1;
		}
		memcpy( buf.s, code.s, code.len);
		buf.s += code.len;
		*(buf.s++) = ' ';
		if (tree->reason.len) {
			memcpy( buf.s, tree->reason.s, tree->reason.len);
			buf.s += tree->reason.len;
		}
		*(buf.s++) = '\n';
		buf.len -= code.len + 1 + tree->reason.len+1;
		
		/* we are sure that this node has been written 
		 * => avoid writing it again */
		tree->node.flags |= MI_WRITTEN;
	}

	if (recur_flush_tree(wrap->fd, &tree->node, &buf,wrap->buffer, 0)<0)
		return -1;

	if (buf.len<=0) {
		LM_ERR("failed to write - EOC does not fit in!\n");
		return -1;
	}
	*(buf.s++)='\n';
	buf.len--;

	if (mi_fifofd_reply(wrap->fd,wrap->buffer,buf.s-wrap->buffer)!=0)
		return -1;

	return 0;
}
