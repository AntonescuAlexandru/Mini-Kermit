#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

int main(int argc, char** argv) {

    // variabile necesare
    msg r, t, ack, nak, last;
    msg *rr;
    int ok = 1;
    int timeoutCounter = 0;
    unsigned short crc;
    int out;
    FILE* f;


    init(HOST, PORT);

    // initializez 2 mesaje, unul de tip ack si celalalt nak
    ack.len = 7;
    ack.payload[0] = 1;
    ack.payload[1] = 5;
    ack.payload[2] = 1;
    ack.payload[3] = ACK;
    ack.payload[4] = 0;
    ack.payload[5] = 0;
    ack.payload[6] = 13;

    ack.len = 7;
    nak.payload[0] = 1;
    nak.payload[1] = 5;
    nak.payload[2] = 1;
    nak.payload[3] = NAK;
    nak.payload[4] = 0;
    nak.payload[5] = 0;
    nak.payload[6] = 13;

    memcpy(&last, &ack, sizeof(msg));

    // bucla pentru primirea si confirmarea trimiterii mesajului send_init
    while (ok) {

        rr = receive_message_timeout(5000);

        if (rr == NULL)
        {
            // daca s-a realizat timeout pentru send_init
            printf("timeout err in receiver(send_init)\n");
            timeoutCounter++;
            (timeoutCounter == 3) ? ok = 0 : 1;

        } else{

            timeoutCounter = 0;
            crc = crc16_ccitt(rr->payload, rr->len - 3);

            if (memcmp(&crc, rr->payload + rr->len - 3, 2)) {

                // daca mesajul send_init e corupt
                printf("corupted receiver(send_init\n)");
                rr->payload[3] = NAK;
                rr->payload[2] = 1;
                send_message(rr);
                printMsg(rr);

            } else {
                if(rr->payload[3] == SEND_INIT) {

                    /* daca s-a primit mesajul send_init si e corect,
                     * se trimite ack*/
                    printf("succes receiver(send_init)\n");
                    rr->payload[3] = ACK;
                    rr->payload[2] = 1;
                    printMsg(rr);
                    send_message(rr);
                    ok = 0;

                }
            }

        }
    }

    // daca s-a realizat timeout de 3 ori se inchide programul
    if (timeoutCounter == 3){

        printf("end transaction in receiver after send_init\n");
        return -1;
    }

    timeoutCounter = 0;
    ok = 1;
    ack.payload[2] = 1;
    last.payload[2] = 1;

    while (ok){

        // se asteapta si se capteaza raspunsul de la sender
        rr = receive_message_timeout(5000);

        if (rr == NULL)
        {
            // timeout
            printf("timeout error in receiver\n");

            timeoutCounter++;

            // daca s-a realizat timeout de 3 ori se inchide programul
            if(timeoutCounter == 3){
                printf("end of transaction receiver (timeout error)\n");
                return -1;
            }

            //in caz de timeout se retrimite ultimul mesaj
            send_message(&last);

            printf("sent last ack/nak\n");
            printMsg(&last);

        } else{

            timeoutCounter = 0;
            crc = crc16_ccitt(rr->payload, rr->len - 3);

            if (memcmp(&crc, rr->payload + rr->len - 3, 2)) {

                // daca mesajul e corupt de trimite nak
                printf("corupted (receiver)\n");

                nak.payload[2] = (last.payload[2] + 2) % 64;
                last.payload[2] = nak.payload[2];
                last.payload[3] = NAK;
                send_message(&nak);

                printf("nak sent\n");
                printMsg(&nak);

            } else{

                /* se verifica numarul de secventa,
                 * daca nu este corespunzator se ignora mesajul*/
                if(((last.payload[2] + 1) % 64) == (unsigned char)rr->payload[2]){
                    if(rr->payload[3] == FILE_HEADER){

                        /* daca s-a primit file_header se deschide fisierul
                         * pentru scriere si se trimite ack*/

                        printf("received file header\n");
                        printMsg(rr);

                        char prefix[100] = "recv_";
                        memcpy(prefix + 5, rr->payload + 4, rr->payload[1] - 5);
                        f = fopen(prefix, "wb");
                        ack.payload[2] = (last.payload[2] + 2) % 64;
                        last.payload[2] = ack.payload[2];
                        last.payload[3] = ACK;
                        send_message(&ack);

                        printf("ack sent\n");
                        printMsg(&ack);

                    } else if(rr->payload[3] == DATA){

                        /* daca s-a primit mesaj de tip data,
                         * se scrie in fisier si se trimite ack*/

                        printf("data received\n");
                        printMsg(rr);

                        fwrite(rr->payload + 4, sizeof(char), (unsigned char)rr->payload[1] - 5, f);

                        ack.payload[2] = (last.payload[2] + 2) % 64;
                        last.payload[2] = ack.payload[2];
                        last.payload[3] = ACK;
                        send_message(&ack);

                        printf("ack sent\n");
                        printMsg(&ack);

                    }
                    else if(rr->payload[3] == EOF){

                        /*daca s-a primit mesaj de tip end_of_file
                         * se inchide fisierul si se trimite ack*/

                        printf("eof received\n");
                        printMsg(rr);

                        fclose(f);
                        //close(out);
                        ack.payload[2] = (last.payload[2] + 2) % 64;
                        last.payload[2] = ack.payload[2];
                        last.payload[3] = ACK;
                        send_message(&ack);

                        printf("ack sent\n");
                        printMsg(&ack);

                    }
                    else if(rr->payload[3] == EOT){

                        /*daca s-a mesaj de tip end_of_transaction
                         * se trimite ack si se inchide programul*/

                        printf("eot received\n");
                        printMsg(rr);

                        ack.payload[2] = (last.payload[2] + 2) % 64;
                        last.payload[2] = ack.payload[2];
                        last.payload[3] = ACK;
                        send_message(&ack);

                        printf("ack sent\n");
                        printMsg(&ack);


                        return -1;
                    }
                }
            }
        }
    }

    return 0;
}
