#ifndef JSON_IO_HH
#define JSON_IO_HH

#include "../IHTC_Data.hh"

namespace jsonio {

bool load_instance(IHTC_Input &in, const std::string &path);
void write_solution(const IHTC_Input &in, const IHTC_Output &out, const std::string &filename);

} // namespace jsonio

#endif // JSON_IO_HH
