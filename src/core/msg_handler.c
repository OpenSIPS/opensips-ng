/*
 * Copyright (C) 2010 OpenSIPS Project
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
 * history:
 * ---------
 *  2010-06-xx  created (bogdan)
 */


#include "mem/mem.h"
#include "msg_handler.h"
#include "context.h"
#include "resolve/resolve.h"
#include "parser/parse_uri.h"
#include "builder/msg_builder.h"
#include "builder/via_builder.h"

#include "db/db_to_user.h"
#include "db/db_res.h"


static  int cleanup(struct osips_ctx *ctx, void *b, int ret_code)
{
	LM_DBG("MSG DONE : ret code is %d context: %p message: %p\n",
		 ret_code, ctx, ctx->msg);
	if (b) shm_free(b);
	context_destroy( ctx );
	return 0;
}

/*
static void sip_resolve_continue  (void * arg, unsigned short port,
								unsigned short proto, struct hostent* he,
								dns_request_t * list)
{
	if( he )
	{
		LM_INFO("Received answer for [%s] port %d proto %d\n",
			 he->h_name, port, proto);
	}else
	{
		LM_ERR("Error querying dns for resolve\n");
	}

	if( list != NULL)
	{
		LM_INFO("Next he\n");
		next_he(list, sip_resolve_continue, arg);
	}
	else
		handle_request_continue((struct osips_ctx *)arg,he,port,proto);
};
*/


static void handle_msg_continue(void *arg, unsigned short port,
			unsigned short proto, struct hostent* he, dns_request_t * list)
{
	struct osips_ctx *ctx = (struct osips_ctx *)arg;
	union sockaddr_union su;
	struct sip_msg* msg = ctx->msg;
	char *buf;
	int n,size;
	str via_name = {"Via",3};
	str body;

	if (!he) {
		LM_ERR("Error querying dns for resolve\n");
		goto error;
	}

	LM_INFO("Received answer for [%s] port %d proto %d\n",
			 he->h_name, port, proto);

	if (msg->first_line.type==SIP_REQUEST) {

		/* build new VIA hdr */
		if (via_builder( msg, msg->rcv.bind_address, &body)) {
			LM_ERR("failed to build via\n");
			goto error;
		}

		/* add a new VIA */
		if ( add_hdr( msg, &via_name, &body, HDR_VIA_T, 0 /*first hdr*/,
		HDR_DUP_NAME|HDR_FREE_BODY)==NULL ) {
			LM_ERR("failed to add via\n");
			goto error;
		}

	} else {

		/* remove top most VIA */
		if ( ((struct via_body*)msg->h_via->parsed)->next==NULL) {
			/*remove entire hdr*/
			rm_hdr( msg, msg->h_via);
		} else {
			/* remove only via1 body FIXME */
			alter_hdr( msg, msg->h_via, 10 , 10, NULL);
		}

		
	}

	/* build message */
	buf = construct_msg( msg, &size);
	if (buf==NULL) {
		LM_ERR("failed to build buffer\n");
		goto error;
	}

	hostent2su( &su, he, 0, port );
	free_hostent(he);
	shm_free(he);

	async( ctx, cleanup, buf, n,
		protos[msg->rcv.proto].funcs.write_message,
		msg->rcv.bind_address,
		buf, size,
		&su , NULL/*extra*/ );

	return;
error:
	cleanup( ctx, NULL, -1);
}



int extract_via( struct via_body* via, str *name, unsigned short *port,
														unsigned short *proto )
{
	int err;

	*port = 0;
	/* "normal" reply, we use rport's & received value if present */
	if (via->rport && via->rport->value.s){
		LM_DBG("using 'rport'\n");
		*port = str2s(via->rport->value.s, via->rport->value.len, &err);
		if (err){
			LM_NOTICE("bad rport value(%.*s)\n",
				via->rport->value.len,via->rport->value.s);
			*port=0;
		}
	}

	if (via->maddr){
		*name= via->maddr->value;
		if (*port==0) *port=via->port?via->port:SIP_PORT; 
	} else if (via->received){
		LM_DBG("using 'received'\n");
		*name=via->received->value;
		/* making sure that we won't do SRV lookup on "received" */
		if (*port==0) *port=via->port?via->port:SIP_PORT; 
	}else{
		LM_DBG("using via host <%.*s>\n",via->host.len,via->host.s);
		*name=via->host;
		if (*port==0) *port=via->port;
	}

	return 0;
}



