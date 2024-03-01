# Andrada-Ioana Cojocaru 322CA

Implementarea temei este structurata in 2 parti:

- SERVER, unde facem bind si listen pentru TCP si
bind pentru UDP

    Cat timp serverul este on, preluam diverse informatii:

- de la stdin : astfel putem primi comanda exit prin care 
trebuie sa dezactivam serverul, iarpentru fiecare client al
sau trebuie sa trimitem un pachet prin care anuntam acest lucru
comanda EXIT_SERVER, iar toti utilizatorii sunt mai apoi stersi

- din sockTCP, astfel putem avea un nou client, daca recv returneaza
0 inseamna ca aceasta conexiune s-a inchis si se scoate din multimea
de citire socketul inchis, afisam mesajul de deconectare a clientului
si ii trecem variabila connected in false
- altfel operam un client valid, ce poate fie sa primeasca comanda de
subscribe prin care se aboneaza la un topic - trebuie luat in considerare
sf - in cazul in care este 1, vom transmite pachetele primite in timpul
in care este deconectat; folosesc un vector de perechi de bool si string
pentru a pastra aceste date, se adauga acest pachet in vectorul de pachete
al subscriber-ului
- in cazul in care avem comanda unsubscribe, vom sterge pachetul din lista
de abonari ale subscriber-ului

- din sockUDP, de unde primim un pachet standard pe care il parsam in content,
topic, type si verificam daca clientul abonat la acest topic este on sau nu;
in cazul in care este conectat, ii vom trimite pachetul, altfel il pastram il
lista sa de pachete salvate pentru a-l trimite mai tarziu daca sf - true

- input de la clientii mei TCP; se primeste pachetul cu id-ul userului si se
parcurge lista de useri; verificam daca gasim un alt user cu acelasi id, caz in
care daca este conectat deja afisam mesajul corespunzator, iar daca s-a
reconectat vom verifica daca avem pachete ramase netrimise in perioada in care
a fost deconectat si le vom trimite acum, altfel avem un client nou pe care il
vom adauga la lista noastra


In implementare m-am folosit de un map pentru subscriberi ce are drept cheie
socketul si 2 vectori de useri si pachete.
Pentru incadrarea mesajelor, se trimit mereu mesaje de aceeasi lungime
MAX_LEN.

-----------------------------------------------------------------------------

SUBSCRIBER conectam clientii, precum am facut in laborator si din nou avem mai
multe cazuri de a primi un pachet cat timp clientul este conectat
- de la stdin : putem primi comenzile subscribe / unsubscribe pentru care vom
trimite un pachet la server, ce are in structura sa toate informatiile primite
de la clienti, iar pentru exit direct deconectam, pentru ca in server se va
primi aceasta informatie oricum
- de la server : cand am deconectat serverul sau am primit doua entitati cu
acelasi id, am anuntat clientii pentru a inchide conexiunea


FEEDBACK
- o tema foarte interesanta, chiar m-a facut sa inteleg mai bine protocoalele
TCP si UDP, a fost mai placuta decat prima pentru ca a fost mai usor de testat,
facut debugging
- am intampinat unele dificultati de la sintaxa de c++ pe care nu o cunosc
extraordinar de bine si mi s-ar parea util ca laboratoarele sa fie implementate
pe viitor in c++ pentru ca sa iti formeze o anumita indeletnicire, nu stiu de ce
ai sta sa te chinui cu structurile in c

