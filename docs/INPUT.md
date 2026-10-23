# Keys: legacy mode against modern mode

Both modes run the same engine and answer the same requests (`ReqKind` in
`src/engine/game.h`); what differs is how each screen reads the keyboard.
Legacy mode reads it as the original did (`src/legacy/`: `& INP`, `& SPACE`,
`USR`); modern mode has its own small set of readers (`src/modern/modern.c`:
`choose`, `yesno`, `text_input`/`number`, `wait_continue`, `wait_tune`). The
hunt, the raft and the travel poll are the same code in both. Checked against
the code, and Esc Esc with the harness (below).

## The readers

| Kind of prompt | Legacy (the original) | Modern |
|---|---|---|
| A numbered choice | the digit, then Return (`& INP,1,"-1n",1`); Delete or left arrow rubs the digit out before Return | the digit picks at once (`choose`), and a Return straight after it is let go (not passed to the next screen); or a click on the choice |
| Yes / no | Y or N (or YES / NO, either case), then Return (`sh_yesno`: `& INP,3,"YESNOyesno"`); asks again on anything else | shown as "1. Yes" and "2. No": 1 or 2 at once, or Y or N, either case (`yesno`); or a click |
| A typed field (names, numbers, epitaph, Top Ten name) | the letters the field allows, then Return; Delete / left arrow rubs out (`& INP`) | the same letters, then Return; Backspace / Delete rubs out (`text_input`) |
| A number out of range | asks again | asks again (`number`) |
| "Press SPACE BAR to continue" | Space or Return (`& SPACE`) | Space or Return (`wait_continue`); or a click |
| "Press SPACE BAR" with the tune (the leg message, Independence and the landmarks after looking around, the arrival, the tombstone, all dead) | any key but Ctrl-S (`sh_space_tune`: `USR(2)`) | the same (`wait_tune`) |
| Matt's store | 1-5 then Return; the space bar leaves at once (`BUY SUPPLIES:250`) | 1-5 at once; the space bar leaves, and so does choice 6, "Leave store" |
| The main menu | 1-4 then Return; Ctrl-A then Return: the management options (`MENU:1000`) | 1-4 at once; Ctrl-A: the management options |
| Travel | Return stops at the next day (`USR(3) = 141`) | the same (`REQ_POLL`) |
| The hunt | arrows / `; P O I K , . /`, Space fires, Return walks, Esc Esc leaves (`hunt.c`) | the same code |
| The raft | left / up, right / down (`USR(3)`) | the same keys |
| Ctrl-S | sound on / off wherever the original reads a key | the same (`key()`) |
| Esc Esc | from any prompt: back to the main menu, the game abandoned (`GOTO 31000`) | the same (`key()`; the hunt its own) |
| Esc (the window's) | the quit dialog (Y or N); Shift+Esc is the Apple's Esc | the same, its answers numbered: 1 or Y quits, 2 or N goes on |

Checked with the harness: at the month question, Esc Esc ends the game and
shows the main menu, in both modes.

## The differences

Every choice in modern mode is numbered and picked by its digit, yes / no
included (1 Yes, 2 No), so any menu can be answered with the number keys
alone.

In modern mode Esc Esc leaves the game too, the tune waits go on at any key,
and Space leaves Matt's store. Modern choices and yes/no answer at once, with a
Return straight after a choice let go so "1 Return" cannot pass the next
screen unread (the travel poll included); YES and NO as words work only in
legacy mode.

## Touch (modern mode)

On a phone, or on the web once a touch is seen, or with `--touch` on the
desktop (`build/harness --touch` and its `tap X Y` / `tapopt N` commands for
scripts), modern mode is played by tapping (`modern_tap()` in
`src/modern/draw.c`), and its words say "tap" instead of naming keys:

| Prompt | Tap |
|---|---|
| A numbered choice, yes / no | the choice (rows at least 20 pixels tall) |
| "Tap to continue" | anywhere |
| Travel | anywhere: Return, to size up the situation at the next stop |
| A message that can be sized up (an event, a part) | "1. Continue" (Space) or "2. Size up the situation" (Return) |
| A typed field | the keyboard drawn under the panel: letters (capital at a word's start; `^` for one capital), `' . - ,`, `123` for digits where the field takes them, Space, Del, OK |
| A number | the number pad: 0-9, Del, OK |
| The hunt | the field: the hunter walks to that spot, round what is in the way, and stops there; a double-tap turns him that way and fires (the hunt's own keys: the expert key for a direction, Return, Space) |
| The raft | the left or right side of the screen |
| The menu button (top right) | a menu: Continue, sound on / off (Ctrl-S), and Quit to the main menu (Esc Esc), Stop hunting in the hunt, or Management options at the main menu (Ctrl-A); Android's Back opens it too |
