<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# ROADMAP.md — Porting Milan Type 17 su Goodix 27c6:5125

## Obiettivo e percorso

Valutare e, se tecnicamente giustificato, sostituire SIGFM con il motore biometrico nativo Milan Type 17 nel driver Goodix `27c6:5125` per Fedora 44 KDE.

**PRIMARY:** porting mirato del motore (preprocessing, feature extraction, enrollment, matching, template) sulla cattura USB/TLS già funzionante del 5125.

**FALLBACK:** conservare SIGFM come implementazione ufficiale su `main` se Milan non supera i gate. Nessun matcher ibrido improvvisato e nessun doppio motore selezionabile dall'utente finale.

La promozione su `main` è una decisione successiva. Consultare `CORROBORATIONS.md` per C1 (codice upstream) e C2 (risposte del manutentore); usare il tag verificato `v0.1.0-19`, commit `4f48c482f35976ea4fac27d5e598730581c4f380`. I test differenziali sono **dichiarati dall'autore**, non riprodotti indipendentemente; non dimostrano compatibilità con il 5125.

## State machine

`M0 → M1 → M2 → M3 → M4 → M5 → M6 (opzionale) → valutazione finale`

**M0 è il primo gate di fattibilità:** identificare il tipo di algoritmo selezionato dall'Engine Adapter Windows del 5125; riga di parametri 12 e tipo di algoritmo 17 sono identificatori diversi. Risoluzione 80×64, firmware APP12508/APP12509 e provisioning ChicagoHS/Type 12 non provano compatibilità Type 17. Seguono gate M2 (immagini), M3 (matching) e M5 (uso live). Incompatibilità comprovata o regressione critica → `NO_GO`; tentare solo adattamenti minimi tecnicamente motivati, non algoritmi ibridi.

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

### M3 — Enrollment e matching comparati

- **PURPOSE:** confrontare Milan e SIGFM sugli stessi input con impronte autentiche, placements/sessioni distinti, dita diverse e catture vuote/scadenti; verificare serializzazione e ripresa. Partire con `flags=0`, senza learning; `flags=1` modifica anche logica di matching. Valutare prima decisioni a singolo frame: il voto 3/10 di FreshHomie è una policy del controller Linux, non una regola Windows dimostrata.
- **PRIMARY_ARTIFACT:** confronto minimo di decisioni, diagnostica, errori, falsi match osservati e limiti dei campioni, senza dichiarare FAR/FRR certificati.
- **EXIT_GATE:** `M3_MATCHING_VALUE=PASS` solo con vantaggio credibile o potenziale tecnico dimostrato e nessun falso match osservato; altrimenti `NO_GO`.
- **NEXT:** M4 solo con PASS.

### M4 — Integrazione candidata in libfprint

- **PURPOSE:** sostituire sperimentalmente SIGFM nel driver 5125 mantenendo USB/TLS/pairing/acquisizione esistenti. Per enrollment/verify/identify usare preferibilmente `libfprint` standard e `fprintd` stock: l'autore non segnala necessità intrinseca di un daemon modificato per le funzioni base. Distinguere i template Milan da GSF1/SIGFM e prevedere nuovo enrollment, senza conversioni implicite. Persistenza del learning fuori scope fino a M6.
- **PRIMARY_ARTIFACT:** build candidata Fedora/libfprint, test offline e rollback al driver SIGFM.
- **EXIT_GATE:** `M4_LIBFPRINT_CANDIDATE=PASS` se enrollment, verify, identify e regressioni pertinenti passano offline; nessuna installazione automatica.
- **NEXT:** M5.

### M5 — Qualificazione sul sensore reale

- **PURPOSE:** verificare con l'Utente sul sensore 5125 enrollment, match/no-match (anche altre dita/persone), identify, catture scadenti, login Fedora/KDE, password fallback, riavvio, ripristino e coesistenza Windows. Confrontare uso quotidiano con SIGFM. La configurazione upstream ha liveness disabilitata e non prova resistenza allo spoofing.
- **PRIMARY_ARTIFACT:** esito live e decisione `GO/NO_GO`, con regressioni, limiti del campione e rollback.
- **EXIT_GATE:** `M5_LIVE_QUALIFICATION=PASS` se affidabilità almeno comparabile a SIGFM nel campione qualificato, nessun falso match osservato, nessuna regressione critica del login e rollback praticabile; altrimenti `NO_GO`.
- **NEXT:** M6 solo se il learning mostra valore potenziale; altrimenti valutazione finale.

### M6 — Apprendimento incrementale (opzionale)

- **PURPOSE:** verificare se aggiornare il template dopo un match affidabile migliori realmente il riconoscimento; esaminare persistenza, rischio di contaminazione e l'eventuale necessità di modificare `fprintd`.
- **PRIMARY_ARTIFACT:** confronto con/senza apprendimento e decisione tecnica di inclusione o esclusione.
- **EXIT_GATE:** `M6_LEARNING=PASS` se il beneficio è dimostrato e il costo/rischio accettabile; `M6_LEARNING=DEFERRED` se il motore statico è già soddisfacente o la modifica non conviene.
- **NEXT:** valutazione finale.

## Condizione di completamento

Porting candidato quando `M0`–`M5` sono PASS e `M6` è PASS o DEFERRED. Il percorso pubblico definitivo deve usare **un solo motore**, Milan in sostituzione di SIGFM, senza opzione per l'utente nell'installer. Un passaggio a `main`, release o modifica dei componenti stock non è implicito in questa roadmap.

I criteri generali di sicurezza, Human Gate, disciplina Git, orchestrazione e documentazione rimangono nei rispettivi file canonici; qui sono definiti soltanto obiettivo, fasi e exit gate del porting.
