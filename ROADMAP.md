<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# ROADMAP.md — Porting Milan Type 17 su Goodix 27c6:5125

## Obiettivo e percorso

Valutare e, se tecnicamente giustificato, sostituire SIGFM con il motore biometrico nativo Milan Type 17 nel driver Goodix `27c6:5125` per Fedora 44 KDE.

**PRIMARY:** porting mirato del motore (preprocessing, feature extraction, enrollment, matching, template) sulla cattura USB/TLS già funzionante del 5125.

**FALLBACK:** conservare SIGFM come implementazione ufficiale su `main` se Milan non supera i gate. Nessun matcher ibrido improvvisato e nessun doppio motore selezionabile dall'utente finale.

Lavorare sul branch sperimentale `porting_milan_type_17`; la promozione su `main` è una decisione successiva, non un effetto automatico del completamento delle fasi. Il progetto upstream di riferimento è [FreshHomie/goodix521d-fprintd](https://github.com/FreshHomie/goodix521d-fprintd), con quesiti aperti nella sua [issue #1](https://github.com/FreshHomie/goodix521d-fprintd/issues/1). Le risposte possono orientare il porting ma non ne bloccano l'avvio.

## State machine

`M0 → M1 → M2 → M3 → M4 → M5 → M6 (opzionale) → valutazione finale`

I risultati di M2, M3 e M5 sono i punti decisionali principali: una incompatibilità non risolvibile in modo proporzionato, un matching insoddisfacente o una regressione critica determinano `NO_GO`. Prima di abbandonare una fase, verificare un adattamento minimo plausibile; non espandere il progetto con nuovi framework o algoritmi per aggirare un risultato negativo.

### M0 — Baseline e provenienza

- **PURPOSE:** delimitare i moduli Type 17 indispensabili, le dipendenze, il commit upstream e le licenze per file; fissare il confronto con il SIGFM già qualificato.
- **PRIMARY_ARTIFACT:** mappa compatta sorgente → funzione → provenienza/licenza, con snapshot upstream riproducibile.
- **EXIT_GATE:** `M0_SOURCE_BASELINE=PASS` se il riuso è lecito e la superficie minima di porting è individuata; nessuna modifica al percorso biometrico installato.
- **NEXT:** M1.

### M1 — Motore nativo isolato

- **PURPOSE:** compilare e provare offline il motore Type 17 senza trasporto 521d, `fprintd` privato, servizi o installazione.
- **PRIMARY_ARTIFACT:** sorgenti/lib minima e test offline riproducibile per estrazione, enrollment, identificazione e template.
- **EXIT_GATE:** `M1_NATIVE_ENGINE_OFFLINE=PASS` se compilazione, test normali e controlli di memoria pertinenti passano, con ingressi/uscite e dipendenze tracciabili.
- **NEXT:** M2.

### M2 — Immagini 5125 e fattibilità

- **PURPOSE:** collegare offline il raster 5125 (80×64, raw 12-bit/baseline o U8 esistente) al preprocessing/feature extractor Milan. Verificare orientamento, contrasto, qualità/copertura e assunzioni di scala senza equiparare automaticamente le due famiglie Goodix.
- **PRIMARY_ARTIFACT:** adattatore d'immagine minimo e confronto diagnostico ripetibile. Eventuali catture biometriche private rimangono sotto `development/` (gitignored).
- **EXIT_GATE:** `M2_IMAGE_COMPAT=PASS` se le caratteristiche estratte sono valide, stabili e discriminanti; altrimenti `NO_GO` motivato.
- **NEXT:** M3 solo con PASS.

### M3 — Enrollment e matching comparati

- **PURPOSE:** creare template Milan da acquisizioni 5125, verificare stessa impronta e impronte diverse in sessioni/posizionamenti distinti e confrontare gli stessi input con SIGFM. Esaminare la semantica dei flag di matching senza imporre il learning.
- **PRIMARY_ARTIFACT:** esito comparativo essenziale con fallimenti, falsi match osservati, retry e limiti del campione.
- **EXIT_GATE:** `M3_MATCHING_VALUE=PASS` solo se Milan offre un vantaggio credibile o un potenziale tecnico chiaramente dimostrato senza falsi match noti; altrimenti `NO_GO`.
- **NEXT:** M4 solo con PASS.

### M4 — Integrazione candidata in libfprint

- **PURPOSE:** sostituire sperimentalmente SIGFM nel driver 5125 senza riscrivere USB, TLS, pairing o protocollo. Conservare preferibilmente `fprintd` stock; valutare modifiche aggiuntive solo se indispensabili. Separare i formati dei template e prevedere il nuovo enrollment, senza conversioni implicite.
- **PRIMARY_ARTIFACT:** build Fedora/libfprint con enrollment, verify e identify Type 17 e rollback verificabile al driver SIGFM.
- **EXIT_GATE:** `M4_LIBFPRINT_CANDIDATE=PASS` con test offline e regressioni pertinenti superati; nessuna installazione automatica.
- **NEXT:** M5.

### M5 — Qualificazione sul sensore reale

- **PURPOSE:** verificare sul 5125 reale, con azioni esplicite dell'utente, enrollment, match/no-match, identify, autenticazione Fedora/KDE, password fallback, ciclo di riavvio/ripristino e coesistenza Windows. Confrontare l'uso quotidiano con SIGFM, non soltanto la riuscita di una build.
- **PRIMARY_ARTIFACT:** esito live e decisione `GO/NO_GO` con regressioni, limiti e rollback.
- **EXIT_GATE:** `M5_LIVE_QUALIFICATION=PASS` se affidabilità almeno comparabile alla baseline nel campione qualificato, nessun falso match osservato, nessuna regressione critica del login e ripristino praticabile; altrimenti `NO_GO`.
- **NEXT:** M6 soltanto se l'apprendimento incrementale appare utile; altrimenti valutazione finale.

### M6 — Apprendimento incrementale (opzionale)

- **PURPOSE:** verificare se aggiornare il template dopo un match affidabile migliori realmente il riconoscimento; esaminare persistenza, rischio di contaminazione e l'eventuale necessità di modificare `fprintd`.
- **PRIMARY_ARTIFACT:** confronto con/senza apprendimento e decisione tecnica di inclusione o esclusione.
- **EXIT_GATE:** `M6_LEARNING=PASS` se il beneficio è dimostrato e il costo/rischio accettabile; `M6_LEARNING=DEFERRED` se il motore statico è già soddisfacente o la modifica non conviene.
- **NEXT:** valutazione finale.

## Condizione di completamento

Porting candidato quando `M0`–`M5` sono PASS e `M6` è PASS o DEFERRED. Il percorso pubblico definitivo deve usare **un solo motore**, Milan in sostituzione di SIGFM, senza opzione per l'utente nell'installer. Un passaggio a `main`, release o modifica dei componenti stock non è implicito in questa roadmap.

I criteri generali di sicurezza, Human Gate, disciplina Git, orchestrazione e documentazione rimangono nei rispettivi file canonici; qui sono definiti soltanto obiettivo, fasi e exit gate del porting.
