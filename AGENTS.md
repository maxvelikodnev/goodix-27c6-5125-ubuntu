# AGENTS.md — Governance permanente Goodix 27c6:5125

Questo file definisce **solo regole permanenti** per gli agenti AI. Il programma e i criteri di avanzamento appartengono a `ROADMAP.md`; `START.md` contiene esclusivamente il bootstrap e il loop operativo.

## 1. Autorità e responsabilità dei file

Ordine di autorità: istruzione esplicita corrente dell'Utente → `AGENTS.md` → `ROADMAP.md` (fasi ed EXIT_GATE) → Git, codice ed evidenze osservabili → `TECHNICAL_MANUAL.md` (conoscenza tecnica corrente) → `PROJECT_STATE.json` (cache non autoritativa) → archivio storico in `development/` → fonti esterne e inferenze.

- `START.md`: avvio/ripresa e handoff PM–Executor–Reviewer.
- `ROADMAP.md`: obiettivi, sequenza, artefatti principali ed EXIT_GATE.
- `PROJECT_STATE.json`: stato operativo minimo, da riconciliare con Git/evidenze; non crea requisiti.
- `ARTIFACT_INDEX.json`: instradamento mirato del contesto, non autoritativo.
- `TECHNICAL_MANUAL.md`: reference pubblica dei fatti tecnici consolidati.
- `development/`: materiale di lavoro privato e archivio storico, già esclusi da Git; non è una fonte autoritativa dello stato presente.
- `CORROBORATIONS.md`, se presente: indice sintetico di prior art, non dossier.

`AGENTS.md`, `START.md` e `ROADMAP.md` sono **read-only per default**. Un agente li modifica solo con richiesta esplicita e specifica dell'Utente nella conversazione corrente, altrimenti presenta `HUMAN_REQUIRED / CANONICAL_FILE_CHANGE_APPROVAL` prima della modifica. Non basta un generico «procedi». Non creare file canonici mancanti per semplice conformità a un modello.

## 2. Sicurezza del sensore e della distribuzione

Target: Goodix USB `27c6:5125` con integrazione Fedora `libfprint → fprintd → PAM/KDE`, compatibilità Windows e preservazione factory. Altri sensori, sistemi o cambiamenti sostanziali richiedono decisione esplicita.

Invarianti:
- non flashare firmware, usare IAP/ClearApp, scrivere OTP o factory data, cambiare persistentemente VID:PID/modalità, né inviare comandi con effetti persistenti non compresi;
- non indebolire intenzionalmente la compatibilità Windows;
- fallire in modo chiuso quando sicurezza o stato persistente sono ambigui;
- un normale aggiornamento, errore o disinstallazione del fingerprint non deve compromettere password login, desktop, sudo o PolicyKit;
- non sostituire componenti stock Fedora/KDE/PAM/systemd con fork privati, né alterarne il comportamento globale, senza decisione specifica e necessità tecnica dimostrata.

**Eccezione delimitata — host pairing già qualificato:** solo nel percorso esplicitamente autorizzato; mai confonderlo con scritture firmware/factory. Contratto permanente:
```text
MAX_LOGICAL_E0_PER_TRANSACTION=1
AUTOMATIC_E0_RETRY=false
PRESERVE_CURRENT_BB010002_BYTE_FOR_BYTE=true
JOURNAL_LOCAL_PSK_BEFORE_E0=true
READBACK_HASH_TLS_PROOF_REQUIRED=true
FAIL_CLOSED_ON_AMBIGUITY=true
```
PSK Linux generata con CSPRNG e conservata root-only; mai usare PSK dummy/null, sintetizzare `BB010002`, esportare la PSK a Windows o emettere un nuovo `E0` in un normale reopen Linux già attivo. Nessuna implementazione esterna autorizza nuove scritture sul sensore.

## 3. Evidence-first e keep it easy

Distinguere `OBSERVED`, `PROVEN`, `INFERRED`, `ASSUMED` e `UNKNOWN`. Per una causa materiale valutare prove a favore/contro, alternative e il minimo esperimento discriminante.

Verificare le affermazioni contro repository, codice, test e target reali; l'archivio storico si consulta **solo con ricerca mirata** e non prova lo stato corrente. Non equiparare successo offline, build o VM a compatibilità live o sicurezza biometrica. Prima di fissare API, ABI e dipendenze di sistema, verificarne il comportamento reale senza modificare il target per adattarlo alla soluzione.

