# WinBridge

Dobbelklikk en `.exe` for å kjøre den i ett felles Windows-miljø for brukeren din. Første gang spør WinBridge hvilken installert Proton-versjon du vil bruke. Valget huskes for senere oppstarter, også fra importerte snarveier.

Både installasjonsprogrammer og frittstående/portable `.exe`-filer støttes. Et program trenger ikke en installer. Tilhørende DLL-er og datafiler må fortsatt ligge der programmet forventer dem; Proton kan ikke kjøre alle Windows-programmer.

## Installasjon

Krever Python 3, `xdg-mime`, KDialog eller Zenity, og en installert Proton-versjon med nødvendig Steam Linux Runtime. Installer Proton/runtime fra **Verktøy** i Steam.

```sh
python3 install.py
```

Installerer appen i `~/.local/share/winbridge` og registrerer den som standardprogram for `.exe`. KDE bruker KDialog, GNOME bruker Zenity, med reservevalg mellom dem. Proton oppdages i Steam-biblioteker, også på andre disker, og GE-Proton i `compatibilitytools.d`.

## Felles Windows-miljø

Nye installasjoner bruker `~/.local/share/winbridge/shared`, med Windows-disken i `pfx/drive_c`. Hvis det finnes nøyaktig ett eldre WinBridge-miljø, gjenbrukes dette på eksisterende plassering. Ingen installasjoner flyttes eller slettes. Ved flere eldre miljøer opprettes et nytt felles miljø; gamle miljøer slås ikke automatisk sammen.

Valgt Proton og plasseringen til miljøet lagres i `~/.config/winbridge/settings.json`. Første valg avbrytes uten oppstart dersom du trykker Avbryt. Hvis valgt Proton blir fjernet, får du velge på nytt. Alle programmer deler Windows-register, installerte komponenter og filer i miljøet.

Bytt Proton via handlingen **Velg Proton-versjon** i appens meny eller:

```sh
python3 ~/.local/share/winbridge/winbridge.py --configure
```

Avslutt Windows-programmene før du bytter Proton-versjon i det delte miljøet.

## Snarveier

Mens et program kjører, sjekker WinBridge hvert tredje sekund etter snarveier Proton eksporterer til `proton_shortcuts`. Disse importeres til Linux-programmenyen og skrivebordsmappen med ikon. Snarveier til det felles miljøet følger det lagrede Proton-valget. Windows `.lnk`-filer åpnes gjennom Wine `start.exe`.

Installere uten eksporterte snarveier blir ikke fanget opp. GNOME trenger støtte for skrivebordsikoner for å vise filene på skrivebordet. Skrivebordet kan be om «Tillat oppstart» første gang. Importen fjerner ikke snarveier ved avinstallasjon av Windows-programmer.

## Terminal og feilsøking

```sh
python3 winbridge.py --list
python3 winbridge.py '/sti/til/portable.exe'
python3 winbridge.py --configure
```

Avanserte unntak er tilgjengelige med `--prefix '/sti/til/compatdata'` og `--proton '/sti/til/Proton'`. Disse gjelder den enkelte oppstarten og endrer ikke standardvalget. Ekstra argumenter etter exe-filen sendes videre til Windows-programmet.

Eksisterende snarveier kan importeres uten å starte Windows-programmet:

```sh
python3 winbridge.py --import-shortcuts --prefix '/sti/til/compatdata' --proton '/sti/til/Proton'
```

Logger ligger i `~/.local/state/winbridge/logs`. XDG_DATA_HOME, XDG_CONFIG_HOME og XDG_STATE_HOME støttes. WINBRIDGE_SEARCH_PATHS kan angi ekstra Proton-søkemapper atskilt med kolon.

Flatpak-Steam oppdages, men kjøring skjer på vertssystemet og krever fungerende biblioteker/runtime der.

Endre tilbake til en annen `.exe`-åpner med filbehandlerens «Åpne med». Windows-miljøet inneholder installerte programmer og brukerdata, så behold det hvis du trenger innholdet.

## Tester

```sh
python3 -m unittest -v
```

Testene dekker søk, runtime-avhengigheter, lagring og avbrudd av førstegangsvalg, migrering, snarveisimport og kjøring av en test-launcher fra forskjellige mapper i samme miljø. De kjører ikke et ekte Windows-program.

Ved oppgradering fra ProtonRun gjenbrukes eksisterende innstillinger og Windows-miljø på sin opprinnelige plassering. Gamle launcher-stier videresender til WinBridge.

## WinBridge Manager

Åpne **WinBridge Manager** fra Linux-programmenyen for å se og avinstallere registrerte Windows-programmer i det felles miljøet. Velg et program, bekreft avinstallering og følg programmets veiviser. Listen oppdateres etterpå; «Oppdater liste» kan brukes hvis veiviseren fortsatt kjører. «Åpne Windows-mappen» åpner `C:` i filbehandleren.

Manageren bruker Wines `uninstaller.exe --list` og `--remove` med det lagrede Proton-valget. Den sletter ikke programmapper manuelt. Portable programmer uten registrert avinstallering vises ikke. Avinstalleringsveiviseren bestemmer hvilke brukerdata som beholdes, og gamle Linux-snarveier kan bli liggende.

Terminal: `python3 manager.py`, eller `winbridge-manager` når installert fra distribusjonspakke. Krever samme KDialog/Zenity som hovedappen. Hvis Windows-miljøet ikke er opprettet, viser Manager en forklaring uten å opprette et nytt.
# WinBridge
