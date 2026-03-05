#ifndef _IHTC_GREEDY_HH_
#define _IHTC_GREEDY_HH_

#include "IHTC_Data.hh"

namespace GreedySolver {

void solvePASandSCP(const IHTC_Input& in, IHTC_Output& out);
void solveNRA(const IHTC_Input& in, IHTC_Output& out);
void runFullSolver(const IHTC_Input& in, IHTC_Output& out);

} // namespace GreedySolver

#endif
