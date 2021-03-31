#include <ObirDhcp.h>                                                                          //Bilioteka ktora umozliwia pobranie IP z DHCP - platforma dla ebsim'a
#include <ObirEthernet.h>                                                                      //Biblioteka niezbedna dla klasy 'ObirEthernetUDP'
#include <ObirEthernetUdp.h>                                                                   //Biblioteka z klasa 'ObirEthernetUDP' do nawiazania polaczenia sieciowego

#include "coap.h"                                                                              //Biblioteka Coap zawierajaca definicje wiadomosci, naglowka i metod pomocniczych.
#include "resources.h"                                                                         //Biblioteka zasobow zawierajaca udostepniane przez serwer metryki

#define UDP_SERVER_PORT 1234                                                                   //Port na ktorym nadaje serwer
#define PACKET_SIZE 60                                                                         //Dlugosc ramki z danymi
#define URIPATH_MAX_SIZE 255                                                                   //Maksymalna ilosc znakow w URI-Path


byte MAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};                                             //MAC adres karty sieciowej - proforma dla ebsim'a

uint8_t packetBuffer[PACKET_SIZE];                                                             //Tablica bajtow do ktorej bedzie zapisywana przychodzaca wiadomosc o dlugosci pakietu
uint16_t serverMid = 0x000A;                                                                   //MessageID serwera, zwiekszana z kazda wyslana wiadomoscia

Resources resources(4);                                                                        //Obiekt zasobow, inicjowany z wartoscia 6 -> Tworzy graf o 6. wezlach
ObirEthernetUDP Udp;                                                                           //Obiekt klasy 'ObirEthernetUDP' do transmisji pakietow UDP

void PrintHeader(CoapHeader &header)                                                           //Funkcja wypisujaca naglowek obslugiwanej wiadomosci
{
    Serial.println(F("----HEADER----"));
    Serial.print(F("Ver: "));
    Serial.println(header.ver, DEC);
    Serial.print(F("Type: "));
    Serial.println(header.type, DEC);
    Serial.print(F("Token Length: "));
    Serial.println(header.tokenLen, DEC);
    Serial.print(F("Code: "));
    Serial.print(header.coapClass, DEC);
    Serial.print(F("."));
    Serial.println(header.coapCode, DEC);
    Serial.print(F("MessageID: "));
    Serial.println(header.mid, DEC);
    Serial.println(F("--------------"));
}

void setup()                                                                                   //Funkcja setup() arduinoIDE inicjujaca program
{
    Serial.begin(115200);                                                                      //Przywitanie z uzytkownikiem 
    Serial.print(F("OBIR PROJEKT INIT..."));                                                   //Potwierdzajace poprawne uruchomienie sie systemu
    Serial.print(F(__FILE__));
    Serial.print(F(", "));
    Serial.print(F(__DATE__));
    Serial.print(F(", "));
    Serial.print(F(__TIME__));
    Serial.println(F("]"));

    ObirEthernet.begin(MAC);                                                                   //Inicjacja karty sieciowej - preforma dla ebsim'a

    Serial.print(F("My IP address: "));                                                        //Wyswietlenie informacji potwierdzajacej na jakim IP dzialamy
    for (byte thisByte = 0; thisByte < 4; thisByte++)
    {
        Serial.print(ObirEthernet.localIP()[thisByte], DEC);
        Serial.print(F("."));
    }
    Serial.println();

    Udp.begin(UDP_SERVER_PORT);                                                                //Uruchomienie nasluchiwania na datagaramy UDP
}

