/*
    Biblioteka Coap na potrzeby przedmiotu OBIR
    Zawiera definicje naglowka wiadomosci oraz samej wiadomosci
*/
#pragma once
#include <stdint.h>

#define PACKET_SIZE 60                                                         //Maksymalny rozmiar pakietu UDP

class CoapHeader{                                                              //Klasa reprezentujaca naglowek wiadomosci COAP
  public:
    uint8_t ver;                                                               // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    uint8_t type;                                                              //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    uint8_t tokenLen;                                                          //|Ver| T |  TKL  |  Class/Code  |           Message ID           |
    uint8_t coapClass;                                                         //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    uint8_t coapCode;
    uint16_t mid;

CoapHeader(){}                                                                 //Pusty konstruktor domyślny
CoapHeader(                                                                    //Konstruktor klasy naglowka
        uint8_t ver, 
        uint8_t type, 
        uint8_t tokenLen, 
        uint8_t coapClass, 
        uint8_t coapCode, 
        uint16_t mid)
    {
        this->ver = ver;
        this->type = type;
        this->tokenLen = tokenLen;
        this->coapClass = coapClass;
        this->coapCode = coapCode;
        this->mid = mid;
    }
};

class CoapMessage{                                                             //Klasa reprezentujaca cala wiadomosc Coap (wraz z naglowkiem, opcjami i zawartoscia)

    CoapHeader header;                                                         //Naglowek wiadomosci
    uint8_t packet[PACKET_SIZE];                                               //Cala wiadomosc Coap o rozmiarze 60B (wraz z naglowkiem, opcjami i zawartoscia)
    unsigned int actualSize = 0;                                               //Rzeczywisty rozmiar wiadomosci Coap (wraz z naglowkiem, opcjami i zawartoscia)

    public:
    CoapMessage(CoapHeader& h, uint8_t* token)                                 //Konstruktor dla wiadomosci Coap. Wpisuje naglowek oraz token.
    {
        header = h;
        packet[0] = (1 << 6) | (header.type << 4) | (header.tokenLen);         //Ustawianie (1)wersji , (header.type)typu wiadomosci i (header.tokenLen) dlugosci tokenu
        packet[1] = (header.coapClass << 5) | (header.coapCode);               //Ustawianie (header.coapClass)klasy wiadomosci i (header.coapCode)kodu wiadomosci, np. 0.1 -> request GET
        packet[2] = (0xFF00 & header.mid) >> 8;                                //Ustawianie (header.mid)MessageID na drugim bajcie.
        packet[3] = (0x00FF) & header.mid;                                     //Ustawianie (header.mid)MessageID na pierwszym bajcie.

        actualSize = 4;                                                        //Ustalenie wielkosci pakietu po zapelnieniu naglowka (4bajty)

        for (int i = 0; i < header.tokenLen; i++)
            packet[actualSize + i] = token[i];                                 //Ustawienie tokena o dlugosci podanej w naglowku TKL
        actualSize += header.tokenLen;                                         //Rozszerzenie rozmiaru wiadomosci o dl. tokena
    }

    void SetContentFormat(uint8_t cf)                                          //Funkcja wpisujaca opcje Content-Format do wiadomosci (zakladamy tylko 1. opcje)
    {
        packet[actualSize++] = 0b11000001; //(0xF0 & 12) << 4 | (0x0F & 1);    //Ustawienie Delta = 12 oraz OptionLen = 1 zapisane w kodzie bin. na jednym bajcie
        packet[actualSize++] = (0xFF & cf);                                    //Ustawienie wartosci opcji Content-Format
    }

    void SetPayload(uint8_t* payload, uint8_t payloadLen)                      //Funkcja wpisujaca tablice bajtow o podanej dlugosci jako payload wiadomosci Coap
    {
        packet[actualSize++] = 0xFF;                                           //Wstawianie Markera payloadu (11111111)

        for (int i = 0; i < payloadLen; i++)
        {
            packet[actualSize + i] = payload[i];                               //Wpisywanie podanej tablicy bajtow jako payload wiadomosci
        }
        actualSize += payloadLen;                                              //Rozszerzenie rozmiaru wiadomosci o dl. payloadu
        packet[actualSize] = '\0';                                             //Wstawienie znaku konca wiadomosci ('\0')
    }

    void SetPayload(String message)                                            //Funkcja wpisujaca string jako payload wiadomosci
    {
        uint8_t payloadContent[PACKET_SIZE - actualSize + 1];                  //Tworzenie tablice bajtow o rozmiarze pozostalym do wypelnienia przez payload  (60B - rozm reszty + 1)
        message.toCharArray(payloadContent, message.length() + 1);             //Mapowanie stringa na tablice bajtow o rozmiarze stringa
        SetPayload(payloadContent, message.length());                          //Wywolanie funkcji SetPayload
    }

    void Send(ObirEthernetUDP& udp)                                            //Funkcja wysylajaca cala wiadomosc Coap za pomoca obiektu ObirEthernetUDP
    {
        udp.beginPacket(udp.remoteIP(), udp.remotePort());                     //Poczatek tworzenia ramki UDP
        udp.write(packet, actualSize);                                         //Wpisanie do ramki wiadomosci Coap
        udp.endPacket();                                                       //Koniec tworzenia ramki UDP - wyslanie wiadomosci
        actualSize = 0;                                                        //Reset rozmiaru wiadomosci Coap
    }

    int GetPacketLen()                                                         //Funkcja zwracajaca aktualny rozmiar wiadomosci Coap (wraz z naglowkiem, opcjami i zawartoscia)
    {
        return actualSize;
    }
};
