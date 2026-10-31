/*=============================================================================
    Name    : datasetup.h
    Purpose : HWI-FEATURE(data-setup): the engine finds the game data itself, with a first-run step for a new
              player (docs/features/data-setup.md)
=============================================================================*/

#ifndef ___DATASETUP_H
#define ___DATASETUP_H

// utyGameSystemsPreInit, when HW_Data is not set: NULL and the data folder in path, or the reason it has none.
// optionsRow: the options file's HomeworldDataPath (GoK's own saved path; empty when the file has none)
char *dsGameDataFind(char *path, int pathSize, const char *optionsRow);

// TRUE when the player chose Quit in the first-run step (main.c: exit 0, no error line)
int dsQuitChosen(void);

// HWI-FEATURE(user-files): the player's Documents folder in path (1), or 0 when there is none
int dsDocumentsDir(char *out, int size);
// HWI-TEST: /catDataSetupScript <answers>: every dialog and picker answered from the list (main.c)
void dsTestScriptSet(char *answers);

#endif
