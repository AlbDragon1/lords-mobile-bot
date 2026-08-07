#include "protocol/mail.h"
#include "net_rw.h"
#include "packet_enum.h"
#include "connection.h"
#include "log.h"

#include "bot/command.h"

void RequestSendMail(Connection *c, const char *player_name, const char *subject, const char *message) {
	uint8_t subject_len = (uint8_t)strlen(subject);
	uint16_t message_len = (uint16_t)strlen(message);
	
	c->size = 2;
	
	// packet type 
	write_u16(c->data + c->size, _MSG_REQUEST_SENDREGMAIL);
	c->size += 2;
	
	// sequence id
	write_u32(c->data + c->size, ++c->protocol.seq_id);
	c->size += 4;
	
	
	write_u32(c->data + c->size, 0);
	c->size += 4;
	
	// player name
	write_raw(c->data + c->size, player_name, 13);
	c->size += 13;
	
	write_u8 (c->data + c->size, 1);
	c->size += 1;
	
	write_u8 (c->data + c->size, subject_len);
	c->size += 1;
	
	write_u16(c->data + c->size, message_len);
	c->size += 2;
	
	write_u8 (c->data + c->size, 0);
	c->size += 1;
	
	
	write_raw(c->data + c->size, subject, subject_len);
	c->size += subject_len;
	
	write_raw(c->data + c->size, message, message_len);
	c->size += message_len;
	
	
	write_zero(c->data + c->size, 10);
	c->size += 10;
	
	write_u16(c->data, c->size); // update packet size
	send_packet(c, true);
}

void RequestSendMailFmt(Connection *c, const char *player_name, const char *subject, const char *fmt, ...) {
    char buf[4096];  // adjust size as needed
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    RequestSendMail(c, player_name, subject, buf);
}


void RecvMailInfo(Connection *c, const uint8_t *data)
{
	uint16_t offset = 0;
	
	MailInfo *mail = &c->mail;
	memset(mail, 0, sizeof(*mail));
	
	mail->serial_id = read_u32(data + offset);
	offset += 4;
	
	read_u8(data + offset); // b
	offset += 1;
	
	mail->send_time = read_u64(data + offset);
	offset += 8;
	
	mail->mail_type = read_u8(data + offset);
	offset += 1;
	
	mail->reply_id = read_u32(data + offset);
	offset += 4;
	
	mail->sender_head = read_u16(data + offset);
	offset += 2;
	
	mail->sender_kingdom = read_u16(data + offset);
	offset += 2;
	
	memcpy(mail->sender_tag, data + offset, 3);
	mail->sender_tag[3] = '\0';
	offset += 3;
	
	memcpy(mail->sender_name, data + offset, 13);
	mail->sender_name[13] = '\0';
	offset += 13;
	
	mail->extra_flag = read_u8(data + offset);
	offset += 1;
	
	uint8_t title_len = read_u8(data + offset);
	offset += 1;
	
	uint16_t content_len = read_u16(data + offset);
	offset += 2;
	
	mail->attachment_count = read_u8(data + offset);
	offset += 1;
	
	for (int i = 0; i < mail->attachment_count; i++) {
		offset += 5; // KingdomID + ZoneID + PointID
	}
	
	memcpy(mail->title, data + offset, title_len);
	offset += title_len;
	
	memcpy(mail->content, data + offset, content_len);
	
	/*
	printf("\n");
	printf("Sender: [%s] %s\n", mail->sender_tag, mail->sender_name);
	
	printf("Kingdom: %u\n", mail->sender_kingdom);
	
	printf("Title: %s\n", mail->title);
	
	printf("Content: %s\n", mail->content);
	*/
	
	command_handler(c, mail->sender_name, mail->content);
}