int handle_new_msg(struct sip_msg *msg)
{
	struct osips_ctx *ctx;
	struct sip_uri uri;

	ctx = context_create( msg );
	msg->new_len = msg->len; // Put this in the right place !!!

	LM_INFO("Received: %d bytes ctxt:%p msg:%p\n",msg->len,ctx,msg);


	/* start custom processing -> to be replaced with script logic */
	if (msg->first_line.type==SIP_REQUEST) {

		if (parse_uri( REQ_LINE(msg).uri.s, REQ_LINE(msg).uri.len, &uri)!=0) {
			LM_ERR("failed to parse RURI\n");
			goto error;
		}

		/* resolve DNS to see where the URI points */
		sip_resolvehost( &uri.host, uri.port_no, uri.proto, 0,
				handle_msg_continue, ctx);

		/* nothing to do here, just resume from  */
		return 0;

	} else if (msg->first_line.type==SIP_REPLY) {

		/* get VIA */
		if (parse_headers( msg, HDR_VIA1_F|HDR_VIA2_F, 0 )==-1 
		|| (msg->via2==0) || (msg->via2->error!=PARSE_OK)) {
			LM_ERR("bogus VIAs in reply\n");
			goto error;
		}
		/* get destination from VIA */
		memset(&uri,0,sizeof(uri));
		extract_via( msg->via2, &uri.host, &uri.port_no, &uri.proto );

		/* resolve DNS to see where the URI points */
		sip_resolvehost( &uri.host, uri.port_no, uri.proto, 0,
				handle_msg_continue, ctx);

		return 0;
	}

error:
	LM_ERR("msg not reply or request -> dropping\n");
	cleanup( ctx, NULL, -1);
	return -1;
}



int init_msg_handler(void)
{
	return 0;
}





#if 0
str mysql_url = str_init("mysql://root:vlad@192.168.2.134/opensips");
//str mysql_url = str_init("postgres://postgres:postgres@127.0.0.1/opensips");

str table_version = str_init("version");
str table_test = str_init("test");
str col_test = str_init("ID");
str * key_test[1] = {&col_test};

str raw_query = str_init("select * from version");

db_val_t val_test = {
	DB_INT, /**< Type of the value                              */
	0,		/**< Means that the column in database has no value */
	0,		/**< Means that the value should be freed */
	{ .int_val = 69 }
};

db_pool_t * pool = NULL;


void db_query_continue (void * arg, db_res_t * res, int ret)
{
	int i;
	db_row_t *row = NULL;

	if (res)
		LM_DBG("Return code %d, Row count %d, column count %d, arg %p\n",
			ret,
			res->res_rows,
			res->col.n,
			arg);

	if (ret < 0)
		goto forward_msg;

	for (i=0;i<res->col.n;i++)
		LM_DBG("Column names are %.*s\n",(*(res->col.names+i))->len,(*(res->col.names+i))->s);

	row = db_allocate_row(res->col.n);
	if (!row)
	{
		LM_ERR("no more pkg memory\n");
		return;
	}

	for (i=0;i<res->res_rows;i++)
	{
		db_fetch_row(pool,res,row);
//		LM_INFO("Row %d 1st col <%s>\n",i,row->values->val.string_val);
//		LM_INFO("Row %d 2nd col <%d>\n",i,(row->values+1)->val.int_val);
		db_free_row_vals(row);
	}

	pkg_free(row);
	db_free_result(pool, res);
forward_msg:
	db_finalize_fetch(res);
	new_msg_continue(arg);

}

void db_insert_continue (void * arg, int ret)
{
//	LM_INFO("Return code %d\n", ret);
	new_msg_continue(arg);
}

