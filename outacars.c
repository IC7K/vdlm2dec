/*
 *  Copyright (c) 2016 Thierry Leconte
 *
 *   
 *   This code is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <netdb.h>
#include "vdlm2.h"
#include "crc.h"
#include "acars.h"
#include "cJSON.h"

extern int verbose;
extern cJSON *json_obj;

extern int DecodeLabel(acarsmsg_t *msg,oooi_t *oooi);

static void printmsg(const acarsmsg_t * msg)
{
	fprintf(logfd, "ACARS\n");

	if (msg->mode < 0x5d) {
		fprintf(logfd, "Aircraft reg: %s ", msg->addr);
		fprintf(logfd, "Flight id: %s", msg->fid);
		fprintf(logfd, "\n");
	}
	fprintf(logfd, "Mode: %1c ", msg->mode);
	fprintf(logfd, "Msg. label: %s\n", msg->label);
	fprintf(logfd, "Block id: %c ", msg->bid);
	fprintf(logfd, "Ack: %c\n", msg->ack);
	fprintf(logfd, "Msg. no: %s\n", msg->no);
	if(msg->txt[0]) fprintf(logfd, "Message :\n%s\n", msg->txt);
	if (msg->be == 0x17)
		fprintf(logfd, "Block End\n");

	fflush(logfd);
}

static void buildacarsjson(acarsmsg_t * msg)
{

	oooi_t oooi;
	char convert_tmp[8];

	snprintf(convert_tmp, sizeof(convert_tmp), "%c", msg->mode);
	cJSON_AddStringToObject(json_obj, "mode", convert_tmp);

	cJSON_AddStringToObject(json_obj, "label", msg->label);

	if(msg->bid) {
		snprintf(convert_tmp, sizeof(convert_tmp), "%c", msg->bid);
		cJSON_AddStringToObject(json_obj, "block_id", convert_tmp);

		if(msg->ack == 0x15) {
			cJSON_AddFalseToObject(json_obj, "ack");
		} else {
			snprintf(convert_tmp, sizeof(convert_tmp), "%c", msg->ack);
			cJSON_AddStringToObject(json_obj, "ack", convert_tmp);
		}

		cJSON_AddStringToObject(json_obj, "tail", msg->addr);
		if(msg->mode <= 'Z') {
			cJSON_AddStringToObject(json_obj, "flight", msg->fid);
			cJSON_AddStringToObject(json_obj, "msgno", msg->no);
		}
	}
	if(msg->txt[0])
		cJSON_AddStringToObject(json_obj, "text", msg->txt);

	if (msg->be == 0x17)
		cJSON_AddTrueToObject(json_obj, "end");

	if(DecodeLabel(msg, &oooi)) {
		if(oooi.sa[0])
			cJSON_AddStringToObject(json_obj, "depa", oooi.sa);
		if(oooi.da[0])
			cJSON_AddStringToObject(json_obj, "dsta", oooi.da);
		if(oooi.eta[0])
			cJSON_AddStringToObject(json_obj, "eta", oooi.eta);
		if(oooi.gout[0])
			cJSON_AddStringToObject(json_obj, "gtout", oooi.gout);
		if(oooi.gin[0])
			cJSON_AddStringToObject(json_obj, "gtin", oooi.gin);
		if(oooi.woff[0])
			cJSON_AddStringToObject(json_obj, "wloff", oooi.woff);
		if(oooi.won[0])
			cJSON_AddStringToObject(json_obj, "wlin", oooi.won);
	}
}


void outacars(unsigned char *txt, int len)
{
	acarsmsg_t msg;
	int i, k, j;
	unsigned int crc;

	crc = 0;
	/* test crc, set le and remove parity */
	for (i = 0; i < len - 1; i++) {
		update_crc(crc, txt[i]);
		txt[i] &= 0x7f;
	}
	if (crc) {
		if(verbose) fprintf(logfd, "crc error\n");
		return;
	}

	/* fill msg struct */

	k = 0;
	msg.mode = txt[k];
	k++;

	for (i = 0, j = 0; i < 7; i++, k++) {
		if (txt[k] != '.') {
			msg.addr[j] = txt[k];
			j++;
		}
	}
	msg.addr[j] = '\0';

	/* ACK/NAK */
	msg.ack = txt[k];
	if (msg.ack == 0x15)
		msg.ack = '!';
	k++;

	msg.label[0] = txt[k];
	k++;
	msg.label[1] = txt[k];
	if (msg.label[1] == 0x7f)
		msg.label[1] = 'd';
	k++;
	msg.label[2] = '\0';

	msg.bid = txt[k];
	if (msg.bid == 0)
		msg.bid = ' ';
	k++;

	/* txt start  */
	msg.bs = txt[k];
	k++;

	msg.no[0] = '\0';
	msg.fid[0] = '\0';
	msg.txt[0] = '\0';

	if (msg.bs != 0x03) {
		if (msg.mode <= 'Z' && msg.bid <= '9') {
			/* message no */
			for (i = 0; i < 4 && k < len - 4; i++, k++) {
				msg.no[i] = txt[k];
			}
			msg.no[i] = '\0';

			/* Flight id */
			for (i = 0; i < 6 && k < len - 4; i++, k++) {
				msg.fid[i] = txt[k];
			}
			msg.fid[i] = '\0';
		}

		/* Message txt */
		for (i = 0; (k < len - 4); i++, k++)
			msg.txt[i] = txt[k];
		msg.txt[i] = 0;
	}
	msg.be = txt[k];

	if (verbose) {
		printmsg(&msg);
	}

	if(json_obj)
		buildacarsjson(&msg);

}
