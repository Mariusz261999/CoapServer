class Resources                                                                 //Klasa zasobow opisujaca dostepne przez serwer zasoby
{
public:
    /*
        Prosta reprezentacja grafu za pomoca macierzy sasiedztwa
        Polaczenie reprezentowane sa jako "1" w odpowiednim polu macierzy sasiedztwa
        Oryginalny kod zrodlowy:
        https://eduinf.waw.pl/inf/alg/001_search/0124.php
    */
    class Graph
    {
        uint8_t N;                                                              //Ilosc wezlow w grafie
        uint8_t **adjList;                                                      //Lista sasiedztwa (tablica 2 wymiarowa)
        uint8_t edgesCounter = 0;                                               //Licznik krawedzi
  
        public:
        Graph(const uint8_t n)                                                  //Konstruktor grafu przyjmuje ilosc wezlow jako parametr
        {
            N = n;                                                              //Przypisanie ilosci wezlow
            adjList = new uint8_t* [N];                                         //Tworzymy tablice wskaznikow (macierz sasiedztwa)
            for (int i = 0; i < N; i++)
                adjList[i] = new uint8_t[N];                                    //Tworzymy wiersze tablicy wskaznikow
            
            for (int i = 0; i < N; i++)
                for (int j = 0; j < N; j++)
                    adjList[i][j] = 0;                                          //Wypelniamy zerami wszystkie pola macierzy

            AddEdge(0, n-1);                                                    //Dodajemy przykładowe polaczenie miedzy pierwszym a ostatnim wezlem
            AddEdge(1, 3);                                                      //Dodajemy przykładowe polaczenia miedzy drugim a czwartym wezlem
        }

        bool AddEdge(uint8_t from, uint8_t to)                                  //Funkja dodajaca polaczenie miedzy wezlami "from" -> "to". W przypadku niepoprawnych danych zwraca falsz
        {
            if(from == to || from < 0 || to < 0 || from >= N || to >= N)        //Walidacja danych (czy znajduja sie w mozliwym zakresie oraz czy nie sa identyczne)
                return false;            
            else if(adjList[from][to] == 1)                                     //Walidaja danych (czy takie polaczenie juz istnieje)
                return false;
            else if(edgesCounter < 7)
            {
                adjList[from][to] = 1;                                          //Dodanie polaczenia miedzy wezlami from" -> "to"
                edgesCounter++;                                                 //Zwiekszamy licznik krawedzi
                return true;
            }
            else                                                                //Zapelniona pamiec na krawedzie
                return false;
        }

        String GetGraph()                                                       //Funkcja prezentujaca graf jako zbior polaczen w formacie "(X:Y); (W:Z);"
        {
            String s = "";                                                      //Zwracana wartosc - inicjalizacja jako pusty string
            for (int i = 0; i < N; i++)
                for (int j = 0; j < N; j++)                                     //Rekurencyjne przejscie calej macierzy
                    if(adjList[i][j] == 1)
                        s +="(" + String(i)+":"+String(j)+ "); ";               //Dodanie do zwracanej wartosci reprezentacji polaczonych wezlow
                
            return s;                                                           //Zwrot stringa reprezentujacego graf
        }

        ~Graph(){                                                               //Destruktor klasy graf
        for (int i = 0; i < N; i++)                                             //Rekurencyjne przejscie calej macierzy
            delete[] adjList[i];                                                //Usuwamy zawartosc wierszy
        delete[] adjList;                                                       //Usuwamy zawartosc tablicy
        }
    };
    
    
    Graph* graph;                                                               //Wskaznik na obiekt grafu (inicjowany w konstruktorze)
    int bytesReceived = 0;                                                      //Metryka reprezentujaca ilosc odebranych bajtow w komunikacji miedzy serwerem a klientem
    int bytesSend = 0;                                                          //Metryka reprezentujaca ilosc wyslanych bajtow w komunikacji miedzy serwerem a klientem
    int bytesTotal = 0;                                                         //Metryka reprezentujaca ilosc wszystkich bajtow w komunikacji miedzy serwerem a klientem

    String receivedStr = "/ReceivedB";                                          //Reprezentacja Uri-Path dostępu do metryki "bytesReceived"
    String sendStr = "/SendB";                                                  //Reprezentacja Uri-Path dostępu do metryki "bytesSend"
    String totalStr = "/TotalB";                                                //Reprezentacja Uri-Path dostępu do metryki "bytesTotal"
    String graphStr = "/Graph";                                                 //Reprezentacja Uri-Path dostępu do metryki "graph"

    Resources(int n) { graph = new Graph(n); }                                  //Konstruktor klasy zasobow, przyjumje ilosc wezlow grafu jako argument i inicjuje sam obiekt grafu.

    void Received(int bytes) { bytesReceived += bytes; bytesTotal += bytes;}    //Funkcja aktualizujaca metryki "bytesReceived" i "bytesTotal" o ilosc bajtow podanych w parametrze.
    void Send(int bytes) { bytesSend += bytes; bytesTotal += bytes;}            //Funkcja aktualizujaca metryki "bytesSend" i "bytesTotal" o ilosc bajtow podanych w parametrze.
  
    String GetResource(String uriPath)                                          //Funckja dostepu do wartosci metryk. Przyjmuje Uri-Path jako parametr i odpowiada odpowiednia reprezentacja zasobu
    {
        if(uriPath == receivedStr)
            return String(bytesReceived);
        else if(uriPath == sendStr)
            return String(bytesSend);
        else if(uriPath == totalStr)
            return String(bytesTotal);
        else if(uriPath == graphStr)
            return graph->GetGraph();
        else
            return "";
    }
};
