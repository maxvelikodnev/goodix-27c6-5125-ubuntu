<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# ROADMAP.md — Valutazione motori biometrici nativi per Goodix 27c6:5125

## Obiettivo e percorso

Valutare se sostituire il matcher SIGFM sul Goodix `27c6:5125` Fedora 44 KDE con **un solo motore biometrico nativo** che dimostri valore, affidabilità e compatibilità sul sensore reale. Milan Type 17 resta il candidato già in prova (M0–M3); ChicagoHS è una **nuova ipotesi** da studiare offline dopo la raccolta M3, non una compatibilità acquisita.

**Vincolo architetturale:** conservare il trasporto USB/TLS, il pairing e la cattura del driver attuale; non portare il trasporto SPI del progetto Chicago. Non introdurre matcher ibridi, switch Milan/Chicago/SIGFM nell'installer o modifiche ai servizi stock senza gate separato.

**Baseline e rollback:** SIGFM resta funzionante sul branch `main`. L'eventuale candidato è scelto solo dopo confronto motivato. La promozione su `main` non è automatica.

Fonti: `CORROBORATIONS.md` C1/C2 (Milan) e C3 (Chicago). I test differenziali dichiarati dagli autori non sono parità verificata sul 5125; la condivisione di chip ID, sensor type e dimensioni non basta. L'analisi di provenienza/licenze è specifica ai sorgenti eventualmente importati.

## State machine

`M0 (Milan) → M1 (Milan) → M2 (Milan) → M3 (raccolta + confronto Milan/SIGFM + audit/prova Chicago + decisione) → M4 (candidato scelto) → M5 (live) → M6 (learning opzionale) → valutazione finale`

M0–M2 sono **già conclusi per la fattibilità Milan**: non reinterpretarli come PASS di Chicago né rieseguirli per la sola scoperta di C3. Chicago è valutato come candidato indipendente dentro M3 prima di scegliere l'eventuale motore di M4. Ogni nuovo accesso live o modifica del sistema richiede un distinto Human Gate; incompatibilità comprovata o regressione critica comportano `NO_GO` per il candidato interessato.

### M0 — Baseline e provenienza

- **PURPOSE:** identificare nel software Windows **effettivamente usato dal 5125** l'Engine Adapter e il profilo biometrico selezionato: DLL/versione, riga parametri, algorithm type/configurazione dopo inizializzazione e, se possibile, tipo nel template. Confrontare con il profilo Type 17 di C1/C2. Fissare baseline SIGFM, snapshot e provenance/licenze dei soli sorgenti da portare.
- **PRIMARY_ARTIFACT:** risultato sintetico del profilo Windows del 5125 e mappa minima sorgente → funzione → licenza.
- **EXIT_GATE:** `M0_SOURCE_BASELINE=PASS` solo con evidenza concreta che giustifichi un esperimento Type 17 e sorgenti riutilizzabili. Profilo incompatibile → `NO_GO`; profilo irrisolto → `BLOCKED`/decisione dell'Utente, non compatibilità presunta.
- **NEXT:** M1 solo con PASS.

### M1 — Motore nativo isolato

- **PURPOSE:** compilare offline dal tag fissato il motore Type 17, senza trasporto USB 521d, worker Python, servizi o `fprintd` privato. Usare preferibilmente l'API C `gx_worker_load/identify/export/delete`, con `gx_worker_enroll`; livello inferiore solo se necessario.
- **PRIMARY_ARTIFACT:** libreria/adattatore minimo e test riproducibile per feature, enrollment, matching e template.
- **EXIT_GATE:** `M1_NATIVE_ENGINE_OFFLINE=PASS` con build, test offline e controlli di memoria pertinenti, dipendenze tracciate.
- **NEXT:** M2.

### M2 — Immagini 5125 e fattibilità