void loop()                                                                                    //Funkcja loop() arduinoIDE - glowna petla programu
{
/////////////////////////////////////////ODCZYT NAGLOWKA///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    int packetSize = Udp.parsePacket();                                                        //Zapis rozmiaru otrzymanego pakietu do zmiennej "packetSize"
    if (packetSize > 0)                                                                        //Instrkcja warunkowa sprawdzajaca jaka jest dligosc pakietu, jesli jest <= 0 to nic nie otrzymalismy
    {
        int len = Udp.read(packetBuffer, PACKET_SIZE);                                         //Odczytujemy pakiet(maksymalnie do liczby bajtów = PACKET_SIZE)

        serverMid += 1;                                                                        //Zwiekszamy MessegeID serwera, w celu unikniecia duplikowania sie wiadomosci

        resources.Received(len);                                                               //Aktualizujaca stanu metryki "receviedBytes" o ilosc otrzymanych bajtow

        Serial.println(F("----------------MESSAGE--------------"));

        CoapHeader header(                                                                     //Obiekt klasy CoapHeader przechowujacy dane naglowka odebranej wiadomosci 
        ((0xC0 & packetBuffer[0]) >> 6),                                                       //Odczyt pola wersji naglowka Coap: Pierwsze dwa bity pierwszego bajtu odebranej wiadomosci
        ((0x30 & packetBuffer[0]) >> 4),                                                       //Odczyt pola typu naglowka Coap: kolejne dwa bity pierwszego bajtu odebranej wiadomosci (1=NON, 0=CON)
        ((0x0F & packetBuffer[0]) >> 0),                                                       //Odczyt pola dlugosci tokena naglowka Coap: Ostatnie cztery bity pierwszego bajtu odebranej wiadomosci
        ((packetBuffer[1] >> 5) & 0x07),                                                       //Odczyt pola Code (class) naglowka Coap: Pierwsze trzy bity drugiego bajtu odebranej wiadomosci
        ((packetBuffer[1] >> 0) & 0x1F),                                                       //Odczyt pola Code (detail) naglowka Coap: Ostatnie piec bitow drugiego bajtu odebranej wiadomosci
        ((packetBuffer[2] << 8) | (packetBuffer[3])));                                         //Odczyt pola MessageID naglowka Coap: Dwa kolejne bajty odebranej wiadomosci
        
        uint8_t token[header.tokenLen];                                                        //Tablica bajtow o dlugosci tkl reprezentujacych token odebranej wiadomosci

        PrintHeader(header);                                                                   //Wywolanie funkcji wypisujacej wartosci naglowka wiadomosci

        if (header.tokenLen > 0)                                                               //Instrkcja warunkowa sprawdzajaca jaka jest dligosc tokena
            for (int i = 0; i < header.tokenLen; ++i)
                token[i] = packetBuffer[i + 4];                                                //Odczyt pola token Coap: od 4 bajtu do TKL

/////////////////////////////////////////ODCZYT OPCJI/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        int marker = 4 + header.tokenLen;                                                      //Znacznik wskazujacy aktualne polozenie kursora w tablicy bajtow wiadomosci (4B + TKL) = Poczatek opcji
        int payloadMarker = -1;                                                                //Znacznik polozenia payloadu w bajtach (-1 for error checking)

        int32_t delta;                                                                         //Delta opcji = (numer aktualnej opcji - numer poprzedniej opcji)
        uint8_t optionLen;                                                                     //Dlugosc opcji
        uint8_t optionNum = 0;                                                                 //Numer aktualnej opcji

        uint16_t contentFormat = 0xFFFF;                                                       //Format wiadomosci - domyslna wartosc do sprawdzenia czy opcja wystapila
        uint16_t acceptFormat = 0xFFFF;                                                        //Akceptowany format wiadomosci - domyslna wartosc do sprawdzenia czy opcja wystapila

        uint8_t _uriPath[URIPATH_MAX_SIZE];                                                    //Tablica bajtow przechowujaca Uri_Path
        String uriPath = "";                                                                   //Znakowa reprezentacja zmiennej _uriPath

        Serial.println(F("Options: "));

        while (packetBuffer[marker] != '\0')                                                   //Petla iterujaca dopoki nie skonczymy odczytu pakietu
        {
            delta = (packetBuffer[marker] & 0xF0) >> 4;                                        //Odczyt delty: Cztery pierwsze bity pierwszego bajtu opcji
            optionLen = (packetBuffer[marker++] & 0x0F);                                       //Odczyt dlugosci opcji: Cztery ostatnie bity pierwszego bajtu opcji //Przesuwamy kursor na nastepny bajt

                                                                                               //Dla delty albo optionLength < 12, brak rozszerzen (D=d, e0 missing, e1 missing):Wyk3-4, str.40
                                                                                               //Bajt markera oznacza wtedy wartosc opcji
            
            if (delta == 13)                                                                   //Delta == 13 -> jest 1 bajt ext.
                delta += packetBuffer[marker++];                                               //Do delty dodajemy wartosc rozszerzenia (D=13+e0, e1 missing):Wyk3-4, str.40

            else if (delta == 14)                                                              //Delta == 14 -> sa 2 bajty ext.
            {
                delta = 269 + 256 * packetBuffer[marker++];                                    //Pierwszy bajt ma wieksza wage
                delta += packetBuffer[marker++];                                               //Dodajemy wartosc kolejnego bajtu (D=269+e0*256+e1):Wyk3-4, str.40
            }

            else if (delta == 15)                                                              //Delta == 15 -> PayloadMarker 0xFF
                payloadMarker = marker;                                                        //Zapamietanie pozycji markera payload'u

            if (optionLen == 13)                                                               //OptionLen == 13 -> jest 1 bajt ext.
                optionLen += packetBuffer[marker++];                                           //Analogicznie do odczytu delty

            else if (optionLen == 14)                                                          //OptionLen == 14 -> sa 2 bajty ext.
            {
              optionLen = 269 + 256 * packetBuffer[marker++];                                  //Analogicznie do odczytu delty
              optionLen += packetBuffer[marker++];                                             //
            }

            optionNum += delta;                                                                //Obliczmy aktualna opcje (OptionNum = actualOption + Delta)

                                                                                               //Marker powinien w tym momencie wskazywac na zawartosc opcji, tzn optionValue.

/////////////////////////////////////////OBSLUGA OPCJI/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

            switch (optionNum)                                                                 //Switch po aktualnej opcji
            {
            case 11:
                Serial.println(F("Option 11: 'URI-Path'"));                                    //Option = 11 -> URI-Path
                uriPath += "/";                                                                //String do reprezentacji Uri-Path
                for (int i = 0; i < optionLen; ++i)
                {
                  _uriPath[i] = packetBuffer[marker++];                                        //Zczytujemy znak po znaku Uri-Path z wartosci opcji do tablicy bajtow
                  uriPath += (char)_uriPath[i];                                                //Wpisujemy do znakowej reprezentacji sciezki wartosc odczytanej opcji
                }
                Serial.println(uriPath);                                                       //Wyswietlenie pelnej zczytanej sciezki
                break;
            case 12:
                Serial.println("Option: Content-Format");                                      //Option = 12 -> Content-Format
                if (optionLen == 0)
                    contentFormat = 0;                                                         //Gdy wartość opcji 0, ustawiamy domyslny, tj. 0(text/plain)
                if (optionLen == 1)                                                            //Dla dlugosci opcji 1 bajt ustawiamy ten bajt
                    contentFormat = packetBuffer[marker++];                                    //Zczytujemy z bajtu wartosci opcji do zmiennej reprezentujacej content-format
                if (optionLen == 2)                                                            //Gdy dlugosc opcji 2, CF znajduje sie na dwoch bajtach
                {
                    contentFormat = packetBuffer[marker++];                                    //Zczytujemy pierwszy bajt opcji
                    contentFormat << 8;                                                        //Przesuwamy zapisana wartosc na bardziej znaczacy bajt
                    contentFormat = contentFormat | packetBuffer[marker++];                    //Zczytujemy drugi bajt opcji dokladajac go do zmiennej contentFormat
                }
                if (contentFormat == 0)                                                        //Gdy wartosc opcji 0, wyswietlamy (text/plain)
                    Serial.println(F("plain text"));
                else if (contentFormat == 40)                                                  //Gdy wartosc opcji 40, wyswietlamy (application/link-format)
                    Serial.println(F("application/link-format"));
                else{                                                                          //Innych formatow nie obslugujemy, nie wprowadzamy zmian w contentFormat
                    Serial.println(F("Given format not supported"));
                    contentFormat = 0xFFFF;
                }
                //reszta formatow raczej nas nie obchodzi
                break;
            case 17:                                                                           //Option = 17 -> Accept
                Serial.println("Option: Accept ");
                if (optionLen == 0)
                    acceptFormat = 0;                                                          //Gdy wartość opcji 0, ustawiamy domyslny, tj. 0(text/plain)
                if (optionLen == 1)                                                            //Dla dlugosci opcji 1 bajt ustawiamy ten bajt
                    acceptFormat = packetBuffer[marker++];                                     //Zczytujemy z bajtu wartosci opcji do zmiennej reprezentujacej acept-format
                if (optionLen == 2)                                                            //Gdy dlugosc opcji 2, AF znajduje sie na dwoch bajtach
                {
                    acceptFormat = packetBuffer[marker++];                                     //Zczytujemy pierwszy bajt opcji
                    acceptFormat << 8;                                                         //Przesuwamy zapisana wartosc na bardziej znaczacy bajt
                    acceptFormat = acceptFormat | packetBuffer[marker++];                      //Zczytujemy drugi bajt opcji dokladajac go do zmiennej acceptFormat
                }
                if (acceptFormat == 0)                                                         //Gdy wartosc opcji 0, wyswietlamy (text/plain)
                    Serial.println(F("plain text"));
                else if (acceptFormat == 40)                                                   //Gdy wartosc opcji 40, wyswietlamy (application/link-format)
                    Serial.println(F("application/link-format"));
                else{                                                                          //Innych formatow nie obslugujemy, nie wprowadzamy zmian w acceptFormat
                    Serial.println(F("Given format not supported"));
                    acceptFormat = 0xFFFF;
                }
                break;
            }
        }

/////////////////////////////////////////ODCZYT PAYLOADU//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

       uint8_t payloadLen = 0;                                                                 //Zmienna przechowujaca dlugosc samego payloadu w odebranej wiadomosci

        if (payloadMarker > 0)                                                                 //Warunek sprawdzenia czy wgl wiadomosc posiada payload (payloadMarker = -1 dla braku payloadu)
            payloadLen = len - payloadMarker;                                                  //Jesli tak, to ustalamy jego dlugosc
        else 
            payloadLen = 0;                                                                    //W przeciwnym wypadku payloadu nie ma

       uint8_t payload[payloadLen];                                                            //Tablica bajtow przechowujaca zawartosc payloadu z odebranej wiadomosci

        Serial.println("\n-------PAYLOAD---------");
        if (payloadMarker > 0)                                                                 //Warunek sprawdzenia czy wgl wiadomosc posiada payload (payloadMarker = -1 dla braku payloadu)
        {
            Serial.print(F("Payload: "));
            for (int i = 0; i < payloadLen; i++)                                               //Iterujemy po calym payloadzie
            {
                payload[i] = packetBuffer[payloadMarker + i];                                  //Zapelniamy lokalny buffer payloadu zawartoscia payloadu odebranej wiadomosci
                Serial.print((char)payload[i]);                                                //Wyswietlamy zawartosc znak po znaku
            }
            Serial.println();
        }
        else                                                                                   //W innym wypadku wiadomosc nie posiada payloadu
            Serial.println("No payload found");

////////////////////////////////////////REAKCJA NA WIADOMOSC//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        if(header.type == 1)                                                                   //Sprawdzamy typ nadchodzacej wiadomosci 1  ---> NON
        {
            if(header.coapClass == 0)                                                          //Sprawdzamy klase kodu nadchodzacej wiadomosci 0 ---> Request
            {
                if(header.coapCode == 1)                                                       //Sprawdzamy kod nadchodzacej wiadomosci 1 ---> GET
                {
                    if (uriPath == "/.well-known/core")                                        //Sprawdzamy czy opcja Uri-Path wynosi /.well-known/core, wtedy powinno nastapic odkrycie pozostalych zasobow serwowanych przez serwer
                    {
                        CoapHeader h(1, 1, header.tokenLen, 2, 5, serverMid);                  //Obiekt naglowka z odpowiadajacym kodem 2.05 -> (Succes Content Get)
                        CoapMessage m(h, token);                                               //Obiek wiadomosci zawierajacy naglowek i odpowiadajacy token (ten sam co w zadaniu)
                        m.SetContentFormat(40);                                                //Ustawiamy opcje wiadomosci Content-Format na 40 (application/link-format)
                        String var = "</ReceivedB>;</SendB>;</TotalB>;</Graph>";               //Reprezentacja udostepniancyh metryk w formacie (application/link-format)
                        m.SetPayload(var);                                                     //Ustawiamy payload wiadomosci zdefiniowana wyzej reprezentacja metryk
                        resources.Send(m.GetPacketLen());                                      //Aktualizujaca stanu metryki "sendBytes" o ilosc wyslanych bajtow
                        m.Send(Udp);                                                           //Wyslanie wypelnionej wiadomosci jako odpowiedz serwera
                    }
                    else                                                                       //W innym wypadku Uri-Path albo nie istnieje albo wskazuje na inny zasob
                    {
                        String result = resources.GetResource(uriPath);                        //Reprezentacja metryki zwroconej przez funkcje GetReasurce() przyjmujacej Uri-Path jako argument
                        if (result != "")                                                      //Sprawdzamy czy zwrocna wartosc wskazuje na jakakolwiek reprezentacje metryki
                        {
                            CoapHeader h(1, 1, header.tokenLen, 2, 5, serverMid);              //Obiekt naglowka z odpowiadajacym kodem 2.05 -> (Succes Content Get)
                            CoapMessage m(h, token);                                           //Obiek wiadomosci zawierajacy naglowek i odpowiadajacy token (ten sam co w zadaniu)
                            if(acceptFormat != 0xFFFF)                                         //Sprawdzamy czy wystapila opcja Accept
                                m.SetContentFormat(0);                                         //Gdy tak, odsylamy payload w jedynym obslugiwanym formacie, tj. "txt/plain"(0), w przeciwnym wypadku nie zwracamy opcji content-Format
                            m.SetPayload(result);                                              //Ustawiamy payload wiadomosci zdefiniowana wyzej reprezentacja metryk
                            resources.Send(m.GetPacketLen());                                  //Aktualizujaca stanu metryki "sendBytes" o ilosc wyslanych bajtow
                            m.Send(Udp);                                                       //Wyslanie wypelnionej wiadomosci jako odpowiedz serwera
                        }
                        else                                                                   //Gdy result nie zwrocil zandej reprezentacji metryki, to oznacza ze pozadana sciezka nie istnieje i odsylamy kod bledu
                        {
                            CoapHeader h(1, 1, header.tokenLen, 4, 4, serverMid);              //Obiekt naglowka z odpowiadajacym kodem 4.04 -> (Client Error Response Not Found)
                            CoapMessage m(h, token);                                           //Obiek wiadomosci zawierajacy naglowek i odpowiadajacy token (ten sam co w zadaniu)
                            resources.Send(m.GetPacketLen());                                  //Aktualizujaca stanu metryki "sendBytes" o ilosc wyslanych bajtow
                            m.Send(Udp);                                                       //Wyslanie wypelnionej wiadomosci jako odpowiedz serwera
                        }
                    }
                }
                else if(header.coapCode == 3)                                                  //Sprawdzamy kod nadchodzacej wiadomosci 3 ---> PUT | Mozliwy format payloadu: 1,2; (1,2); 12; 1 2; 1;2; 1-2; 1x2; (TYLKO JEDEN NA RAZ!)
                {
                    //String result = resources.GetResource(uriPath);                            //Reprezentacja metryki zwroconej przez funkcje GetReasurce() przyjmujacej Uri-Path jako argument
                    if(uriPath == "/Graph")                                                    //Sprawdzamy czy zwrocna wartosc wskazuje na metryke korzystajacej z metody PUT (tylko Graph)
                    {
                        int from = -1, to = -1;                                                //Zmienna przechowujaca wartosc ktora ma zostac uzyta jako nowe polaczenie w grafie
                        bool firstDigit = false;                                               //Flaga prezentujaca czy odczytujemy pierwsza czy druga wartosc z payloadu wiadomosci
                        Serial.println(F("PUT"));
                        for (int i = 0; i < payloadLen; i++)                                   //Iterujemy po calym paylodzie
                        {
                            if(payload[i]>='0' && payload[i]<= '9')                            //Sprawdzamy czy ktoras z wartosci podanych w paylodzie jest cyfra z zakresu [0;9]
                            {
                                if(!firstDigit)                                                //Sprawdzamy czy odczytujemy pierwsza cyfre z payloadu 
                                {
                                    from = payload[i] - 48;                                    //Wpisujemy wartosc pierwszej cyfry jako bajt znaleziony w payloadzie - 48 (Konwersja ASCII to int)
                                    firstDigit = true;                                         //Po znalezieniu pierwszej cyfry ustawiamy wartosc flagi na prawde
                                }
                                else                                                           //Sprawdzamy czy odczytujemy druga cyfre z payloadu 
                                    to = payload[i] - 48;                                      //Wpisujemy wartosc drugiej cyfry jako bajt znaleziony w payloadzie - 48 (Konwersja ASCII to int)
                            }
                        }
                        bool succes = resources.graph->AddEdge(from, to);                      //Zmienna przechowujaca wynik operacji dodania nowej krawedzi do grafu (Prawda - udana/ Falsz - nieudana)
                        if(succes)                                                             //Gdy poprawnie dodano nowa krawedz
                        {
                            CoapHeader h(1, 1, header.tokenLen, 2, 1, serverMid);              //Obiekt naglowka z odpowiadajacym kodem 2.01 -> (Succes Created PUT)
                            CoapMessage m(h, token);                                           //Obiek wiadomosci zawierajacy naglowek i odpowiadajacy token (ten sam co w zadaniu)
                            m.SetPayload("Edge was added.");                                   //Ustawiamy payload wiadomosci zawierajacy komunikat o poprawnie wykonanej operacji
                            resources.Send(m.GetPacketLen());                                  //Aktualizujaca stanu metryki "sendBytes" o ilosc wyslanych bajtow
                            m.Send(Udp);                                                       //Wyslanie wypelnionej wiadomosci jako odpowiedz serwera
                        }
                        else                                                                   //Gdy nie udalo sie dodac nowej krawedzi
                        {
                            CoapHeader h(1, 1, header.tokenLen, 4, 0, serverMid);              //Obiekt naglowka z odpowiadajacym kodem 4.00 -> (Client Error Response Bad Request)
                            CoapMessage m(h, token);                                           //Obiek wiadomosci zawierajacy naglowek i odpowiadajacy token (ten sam co w zadaniu)
                            m.SetPayload("Can not add new Edge.");                             //Ustawiamy payload wiadomosci zawierajacy komunikat o bledzie
                            resources.Send(m.GetPacketLen());                                  //Aktualizujaca stanu metryki "sendBytes" o ilosc wyslanych bajtow
                            m.Send(Udp);                                                       //Wyslanie wypelnionej wiadomosci jako odpowiedz serwera
                        }
                    }
                    else                                                                       //Gdy wartosc Uri-Path wskazuje na obiekt nie obslugujacy metody PUT
                    {
                        CoapHeader h(1, 1, header.tokenLen, 4, 5, serverMid);                  //Obiekt naglowka z odpowiadajacym kodem 4.05 -> (Client Error Response - Method Not Allowed)
                        CoapMessage m(h, token);                                               //Obiek wiadomosci zawierajacy naglowek i odpowiadajacy token (ten sam co w zadaniu)
                        resources.Send(m.GetPacketLen());                                      //Aktualizujaca stanu metryki "sendBytes" o ilosc wyslanych bajtow
                        m.Send(Udp);                                                           //Wyslanie wypelnionej wiadomosci jako odpowiedz serwera
                    }
                }
                else                                                                           //Gdy metoda zadania jest inna niz GET lub PUT - Not implemented
                {
                    CoapHeader h(1, 1, header.tokenLen, 5, 1, serverMid);                      //Obiekt naglowka z odpowiadajacym kodem 5.01 -> (Server Error Response - Not Implemented)
                    CoapMessage m(h, token);                                                   //Obiek wiadomosci zawierajacy naglowek i odpowiadajacy token (ten sam co w zadaniu)
                    resources.Send(m.GetPacketLen());                                          //Aktualizujaca stanu metryki "sendBytes" o ilosc wyslanych bajtow
                    m.Send(Udp);                                                               //Wyslanie wypelnionej wiadomosci jako odpowiedz serwera
                }
            }
        }
        else if(header.type == 0)                                                              //Sprawdzamy typ nadchodzacej wiadomosci 0  ---> CON
        {
            if(header.coapClass == 0)                                                          //Sprawdzamy klase kodu nadchodzacej wiadomosci 0 ---> Request
            {
                if(header.coapCode == 1)                                                       //Sprawdzamy kod nadchodzacej wiadomosci 1 ---> GET
                {
                    if (uriPath == resources.totalStr)                                         //Sprawdzamy Uri-Path wskazuje na metryke o dlugim dostepie (tylko "bytesTotal")
                    {
                        CoapHeader h(1, 2, 0, 0, 0, header.mid);                               //Obiekt naglowka z odpowiadajacym kodem 2.00 -> (Succes - Empty) i  odpoiwdajacym Mid
                        CoapMessage m(h, NULL);                                                //Obiek wiadomosci zawierajacy naglowek
                        resources.Send(m.GetPacketLen());                                      //Aktualizujaca stanu metryki "sendBytes" o ilosc wyslanych bajtow
                        m.Send(Udp);                                                           //Wyslanie wypelnionej wiadomosci jako odpowiedz serwera

                        delay(10000);                                                          //Symulacja dlugiego destepu - sleep progrgamu (tylko symulacja, nie powinno byc w finalnym programie)

                        CoapHeader hres(1, 1, header.tokenLen, 2, 5, serverMid);               //Obiekt naglowka z odpowiadajacym kodem 2.05 -> (Succes Content Get)
                        CoapMessage mres(hres, token);                                         //Obiek wiadomosci zawierajacy naglowek i odpowiadajacy token (ten sam co w zadaniu)
                        mres.SetPayload(resources.GetResource(uriPath));                       //Ustawiamy payload wiadomosci reprezentacja metryki zworcony przez funkcje GetResource()
                        resources.Send(mres.GetPacketLen());                                   //Aktualizujaca stanu metryki "sendBytes" o ilosc wyslanych bajtow
                        mres.Send(Udp);                                                        //Wyslanie wypelnionej wiadomosci jako odpowiedz serwera
                    }
                    else                                                                       //Gdy Uri-Path nie wskazuje na metryke o dlugim dostepie
                    {
                        String result = resources.GetResource(uriPath);                        //Reprezentacja metryki zwroconej przez funkcje GetReasurce() przyjmujacej Uri-Path jako argument
                        if (result != "")                                                      //Sprawdzamy czy zwrocna wartosc wskazuje na jakakolwiek reprezentacje metryki
                        {
                            CoapHeader h(1, 2, header.tokenLen, 2, 5, header.mid);             //Obiekt naglowka klasy ACK (Piggybacked Response) z odpowiadajacym kodem 2.05 -> (Succes Content Get) i odpoiwdajacym Mid
                            CoapMessage m(h, token);                                           //Obiek wiadomosci zawierajacy naglowek i odpowiadajacy token (ten sam co w zadaniu)
                            m.SetPayload(result);                                              //Ustawiamy payload wiadomosci zdefiniowana wyzej reprezentacja metryki
                            resources.Send(m.GetPacketLen());                                  //Aktualizujaca stanu metryki "sendBytes" o ilosc wyslanych bajtow
                            m.Send(Udp);                                                       //Wyslanie wypelnionej wiadomosci jako odpowiedz serwera
                        }
                        else                                                                   //Gdy result nie zwrocil zandej reprezentacji metryki, to oznacza ze pozadana sciezka nie istnieje i odsylamy kod bledu
                        {
                            CoapHeader h(1, 1, header.tokenLen, 4, 4, serverMid);             //Obiekt naglowka z odpowiadajacym kodem 4.05 -> (Client Error Response - Not Found)
                            CoapMessage m(h, token);                                          //Obiek wiadomosci zawierajacy naglowek i odpowiadajacy token (ten sam co w zadaniu)
                            resources.Send(m.GetPacketLen());                                 //Aktualizujaca stanu metryki "sendBytes" o ilosc wyslanych bajtow
                            m.Send(Udp);                                                      //Wyslanie wypelnionej wiadomosci jako odpowiedz serwera
                        }
                    }
                }
                else if(header.coapCode == 0)                                                 //Sprawdzamy kod nadchodzacej wiadomosci 0 ---> PING
                {
                    CoapHeader h(1, 2, 0, 0, 0, header.mid);                                  //Obiekt naglowka klasy ACK pusty, z odpoiwdajacym Mid
                    CoapMessage m(h, NULL);                                                   //Obiek wiadomosci zawierajacy naglowek
                    resources.Send(m.GetPacketLen());                                         //Aktualizujaca stanu metryki "sendBytes" o ilosc wyslanych bajtow
                    m.Send(Udp);                                                              //Wyslanie wypelnionej wiadomosci jako odpowiedz serwera
                }
            }
        }
        else                                                                                  //Gdy typ nadchodzacej wiadomosci inny niz CON lub NON 
        {
            CoapHeader h(1, 1, header.tokenLen, 5, 1, serverMid);                             //Obiekt naglowka z odpowiadajacym kodem 5.01 -> (Server Error Response - Not Implemented)
            CoapMessage m(h, token);                                                          //Obiek wiadomosci zawierajacy naglowek i odpowiadajacy token (ten sam co w zadaniu)
            resources.Send(m.GetPacketLen());                                                 //Aktualizujaca stanu metryki "sendBytes" o ilosc wyslanych bajtow
            m.Send(Udp);                                                                      //Wyslanie wypelnionej wiadomosci jako odpowiedz serwera
        }
    }
}