void db_fetch_continue(void *arg,db_item_t *it,int ret)
{
//	int i;
//	db_res_t *res = 0;
//	LM_INFO("Return code %d\n", ret);
//
//	db_fetch_result(it,&res,1);
//
//	LM_INFO("Succesfully fetched result \n");
//	for (i=0;i<res->col.n;i++)
//		LM_INFO("Column names are %.*s\n",(*(res->col.names+i))->len,(*(res->col.names+i))->s);
//
//	i=0;
//	LM_INFO("First column : <%s>\n",(res->rows+i)->values->val.string_val);
//	db_free_result(pool, res);
//	db_finalize_fetch(it);
//	new_msg_continue(arg);
}

int handle_new_msg(struct sip_msg *msg)
{
	static int my_ps = 0;
//	struct hdr_field *hdr,*new;
//	str name = str_init("Via");
//	str body = str_init("testing\r\n");
//	str new_body = str_init("megatest");
//	int new_len;
//	char *buf = 0;

	/* TODO - maybe better place to put this ?
	 * problem if setting in tcp/tcp.c 
	 */
	msg->new_len = msg->len;
	/**** TESTING CODE FOR DNS RESOLVER ****/

	//str domain = str_init("opensip.com.ar");
	//str domain = str_init("85.55.55.55");
	//get_record("_sip._udp.siphub.net", ns_t_srv ,get_record_continue, msg);
	//resolvehost("google.com", resolve_continue, msg);
	//sip_resolvehost( &domain, 0, PROTO_NONE, 0, sip_resolve_continue, msg);
	
	/**** TESTING CODE FOR DB ****/

//	db_insert(pool, &table_test,
//			   key_test, &val_test,
//			   1, 0,
//			   db_insert_continue, msg);

db_select(pool, &table_version,
		NULL, NULL, NULL, NULL, 0, 0, NULL,0,
		db_query_continue, msg);

//	db_raw_query(pool,&table_version,&raw_query,db_query_continue,msg);

	/* XXX works */
//	db_delete(pool,&table_test,0,0,0,0,0,db_insert_continue,msg);
	
//	LM_ERR("Before : \n\n");
//	for (hdr=msg->headers;hdr;hdr=hdr->next)
//		LM_ERR("<%.*s>:<%.*s> - %d\n",hdr->name.len,hdr->name.s,hdr->body.len,hdr->body.s,hdr->len);
	
//	new = add_hdr(msg,&name,&body,-1,msg->headers->next->next->next->next->next->next->next,HDR_DUP_NAME | HDR_DUP_BODY);

//	LM_ERR("After add : \n\n");
//	for (hdr=msg->headers;hdr;hdr=hdr->next)
//		LM_ERR("<%.*s>:<%.*s>\n",hdr->name.len,hdr->name.s,hdr->body.len,hdr->body.s);

//	alter_hdr(msg,msg->headers->next->next->next,msg->headers->next->next->next->body.len,0,&new_body);
//	LM_ERR("After alter : \n\n");
//	for (hdr=msg->headers;hdr;hdr=hdr->next)
//		LM_ERR("<%.*s>:<%.*s>\n",hdr->name.len,hdr->name.s,hdr->body.len,hdr->body.s);

///	replace_hdr(msg,msg->headers->next,&body);
//	LM_ERR("After replace : \n\n");
//	for (hdr=msg->headers;hdr;hdr=hdr->next)
//		LM_ERR("<%.*s>:<%.*s>\n",hdr->name.len,hdr->name.s,hdr->body.len,hdr->body.s);

//	rm_hdr(msg,msg->headers->next->next,msg->headers->next);
//	LM_ERR("After rm : \n\n");
//	for (hdr=msg->headers;hdr;hdr=hdr->next)
//		LM_ERR("<%.*s>:<%.*s>\n",hdr->name.len,hdr->name.s,hdr->body.len,hdr->body.s);
//	buf = construct_msg(msg);

//	printf("New message is\n\n%.*s\n\n",msg->new_len,buf);
//	shm_free(buf);
//	new_msg_continue(msg);
//
	
	return 0;
};
#endif