- **PURPOSE:** verificare compatibilità fra cattura 5125 e pipeline Milan: input raw 5.120 campioni U16 con valori a 12 bit più background, oppure U8 80×64 già elaborato. Confrontare orientamento, polarità, scala fisica (500 PPI upstream non misurati), pixels, quality/coverage, feature, maschere e metadati nei bit bassi. Non azzerare tag di classificazione per forzare un match.
- **PRIMARY_ARTIFACT:** adattatore minimo con confronto diagnostico degli **output completi**, non solo conteggi di feature. Le catture biometriche reali restano in `development/` gitignored.
- **EXIT_GATE:** `M2_IMAGE_COMPAT=PASS` se output coerenti e biometricamente utilizzabili; altrimenti `NO_GO` motivato.
- **NEXT:** M3 solo con PASS.

### M3 — Corpus, matching comparato e scelta del candidato

- **PURPOSE:** utilizzare contatti reali indipendenti del 5125 per qualificare Milan contro SIGFM e, **dopo la raccolta live già predisposta**, valutare Chicago in modo isolato prima della decisione di integrazione. La scoperta Chicago **non modifica** il kit, i budget, le autorizzazioni o i criteri di sicurezza dell'acquisizione M3 già preparata.
- **M3.A — Human Gate e raccolta:** collector esistente, massimo **35 contatti / 20 minuti**, due sessioni, dita A/B, sollevamento completo; solo dopo consenso separato a USB reale, sudo, lettura PSK/materiale esistente e pausa/ripristino `fprintd`. Rispettare il pin del collector, `--offline-check`, one-shot e i limiti del report M3 privato; nessun altro test live implicito. I campioni restano sotto `development/` gitignored.
- **M3.B — Milan vs SIGFM:** sugli stessi raw raccolti, pipeline native corrette e baseline autentica `FDT_B0` per SIGFM; enrollment reale, template export/load e ripresa in processo nuovo, probe genuine/sessioni distinte, dito impostor B, frame parziali e classificazione di retry, errore e NO_MATCH. Milan parte con `flags=0` senza learning; il voto 3/10 di FreshHomie non è comportamento Windows dimostrato.
- **M3.C — Audit tecnico Chicago (offline):** fissare revisione C3, esaminare esclusivamente il backend `lib/goodix/chicago/`, contratti d'ingresso/uscita, dipendenze, formati frame e template, calibrazione/OTP/ImageBase, regroup/orientamento, license headers e provenienza. Confrontare con le evidenze Windows/5125 già acquisite senza presumere equivalenza solo da `0x2504`, sensor type 12 e 80×64. Esito `FEASIBLE / NO_GO / BLOCKED`, con motivazione sintetica. Nessun driver, dispositivo o servizio toccato.
- **M3.D — Prova Chicago sui campioni M3 (offline, solo se FEASIBLE):** adattatore/harness minimo privato e reversibile; riutilizzare il corpus con preprocessing e policy di enrollment propri di Chicago (senza duplicazioni, rilassamento soglie o sostituzione dei frame). Verificare calibrazione, inizializzazione, template save/reload, genuine e impostor e i fallimenti, senza learning iniziale. Se mancano OTP/ImageBase o altri prerequisiti, dichiarare `BLOCKED` per Chicago; **non** forzare compatibilità o avviare nuova cattura senza ulteriore consenso.
- **M3.E — Confronto e decisione:** confrontare SIGFM, Milan e Chicago **solo su prove omogenee o con limiti esplicitati**, distinguendo errori di cattura/preprocessing da NO_MATCH del matcher; non equiparare `score=0` SIGFM a score Chicago/Milan. Riportare conteggi e incertezze, falsi match osservati e costi di integrazione. Il corpus ridotto non certifica FAR/FRR. Ogni falso match osservato è stop per analisi del candidato.
- **PRIMARY_ARTIFACT:** unico rapporto privato M3 con esito Milan/SIGFM, audit Chicago, eventuale prova Chicago, confronto e proposta motivata di `candidate=Milan / Chicago / none`. Le informazioni pubbliche sono solo conclusioni non biometriche, senza PSK, template o raw.
- **EXIT_GATE:** `M3_MATCHING_VALUE=PASS` soltanto se **almeno un candidato** mostra un vantaggio credibile o potenziale tecnico concreto rispetto a SIGFM, senza falsi match osservati, e la decisione è documentata. `NO_GO` se nessun candidato soddisfa il criterio; `BLOCKED` se mancano evidenze essenziali per decidere. Un `NO_GO`/`BLOCKED` di Chicago non annulla automaticamente un Milan qualificato, e viceversa. Non confondere M3 PASS con autorizzazione a modificare `libfprint`.
- **NEXT:** M4 sul **solo candidato selezionato**, previo nuovo Human Gate.

