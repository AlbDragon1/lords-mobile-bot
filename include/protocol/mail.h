#ifndef MAIL_H
#define MAIL_H

#include <stdint.h>

struct Connection;

typedef struct {
    uint32_t serial_id;
    uint64_t send_time;
    uint8_t mail_type;
    uint32_t reply_id;
    uint16_t sender_head;
    uint16_t sender_kingdom;
    char sender_tag[4];
    char sender_name[14];
    uint8_t extra_flag;
    char title[256];
    char content[4096];
    uint8_t attachment_count;
} MailInfo;

void RequestSendMail(struct Connection*, const char*, const char*, const char*);
void RequestSendMailFmt(struct Connection*, const char*, const char*, const char*, ...);
void RecvMailInfo(struct Connection*, const uint8_t*);

#endif