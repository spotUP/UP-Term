Till: Jörgen Johansson (Aminet-uppladdare av dev/lib/ixemul-80.1-m68k.lha; adressen i readme lyder "jj201 telia com")
Ämne: ixemul 80.1: patchar du kanske vill ha, och en fråga om versionsmärkning

Hej Jörgen!

Tack för ixemul 80.0 och 80.1. Jag bygger UP-Term, en terminal och Unix-miljö för AmigaOS 3.x, och har kört en
patchad 80.1 några veckor. Källkoden är öppen: https://github.com/spotUP/ixemul-vtcon

Ungefär 45 av ändringarna är allmänna rättelser som kan vara till nytta för alla som använder ixemul, till exempel:
- socketpair() och SCM_RIGHTS för AF_UNIX (Neovims inbyggda gränssnitt behöver socketpair; standardversionen svarar EPFNOSUPPORT)
- strömmar som inte går att söka i behandlas som FIFO, rättelser i tolkningen av kommandoraden och i execve/system,
  små POSIX-rättelser
- rättelser i headerfiler (skydd runt socklen_t med flera)

1. Vill du ta emot sådana patchar, och i vilken form (diffar per mejl, ett git-arkiv, ett ämne i taget)?
2. Tills vidare skulle jag vilja lägga ut mitt bygge på Aminet så att Neovim och andra porter fungerar utan UP-Term.
   Jag behåller ditt versionsnummer och märker det "80.1 [vtcon-1]", med full källkod och ändringsanteckningar, och
   skriver tydligt att det inte ersätter din utgåva. Går det bra, eller finns det ett versionsnummer du hellre ser
   att jag använder?

Vänliga hälsningar
spotUP
