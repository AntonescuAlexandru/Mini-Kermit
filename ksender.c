#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000


int main(int argc, char** argv) {

    // variabile necesare
    msg t;
    msg* r;
    kPackage packet;
    char* buff = calloc(251, sizeof(char));
    int ok = 1;
    int timeoutCounter = 0;
    int dataSize;
    FILE* f;

    init(HOST, PORT);

    // se initializeaza si se trimite pachetul send_init
    initPackage(&packet);
    makeMsg(&t, &packet);
    send_message(&t);

    printPackage(&packet);
    printMsg(&t);

    // bucla pentru trimiterea si confirmarea primirii pachetului send_init
    while (ok)
    {
        // se asteapta si se capteaza raspunsul de la receiver
        r = receive_message_timeout(5000);
        if (r == NULL) {

            // timeout
            printf("timeout in sender(send_init)\n");
            timeoutCounter++;
            (timeoutCounter == 3) ? ok = 0 : 1;

        } else{

            timeoutCounter = 0;
            if(r->payload[3] == NAK){

                // s-a primit nak
                printf("send_init corupted(sender)\n");
                send_message(&t);

            } else{

                // pachetul send_init a fost trimis cu succes
                printf("succes in sender(send_init)\n");
                ok = 0;

            }

        }
    }

    /* in cazul s-a realizat timeout de ori pentru pachetul
     * send_init se inchide programul*/
    if (timeoutCounter == 3){

        printf("transaction ended after send_init\n");
        return -1;

    }

    packet.seq = 0;

    /* pentru fiecare argument din argv[] creez un mesaj
     * de tip file_header si il trimit, dupa aceea astept
     * raspuns pentru fiecare pachet trimis si trimit
     * urmatorul pachet*/
    for (int i = 1; i < argc; i++) {

        // se actualizeaza informatiile din packet pentru file_header
        f = fopen(argv[i], "rb");
        packet.len = strlen(argv[i]) + 5;
        packet.seq += 2;
        packet.seq %=64;
        packet.type = FILE_HEADER;
        packet.data = (unsigned char *)realloc(packet.data, strlen(argv[i]));
        strcpy(packet.data, argv[i]);

        // se trimite pachetul de tip file_header
        makeMsg(&t, &packet);
        send_message(&t);

        printf("File header msg sent: \n");
        printMsg(&t);

        timeoutCounter = 0;
        ok = 1;

        while (ok) {

            // se asteapta si se capteaza raspunsul de la receiver
            r = receive_message_timeout(5000);

            if(r == NULL){

                // timeout (nu s-a primit raspuns de la receiver)
                printf("timeout in sender\n");
                printf("\n");
                timeoutCounter++;

                // daca s-a realizat de 3 ori timeout, se inchide programul
                if(timeoutCounter == 3){
                    printf("end of transaction sender (timeout error)\n");
                    return -1;
                }

                // in caz de timeout se retrimite mesajul
                send_message(&t);

            } else{

                timeoutCounter = 0;

                /*daca s-a primit raspuns se verifica numarul lui de secventa,
                 * iar in cazul in care nu este corespunzator se ignora*/
                if(packet.seq == ((unsigned char)r->payload[2] - 1)) {

                    if(r->payload[3] == NAK){

                        /* s-a primit nak si se retrimite mesajul
                         * cu numar de secventa actualizat*/
                        packet.seq +=2;
                        packet.seq %=64;
                        t.payload[2] = packet.seq;
                        makeMsg(&t, &packet);
                        send_message(&t);

                        printf("corupted (sender)\n");
                        printMsg(&t);

                    } else{

                        /* daca pachetul precedent trimis a fost
                         * de tip file_header, se citeste din fisier
                         * si se trimite mesaj de tip data*/
                        if(packet.type == FILE_HEADER){

                            dataSize = fread(buff, sizeof(char), 250, f);

                            if(dataSize <= 0 || dataSize > 250){
                                ok = 0;
                            } else{

                                packet.len = dataSize + 5;
                                packet.seq +=2;
                                packet.seq %=64;
                                packet.type = DATA;
                                packet.data = (unsigned char *)realloc(packet.data, dataSize);
                                memcpy(packet.data, buff, dataSize);
                                makeMsg(&t, &packet);
                                send_message(&t);

                                printf("data msg sent\n");
                                printMsg(&t);

                            }

                        } else {

                            /* daca pachetul precedent trimis a fost de tip
                             * data, se verifica daca a continut mai putin de 250
                             * de caractere( in acest caz de trimite mesaj
                             * de tip end_of_file) sau in caz contrar se
                             * efectueaza din nou citire si se trimite un
                             * mesaj de tip data*/
                            if (packet.type == DATA) {
                                if (dataSize < 250) {

                                    packet.len = 5;
                                    packet.seq += 2;
                                    packet.seq %= 64;
                                    packet.type = EOF;
                                    makeMsg(&t, &packet);
                                    ok = 0;
                                    send_message(&t);

                                    printf("eof msg sent\n");
                                    printMsg(&t);
                                    break;

                                } else {

                                    dataSize = fread(buff, sizeof(char), 250, f);
                                    if (dataSize <= 0 || dataSize > 250) {
                                        ok = 0;
                                    } else {

                                        packet.len = dataSize + 5;
                                        packet.seq += 2;
                                        packet.seq %= 64;
                                        packet.type = DATA;
                                        packet.data = (unsigned char *) realloc(packet.data, dataSize);
                                        memcpy(packet.data, buff, dataSize);
                                        makeMsg(&t, &packet);
                                        send_message(&t);

                                        printf("data msg sent\n");
                                        printMsg(&t);

                                    }
                                }
                            }
                        }
                    }
                }


            }

        }
        fclose(f);
    }



    // se creaza pachet de tip eot si se trimite
    packet.seq +=2;
    packet.seq %=64;
    packet.type = EOT;
    packet.len = 5;
    free(packet.data);
    makeMsg(&t, &packet);
    send_message(&t);

    printf("eot msg sender\n");
    printMsg(&t);

    ok = 1;
    timeoutCounter++;

    // se asteapta raspuns pentru mesajul de tip eot(la fel ca la send_init)
    while (ok)
    {
        r = receive_message_timeout(5000);
        if (r == NULL)
        {
            timeoutCounter++;

            (timeoutCounter == 3) ? ok = 0 : 1;
            send_message(&t);
        } else{
            timeoutCounter = 0;
            if(packet.seq == ((unsigned char)r->payload[2] - 1)) {
                if (r->payload[3] == NAK) {
                    packet.seq += 2;
                    packet.seq %= 64;
                    makeMsg(&t, &packet);
                    send_message(&t);
                } else {

                    printf("succes ending transaction\n");
                    ok = 0;
                    return -1;
                }
            }
        }
    }

    if (timeoutCounter == 3){
        return -1;
    }


    
    return -1;
}
