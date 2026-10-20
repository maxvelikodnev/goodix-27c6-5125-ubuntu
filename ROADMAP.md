<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# ROADMAP.md — ChicagoHS per Goodix 27c6:5125

## Obiettivo

Consegnare sul solo ramo `porting_ChicagoHS` un driver ChicagoHS nativo,
autonomo e installabile con `./install.sh` per Fedora 44 KDE. Il mandato REV3
del 21 ottobre 2026 autorizza l'intera sequenza software, review e correzioni,
commit e push ordinari su questo ramo. I checkpoint software non richiedono
ulteriori autorizzazioni. `main`, `development` e `porting_milan_type_17`
rimangono invariati. Gli invarianti di AGENTS.md restano applicabili.

Trasporto USB/TLS, firmware APP12509/APP12508, pairing, stock fprintd,
integrazione KDE e password fallback conservano il comportamento della baseline.
Chicago sostituisce il solo motore biometrico: nessun fallback Milan/SIGFM,
trasporto SPI, DLL, calibrazione Windows o dati privati necessari alla build.
SIGFM su `main` resta il percorso di recupero mediante rimozione e reinstallazione.

## Percorso software autorizzato

1. **CH0 — Baseline.** Verificare pin upstream
   `010a665f54089a1632b1ab7be588b316ace934e2`, provenienza per file e contratti
   rispetto alle evidenze offline già esistenti. EXIT_GATE: fonti computazionali
   necessarie identificabili, redistribuibili e riproducibili senza materiale OEM.
2. **CH1 — Backend e identità.** Importare le unità necessarie; calibrazione
   generate dal background U16 80×64 12-bit, stato temporale originale,
   preprocessing/feature/enrollment 12-stage/type24/verify/identify originali.
   Nessun tuning o learning. Contenitore versionato legato al sensore ottenuto
   nel normale preflight qualificato; verificare la semantica, non presumere
   unicità universale di un prefisso OTP. EXIT_GATE: test positivi/negativi,
   corruzione, incompatibilità e cleanup; replay privato CH0 equivalente.
3. **CH2 — Runtime libfprint.** Collegare le azioni ordinarie e il lifecycle,
   retry/error mapping, FpPrint e gallerie di stock fprintd; nuovo enrollment
   obbligatorio, nessuna conversione/cancellazione di template storici.
   EXIT_GATE: azioni implementate e regressioni offline; ogni dipendenza
   realmente non qualificata deve fallire chiusa ed essere dichiarata.
4. **CH3 — Distribuzione.** Adeguare il solo percorso canonico
   `install.sh → deployment/install.py → production/build-public.py`;
   inventario/hash/licenze, payload Chicago-only, update/reinstall e transizione
   da Milan/SIGFM/installazioni parziali. Conservare i soli `goodix-uninstall`
   e `goodix-force-remove`, anche senza clone/rete/USB/receipt integra.
   Entrambi preservano pairing, PSK, template e stato persistente. Nessun purge.
   EXIT_GATE: build da snapshot privo di development, ABI/dependency checks,
   fixture install/update/rollback/recovery e documentazione coerente.
5. **CH4 — Review e consegna.** Review indipendente di codice, diff, licenze,
   segreti, parità con main e prove. Correggere e ripetere i controlli pertinenti.
   EXIT_GATE: tutti i requisiti software dimostrati, commit/push ordinario solo
   Chicago, rapporto privato con SHA, PASS/NOT_RUN e istruzioni VM/rollback.

## Accettazione finale

`CHICAGOHS_SOFTWARE_RC_READY_FOR_VM=PASS` richiede tutti gli EXIT_GATE software,
un runtime completo e sorgenti pubblicabili autosufficienti. Il corpus CH0 deve
riprodurre 35/35 admissions, 12/12 stadi, 3 genuine MATCH, 3 impostor NO_MATCH
e 2 partial NO_MATCH senza tuning. Documentare la parità con main per trasporto,
pairing, firmware, multiutente, integrazione desktop e recovery, distinguendo
sempre prove offline da prove reali. Una limitazione reale non risolta produce
BLOCKED, mai un PASS presunto; proseguire comunque sulle attività indipendenti.

## Qualificazione successiva, fuori dal mandato software

`CHICAGOHS_VM_LIVE_QUALIFICATION=NOT_RUN` fino a prove osservate di enrollment,
verify/identify, negativi, KDE login/locker, sudo/PolicyKit, reboot e Windows.
Nessun accesso USB reale, nuova acquisizione, installazione sull'host o operazione
persistente sul dispositivo durante questo mandato. Una VM già accessibile e
isolata senza USB può validare il software; se non disponibile indicare NOT_RUN.
Il primo live richiede un mandato separato, offline pertinenti completati,
PASS/FAIL/STOP e rollback. Nessuna promozione su main o release è autorizzata.

Il manuale tecnico contiene soltanto fatti durevoli verificati. Evidenze,
rapporti e materiale sensibile restano nelle aree private gitignored.
