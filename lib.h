#ifndef LIB
#define LIB

#define SEND_INIT 'S'
#define FILE_HEADER 'F'
#define DATA 'D'
#define EOF 'Z'
#define EOT 'B'
#define ACK 'Y'
#define NAK 'N'


typedef struct {
    int len;
    char payload[1400];
} msg;


// structura unui pachet kermit
typedef struct __attribute__ (( packed )) {
    unsigned char soh;
    unsigned char len;
    unsigned char seq;
    unsigned char type;
    unsigned char* data;
    unsigned short check;
    unsigned char mark;

} kPackage;


// functie de initializare a pachetului de tip send_init
void initPackage(kPackage* pack) {
    pack->type = SEND_INIT;
    pack->soh = 1;
    pack->len = 16;
    pack->seq = 0;
    pack->mark = 13;
    pack->data = (unsigned char *) calloc(11, sizeof(unsigned char));
    pack->data[0] = 250;
    pack->data[1] = 5;
    pack->data[2] = 0;
    pack->data[3] = 0;
    pack->data[4] = 13;
    pack->data[5] = 0;
    pack->data[6] = 0;
    pack->data[7] = 0;
    pack->data[8] = 0;
    pack->data[9] = 0;
    pack->data[10] = 0;
}


// functie de afisare a continutului unei variabile de tip kPackage
void printPackage(kPackage* pack) {
    printf("     ");
    int n = pack->len;
    n += 2;
    printf("%d  ", (unsigned char)pack->soh);
    printf("%d  ", (unsigned char)pack->len);
    printf("%d  ", (unsigned char)pack->seq);
    printf("%d  ", (unsigned char)pack->type);
    for (int i = 0; i < pack->len - 5; ++i) {
        printf("%d  ", (unsigned char)pack->data[i]);
    }
    printf("%d  ", (int)pack->check);
    printf("%d\n", (unsigned char)pack->mark);
}


// functie de afisare a continutului unui mesaj (fara campul  de date)
void printMsg(msg* m) {
    printf("%d   ", m->len);

    printf("len: %d  seq: %d type: %c check: %d %d \n",
           (unsigned char) m->payload[1],
           (unsigned char) m->payload[2],
           (unsigned char) m->payload[3],
           (unsigned char) m->payload[m->len - 3],
           (unsigned char) m->payload[m->len -2]);
    printf("\n");
}



void init(char* remote, int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);
msg* receive_message_timeout(int timeout); //timeout in milliseconds
unsigned short crc16_ccitt(const void *buf, int len);


/* functie de compunere a unui mesaj a unui mesaj,
 * se transfera datele dintr-o variabila de tip kPackage
 * in mesajul care urmeaza sa fie trimis*/
void makeMsg(msg* m, kPackage* pack) {
    memcpy(m->payload, pack, 4);
    memcpy(m->payload + 4, pack->data, pack->len - 5);

    pack->check = crc16_ccitt(m->payload, pack->len -1);

    memcpy(m->payload + pack->len -1, &pack->check, 3);
    m->len = pack->len + 2;
}

#endif