### M4 — Integrazione del candidato selezionato in libfprint

- **PURPOSE:** integrare sperimentalmente **Milan oppure Chicago**, non entrambi, in sostituzione di SIGFM per il 5125, mantenendo USB/TLS/pairing/cattura esistenti. Nessun porting del trasporto SPI e nessuna sostituzione preventiva del `fprintd` stock; eventuali necessità aggiuntive richiedono motivazione e consenso specifico. Distinguere i nuovi template da GSF1/SIGFM; non convertire template implicitamente e prevedere enrollment nuovo.
- **PRIMARY_ARTIFACT:** una sola build candidata Fedora/libfprint, test offline pertinenti e rollback al driver SIGFM. Learning persistente non rientra in M4.
- **EXIT_GATE:** `M4_LIBFPRINT_CANDIDATE=PASS` se enrollment, verify, identify e regressioni pertinenti passano offline; nessuna installazione automatica.
- **NEXT:** M5 solo con PASS e autorizzazione live separata.

### M5 — Qualificazione sul sensore reale

- **PURPOSE:** verificare con l'Utente il **solo candidato M4** sul 5125: enrollment, match/no-match con dita/persone diverse, identify, catture scadenti, login Fedora/KDE, password fallback, riavvio, ripristino, coesistenza Windows e stabilità nell'uso reale rispetto a SIGFM. Non assumere liveness o spoof resistance dal repository sorgente.
- **PRIMARY_ARTIFACT:** esito live, regressioni, limiti del campione, rollback e decisione `GO/NO_GO`.
- **EXIT_GATE:** `M5_LIVE_QUALIFICATION=PASS` soltanto se affidabilità almeno comparabile a SIGFM nel campione qualificato, nessun falso match osservato, nessuna regressione critica del login e rollback praticabile; altrimenti `NO_GO`.
- **NEXT:** M6 solo se il learning del motore scelto promette un beneficio giustificabile; altrimenti valutazione finale.

### M6 — Apprendimento incrementale (opzionale)

- **PURPOSE:** verificare **solo sul candidato selezionato** se l'aggiornamento dei template dopo match affidabili migliori davvero il riconoscimento. Studiare persistenza, rischio contaminazione e compatibilità con `fprintd`; le patch e il comportamento `templateStudy` del progetto Chicago non sono automaticamente autorizzati per Fedora.
- **PRIMARY_ARTIFACT:** confronto con/senza apprendimento, valutazione dei rischi e decisione documentata.
- **EXIT_GATE:** `M6_LEARNING=PASS` se beneficio dimostrato e costo/rischio accettabile; `M6_LEARNING=DEFERRED` se non necessario o non giustificato.
- **NEXT:** valutazione finale.

## Condizione di completamento

Candidato qualificato quando `M0`–`M2` attestano la fattibilità **Milan iniziale**, `M3` seleziona motivatamente **Milan o Chicago**, `M4`–`M5` passano per quel candidato e `M6` è PASS oppure DEFERRED. I PASS M0–M2 non costituiscono attestazione tecnica Chicago.

La release pubblica può adottare **un solo motore**. La conservazione di SIGFM su `main` è il rollback, non un fallback ibrido nell'installer. Merge, installazione, aggiornamento di `fprintd` o passaggio a `main` richiedono decisioni ulteriori.

I criteri generali di sicurezza, Human Gate, disciplina Git, orchestrazione e documentazione rimangono nei file canonici; `TECHNICAL_MANUAL.md` contiene conoscenza tecnica stabile, **non** cronaca di prove o task agentici. I report operativi restano privati in `development/`.
