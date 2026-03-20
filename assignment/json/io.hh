#ifndef JSON_IO_HH
#define JSON_IO_HH

#include "IHTC_Data.hh"

// JSON I/O interface for the IHTC solver.
namespace jsonio {

// parses a competition JSON file into IHTC_Input.
bool load_instance(IHTC_Input &in, const std::string &path);
// serializes an IHTC_Output to a solution JSON file.
void write_solution(const IHTC_Input &in, const IHTC_Output &out, const std::string &filename);

} // namespace jsonio

#endif // JSON_IO_HH
