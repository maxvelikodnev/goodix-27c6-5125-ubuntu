# START.md — Avvio e ripresa dell'orchestrazione

Agisci come **AI PM**. Questo è il punto di ingresso per avviare o riprendere il lavoro, non un documento di governance o di pianificazione. Applica `AGENTS.md`; ricava programma e condizioni di avanzamento esclusivamente da `ROADMAP.md`.

## Bootstrap / recovery

1. Determina root Git, branch effettivamente selezionato, HEAD, stato del worktree e modifiche recenti. Non scegliere o cambiare branch per deduzione.
2. Leggi `AGENTS.md`, `START.md`, `ROADMAP.md` e, se presente, `CORROBORATIONS.md`; consulta `PROJECT_STATE.json` e `ARTIFACT_INDEX.json`.
3. Riconcilia la cache con Git, risultati ed evidenze; correggila se obsoleta. Individua il primo requisito non soddisfatto del programma corrente senza cancellare lavoro parziale.
4. Carica **solo** codice, test, sezioni di `TECHNICAL_MANUAL.md` e altri artefatti realmente pertinenti. Consulta l'archivio storico in `development/` soltanto per una ricerca mirata.

## Esecuzione e ripresa

Definisci un `CURRENT_TASK` breve con `GOAL`, `STATE_DELTA`, `SCOPE`, `REQUIRED_WORK`, `VERIFY`, `STOP_IF`. Affida l'implementazione a Executor e sottoponi il risultato a review indipendente. In caso di difetto, assegna un correttivo locale; in caso di ostacolo reale o decisione protetta, applica l'esito previsto da `AGENTS.md`.

Dopo ogni avanzamento significativo aggiorna la cache operativa e, solo se serve, il routing degli artefatti. Prosegui con il prossimo task consentito o fermati al gate applicabile. **Non ripetere regole canoniche negli handoff e non generare documenti o procedure superflui.**
