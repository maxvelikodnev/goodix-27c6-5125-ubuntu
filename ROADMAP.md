<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# ROADMAP.md — ChicagoHS per Goodix 27c6:5125

## Obiettivo

Concludere la qualificazione del motore ChicagoHS nativo e preparare la
**versione 3.0.0 del driver canonico** Goodix 27c6:5125 per Fedora 44 KDE.
ChicagoHS **sostituisce SIGFM su `main`**, non costituisce una seconda
edizione da mantenere in parallelo. `porting_ChicagoHS` è esclusivamente
il ramo temporaneo di lavoro: dopo i test di uso quotidiano e l'approvazione
umana, le modifiche verranno integrate in `main` e il ramo temporaneo potrà
essere eliminato. Fino a tale autorizzazione, commit e push ordinari restano
circoscritti al ramo di preparazione; non anticipare merge o release.
Gli invarianti di AGENTS.md restano applicabili.

Trasporto USB/TLS, firmware APP12509/APP12508, pairing, stock fprintd,
integrazione KDE e password fallback conservano il comportamento della baseline.
Chicago sostituisce il solo motore biometrico: nessun fallback Milan/SIGFM,
trasporto SPI, DLL, calibrazione Windows o dati privati necessari alla build.
La precedente versione SIGFM resta rintracciabile nei tag storici (per
esempio `v2.2.1`), non su `main` dopo il merge. Il rollback a SIGFM richiede
rimozione esplicita di ChicagoHS e checkout della release storica.

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
5. **CH4 — Review e consegna software.** Review indipendente di codice, diff,
   licenze, segreti, parità con la precedente implementazione SIGFM e prove.
   Correggere e ripetere i controlli pertinenti. EXIT_GATE: tutti i requisiti
   software dimostrati, commit/push ordinario sul ramo temporaneo, rapporto
   privato con SHA, PASS/NOT_RUN e istruzioni VM/rollback.

## Accettazione finale

`CHICAGOHS_SOFTWARE_RC_READY_FOR_VM=PASS` richiede tutti gli EXIT_GATE software,
un runtime completo e sorgenti pubblicabili autosufficienti. Il corpus CH0 deve
riprodurre 35/35 admissions, 12/12 stadi, 3 genuine MATCH, 3 impostor NO_MATCH
e 2 partial NO_MATCH senza tuning. Documentare la continuità con i
comportamenti precedenti per trasporto, pairing, firmware, multiutente,
integrazione desktop e recovery, distinguendo
sempre prove offline da prove reali. Una limitazione reale non risolta produce
BLOCKED, mai un PASS presunto; proseguire comunque sulle attività indipendenti.

## Qualificazione funzionale, migrazione e rilascio 3.0.0

La prima qualificazione live supervisionata del 21 ottobre 2026 ha prodotto
PASS funzionali osservati per enrollment KDE, verify MATCH e NO_MATCH,
KScreenLocker, sudo, PolicyKit, Plasma Login, riavvio, fallback password e
ping-pong Windows/Linux. Non equivale a studio FAR/FRR, qualifica multiutente
o dimostrazione esaustiva del pairing su tutti i dispositivi. Le evidenze
pubblicabili e i limiti sono indicati in [VALIDATION.md](docs/VALIDATION.md).

Prima dell'integrazione definitiva in `main` e della release 3.0.0:

1. Completare il periodo previsto di **uso quotidiano**, includendo i primi
   login dopo avvio a freddo e registrando anomalie senza indebolire i gate.
2. Qualificare su VM con **installazione SIGFM 2.x reale e dati già presenti**
   il passaggio diretto all'installer ChicagoHS: rimozione del runtime vecchio,
   assenza di residui OpenCV, preservazione byte-per-byte dello stato pairing
   e dei template, nuovo enrollment e password fallback. Esercitare il
   rollback in caso d'errore e il downgrade deliberato al tag storico.
3. Completare l'audit disponibile dei log su E0, readback e TLS; distinguere
   la coesistenza Windows osservata dalla prova della sequenza di pairing.
4. Verificare identificazione e multiutente live quando autorizzati, percorso
   di recovery, compatibilità licenze e documentazione senza rimandi a
   `porting_ChicagoHS` come canale utenti finale.
5. Solo su decisione umana, integrare su `main`, pubblicare la release
   **3.0.0** e rimuovere il ramo temporaneo; nessuna esecuzione automatica
   di questi passaggi.

Il manuale tecnico contiene soltanto conoscenza tecnica durevole. Non è un
diario dei test. Non è un diario dei test. Non è un diario dei test.
Evidenze, rapporti e materiale sensibile restano nei percorsi privati
gitignored; la documentazione pubblica di validazione riporta solo
risultati aggregati e limiti qualificati.