**Keep it easy / lean management:** scegliere il più piccolo intervento reversibile e verificabile; minimizzare letture, chiamate, token, test ripetuti e output non necessari. Riusare evidenze e strumenti esistenti. Non inventare framework, harness, report, subagent o procedure se non forniscono valore tecnico concreto. La riduzione dei costi AI non può eliminare verifiche o protezioni necessarie.

## 4. Git e integrità del lavoro

Il branch operativo è **quello correntemente selezionato dall'Utente nell'ambiente di lavoro**: non dedurlo dal default remoto, dai file di stato o da un nome hard-coded e non cambiarlo autonomamente.

Prima delle modifiche identificare root Git, branch, HEAD e worktree. Se il branch corrente non è determinabile univocamente, fermarsi con `HUMAN_REQUIRED / BRANCH_SELECTION_REQUIRED`. Preservare lavoro locale preesistente anche se non committato. Commit/push normali sono ammessi soltanto sul branch operativo e nei limiti dell'autorizzazione.

Senza autorizzazione specifica: no force push, rebase/amend pubblicati, merge/cherry-pick che alterino la topologia, creazione/cancellazione di branch, reset/stash distruttivi o sovrascrittura di lavoro altrui.

## 5. Human Gate e test reali

Fermarsi **prima** di azioni che richiedono consenso o intervento umano: USB/sensore reale, installazione runtime attivabile, sudo/root, accesso o manipolazione di PSK/biometria protetta, scritture persistenti, modifiche di sicurezza/licensing/scope, operazioni Git protette, release, distribuzione di materiale riservato e cambi dei file canonici non autorizzati. Anche un'ambiguità materiale non risolvibile autonomamente è un gate.

Un `HUMAN_REQUIRED` deve indicare concisamente: `GATE`, `PURPOSE`, `EXPECTED_RESULT`, `COMMANDS` se utili, `RISK`, `ROLLBACK`, `EVIDENCE_TO_RETURN`; nessuna cerimonia aggiuntiva se non necessaria.

Prima del live, completare le verifiche offline pertinenti e fornire all'Utente istruzioni brevi, limiti, criteri PASS/FAIL/STOP e rollback. L'agente non esegue autonomamente attività protette. Per VERIFY/MATCH sul lettore reale, salvo nuova istruzione: massimo tre tentativi fisici, stop al primo MATCH, nessun quarto tentativo o retry nascosto. Mantenere cleanup, release e fallimento sicuro in ogni percorso.

Quando un EXIT_GATE della roadmap è raggiunto, chiudere e registrare il lavoro, poi **attendere autorizzazione esplicita prima di avviare la fase successiva**. Non confondere questo controllo con richieste di approvazione per ogni piccolo task interno.

## 6. Documentazione, dati e provenienza

**`TECHNICAL_MANUAL.md` contiene conoscenza tecnica stabile, non cronaca dei test.**

**`TECHNICAL_MANUAL.md` contiene conoscenza tecnica stabile, non diario delle AI.**

**`TECHNICAL_MANUAL.md` contiene conoscenza tecnica stabile, non step-by-step dello sviluppo.**

Non inserirvi log, transcript, gate, commit o tentativi. Aggiornarlo solo quando cambia un fatto tecnico durevole verificato. L'archivio storico privato in `development/` rimane separato e non si riscrive senza richiesta esplicita.

Dati biometrici reali, catture, PSK, chiavi e materiale proprietario non pubblicabile possono risiedere **solo nelle aree private appropriate di `development/`**, già gitignored, con protezioni coerenti con la sensibilità dei dati. Non aggiungerli a commit, artifact pubblici, issue o log condivisi. Verificare l'esclusione da Git prima di creare nuovi materiali riservati.

Per codice di terzi preservare licenze e avvisi per file; verificare origine, revisioni e compatibilità di redistribuzione prima del riuso e aggiornare la provenance pubblica solo quando pertinente.

## 7. Orchestrazione e verifica

AI PM coordina i task; Executor implementa; Reviewer verifica in modo indipendente diff, codice, test e rischi prima di accettare. Subagent interni sono ammessi se utili, senza ampliare privilegi o permessi. I task descrivono soltanto il delta; nessuna ripetizione di queste regole negli handoff.

Esiti: `ACCEPT_AND_CONTINUE`, `CORRECTIVE`, `REPLAN`, `HUMAN_REQUIRED`, `BLOCKED`, `MILESTONE_COMPLETE`. L'ultimo si usa soltanto quando tutti i criteri finali della roadmap vigente sono dimostrati; test verdi o commit da soli non bastano. Aggiornare `PROJECT_STATE.json` solo per cambiamenti materiali e senza ricostruire diari.
