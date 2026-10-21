/* Poseidon over the BN254 scalar field (S11 step 11.3, O101): the circomlib 2.0.5
 * Poseidon(1) (t = 2) and Poseidon(2) (t = 3), computed in C by `langc eval`. */

#ifndef LANG_FRONT_POSEIDON_H
#define LANG_FRONT_POSEIDON_H

#include <stddef.h>

#include "u256.h"

/* Put the circomlib Poseidon(n) of the n words at in into *out, for n = 1 or 2.
 * The function takes each input word mod r first. It returns 0 on another n. */
int poseidon(U256 *out, const U256 *in, size_t n);

#endif
